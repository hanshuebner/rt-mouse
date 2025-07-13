# USB Mouse Adapter for IBM 6150 (RT PC)

This repository contains firmware for a Raspberry Pi Pico to act as
an adapter to make a USB mouse usable with the IBM 6150 (RT PC).

In addition to the Pi Pico, a MAX3232 and a couple of capacitors
are required to adapt the serial port of the Pico to RS232 levels.

Schematics are provided in the pcb/ directory.  I've not actually
designed a PCB as I believe that nobody would care, and my bread-
boarded prototype suffices for me.

The research/ directory contains information about the mouse
protocol, historic header files and mouse emulation implementations
that I used for testing before implementing the Pico firmware.
