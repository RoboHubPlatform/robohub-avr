---
id: ADR-0001
title: Make an AVR's nonce from an EEPROM boot counter, not from entropy
status: proposed
date: 2026-11-13
requirements: [AVR-0007, SYS-0020, SYS-0023]
supersedes: []
superseded_by: []
---

# Make an AVR's nonce from an EEPROM boot counter, not from entropy

## Context

A board opens a session with a nonce (robohub:ADR-0010). The ATmega328P has no
random generator, and gathering entropy from clock jitter or a floating ADC
input is slow, board-dependent and hard to check. The session key is derived
under a key an attacker does not hold, so what must never happen is the same
nonce twice — not that a nonce be unpredictable.

## Decision

The nonce is a 32-bit boot counter, kept in the last four bytes of EEPROM and
advanced once per boot on the first nonce asked for, followed by a 32-bit
counter of the nonces asked for since that boot; any further bytes are zero.
The device keystore sits directly below the counter (AVR-0006).

## Consequences

- No two nonces repeat while EEPROM keeps its value, across resets and reflashes
  that keep EEPROM.
- A chip erase with the EESAVE fuse unprogrammed resets the counter, so nonces
  of earlier boots could recur under the board's compiled-in key. Amended by
  ADR-0002: an erased counter is seeded from the watchdog's jitter rather than
  advanced from zero, and the flashing path never asks for a chip erase.
- One EEPROM write per boot; the cells' endurance is not a concern at boot rates.
