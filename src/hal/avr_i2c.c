/*
 * avr_i2c.c — I2C interface implementation for AVR parts.
 *
 * The TWI peripheral is a state machine of its own, and every step here waits
 * for it to say it has finished and then reads what status it finished with. A
 * wait that gives up is a timeout rather than a hang: a bus with a device that
 * holds it low would otherwise stop the program.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_i2c.h"

#include <avr/io.h>

#include <stddef.h>

/* How long the peripheral is given to finish a step, in loops of a few
 * microseconds. A bus that is not there gives up rather than waiting forever. */
#define AVR_I2C_TIMEOUT 200000u

/* The status codes the peripheral reports. */
#define AVR_TWI_START         0x08u
#define AVR_TWI_REP_START      0x10u
#define AVR_TWI_MT_RECEIVED    0x18u
#define AVR_TWI_MT_ADDRESSED   0x28u
#define AVR_TWI_MT_DATA        0x28u
#define AVR_TWI_MR_RECEIVED    0x40u
#define AVR_TWI_MR_ADDRESSED   0x50u
#define AVR_TWI_NO_STATE       0xF8u
#define AVR_TWI_BUS_ERROR      0x00u

static rh_bool avr_i2c_wait(void)
{
    for (rh_u32 attempt = 0; attempt < AVR_I2C_TIMEOUT; attempt++) {
        if (TWCR & (uint8_t)(1u << TWINT)) {
            return RH_TRUE;
        }
    }
    return RH_FALSE;
}

static rh_u8 avr_i2c_status(void)
{
    return (rh_u8)((TWSR & (uint8_t)0xF8u) >> 3);
}

/* A start, and the address that follows it. The address is the seven-bit one
 * the caller gave, shifted left with the read bit in the low position. */
static RoboHubError avr_i2c_start(RoboHubAVRI2C* i2c, rh_u8 addr, rh_bool reading, rh_bool repeated)
{
    (void)i2c;
    TWCR = (uint8_t)((1u << TWEN) | (1u << TWINT) | (1u << TWSTA));
    if (!avr_i2c_wait()) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    if (avr_i2c_status() != (reading ? AVR_TWI_REP_START : AVR_TWI_START)) {
        return ROBOHUB_ERR_IO;
    }
    TWDR = (uint8_t)((addr << 1) | (reading ? 1u : 0u));
    TWCR = (uint8_t)((1u << TWEN) | (1u << TWINT) | (repeated ? (1u << TWSTA) : 0u));
    if (!avr_i2c_wait()) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    rh_u8 status = avr_i2c_status();
    if (status == AVR_TWI_NO_STATE || status == AVR_TWI_BUS_ERROR) {
        return ROBOHUB_ERR_IO;
    }
    if (status != (reading ? AVR_TWI_MR_ADDRESSED : AVR_TWI_MT_ADDRESSED)) {
        return ROBOHUB_ERR_NOT_FOUND;
    }
    return ROBOHUB_OK;
}

static void avr_i2c_stop(void)
{
    TWCR = (uint8_t)((1u << TWEN) | (1u << TWINT) | (1u << TWSTO));
}

static RoboHubError avr_i2c_write_byte(RoboHubAVRI2C* i2c, rh_u8 value)
{
    (void)i2c;
    TWDR = value;
    TWCR = (uint8_t)((1u << TWEN) | (1u << TWINT));
    if (!avr_i2c_wait()) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    rh_u8 status = avr_i2c_status();
    if (status != AVR_TWI_MT_DATA) {
        return ROBOHUB_ERR_IO;
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_i2c_read_byte(RoboHubAVRI2C* i2c, rh_u8* value, rh_bool last)
{
    (void)i2c;
    TWCR = (uint8_t)((1u << TWEN) | (1u << TWINT) | (last ? (1u << TWSTO) : 0u));
    if (!avr_i2c_wait()) {
        return ROBOHUB_ERR_TIMEOUT;
    }
    *value = TWDR;
    return ROBOHUB_OK;
}

static RoboHubError avr_i2c_op_init(RoboHubI2C* self, rh_u32 speed_hz)
{
    RoboHubAVRI2C* i2c = (RoboHubAVRI2C*)self->context;
    if (speed_hz == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    i2c->speed_hz = speed_hz;

    /* The SCL period is 16 + 2 * TWBR clocks, and the peripheral counts the
     * prescaler of four; the divider is the one nearest the speed asked for. */
    uint32_t divider = 72u; /* 100 kHz at 16 MHz */
    if (speed_hz >= 400000u) {
        divider = 18u;
    } else if (speed_hz >= 100000u) {
        divider = 72u;
    } else {
        divider = 288u;
    }
    TWSR = 0;
    TWBR = (uint8_t)divider;
    TWCR = (uint8_t)(1u << TWEN);
    return ROBOHUB_OK;
}

static RoboHubError avr_i2c_op_write(RoboHubI2C* self, rh_u8 addr, const rh_u8* data, rh_size len)
{
    RoboHubAVRI2C* i2c = (RoboHubAVRI2C*)self->context;
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubError err = avr_i2c_start(i2c, addr, RH_FALSE, RH_FALSE);
    if (ROBOHUB_FAILED(err)) {
        avr_i2c_stop();
        return err;
    }
    for (rh_size i = 0; i < len; i++) {
        err = avr_i2c_write_byte(i2c, data[i]);
        if (ROBOHUB_FAILED(err)) {
            avr_i2c_stop();
            return err;
        }
    }
    avr_i2c_stop();
    return ROBOHUB_OK;
}

static RoboHubError avr_i2c_op_read(RoboHubI2C* self, rh_u8 addr, rh_u8* data, rh_size len)
{
    RoboHubAVRI2C* i2c = (RoboHubAVRI2C*)self->context;
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubError err = avr_i2c_start(i2c, addr, RH_TRUE, RH_FALSE);
    if (ROBOHUB_FAILED(err)) {
        avr_i2c_stop();
        return err;
    }
    for (rh_size i = 0; i < len; i++) {
        err = avr_i2c_read_byte(i2c, &data[i], (i + 1u == len) ? RH_TRUE : RH_FALSE);
        if (ROBOHUB_FAILED(err)) {
            avr_i2c_stop();
            return err;
        }
    }
    avr_i2c_stop();
    return ROBOHUB_OK;
}

RoboHubError avr_i2c_create(RoboHubAVRI2C* i2c, rh_name name)
{
    if (i2c == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    i2c->speed_hz = 0u;
    i2c->interface.name = name;
    i2c->interface.context = i2c;
    i2c->interface.init = avr_i2c_op_init;
    i2c->interface.write = avr_i2c_op_write;
    i2c->interface.read = avr_i2c_op_read;
    return ROBOHUB_OK;
}
