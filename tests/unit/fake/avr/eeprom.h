/*
 * avr/eeprom.h, for the host: an ATmega328P's 1 KiB of EEPROM as an array the
 * test can erase, so the nonce source is tested off the part.
 *
 * @project RoboHub Platform
 * @license MIT
 */
#ifndef ROBOHUB_AVR_TEST_FAKE_EEPROM_H
#define ROBOHUB_AVR_TEST_FAKE_EEPROM_H

#include <stdint.h>
#include <string.h>

#define E2END 0x3FF

extern uint8_t fake_eeprom[E2END + 1];
extern unsigned fake_eeprom_writes;

static inline uint32_t eeprom_read_dword(const uint32_t* address)
{
    uint32_t value;
    memcpy(&value, &fake_eeprom[(uintptr_t)address], sizeof(value));
    return value;
}

static inline void eeprom_update_dword(uint32_t* address, uint32_t value)
{
    memcpy(&fake_eeprom[(uintptr_t)address], &value, sizeof(value));
    fake_eeprom_writes++;
}

#endif
