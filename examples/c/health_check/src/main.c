/**
 * @file main.c
 * @brief health_check — the health contracts and the keystore on an UNO R3
 *        (AVR-0011, AVR-0012).
 *
 * The UNO R3 runs no core, so this calls the contracts itself. Three boots, on
 * the serial console at 115200:
 *
 *   1. key A is stored, the watchdog started and never fed: its early warning
 *      writes the fault record, and the board resets one period later;
 *   2. the record says watchdog, key A is still there; key B is stored and the
 *      watchdog left unfed once more;
 *   3. key B is still there, and the watchdog is fed for 20 s:
 *      "health-check: passed".
 *
 * Which boot this is comes from the keystore: A means the second, B the third.
 * The check's keys have their own scope and epoch 0, so no session uses them;
 * they replace a key the board was rotated to, so flash it from Mesh afterwards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/timer.h"
#include "robohub/health.h"
#include "robohub/keystore.h"

#define WATCHDOG_MS 2000u

static void put(const char* text)
{
    (void)robohub_console_puts(text);
}

static void put_u32(uint32_t value)
{
    char digits[12];
    uint8_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0u);
    char out[12];
    uint8_t at = 0;
    while (count > 0u) {
        out[at++] = digits[--count];
    }
    out[at] = '\0';
    put(out);
}

/* The check's keys: scope "rhcheck", epoch 0, a body that says which it is. */
static rh_key_t check_key(uint8_t which)
{
    static const char scope[8] = {'r', 'h', 'c', 'h', 'e', 'c', 'k', 0};
    rh_key_t key;
    key.epoch = 0u;
    for (uint8_t i = 0; i < 8u; i++) {
        key.scope[i] = (uint8_t)scope[i];
    }
    for (uint8_t i = 0; i < 16u; i++) {
        key.key[i] = (uint8_t)(which ^ (uint8_t)(i * 17u));
    }
    return key;
}

static bool keystore_holds(uint8_t which)
{
    rh_key_t want = check_key(which);
    rh_key_t got;
    if (rh_keystore_load(&got) != 0 || got.epoch != want.epoch) {
        return false;
    }
    for (uint8_t i = 0; i < 8u; i++) {
        if (got.scope[i] != want.scope[i]) {
            return false;
        }
    }
    for (uint8_t i = 0; i < 16u; i++) {
        if (got.key[i] != want.key[i]) {
            return false;
        }
    }
    return true;
}

static bool keystore_put(uint8_t which)
{
    rh_key_t key = check_key(which);
    return rh_keystore_store(&key) == 0 && keystore_holds(which);
}

/* Start the watchdog and stop feeding it: the early warning, then the reset. */
static void starve(void)
{
    uint32_t chosen = 0u;
    if (rh_watchdog_start(WATCHDOG_MS, &chosen) != ROBOHUB_OK) {
        put("health-check: FAILED — the watchdog did not start\n");
        for (;;) {
        }
    }
    put("health-check: the watchdog runs (");
    put_u32(chosen);
    put(" ms) and is not fed; the board should reset\n");
    for (;;) {
    }
}

// @req AVR-0012
int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }
    RoboHubTimer* timer = robohub_get_timer("system_timer");
    rh_health_boot();

    rh_reset_reason reason = rh_health_reset_reason();
    rh_fault_record fault;
    bool has_fault = rh_health_fault_take(&fault);
    put("\nhealth-check: boot ");
    put_u32(rh_health_boot_count());
    put(", reset reason ");
    put(rh_reset_reason_name(reason));
    put(has_fault ? ", fault code " : ", no fault record\n");
    if (has_fault) {
        put_u32(fault.code);
        put("\n");
    }

    if (reason != RH_RESET_WATCHDOG) {
        /* Boot 1. */
        put(keystore_put(0xA0u) ? "health-check: keystore — key A stored and read back\n"
                                : "health-check: FAILED — key A could not be stored\n");
        starve();
    }
    if (!has_fault || fault.code != RH_FAULT_WATCHDOG) {
        put("health-check: FAILED — a watchdog reset without the record its early warning writes\n");
        for (;;) {
        }
    }
    if (keystore_holds(0xA0u)) {
        /* Boot 2. */
        put("health-check: keystore — key A held across the watchdog reset\n");
        put(keystore_put(0xB0u) ? "health-check: keystore — key B stored and read back\n"
                                : "health-check: FAILED — key B could not be stored\n");
        starve();
    }
    if (!keystore_holds(0xB0u)) {
        put("health-check: FAILED — the keystore did not hold its keys across the resets\n");
        for (;;) {
        }
    }
    /* Boot 3. */
    put("health-check: keystore — key B held across the second reset\n");
    uint32_t chosen = 0u;
    if (rh_watchdog_start(WATCHDOG_MS, &chosen) != ROBOHUB_OK) {
        put("health-check: FAILED — the watchdog did not start\n");
        for (;;) {
        }
    }
    for (uint16_t tick = 0; tick < 200u; tick++) {
        rh_watchdog_feed();
        if (timer != NULL) {
            timer->delay_ms(timer, 100u);
        }
    }
    put("health-check: passed\n");
    for (;;) {
        rh_watchdog_feed();
        if (timer != NULL) {
            timer->delay_ms(timer, 100u);
        }
    }
}
