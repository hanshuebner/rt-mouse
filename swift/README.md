# IBM RT PC Mouse Emulator (Swift Implementation)

This is an LLM-generated macOS implementation that emulates an IBM RT PC mouse by capturing local mouse events and translating them to the IBM RT PC serial protocol.

## Prerequisites

1. Install Xcode Command Line Tools:
```bash
xcode-select --install
```

## Setup

1. Build the project:
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

## Debug Options

### --debug
Enables general debug logging including:
- Mouse event capture details (movements, clicks)
- Button state changes
- Window and event monitoring status
- Program initialization and shutdown events

### --debug-serial
Enables detailed serial port communication logging:
- Raw data packets being sent
- Protocol conversion details
- Serial port connection status
- Communication errors and retries

### --debug-all
Enables all logging options above plus:
- Detailed protocol state machine transitions
- Timing and performance metrics
- Memory usage statistics
- System resource monitoring

Multiple debug flags can be combined, for example:
```bash
.build/debug/MouseEmulator --debug --debug-serial /dev/cu.usbserial-XXXX
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