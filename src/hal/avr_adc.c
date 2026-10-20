/*
 * avr_adc.c — ADC interface implementation for AVR parts.
 *
 * One conversion per read: the input is selected, the converter is started, and
 * the result is taken. The first conversion after the converter is enabled takes
 * longer than the rest, so the first reading is taken twice and the second is
 * kept — a part that has just been woken reads its own supply.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_adc.h"

#include <avr/io.h>

#include <stddef.h>

static RoboHubError avr_adc_op_init(RoboHubADC* self)
{
    (void)self;
    /* The reference is the part's supply, and the prescaler is 128, which puts
     * the converter at 125 kHz — inside what the datasheet asks for at this
     * clock. */
    ADMUX = (uint8_t)(1u << REFS0);
    ADCSRA = (uint8_t)((1u << ADEN) | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0));
    return ROBOHUB_OK;
}

static RoboHubError avr_adc_op_deinit(RoboHubADC* self)
{
    (void)self;
    ADCSRA = 0;
    return ROBOHUB_OK;
}

static rh_u8 avr_adc_op_channels(RoboHubADC* self)
{
    (void)self;
    return AVR_ADC_CHANNELS;
}

static rh_u8 avr_adc_op_resolution_bits(RoboHubADC* self)
{
    (void)self;
    return AVR_ADC_BITS;
}

static RoboHubError avr_adc_op_read(RoboHubADC* self, rh_u8 channel, rh_f32* level)
{
    (void)self;
    if (level == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (channel >= AVR_ADC_CHANNELS) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ADMUX = (uint8_t)((1u << REFS0) | (channel & 0x07u));
    for (rh_u8 attempt = 0; attempt < 2u; attempt++) {
        ADCSRA |= (uint8_t)(1u << ADSC);
        while (ADCSRA & (uint8_t)(1u << ADSC)) {
            /* The conversion is running; wait for it to finish. */
        }
    }
    uint16_t raw = ADC;
    *level = (rh_f32)raw / (rh_f32)((1u << AVR_ADC_BITS) - 1u);
    return ROBOHUB_OK;
}

RoboHubError avr_adc_create(RoboHubAVRADC* adc, rh_name name)
{
    if (adc == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    adc->interface.name = name;
    adc->interface.context = adc;
    adc->interface.init = avr_adc_op_init;
    adc->interface.deinit = avr_adc_op_deinit;
    adc->interface.channels = avr_adc_op_channels;
    adc->interface.resolution_bits = avr_adc_op_resolution_bits;
    adc->interface.read = avr_adc_op_read;
    return ROBOHUB_OK;
}
