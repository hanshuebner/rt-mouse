# pico-rt-mouse - A USB to RT PC Mouse Converter

This program converts a USB mouse to output the RT PC mouse protocol using a Raspberry Pi Pico. It uses TinyUSB in host mode to interface with a standard USB mouse and outputs mouse reports in the RT PC 4-byte format over UART, making a USB mouse usable for an IBM RT PC.

## Hardware Connections

### RT PC Mouse Port Pinout
Looking into the male receptacle socket on the back of the RT system unit:
```
         +--------------+            1 = Ground
         |    1  3  5   |            2 = Transmit to device (RT → Mouse)
         |              |            3 = +12 Volts
          -   2  4  6  -             4 = -12 Volts
           |          |              5 = +5 Volts
           +----------+              6 = Receive from device (Mouse → RT)
```

### Level Conversion
The Pico's UART operates at 3.3V logic levels, while the RT PC mouse port uses RS-232 levels (±12V). A MAX3232 level converter is required to adapt the signals:

1. Power the Pico from the RT PC's +5V (pin 5) and Ground (pin 1)
2. Power the MAX3232's 3.3V supply from the Pico's 3.3V output
3. Connect the MAX3232 as follows:
   - Pico GP8 (UART1 TX) → MAX3232 T1IN
   - MAX3232 T1OUT → RT PC pin 2 (Transmit to device)
   - RT PC pin 6 (Receive from device) → MAX3232 R1IN
   - MAX3232 R1OUT → Pico GP9 (UART1 RX)

### UART Connections
| Pico pin | Pin# | Function | Description |
| -------- | ---- | -------- | ----------- |
| GP0      | 1    | UART0 TX | Debug output (optional) |
| GP1      | 2    | UART0 RX | Debug input (optional) |
| GP8      | 11   | UART1 TX | RT mouse protocol output (to MAX3232) |
| GP9      | 12   | UART1 RX | RT mouse protocol input (from MAX3232) |
| 3V3      | 36   | 3.3V     | Power for MAX3232 |
| VSYS     | 39   | 5V       | Connect to RT PC pin 5 (+5V) |
| GND      | 38   | GND      | Connect to RT PC pin 1 |

The UART1 is configured for the RT mouse protocol with the following settings:
- Baud rate: 9600
- Data bits: 8
- Stop bits: 1
- Parity: Odd

## Usage

- Connect a USB mouse to the Pico via a USB OTG adapter.
- Connect the Pico to the RT PC mouse port through the MAX3232 level converter as described above.
- Optionally connect UART0 (GP0/GP1) to a serial terminal for debug output.
- Flash the compiled UF2 to your Pico.
- Mouse movements and button presses will be translated to RT PC mouse reports and output over UART1.

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

The RT PC mouse protocol uses 4-byte reports transmitted over UART1:
- Byte 0: 0x0b (data report magic)
- Byte 1: status (button bits, sign bits, etc.)
- Byte 2: X movement (two's complement)
- Byte 3: Y movement (two's complement)

The implementation supports the full RT mouse protocol including:
- Reset and initialization
- Configuration reading
- Enable/disable
- Wrap mode
- Scaling (linear/exponential)
- Resolution and sample rate settings
- Stream/remote modes

See the code and comments for details on the translation from USB HID mouse reports to RT PC format. 