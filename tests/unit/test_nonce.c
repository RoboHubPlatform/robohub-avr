/*
 * test_nonce.c — the nonce source, across reboots and a chip erase, on a host.
 *
 * The source is compiled here with a fake EEPROM (fake/avr/eeprom.h) and a fake
 * watchdog jitter, so that what matters can be checked without a part: no nonce
 * is ever said twice — not within a boot, not across reboots, and not across a
 * chip erase that put the boot counter back to all ones while the board's key
 * stayed what it was (ADR-0002). A jitter source that does not vary gives no
 * nonce at all, and a counter that is not erased goes on counting as it did.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The source itself, so that a reboot can be simulated by clearing its state. */
#include "../../src/platform/avr_nonce.c"

uint8_t fake_eeprom[E2END + 1];
unsigned fake_eeprom_writes;

/* The watchdog jitter: a small generator whose state differs from one life of
 * the board to the next, as the clocks of a real part do. `jitter_flat` makes
 * it a source that never varies. */
static uint32_t jitter_state = 1u;
static int jitter_flat = 0;
static unsigned jitter_sampled = 0;
static int jitter_open = 0;
static int jitter_unavailable = 0;

int rh_jitter_begin(void)
{
    if (jitter_unavailable) {
        return -1;
    }
    jitter_open = 1;
    return 0;
}
void rh_jitter_end(void) { jitter_open = 0; }

uint16_t rh_jitter_sample(void)
{
    jitter_sampled++;
    if (jitter_flat) {
        return 41000u;
    }
    jitter_state = jitter_state * 1103515245u + 12345u;
    return (uint16_t)(41000u + ((jitter_state >> 16) & 0x0Fu));
}

static int failures = 0;

static void expect(int condition, const char* what)
{
    if (condition) {
        printf("[PASS] %s\n", what);
    } else {
        printf("[FAIL] %s\n", what);
        failures++;
    }
}

/* A reset: RAM is gone, EEPROM is not. */
static void reboot(void)
{
    rh_boot = 0;
    rh_calls = 0;
    rh_started = 0;
}

/* A chip erase with EESAVE unprogrammed: every EEPROM byte back to 0xFF. */
static void chip_erase(void)
{
    memset(fake_eeprom, 0xFF, sizeof(fake_eeprom));
    reboot();
}

/* Every nonce said in a test, to find one said twice. */
#define SEEN_MAX 256
static uint8_t seen[SEEN_MAX][8];
static unsigned seen_count = 0;

static int said_before(const uint8_t nonce[8])
{
    for (unsigned i = 0; i < seen_count; i++) {
        if (memcmp(seen[i], nonce, 8) == 0) {
            return 1;
        }
    }
    if (seen_count < SEEN_MAX) {
        memcpy(seen[seen_count++], nonce, 8);
    }
    return 0;
}

/* `boots` boots of `per_boot` nonces each; answers how many repeated one said
 * earlier in the test. */
static int life(unsigned boots, unsigned per_boot)
{
    int repeats = 0;
    for (unsigned b = 0; b < boots; b++) {
        reboot();
        for (unsigned n = 0; n < per_boot; n++) {
            uint8_t nonce[8];
            if (robohub_platform_nonce(nonce, sizeof(nonce)) != 0) {
                return -1;
            }
            repeats += said_before(nonce);
        }
    }
    return repeats;
}

static uint32_t counter(void) { return eeprom_read_dword(RH_NONCE_EEPROM_ADDRESS); }

// @verifies AVR-0007, AVR-0009
int main(void)
{
    /* A new part: EEPROM erased. The first nonce seeds the counter. */
    chip_erase();
    jitter_state = 0x1234u;
    expect(life(6, 4) == 0, "no nonce repeats within a life of six boots");
    expect(counter() != RH_NONCE_ERASED, "the first nonce of a new part leaves the counter written");
    expect(!jitter_open, "the watchdog is given back after seeding");

    /* The same key, after a chip erase that kept nothing. */
    chip_erase();
    jitter_state = 0x9876u;
    expect(life(6, 4) == 0, "no nonce of the life before a chip erase is said again after it");

    /* And another, with a third life's clocks. */
    chip_erase();
    jitter_state = 0x5555u;
    expect(life(6, 4) == 0, "nor after a second chip erase");

    /* A counter that is not erased counts on, with no seeding. */
    reboot();
    memset(fake_eeprom, 0xFF, sizeof(fake_eeprom));
    {
        uint32_t forty_one = 41u;
        memcpy(&fake_eeprom[E2END - 3], &forty_one, sizeof(forty_one));
    }
    jitter_sampled = 0;
    uint8_t nonce[12];
    memset(nonce, 0xAA, sizeof(nonce));
    expect(robohub_platform_nonce(nonce, sizeof(nonce)) == 0, "a written counter gives a nonce");
    expect(jitter_sampled == 0, "a written counter is not reseeded");
    expect(counter() == 42u, "a written counter is advanced by one per boot");
    const uint8_t expected[12] = { 42, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
    expect(memcmp(nonce, expected, sizeof(expected)) == 0, "the nonce is the boot count, the call count, then zeros");

    /* Seeding touches the counter's four bytes and nothing below them. */
    memset(fake_eeprom, 0x5A, sizeof(fake_eeprom));
    memset(&fake_eeprom[E2END - 3], 0xFF, 4);
    reboot();
    jitter_state = 0x4242u;
    expect(robohub_platform_nonce(nonce, 8) == 0, "an erased counter below a kept keystore is seeded");
    int below_untouched = 1;
    for (unsigned i = 0; i < E2END - 3; i++) {
        below_untouched &= fake_eeprom[i] == 0x5A;
    }
    expect(below_untouched, "seeding writes no byte of the keystore or the application's EEPROM");

    /* A jitter source that does not vary is no source: no nonce, and the counter
     * is left erased so that the next boot tries again. */
    chip_erase();
    jitter_flat = 1;
    fake_eeprom_writes = 0;
    expect(robohub_platform_nonce(nonce, 8) != 0, "a flat jitter source gives no nonce");
    expect(counter() == RH_NONCE_ERASED && fake_eeprom_writes == 0, "and writes nothing");
    expect(!jitter_open, "and gives the watchdog back");
    jitter_flat = 0;
    reboot();
    expect(robohub_platform_nonce(nonce, 8) == 0 && counter() != RH_NONCE_ERASED,
           "the next boot with a varying source is seeded");

    /* A part whose watchdog has no interrupt mode (WDTON) is no source either. */
    chip_erase();
    jitter_unavailable = 1;
    fake_eeprom_writes = 0;
    expect(robohub_platform_nonce(nonce, 8) != 0 && counter() == RH_NONCE_ERASED && fake_eeprom_writes == 0,
           "a part with no jitter source gives no nonce and writes nothing");
    jitter_unavailable = 0;

    printf("%s\n", failures == 0 ? "all nonce tests passed" : "nonce tests FAILED");
    return failures == 0 ? 0 : 1;
}
