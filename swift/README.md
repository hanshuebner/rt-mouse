# IBM RT PC Mouse Emulator (Swift Implementation)

This is an LLM-generated macOS implementation that emulates an IBM RT PC mouse by capturing local mouse events and translating them to the IBM RT PC serial protocol.

## Prerequisites

1. Install Xcode Command Line Tools:
```bash
xcode-select --install
```

2. Install socat for virtual serial port (optional, for testing):
```bash
brew install socat
```

## Setup

1. Create a virtual serial port pair (optional, for testing):
```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```
This will output two port paths. Note one of them for use with the emulator.

2. Build the project:
```bash
swift build
```

## Usage

Run the emulator with the serial port path:
```bash
# Basic usage
.build/debug/MouseEmulator /dev/cu.usbserial-XXXX  # Replace with your actual port path

# With debug logging
.build/debug/MouseEmulator --debug-serial /dev/cu.usbserial-XXXX

# Available debug options:
# --debug        : General debug logging
# --debug-serial : Serial port communication logging
# --debug-all    : All debug logging
```

The program will:
- Open the specified serial port
- Start monitoring mouse movements and clicks
- Convert events to IBM RT PC protocol format
- Send data over the serial port

To stop the program, press Ctrl+C.

## Button Mapping

The Swift implementation provides flexible button mapping:
- Left button: Physical left button (unless modified by Ctrl/Alt)
- Middle button: Physical middle button OR Ctrl+Left button
- Right button: Physical right button OR Alt+Left button

## Troubleshooting

1. If you get permission errors accessing the serial port:
```bash
sudo chmod 666 /dev/cu.usbserial-XXXX  # Replace with your port path
```

2. If the serial port connection fails, the program will attempt to reconnect every 5 seconds.

3. Check the console output for detailed error messages and packet data.

## Implementation Details

This implementation:
- Uses native macOS APIs for mouse event capture
- Implements a transparent window for event monitoring
- Provides rate-limited movement reporting
- Supports all protocol modes and scaling options
- Includes comprehensive debug logging
- Handles serial port reconnection automatically

## License

This implementation is part of the RT-Mouse Emulator project and is licensed under the MIT License. See the top-level [LICENSE](../LICENSE) file for details. 