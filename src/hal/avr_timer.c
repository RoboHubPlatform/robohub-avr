/*
 * avr_timer.c — timer interface implementation for AVR parts.
 *
 * Two counters, two jobs. The eight-bit one overflows every millisecond and its
 * interrupt is the millisecond clock. The sixteen-bit one runs free at half a
 * microsecond a count, which is what a delay waits on and what the microsecond
 * clock is read from.
 *
 * The millisecond counter and the free-running counter are read together, and
 * the reading is the millisecond count plus the fraction of the current
 * millisecond — the counter's value at the last millisecond boundary is kept, so
 * the fraction is the difference and not a guess.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include <stddef.h>

/* The millisecond count, and the free-running counter's value at the last
 * boundary. */
static volatile uint32_t avr_millis;
static volatile uint16_t avr_ticks_at_millisecond;

ISR(TIMER0_OVF_vect)
{
    avr_millis++;
    avr_ticks_at_millisecond = TCNT1;
}

void avr_timer_start(void)
{
    /* Timer0: overflow every millisecond. The prescaler is 64, so a count is
     * 4 us and 250 of them make the millisecond. */
    TCCR0A = 0;
    TCCR0B = (uint8_t)((1u << WGM02) | (1u << CS01) | (1u << CS00)); /* fast PWM, /64 */
    TIMSK0 = (uint8_t)(1u << TOIE0);
    sei();

    /* Timer1: free-running, counting to the top and wrapping. The prescaler is
     * 8, so a count is half a microsecond. */
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1C = 0;
    TIFR1 = 0;
    TCNT1 = 0;
    TCCR1B = (uint8_t)(1u << CS11); /* /8 */
}

static rh_millis avr_timer_get_millis(RoboHubTimer* self)
{
    (void)self;
    /* The millisecond count is a single volatile read, which is atomic on this
     * part: a boundary that arrives between the read and the use moves the
     * clock on by one, which is a millisecond late rather than wrong. */
    return (rh_millis)avr_millis;
}

static rh_micros avr_timer_get_micros(RoboHubTimer* self)
{
    (void)self;
    uint32_t millis;
    uint16_t ticks;
    uint16_t now = TCNT1;
    do {
        millis = avr_millis;
        ticks = avr_ticks_at_millisecond;
    } while (millis != avr_millis);
    /* The fraction of the millisecond, in microseconds: half a count each. */
    uint16_t elapsed = (uint16_t)(now - ticks);
    return (rh_micros)((rh_u64)millis * 1000u + ((rh_u64)elapsed >> 1));
}

static void avr_timer_delay_ms(RoboHubTimer* self, rh_u32 ms)
{
    (void)self;
    uint32_t start = avr_millis;
    while ((uint32_t)(avr_millis - start) < ms) {
        /* Wait for the clock to move. */
    }
}

static void avr_timer_delay_us(RoboHubTimer* self, rh_u32 us)
{
    (void)self;
    /* Wait on the counter itself, so an interrupt in the middle does not make
     * the delay short: the counter keeps counting through it. */
    uint16_t start = TCNT1;
    uint16_t ticks = (uint16_t)((us * 2u) + 1u);
    while ((uint16_t)(TCNT1 - start) < ticks) {
    }
}

RoboHubError avr_timer_create(RoboHubAVRTimer* timer, rh_name name)
{
    if (timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    timer->interface.name = name;
    timer->interface.context = timer;
    timer->interface.get_millis = avr_timer_get_millis;
    timer->interface.get_micros = avr_timer_get_micros;
    timer->interface.delay_ms = avr_timer_delay_ms;
    timer->interface.delay_us = avr_timer_delay_us;
    return ROBOHUB_OK;
}
