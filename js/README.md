# RT PC Mouse Emulator (JavaScript Implementation)

This is an LLM-generated browser-based implementation that emulates an IBM RT PC mouse using a modern web browser. It captures mouse movements from a web interface and converts them to the RT PC mouse protocol, sending the data over a serial connection through a Node.js server using WebSocket connections.

## Requirements

- Node.js 14 or later
- USB Serial adapter (9600 baud, 8N1)
- Modern web browser with WebSocket support
- Local network connection (for browser to server communication)

## Setup

1. Install dependencies:
```bash
npm install
```

2. Find your serial port path:
   - On macOS: `ls /dev/cu.*`
   - On Linux: `ls /dev/ttyUSB*`
   - On Windows: Check Device Manager for COM ports

3. Start the server:
```bash
# Basic start
npm start -- /dev/cu.usbserial-XXXX

# With debug logging
DEBUG=rt-mouse node server.js /dev/cu.usbserial-XXXX

# Development mode with auto-restart
npm run dev -- /dev/cu.usbserial-XXXX
```

Replace `/dev/cu.usbserial-XXXX` with your actual serial port path.

## Development

The project includes a development mode that automatically restarts the server when changes are made to the code. To use it:

```bash
npm run dev -- /dev/cu.usbserial-XXXX
```

This will:
- Watch for changes in server.js and files in the public directory
- Automatically restart the server when changes are detected
- Show debug output
- The web interface will automatically reconnect when the server restarts

## Usage

1. Open http://localhost:3000 in your browser
2. The web interface will automatically connect to the server via WebSocket
3. The web interface shows:
   - A mouse movement area
   - Three buttons for mouse clicks
   - Connection status indicator
   - Debug console for protocol messages
4. Move your mouse in the designated area to generate movement data
5. Click the buttons to simulate mouse button presses

## Implementation Details

This implementation:
- Uses a Node.js server to handle serial port communication
- Connects browser clients via WebSocket
- Captures mouse events through a browser window
- Implements the same protocol state machine as the Swift version
- Provides real-time protocol debugging through the browser console
- Supports all mouse modes and scaling options
- Includes automatic reconnection handling
- Features a responsive web interface

## Debugging

The server uses the `debug` module for logging. To see debug output, run:

```bash
DEBUG=rt-mouse node server.js <serial-port-path>
```

The web interface also includes a debug console that shows:
- Protocol messages
- Connection status
- Button states
- Movement data

## Browser Compatibility

The implementation requires:
- A modern web browser with WebSocket support
- Local network access to the Node.js server
- JavaScript enabled

## License

This implementation is part of the RT-Mouse Emulator project and is licensed under the MIT License. See the top-level [LICENSE](../LICENSE) file for details. 