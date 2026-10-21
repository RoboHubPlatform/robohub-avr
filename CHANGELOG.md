# Changelog

All notable changes to this module are recorded here, one section per release.
A module's description says what it is and does not change between releases; this
says what changed, which is what tells two versions of one module apart. The
section for the version being published travels with it, so a registry shows it
against that version.

## [0.1.0] - 2026-10-20

The 8-bit AVR platform for the RoboHub Platform: the interfaces declared by
`robohub-common`, implemented on an ATmega328P, with a board support package for
the Arduino UNO R3.

Two examples. `examples/c/hello_world` is the smallest program that runs on
real hardware — it blinks the user LED and writes a line to the console every
second, and it is C from `#include` to `main`. `examples/rho/hello_world` is
that program in Rho: written against the contracts and nothing else, with the
foreign calls derived from the contract headers rather than written out beside
them, so the same program builds for any board the platform supports.

The board's own manifest now says what the toolchain's schema asks of it: a
connector is named and what it carries is said of it, a programmer is a map, and
the peripherals are a list. A manifest that does not match is not read at all,
and a board whose manifest is not read is a board that does not exist.

The platform says how a program on it stops. A panic ends in the part's own
debug break rather than in the C library's `exit`, which on a part with 2 KB of
RAM is a firmware carrying an exit path it has nowhere to go from.

The part says that the toolchain brings its own linker script, so none is
written for it: `avr-gcc` knows where an AVR's vector table belongs and how its
runtime expects to be reached.

The host test that the build has been asking for is here, and the vendored
platform's tests are no longer registered in a build that does not build them.

The tests are registered in a registry of their own, so that the orchestrator can
say what they are, run one of them by its name, and say whether it is run at
all: `robohub test list`, `robohub test run`, `robohub test enable` and
`disable`. A test that is not enabled is still a test — it is there, and it is
not run.

The platform's tests are in folders now, as the unit tests and the smoke tests
are two different things: `tests/unit/` holds the tests of the chip-independent
half — the error codes every layer agrees on, and the console, which is the one
part of the platform that has no part in it — and `tests/smoke/` holds the
examples, built for the board the platform carries. A manifest says which is
which, and the orchestrator runs either on its own.

The console's state is declared in its header now, because a test that cannot
see whether a console is open cannot check that a console which was not opened
says so.

Which serial line the board is on is no longer written into the board's spec.
The flash command reads it from the environment — `AVRDUDE_PORT` — and the
value in the spec is what the machine falls back to, so a board on another line
is flashed without the platform being changed.
