/**
 * @file avr_nonce.c
 * @brief The platform's nonce source on a part with no random generator.
 *
 * An ATmega has no RNG, but a nonce only has to be unique, not unpredictable:
 * the session it opens is bound to it by a key the attacker does not hold, and
 * what must never happen is the same nonce twice. So a nonce is a boot counter,
 * kept in the last four bytes of EEPROM and advanced once per boot, followed by a
 * counter of the nonces asked for since that boot. The pair never repeats while
 * the EEPROM keeps its value (it survives a reflash unless the chip is erased with
 * EESAVE unset). See robohub_platform_nonce in platform.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"

#include <avr/eeprom.h>

#ifndef RH_NONCE_EEPROM_ADDRESS
#define RH_NONCE_EEPROM_ADDRESS ((uint32_t*)(E2END - 3))
#endif

static uint32_t rh_boot = 0;
static uint32_t rh_calls = 0;
static uint8_t rh_started = 0;

int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    if (!rh_started) {
        rh_boot = eeprom_read_dword(RH_NONCE_EEPROM_ADDRESS) + 1u;
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
