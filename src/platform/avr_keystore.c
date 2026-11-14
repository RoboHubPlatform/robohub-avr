/**
 * @file avr_keystore.c
 * @brief The device keystore in EEPROM, in the 80 bytes below the nonce counter.
 *
 * The nonce counter (avr_nonce.c) holds the last four bytes of EEPROM. The
 * keystore's two 40-byte slots sit directly below it, slot A first:
 *
 *     E2END-83 .. E2END-44   slot A
 *     E2END-43 .. E2END-4    slot B
 *     E2END-3  .. E2END      nonce boot counter
 *
 * On an ATmega328P (1 KiB EEPROM) that is 0x3AC..0x3FB. An application that uses
 * EEPROM itself keeps below RH_KEYSTORE_EEPROM_BASE. Both survive a reflash as
 * long as the fuses keep EESAVE programmed or the programmer does not erase the
 * chip; a chip erase without it returns the board to its compiled-in key.
 *
 * EEPROM is written byte by byte and needs no erase, so the backend has none;
 * eeprom_update_block skips bytes that already hold their value, which also
 * spares the cells' write endurance. The A/B logic, CRC32 and sequence numbers
 * are robohub-common's (robohub/keystore_core.h).
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/keystore.h"
#include "robohub/keystore_core.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive; see keystore_core.h. */
const char rh_keystore_platform[] = "avr";

#ifdef __AVR__

#include <avr/eeprom.h>

#ifndef RH_KEYSTORE_EEPROM_BASE
/* Below the nonce counter's four bytes, two records long. */
#define RH_KEYSTORE_EEPROM_BASE (E2END + 1 - 4 - RH_KEYSTORE_SLOTS * RH_KEYSTORE_RECORD_SIZE)
#endif

static void *slot_address(unsigned slot)
{
    return (void *)(uintptr_t)(RH_KEYSTORE_EEPROM_BASE + slot * RH_KEYSTORE_RECORD_SIZE);
}

static int avr_read(void *ctx, unsigned slot, uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > RH_KEYSTORE_RECORD_SIZE) {
        return -1;
    }
    eeprom_read_block(buf, slot_address(slot), len);
    return 0;
}

static int avr_write(void *ctx, unsigned slot, const uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > RH_KEYSTORE_RECORD_SIZE) {
        return -1;
    }
    eeprom_update_block(buf, slot_address(slot), len);
    return 0;
}

static const rh_keystore_backend_t avr_backend = { 0, avr_read, 0, avr_write };

int rh_keystore_load(rh_key_t *out)
{
    return rh_keystore_core_load(&avr_backend, out);
}

int rh_keystore_store(const rh_key_t *key)
{
    return rh_keystore_core_store(&avr_backend, key);
}

#endif /* __AVR__ */
