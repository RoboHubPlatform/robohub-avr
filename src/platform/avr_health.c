/**
 * @file avr_health.c
 * @brief robohub-common's health contracts on the ATmega328P (AVR-0011).
 *
 * - **Watchdog**: the ATmega's watchdog in interrupt-and-reset mode. Its first
 *   timeout raises WDT_vect — the early warning, which writes the record and
 *   marks the reset as the watchdog's — and the hardware then drops to
 *   reset-only, so the board resets one period later. The prescaler gives 16 ms
 *   to 8 s per period, and a reset comes two periods after the last feed, so a
 *   period of up to 16 s is reached as two halves: 8000 ms is a 4 s warning and
 *   a reset 4 s after it. It can be stopped, unless the WDTON fuse is programmed
 *   — then the watchdog is always on, has no interrupt, and this says so.
 * - **Reset reason**: MCUSR, saved and cleared at startup (`.init3`) with the
 *   watchdog switched off there, because after a watchdog reset it is still on
 *   at its shortest period and would reset the board again before `main`. The
 *   UNO's bootloader (optiboot) clears MCUSR itself and hands the value over in
 *   r2, which is taken in `.init0` when MCUSR reads 0.
 *   Optiboot also leaves every reset-pin press and every upload through its own
 *   watchdog, so a watchdog flag is this program's watchdog only when the early
 *   warning marked it; otherwise the reason is unknown — a hang with interrupts
 *   off, which no interrupt can mark, reads the same.
 * - **Retained memory**: 32 bytes of `.noinit` RAM.
 *
 * The nonce source (avr_nonce.c) borrows the watchdog for its seed once per
 * EEPROM erase; it puts the watchdog's settings back as it found them.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/health.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive over robohub-common's
 * weak defaults. */
// @req AVR-0011
const char rh_health_platform[] = "avr";

#if defined(__AVR__)

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define AVR_WDT_FIRED 0xA5u

static uint32_t avr_health_retained[RH_HEALTH_RETAINED_SIZE / 4u] __attribute__((section(".noinit")));
static uint8_t avr_boot_r2 __attribute__((section(".noinit")));
static uint8_t avr_wdt_marker __attribute__((section(".noinit")));
static uint8_t avr_mcusr __attribute__((section(".noinit")));
static uint8_t avr_wdt_fired_last_boot __attribute__((section(".noinit")));
static uint8_t avr_wdt_control;  /* WDTCSR as started: WDIE | WDE | prescaler */

/* Before anything touches r2: optiboot's copy of MCUSR. */
__attribute__((naked, used, section(".init0"))) static void avr_health_init0(void)
{
    __asm__ __volatile__("sts %0, r2\n\t" : : "i"(&avr_boot_r2) : "memory");
}

/* Before .data and .bss: the flags, cleared; the watchdog, off. */
__attribute__((naked, used, section(".init3"))) static void avr_health_init3(void)
{
    uint8_t flags = MCUSR;
    MCUSR = 0;
    wdt_disable();
    avr_mcusr = flags != 0u ? flags : avr_boot_r2;
    avr_wdt_fired_last_boot = avr_wdt_marker;
    avr_wdt_marker = 0u;
}

static bool avr_wdton(void)
{
    /* A fuse bit reads 0 when it is programmed. */
    return !(boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS) & (uint8_t)~FUSE_WDTON);
}

// @req AVR-0011
void rh_watchdog_caps_get(rh_watchdog_caps *caps)
{
    if (caps) {
        bool fixed = avr_wdton();
        caps->present = true;
        caps->stoppable = !fixed;
        caps->early_warning = !fixed;
        caps->min_ms = 32u;     /* two periods of 16 ms */
        caps->max_ms = 16000u;  /* two periods of 8 s */
    }
}

// @req AVR-0011
RoboHubError rh_watchdog_start(uint32_t period_ms, uint32_t *chosen_ms)
{
    static const uint16_t half_ms[10] = { 16u, 32u, 64u, 125u, 250u, 500u, 1000u, 2000u, 4000u, 8000u };
    if (period_ms == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    /* The longest half that fits twice into the period; the shortest if none. */
    uint8_t wdp = 0u;
    for (uint8_t i = 0u; i < 10u; i++) {
        if ((uint32_t)half_ms[i] * 2u <= period_ms) {
            wdp = i;
        }
    }
    uint8_t prescaler = (uint8_t)(((wdp & 0x08u) ? _BV(WDP3) : 0u) | (wdp & 0x07u));
    avr_wdt_control = (uint8_t)((avr_wdton() ? 0u : _BV(WDIE)) | _BV(WDE) | prescaler);

    uint8_t sreg = SREG;
    cli();
    __asm__ __volatile__("wdr\n\t"
                         "sts %0, %1\n\t"
                         "sts %0, %2\n\t"
                         :
                         : "n"(_SFR_MEM_ADDR(WDTCSR)), "r"((uint8_t)(_BV(WDCE) | _BV(WDE))), "r"(avr_wdt_control)
                         : "memory");
    SREG = sreg;

    if (chosen_ms) {
        *chosen_ms = (uint32_t)half_ms[wdp] * 2u;
    }
    return ROBOHUB_OK;
}

/* Fed: the count restarts, and an early warning that had already fired (the
 * loop recovered in its last period) is armed again. */
// @req AVR-0011
void rh_watchdog_feed(void)
{
    wdt_reset();
    if ((avr_wdt_control & _BV(WDIE)) && !(WDTCSR & _BV(WDIE))) {
        WDTCSR |= _BV(WDIE);
    }
}

// @req AVR-0011
RoboHubError rh_watchdog_stop(void)
{
    if (avr_wdton()) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    wdt_disable();
    avr_wdt_control = 0u;
    return ROBOHUB_OK;
}

// @req AVR-0011
rh_reset_reason rh_platform_reset_reason(void)
{
    uint8_t flags = avr_mcusr;
    if (flags & _BV(WDRF)) {
        return avr_wdt_fired_last_boot == AVR_WDT_FIRED ? RH_RESET_WATCHDOG : RH_RESET_UNKNOWN;
    }
    if (flags & _BV(PORF)) {
        return RH_RESET_POWER_ON;  /* a power-on can raise BORF too */
    }
    if (flags & _BV(BORF)) {
        return RH_RESET_BROWN_OUT;
    }
    if (flags & _BV(EXTRF)) {
        return RH_RESET_PIN;
    }
    return RH_RESET_UNKNOWN;
}

// @req AVR-0011
void *rh_platform_retained(void)
{
    return avr_health_retained;
}

/* The early warning: one period before the reset. */
// @req AVR-0011
ISR(WDT_vect)
{
    avr_wdt_marker = AVR_WDT_FIRED;
    rh_health_fault(RH_FAULT_WATCHDOG, 0u);
}

#endif /* __AVR__ */
