# pico-rt-mouse - A USB to RT PC Mouse Converter

This program converts a USB mouse to output the RT PC mouse protocol using a Raspberry Pi Pico. It uses TinyUSB in host mode to interface with a standard USB mouse and outputs mouse reports in the RT PC 4-byte format on a parallel GPIO bus, suitable for retrocomputing projects or RT PC emulation.

## Hardware Connections

| Pico pin | Pin# | RT PC Mouse Signal |
| -------- | ---- | ----------------- |
| VBUS     | 40   | +5V               |
| GND      | 38   | GND               |
| GP12     | 16   | D0                |
| GP13     | 17   | D1                |
| GP14     | 19   | D2                |
| GP15     | 20   | D3                |
| GP16     | 21   | D4                |
| GP17     | 22   | D5                |
| GP18     | 24   | D6                |
| GP19     | 25   | D7                |
| GP20     | 26   | STB (Strobe)      |
| GP21     | 27   | AKD (Any Key Down) |

The STB and AKD signals are active high. Connect the GPIO bus to your RT PC mouse port or logic analyzer as needed.

## Usage

- Connect a USB mouse to the Pico via a USB OTG adapter.
- Connect the GPIO pins as described above.
- Flash the compiled UF2 to your Pico.
- Mouse movements and button presses will be translated to RT PC mouse reports and output on the GPIO bus.

## Building

1. Set up the Raspberry Pi Pico SDK as described in the official documentation.
2. Create a `build` directory inside `pico-rt-mouse`.
3. Run:
   ```
   cd pico-rt-mouse/build
   cmake .. -DPICO_SDK_PATH=<path-to-pico-sdk>
   make
   ```
4. Flash the resulting `pico-rt-mouse.uf2` to your Pico.

## Protocol

The RT PC mouse protocol uses 4-byte reports:
- Byte 0: 0x0b (data report magic)
- Byte 1: status (button bits, sign bits, etc.)
- Byte 2: X movement (two's complement)
- Byte 3: Y movement (two's complement)

See the code and comments for details on the translation from USB HID mouse reports to RT PC format. 