/*
 * avr_pwm.c — PWM interface implementation for AVR parts.
 *
 * The counter runs at the system clock over a prescaler, and reaches its wrap
 * before starting again; the frequency is the clock over the prescaler and the
 * wrap together. The prescaler is chosen per timer so that the wrap fits the
 * counter it belongs to, and the duty is a fraction of that wrap.
 *
 * Two timers are used: the sixteen-bit one for D9 and D10, the eight-bit one for
 * D11 and D3. The eight-bit timer is Timer2 — Timer0 is this platform's
 * millisecond clock, and a timer cannot be two things at once.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_pwm.h"

#include <avr/io.h>

#include <stddef.h>

/* The pins behind the channels, in channel order: D9, D10 on the sixteen-bit
 * timer, D11 and D3 on the eight-bit one. */
#define AVR_PWM_PINS { 9u, 10u, 11u, 3u }

/* The prescalers, in the order they are tried: the finest that still gives a
 * wrap the counter can hold. */
static const uint16_t avr_timer1_prescalers[] = { 1u, 8u, 64u, 256u, 1024u };
static const uint16_t avr_timer2_prescalers[] = { 1u, 8u, 32u, 64u, 128u, 256u, 1024u };

/* The clock-control bits a prescaler is written as. The two timers spell their
 * prescalers differently, which is why this is two functions and not one. */
static uint8_t avr_timer1_cs(uint16_t prescaler)
{
    switch (prescaler) {
    case 1u:   return 0u;
    case 8u:   return (uint8_t)(1u << CS10);
    case 64u:  return (uint8_t)(1u << CS11);
    case 256u: return (uint8_t)((1u << CS11) | (1u << CS10));
    case 1024u: return (uint8_t)(1u << CS12);
    default:   return 0u;
    }
}

static uint8_t avr_timer2_cs(uint16_t prescaler)
{
    switch (prescaler) {
    case 1u:   return 0u;
    case 8u:   return (uint8_t)(1u << CS21);
    case 32u:  return (uint8_t)(1u << CS22);
    case 64u:  return (uint8_t)((1u << CS22) | (1u << CS21));
    case 128u: return (uint8_t)(1u << CS22);
    case 256u: return (uint8_t)((1u << CS22) | (1u << CS20));
    case 1024u: return (uint8_t)((1u << CS22) | (1u << CS21));
    default:   return 0u;
    }
}

/* The wrap a prescaler and a frequency give, or zero when the timer cannot
 * reach it. */
static uint16_t avr_wrap_for(uint32_t clock, uint16_t prescaler, uint32_t frequency_hz, uint16_t limit)
{
    uint32_t counts = (clock / prescaler) / frequency_hz;
    if (counts < 2u) {
        return 0u;
    }
    if (counts - 1u > (uint32_t)limit) {
        return 0u;
    }
    return (uint16_t)(counts - 1u);
}

static void avr_pwm_apply(RoboHubAVRPWM* pwm, rh_u8 channel)
{
    rh_f32 duty = pwm->duty[channel];
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    uint16_t level = (uint16_t)(duty * (rh_f32)pwm->top);
    if (channel < 2u) {
        if (channel == 0u) {
            OCR1A = level;
        } else {
            OCR1B = level;
        }
    } else {
        if (channel == 2u) {
            OCR2A = level;
        } else {
            OCR2B = level;
        }
    }
}

static RoboHubError avr_pwm_op_init(RoboHubPWM* self)
{
    RoboHubAVRPWM* pwm = (RoboHubAVRPWM*)self->context;
    static const uint32_t pins[AVR_PWM_CHANNELS] = AVR_PWM_PINS;

    for (rh_u8 i = 0; i < AVR_PWM_CHANNELS; i++) {
        pwm->pin[i] = pins[i];
        pwm->duty[i] = 0.0f;
        /* The pin has to be an output before the peripheral drives it, or the
         * first pulse is the pin fighting the output driver. */
        if (pins[i] < 8u) {
            DDRB |= (uint8_t)(1u << pins[i]);
        } else if (pins[i] < 14u) {
            DDRC |= (uint8_t)(1u << (pins[i] - 8u));
        } else {
            DDRD |= (uint8_t)(1u << (pins[i] - 14u));
        }
    }

    /* The sixteen-bit timer, in fast PWM with the wrap as its top. */
    TCCR1A = 0;
    TCCR1B = (uint8_t)((1u << WGM11) | (1u << WGM10) | (1u << COM1B1) | (1u << COM1A1));
    TCCR1C = 0;

    /* The eight-bit timer, in fast PWM with the wrap as its top. */
    TCCR2A = (uint8_t)((1u << WGM21) | (1u << WGM20) | (1u << COM2B1) | (1u << COM2A1));
    TCCR2B = 0;

    pwm->top = 0xFFFFu;
    pwm->frequency_hz = 0u;
    pwm->ready = RH_TRUE;
    return ROBOHUB_OK;
}

static RoboHubError avr_pwm_op_deinit(RoboHubPWM* self)
{
    RoboHubAVRPWM* pwm = (RoboHubAVRPWM*)self->context;
    pwm->ready = RH_FALSE;
    TCCR1B = 0;
    TCCR2B = 0;
    return ROBOHUB_OK;
}

static rh_u8 avr_pwm_op_channels(RoboHubPWM* self)
{
    (void)self;
    return AVR_PWM_CHANNELS;
}

static RoboHubError avr_pwm_op_set_frequency(RoboHubPWM* self, rh_u32 frequency_hz)
{
    RoboHubAVRPWM* pwm = (RoboHubAVRPWM*)self->context;
    if (!pwm->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (frequency_hz == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* The two timers have different counters, so each is given the finest
     * prescaler that can still hold the wrap its frequency asks for. */
    uint16_t top1 = 0u;
    uint16_t divider1 = 0u;
    for (rh_u8 i = 0; i < 5u; i++) {
        uint16_t prescaler = avr_timer1_prescalers[i];
        top1 = avr_wrap_for(F_CPU, prescaler, frequency_hz, 0xFFFFu);
        if (top1 != 0u) {
            divider1 = prescaler;
            break;
        }
    }
    uint16_t top2 = 0u;
    uint16_t divider2 = 0u;
    for (rh_u8 i = 0; i < 7u; i++) {
        uint16_t prescaler = avr_timer2_prescalers[i];
        top2 = avr_wrap_for(F_CPU, prescaler, frequency_hz, 0xFFu);
        if (top2 != 0u) {
            divider2 = prescaler;
            break;
        }
    }
    if (top1 == 0u || top2 == 0u) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    /* The sixteen-bit timer: its top is the input capture register, which is
     * what a fast PWM mode with a chosen period uses. */
    ICR1 = top1;
    OCR1A = 0;
    OCR1B = 0;
    TCCR1B = (uint8_t)((1u << WGM11) | (1u << WGM10) | (1u << COM1B1) | (1u << COM1A1) |
                        avr_timer1_cs(divider1));

    /* The eight-bit timer has no input capture register, so its period is the
     * larger of its two compare registers and the other is the duty. */
    OCR2A = top2;
    OCR2B = 0;
    TCCR2A = (uint8_t)((1u << WGM21) | (1u << WGM20) | (1u << COM2B1) | (1u << COM2A1));
    TCCR2B = avr_timer2_cs(divider2);

    pwm->top = top1;
    pwm->frequency_hz = frequency_hz;
    /* The duties were written against the old wrap; write them again. */
    for (rh_u8 i = 0; i < AVR_PWM_CHANNELS; i++) {
        avr_pwm_apply(pwm, i);
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_pwm_op_set_duty(RoboHubPWM* self, rh_u8 channel, rh_f32 duty)
{
    RoboHubAVRPWM* pwm = (RoboHubAVRPWM*)self->context;
    if (!pwm->ready) {
        return ROBOHUB_ERR_STATE;
    }
    if (channel >= AVR_PWM_CHANNELS) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    pwm->duty[channel] = duty;
    avr_pwm_apply(pwm, channel);
    return ROBOHUB_OK;
}

static RoboHubError avr_pwm_op_stop(RoboHubPWM* self, rh_u8 channel)
{
    return avr_pwm_op_set_duty(self, channel, 0.0f);
}

RoboHubError avr_pwm_create(RoboHubAVRPWM* pwm, rh_name name)
{
    if (pwm == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    pwm->top = 0xFFFFu;
    pwm->frequency_hz = 0u;
    pwm->ready = RH_FALSE;

    pwm->interface.name = name;
    pwm->interface.context = pwm;
    pwm->interface.init = avr_pwm_op_init;
    pwm->interface.deinit = avr_pwm_op_deinit;
    pwm->interface.channels = avr_pwm_op_channels;
    pwm->interface.set_frequency = avr_pwm_op_set_frequency;
    pwm->interface.set_duty = avr_pwm_op_set_duty;
    pwm->interface.stop = avr_pwm_op_stop;
    return ROBOHUB_OK;
}
