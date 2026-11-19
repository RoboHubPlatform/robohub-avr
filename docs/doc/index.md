# RoboHub AVR Platform

The 8-bit AVR platform for the RoboHub ecosystem: the interfaces declared by
`robohub-common`, implemented on an ATmega328P, with a board support package
for the Arduino UNO R3.

## The platform

A Rho program on this platform has no console of its own and no way out, and the
runtime asks the platform for both:

- `rho_avr_write` is where a program's output goes — the console the board's
  support package gave the platform.
- `rho_avr_halt` is where a panic ends: the part's own debug break.

The part says that the toolchain brings its own linker script, so none is
written for it: `avr-gcc` knows where an AVR's vector table belongs.

## Building

```sh
cd examples/c/hello_world
rho build -target uno-r3 .
```

The build needs `avr-gcc` on the machine; the image lands in `build/output/`.

## Flashing

```sh
rho flash -target uno-r3 build/output/main
```

Which line the board is on is a fact about the machine, so the command reads it
from the environment:

```sh
AVRDUDE_PORT=/dev/ttyUSB1 rho flash -target uno-r3 build/output/main
```

The flash goes through the board's bootloader with avrdude's `-D`: the
bootloader erases each page as it writes it, so no chip erase is asked for, and
the EEPROM — the nonce counter and the device keystore — is left as it is.

An ISP programmer (a USBasp, an Arduino as ISP, burning a bootloader) erases the
whole chip, EEPROM included, unless the EESAVE fuse is programmed, and the UNO
R3's stock high fuse (`0xDE`) leaves it unprogrammed. Program it before using one:

```sh
avrdude -p m328p -c usbasp -U hfuse:w:0xD6:m     # 0xDE with EESAVE programmed
```

A board erased anyway does not repeat its nonces (see below), but it loses a
rotated key and falls back to its compiled-in one.

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware: it
blinks the user LED and writes a line to the console every second.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.

## The device keystore

The rotated device key (`robohub/keystore.h`) is kept in EEPROM, in the 80 bytes
directly below the nonce counter (which holds the last four bytes): slot A at
`E2END-83`, slot B at `E2END-43` — `0x3AC` and `0x3D4` on an ATmega328P. The A/B
records, CRC32 and sequence numbers are robohub-common's
(`robohub/keystore_core.h`); `src/platform/avr_keystore.c` reads and writes the
bytes with `eeprom_read_block` / `eeprom_update_block`, which skips bytes that
already hold their value. An application that uses EEPROM keeps below
`RH_KEYSTORE_EEPROM_BASE`. Like the nonce counter, the keys survive a reflash
unless the chip is erased with the EESAVE fuse unprogrammed; then the board
falls back to its compiled-in key.

## The nonce counter

A session's nonce (`robohub_platform_nonce`) is a 32-bit boot counter, kept in
the last four bytes of EEPROM and advanced once per boot, followed by the count of
nonces asked for since that boot (ADR-0001). The pair never repeats while EEPROM
keeps its value.

A counter that reads as erased (`0xFFFFFFFF`) — a new part, or a chip erase — is
not advanced from zero, because the board's compiled-in key is the same as before
the erase and its first nonces would be said again under it. Its first value is
drawn once from the jitter between the watchdog's RC oscillator and the CPU
clock: 64 periods of 16 ms, about a second with interrupts off, on the first nonce
after the erase (ADR-0002). A source that does not vary, or a part with the WDTON
fuse programmed (whose watchdog has no interrupt mode), gives no nonce: the board
opens no session and tries again on its next boot.

There is no production preset for AVR: an ATmega has lock bits but no secure
boot or encrypted storage, and `rho flash --production` says this board has no
production mode.
