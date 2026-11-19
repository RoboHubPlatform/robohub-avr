---
id: ADR-0002
title: Seed an erased nonce counter from the watchdog's jitter, and never chip-erase when flashing
status: proposed
date: 2026-11-19
requirements: [AVR-0007, AVR-0009, AVR-0010, SYS-0020]
supersedes: []
superseded_by: []
---

# Seed an erased nonce counter from the watchdog's jitter, and never chip-erase when flashing

This amends ADR-0001: the nonce is still the boot counter and the call counter,
and only what the counter starts from after an erase changes.

## Context

ADR-0001 keeps the nonce's boot counter in the last four bytes of EEPROM, and
the device keystore (AVR-0006) directly below it. A chip erase with the EESAVE
fuse unprogrammed — what an ISP programmer does by default, and what burning a
bootloader does on an UNO R3, whose stock high fuse (0xDE) leaves EESAVE
unprogrammed — erases both, and the flash with them:

- The keystore's rotated key is gone, and the board falls back to the key
  compiled into its firmware. That key is derived from the gateway's key and
  the board's id (`K_dev = HMAC(K_gw, "rh-dev1" | uid)`), so it is the same key
  the board used before the erase, in every build made for that board.
- The counter reads 0xFFFFFFFF and the next boot made it 0: the board said the
  nonces of its first boots again, under the same key.

What a repeated nonce opens is on the board's side, not the gateway's. A
session answer is authenticated over the board's nonce and the gateway's
(robohub-transport secure.c), and the board checks nothing else. Someone who
recorded the answer the gateway gave to nonce N before the erase can send it
again when the board says N after the erase: the board takes it, derives the
session keys of the recorded session, restarts its counters at zero, and then
takes the recorded commands of that session as new — and masks its own frames
with the same key stream as before. The gateway's own replay check (GW-SEC-0004)
remembers a board's latest eight hello nonces, so after an erase it answers the
repeated hellos rather than refusing them, and it is not what would stop this.

The bootloader path does not erase EEPROM: Optiboot erases each flash page as it
writes it and has no chip erase. But avrdude asks for one unless told not to,
and a board that leaves the bootloader for an ISP programmer once is erased.

## Decision

1. **The flashing path never asks for a chip erase.** The uno-r3 spec's flash
   command passes avrdude `-D` (as the Arduino IDE does for a bootloader
   upload), so `rho flash` writes the image and leaves EEPROM as it is. The
   platform's documentation says that an ISP programmer erases EEPROM unless
   EESAVE is programmed, and gives the fuse (high fuse 0xD6 on an UNO R3).

2. **An erased counter is seeded, not advanced.** When the counter reads as
   erased (0xFFFFFFFF), the first nonce of that boot takes the counter's value
   from the part's one source of randomness: the number of times a loop goes
   round while the watchdog — clocked by its own 128 kHz RC oscillator — counts
   one 16 ms period, taken 64 times with interrupts off, mixed (FNV-1a, then an
   avalanche) into 32 bits. The value is written to the counter before the
   nonce is returned, and from then on the counter counts up as ADR-0001 says.
   This happens once per erase, on a new part and after a chip erase alike,
   and costs about a second.

3. **A source that does not vary is no source.** When fewer than 8 of the 64
   periods differ from the one before, or the WDTON fuse leaves the watchdog no
   interrupt mode, there is no seed: the nonce source
   answers that it has no nonce (platform.h), no session is opened, and the
   counter is left erased so that the next boot tries again.

A counter that is not erased is advanced exactly as before; boards already in
the field keep their counts and see no change.

## Alternatives considered

- **Refuse a session when the counter reads as erased.** After a chip erase the
  keystore is erased too, so "erased counter, key present" only catches a
  partial erase; and a new part reads exactly like an erased one, so refusing
  would leave every new board unable to connect without a provisioning step.
  The stock UNO bootloader cannot write EEPROM, so that step would need an ISP
  programmer — the tool that causes the problem.
- **A salt stored with the key.** A rotated key needs none: it is erased with
  the counter and never used again. The compiled-in key is the one that
  recurs, and a salt kept beside it in flash is reflashed with the same image.
- **A per-build salt.** Reflashing the same image after an erase repeats it.
- **The gateway refusing a board's non-increasing nonce.** It would not stop a
  recorded session answer, which the board accepts without the gateway, and
  the nonce's layout is the platform's, not the protocol's.
- **Entropy for every nonce.** ADR-0001's reason stands: it is slow, and the
  counter is what makes uniqueness checkable. Here entropy only places the
  counter after an erase, once.

## Consequences

- No nonce said before a chip erase is said again after it, except by a chance
  of about B / 2^32 per erase that the new start lands within the B boots of an
  earlier life. `tests/unit/test_nonce.c` erases a simulated EEPROM between
  lives and checks that no nonce of an earlier life recurs.
- How much the jitter varies is the part's and its clock's; the health check
  (3) catches a source that does not vary at all, not a weak one. A board that
  never reaches a session after an erase is one whose source the check refused.
  The source has not yet been measured on a real UNO R3.
- The first nonce after an erase holds interrupts off for about a second;
  timer ticks and UART bytes of that second are lost. Nothing is expected on
  the link then: the hello has not been sent.
- The watchdog is put in interrupt mode for the second and given back as the
  program had it. A part with the WDTON fuse programmed has no interrupt mode
  (its watchdog resets the part on the first period), so the source reads the
  high fuse first and there counts as no source: such a board opens no session
  on an erased counter. The UNO R3's stock fuses leave WDTON unprogrammed.
- An application that writes the last four bytes of EEPROM back to 0xFF
  reseeds the counter; one that writes another value moves it. Neither should
  happen: an application keeps below RH_KEYSTORE_EEPROM_BASE.
