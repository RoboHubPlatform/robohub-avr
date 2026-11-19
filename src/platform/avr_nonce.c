/**
 * @file avr_nonce.c
 * @brief The platform's nonce source on a part with no random generator.
 *
 * An ATmega has no RNG, but a nonce only has to be unique, not unpredictable:
 * the session it opens is bound to it by a key the attacker does not hold, and
 * what must never happen is the same nonce twice. So a nonce is a boot counter,
 * kept in the last four bytes of EEPROM and advanced once per boot, followed by a
 * counter of the nonces asked for since that boot (ADR-0001).
 *
 * The pair never repeats while the EEPROM keeps its value. A chip erase with the
 * EESAVE fuse unprogrammed (an ISP programmer, burning a bootloader) returns the
 * counter to all ones while the board's compiled-in key stays what it was, and a
 * counter that restarted from there would say the nonces of the board's first
 * boots again: a session answer recorded then would be taken again (ADR-0002).
 * So a counter that reads as erased is not advanced: its first value is drawn
 * once from the only randomness the part has, the jitter between the watchdog's
 * own RC oscillator and the CPU clock, and from then on it counts up as before. A
 * source that does not vary, or a part whose WDTON fuse leaves the watchdog no
 * interrupt mode, gives no seed, and then no nonce and no session rather than a
 * guessable one.
 *
 * Seeding takes about a second (64 watchdog periods of 16 ms) with interrupts
 * disabled, once per erase, on the first nonce asked for. See
 * robohub_platform_nonce in platform.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"

#include <stdint.h>
#include <avr/eeprom.h>

#ifndef RH_NONCE_EEPROM_ADDRESS
#define RH_NONCE_EEPROM_ADDRESS ((uint32_t*)(E2END - 3))
#endif

/* What an erased EEPROM reads as. */
#define RH_NONCE_ERASED 0xFFFFFFFFu

/* How many watchdog periods a seed is drawn from, and how many of them must
 * differ from the one before for the source to count as varying at all. */
#ifndef RH_NONCE_SEED_SAMPLES
#define RH_NONCE_SEED_SAMPLES 64u
#endif
#ifndef RH_NONCE_SEED_MIN_CHANGES
#define RH_NONCE_SEED_MIN_CHANGES 8u
#endif

#if defined(__AVR__)

#include <avr/boot.h>
#include <avr/io.h>

/* WDTCSR takes WDE and the prescaler only within four cycles of WDCE being set,
 * so the two stores are written out rather than left to the optimizer. */
static void rh_wdt_write(uint8_t value)
{
    __asm__ __volatile__("wdr\n\t"
                         "sts %0, %1\n\t"
                         "sts %0, %2\n\t"
                         :
                         : "n"(_SFR_MEM_ADDR(WDTCSR)), "r"((uint8_t)(_BV(WDCE) | _BV(WDE))), "r"(value)
                         : "memory");
}

static uint8_t rh_jitter_sreg;
static uint8_t rh_jitter_wdtcsr;

/* The watchdog in interrupt mode at its shortest period (16 ms), with interrupts
 * off so that no handler runs: only its flag is watched. A part with WDTON
 * programmed has no interrupt mode — its watchdog resets on the first period —
 * so there it is no source at all. */
static int rh_jitter_begin(void)
{
    /* A fuse reads 0 when it is programmed. */
    if (!(boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS) & (uint8_t)~FUSE_WDTON)) {
        return -1;
    }
    rh_jitter_sreg = SREG;
    __asm__ __volatile__("cli" ::: "memory");
    rh_jitter_wdtcsr = WDTCSR;
    rh_wdt_write((uint8_t)(_BV(WDIF) | _BV(WDIE)));
    return 0;
}

/* How many times a loop went round while the watchdog counted one period: the
 * two clocks drift against each other, and the low bits of the count with them. */
static uint16_t rh_jitter_sample(void)
{
    uint16_t count = 0;
    while (!(WDTCSR & _BV(WDIF))) {
        count++;
    }
    WDTCSR = (uint8_t)(_BV(WDIF) | _BV(WDIE));
    return count;
}

/* The watchdog as the program had it, its flag cleared, and interrupts as they
 * were. */
static void rh_jitter_end(void)
{
    rh_wdt_write((uint8_t)((rh_jitter_wdtcsr & (uint8_t)~_BV(WDCE)) | _BV(WDIF)));
    SREG = rh_jitter_sreg;
}

#else

/* A host test supplies the source, so that an erase can be simulated. */
int rh_jitter_begin(void);
uint16_t rh_jitter_sample(void);
void rh_jitter_end(void);

#endif

/* A 32-bit value from the watchdog's jitter, mixed so that every sample moves
 * every bit. Answers 0 when the source varied, and -1 when it did not. */
static int rh_nonce_seed(uint32_t* seed)
{
    uint32_t hash = 2166136261u; /* FNV-1a over the samples, then an avalanche */
    uint16_t previous = 0;
    uint8_t changes = 0;
    if (rh_jitter_begin() != 0) {
        return -1;
    }
    for (uint8_t i = 0; i < RH_NONCE_SEED_SAMPLES; i++) {
        uint16_t sample = rh_jitter_sample();
        if (i > 0 && sample != previous) {
            changes++;
        }
        previous = sample;
        hash = (hash ^ (uint8_t)sample) * 16777619u;
        hash = (hash ^ (uint8_t)(sample >> 8)) * 16777619u;
    }
    rh_jitter_end();
    if (changes < RH_NONCE_SEED_MIN_CHANGES) {
        return -1;
    }
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    *seed = hash;
    return 0;
}

static uint32_t rh_boot = 0;
static uint32_t rh_calls = 0;
static uint8_t rh_started = 0;

// @req AVR-0007, AVR-0009
int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    if (!rh_started) {
        uint32_t stored = eeprom_read_dword(RH_NONCE_EEPROM_ADDRESS);
        if (stored == RH_NONCE_ERASED) {
            /* Erased: this board's earlier boot counts are gone, not ended. */
            if (rh_nonce_seed(&rh_boot) != 0) {
                return -1;
            }
        } else {
            rh_boot = stored + 1u;
        }
        eeprom_update_dword(RH_NONCE_EEPROM_ADDRESS, rh_boot);
        rh_started = 1;
    }
    rh_calls++;
    for (rh_size i = 0; i < length; i++) {
        if (i < 4) {
            out[i] = (rh_u8)(rh_boot >> (8 * i));
        } else if (i < 8) {
            out[i] = (rh_u8)(rh_calls >> (8 * (i - 4)));
        } else {
            out[i] = 0;
        }
    }
    return 0;
}
