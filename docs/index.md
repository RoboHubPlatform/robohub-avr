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

There is no production preset for AVR: an ATmega has lock bits but no secure
boot or encrypted storage, and `rho flash --production` says this board has no
production mode.
