/*
 * avr_spi.c — SPI interface implementation for AVR parts.
 *
 * The peripheral shifts a byte out and a byte in at the same time, so a transfer
 * sends and receives together: the bytes to send are written one after another
 * and the bytes that came back are collected one after another, and the last
 * byte exchanged is the one that finishes the transfer.
 *
 * The prescaler is chosen from the requested speed: the peripheral runs at half
 * the system clock, and the divider is the one nearest the speed asked for.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_spi.h"

#include <avr/io.h>

#include <stddef.h>

/* The UNO's SPI pins: D13 is the clock, D12 the input and D11 the output. */
#define AVR_SPI_MOSI 11u
#define AVR_SPI_MISO 12u
#define AVR_SPI_SCK  13u
#define AVR_SPI_SS   10u

static RoboHubError avr_spi_op_init(RoboHubSPI* self, rh_u32 speed_hz)
{
    RoboHubAVRSPI* spi = (RoboHubAVRSPI*)self->context;
    if (speed_hz == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    spi->speed_hz = speed_hz;

    /* The pins the peripheral drives have to be outputs before it drives them. */
    DDRB |= (uint8_t)((1u << AVR_SPI_MOSI) | (1u << AVR_SPI_SCK) | (1u << AVR_SPI_SS));
    DDRB &= (uint8_t)~(1u << AVR_SPI_MISO);
    PORTB |= (uint8_t)(1u << AVR_SPI_SS);

    /* The peripheral runs at half the system clock; the divider is the one
     * nearest the speed asked for. */
    uint8_t divider = 0u; /* /4 */
    uint32_t base = F_CPU / 2u;
    uint32_t rate = base / 4u;
    for (rh_u8 step = 0; step < 3u; step++) {
        uint32_t candidate = base >> (step + 2u);
        if (candidate > speed_hz) {
            rate = candidate;
            divider = (uint8_t)(step + 1u);
            break;
        }
        rate = candidate;
        divider = (uint8_t)(step + 2u);
    }
    if (rate < speed_hz) {
        rate = base >> 3u;
        divider = 3u;
    }

    SPSR = 0;
    /* Mode 0: the clock idles low and is sampled on the rising edge. */
    SPCR = (uint8_t)((1u << SPE) | (1u << MSTR) | (divider << 0));
    return ROBOHUB_OK;
}

static RoboHubError avr_spi_op_transfer(RoboHubSPI* self, const rh_u8* tx, rh_u8* rx, rh_size len)
{
    (void)self;
    if (len == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* The bytes that come back are the ones shifted in while the bytes went
     * out, so the last byte exchanged is collected separately: it is still in the
     * shift register when the loop ends. */
    for (rh_size i = 0; i + 1u < len; i++) {
        while (!(SPSR & (uint8_t)(1u << SPIF))) {
        }
        SPDR = (tx != NULL) ? tx[i] : 0xFFu;
        if (rx != NULL) {
            rx[i] = SPDR;
        }
    }
    while (!(SPSR & (uint8_t)(1u << SPIF))) {
    }
    SPDR = (tx != NULL) ? tx[len - 1u] : 0xFFu;
    if (rx != NULL) {
        rx[len - 1u] = SPDR;
    }
    return ROBOHUB_OK;
}

RoboHubError avr_spi_create(RoboHubAVRSPI* spi, rh_name name)
{
    if (spi == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    spi->speed_hz = 0u;
    spi->interface.name = name;
    spi->interface.context = spi;
    spi->interface.init = avr_spi_op_init;
    spi->interface.transfer = avr_spi_op_transfer;
    return ROBOHUB_OK;
}
