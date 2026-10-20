# robohub-avr

The 8-bit AVR platform for the RoboHub Platform: the interfaces declared by
`robohub-common`, implemented on an Atmel/Microchip AVR part, with a board
support package for each supported board.

An AVR is a small part. The ATmega328P of an Arduino UNO R3 has 32 KB of flash
and 2 KB of RAM, and that shapes what this platform is:

- There is no vendor HAL to sit on. The part's registers are the interface, the
  toolchain carries the startup code, the linker script and the headers, and what
  is here is the drivers and the board.
- There is nothing to fetch. Where the STM32 platform vendors the Cube
  repositories, an AVR needs a *compiler* — `avr-gcc` — and that is a program on
  the machine, not a tree beside the source. It is declared as a system
  dependency, so `rho doctor` reports it and `rho keeper` can install it.
- Some things are shared. Timer0 counts the milliseconds, so the PWM is left with
  Timer1 and Timer2: four modulated pins, not six. The UNO's D5 and D6 are the
  two that are lost, and the board says so in its pin map.

## Boards

- **UNO R3** — an ATmega328P on the standard UNO headers. The serial lines are
  on D0 and D1, I2C on A4 and A5, SPI on D10 to D13, and the user LED is on D13.

## Interfaces

| Contract | On the part |
|---|---|
| GPIO | three ports of eight pins; a pin is the board's own number, D0 to D13 and A0 to A5 |
| UART | the one hardware USART, on D0 and D1 |
| Timer | Timer0 counts the milliseconds; Timer1 runs free at half a microsecond a count, which is what a delay waits on |
| ADC | the ten-bit converter with six inputs, A0 to A5 |
| PWM | Timer1 and Timer2: D9, D10, D11 and D3 |
| I2C | the TWI peripheral, on A4 and A5 |
| SPI | the hardware SPI, on D13, D12 and D11 |
| OneWire | a line driven by hand and timed with the timer, on D2 |
| DHT | a line driven by hand and timed with the timer, on D4 |

## The runtime

A Rho program on this platform has no console of its own and no way out, and
the runtime asks the platform for both:

- `rho_avr_write` is where a program's output goes — the console the board's
  support package gave the platform.
- `rho_avr_halt` is where a panic ends: the part's own debug break, and a
  program that stops where a debugger can find it rather than in a C library's
  `exit`.

The platform says so in its build, and the runtime uses what it is told:

```cmake
target_compile_definitions(robohub-avr PUBLIC
    RHO_PROGRAM_WRITE=rho_avr_write
    RHO_PROGRAM_HALT=rho_avr_halt)
```

The part says something else of the same kind: `avr-gcc` brings its own linker
script, so none is written for it. The memory map is still declared — what a
program is told about the part it runs on does not depend on who writes the
script.

## Building

```sh
cd examples/c/hello_world
rho build -target uno-r3 .
```

The build needs `avr-gcc` on the machine; the image lands in `build/output/`.

The same build, from a Rho program, is one directory over:

```sh
cd examples/rho/hello_world
rho build -target uno-r3 .
```

## Flashing

The UNO R3's bootloader is written over the serial line, with avrdude:

```sh
rho flash -target uno-r3 build/output/hello_world
```

A Rho program is flashed the same way, from the image its build produced:

```sh
rho flash -target uno-r3 build/output/main
```

Which line the board is on is a fact about the machine, so the command reads it
from the environment and the value in the spec is what the machine falls back
to:

```sh
AVRDUDE_PORT=/dev/ttyUSB1 rho flash -target uno-r3 build/output/main
```

The port is in the board's own spec, because that is where a board says how it
is reached. A board on another line has it changed there.

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware: it
blinks the user LED and writes a line to the console every second. It is the
whole path in one program — the cross compiler, the startup and the linker script
the toolchain carries, the platform's bring-up, and the flash — and it names no
board and no pin: it asks for `user_led` and for the clock, and the board's
support package says which pins those are.

`examples/rho/hello_world` is that program in Rho, and it is the one that shows
what the platform is for. The program is written against the contracts and
nothing else: no peripheral, no pin, no register. The console is a console, the
delay is a delay, and the LED is an LED somebody named — so the same program
builds for an AVR, an STM32, a Pico and an ESP32, and the board is chosen by the
preset the build is given.

What the foreign calls are is derived rather than written out: the program
declares the contract headers it is written against, `rho-cimport` reads them,
and the declarations it calls are generated from them. A contract that changes
changes the calls with it.

The image is smaller than the C one — 9 KB against 11.5 KB — because the Rho
runtime's `print` goes straight out of the platform's console rather than
through the C library's formatting, which on a part with 2 KB of RAM is a large
thing to pull in.
