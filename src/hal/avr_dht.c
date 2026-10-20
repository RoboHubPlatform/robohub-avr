/*
 * avr_dht.c — DHT interface implementation for AVR parts.
 *
 * The conversation: the host pulls the line low for at least eighteen
 * milliseconds, which is the part's cue to measure; the part answers with a
 * low and a high of about eighty microseconds each; and then forty bits follow,
 * each of them a fifty-microsecond low followed by a high whose length says
 * whether the bit is a zero or a one.
 *
 * A bit is read by looking at the line a little after the low ends: a one is
 * still high then, and a zero has already gone low. That is why the sample is
 * taken at a fixed moment rather than measured.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_dht.h"

#include <avr/io.h>

#include <stddef.h>

/* The timings of the conversation, in microseconds. */
#define DHT_START_LOW_US      18000u
#define DHT_RESPONSE_LOW_US      80u
#define DHT_RESPONSE_HIGH_US     80u
#define DHT_BIT_LOW_US           50u
#define DHT_BIT_SAMPLE_US        40u

static void dht_drive_low(RoboHubAVRDHT* dht)
{
    if (dht->pin < 8u) {
        PORTB &= (uint8_t)~(1u << dht->pin);
        DDRB |= (uint8_t)(1u << dht->pin);
    } else {
        PORTD &= (uint8_t)~(1u << (dht->pin - 14u));
        DDRD |= (uint8_t)(1u << (dht->pin - 14u));
    }
}

static void dht_release(RoboHubAVRDHT* dht)
{
    if (dht->pin < 8u) {
        DDRB &= (uint8_t)~(1u << dht->pin);
    } else {
        DDRD &= (uint8_t)~(1u << (dht->pin - 14u));
    }
}

static rh_bool dht_level(RoboHubAVRDHT* dht)
{
    if (dht->pin < 8u) {
        return (PINB & (uint8_t)(1u << dht->pin)) != 0u ? RH_TRUE : RH_FALSE;
    }
    return (PIND & (uint8_t)(1u << (dht->pin - 14u))) != 0u ? RH_TRUE : RH_FALSE;
}

static void dht_delay(RoboHubAVRDHT* dht, rh_u32 us)
{
    if (dht->timer != NULL && dht->timer->delay_us != NULL) {
        dht->timer->delay_us(dht->timer, us);
    }
}

static RoboHubError avr_dht_op_init(RoboHubDHT* self)
{
    RoboHubAVRDHT* dht = (RoboHubAVRDHT*)self->context;
    if (dht->timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dht_release(dht);
    return ROBOHUB_OK;
}

static RoboHubError avr_dht_op_deinit(RoboHubDHT* self)
{
    (void)self;
    return ROBOHUB_OK;
}

static RoboHubError avr_dht_op_read(RoboHubDHT* self, rh_u8* data, rh_size len)
{
    RoboHubAVRDHT* dht = (RoboHubAVRDHT*)self->context;
    if (data == NULL || len == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* The cue: hold the line low long enough for the part to notice. */
    dht_drive_low(dht);
    dht_delay(dht, DHT_START_LOW_US);
    dht_release(dht);

    /* The part answers with a low and a high; if it does not, it is not there. */
    dht_delay(dht, 30u);
    if (dht_level(dht) == RH_TRUE) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    dht_delay(dht, DHT_RESPONSE_LOW_US);
    if (dht_level(dht) == RH_FALSE) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    dht_delay(dht, DHT_RESPONSE_HIGH_US);

    /* Forty bits: five bytes, the first of which is the humidity's whole part,
     * the second its decimals, the third the temperature's whole part, the
     * fourth its decimals, and the fifth the checksum. */
    for (rh_size byte = 0; byte < len; byte++) {
        rh_u8 value = 0u;
        for (rh_u8 bit = 0; bit < 8u; bit++) {
            dht_delay(dht, DHT_BIT_LOW_US);
            dht_delay(dht, DHT_BIT_SAMPLE_US - DHT_BIT_LOW_US);
            rh_u64 one = (dht_level(dht) == RH_TRUE) ? 1u : 0u;
            value = (rh_u8)((value << 1) | one);
            /* The rest of the slot, so the next bit starts where it should. */
            dht_delay(dht, 60u - (DHT_BIT_SAMPLE_US - DHT_BIT_LOW_US));
        }
        data[byte] = value;
    }
    return ROBOHUB_OK;
}

RoboHubError avr_dht_create(RoboHubAVRDHT* dht, rh_name name, rh_u32 pin, RoboHubTimer* timer)
{
    if (dht == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    dht->pin = pin;
    dht->timer = timer;
    dht->interface.name = name;
    dht->interface.context = dht;
    dht->interface.init = avr_dht_op_init;
    dht->interface.deinit = avr_dht_op_deinit;
    dht->interface.read = avr_dht_op_read;
    return ROBOHUB_OK;
}
