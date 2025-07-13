const express = require('express');
const { SerialPort } = require('serialport');
const { WebSocketServer } = require('ws');
const debug = require('debug')('rt-mouse');
const wsDebug = require('debug')('rt-mouse:ws');
const serialDebug = require('debug')('rt-mouse:serial');
const path = require('path');

// Debug logging is controlled by the DEBUG environment variable
// Examples:
// DEBUG=rt-mouse:*        # Enable all logging
// DEBUG=rt-mouse:serial   # Enable only serial packet logging
// DEBUG=rt-mouse:ws       # Enable only WebSocket logging
// DEBUG=rt-mouse          # Enable only general logging

// Check for required serial port argument
if (process.argv.length < 3) {
    console.error('Error: Serial port path is required');
    console.error('Usage: node server.js <serial-port-path>');
    console.error('Example: node server.js /dev/tty.usbserial-1234');
    process.exit(1);
}

const serialPortPath = process.argv[2];
const app = express();
const port = 3000;

// Mouse state
let mouseState = {
    initialized: false,
    lastCommand: null,
    lastCommandTime: 0,
    enabled: false,
    mode: 'stream', // 'stream' or 'remote'
    scaling: 'linear', // 'linear' or 'exponential'
    resolution: 100, // counts per inch
    sampleRate: 100, // reports per second
    wrapMode: false,
    lastX: 0,      // Track last X position
    lastY: 0,      // Track last Y position
    leftButton: false,
    middleButton: false,
    rightButton: false
};

// Mouse protocol constants from mouseio.h
const MOUSE_CMD = {
    RESET: 0x01,           // returns a reset response
    READ_CONFIG: 0x06,     // returns one byte = 0x20
    ENABLE: 0x08,          // no response
    DISABLE: 0x09,         // no response
    READ_DATA: 0x0b,       // returns a data report
    WRAP_ON: 0x0e,         // no response; enters wrap mode
    WRAP_OFF: 0x0f,        // no response; leaves wrap mode
    SET_SCALE_EXP: 0x78,   // no response
    SET_SCALE_LIN: 0x6c,   // no response
    READ_STATUS: 0x73,     // returns a status report
    SET_RATE: 0x8a,        // no response; rate byte must follow
    SET_MODE: 0x8d,        // no response; mode byte must follow
    SET_RESOLUTION: 0x89   // no response; resolution byte must follow
};

// Command names for logging
const MOUSE_CMD_NAMES = {
    0x01: 'RESET',
    0x06: 'READ_CONFIG',
    0x08: 'ENABLE',
    0x09: 'DISABLE',
    0x0b: 'READ_DATA',
    0x0e: 'WRAP_ON',
    0x0f: 'WRAP_OFF',
    0x78: 'SET_SCALE_EXP',
    0x6c: 'SET_SCALE_LIN',
    0x73: 'READ_STATUS',
    0x8a: 'SET_RATE',
    0x8d: 'SET_MODE',
    0x89: 'SET_RESOLUTION'
};

// Mouse response constants
const MOUSE_RESPONSE = {
    RESET_ACK: [0xff, 0x08, 0x00, 0x00],  // Reset acknowledgment for generic mouse
    RESET_ACK2: [0xff, 0x04, 0x00, 0x00], // Reset acknowledgment for optical mouse
    CONFIGURED: [0x20, 0x00, 0x00, 0x00], // Configuration response
    DATA_REPORT: 0x0b,     // Magic number for data reports
    STATUS_REPORT: 0x61    // Magic number for status reports
};

// Mouse mode constants
const MOUSE_MODE = {
    STREAM: 0x00,
    REMOTE: 0x03
};

// Mouse resolution values (counts per inch)
const MOUSE_RESOLUTION = {
    RES200: 0x00,  // 200 counts per inch
    RES100: 0x01,  // 100 counts per inch
    RES50: 0x02,   // 50 counts per inch
    RES25: 0x03    // 25 counts per inch
};

// Mouse sample rate values (reports per second)
const MOUSE_SAMPLE_RATE = {
    RATE10: 0x0a,  // 10 reports per second
    RATE20: 0x14,  // 20 reports per second
    RATE40: 0x28,  // 40 reports per second
    RATE60: 0x3c,  // 60 reports per second
    RATE80: 0x50,  // 80 reports per second
    RATE100: 0x64  // 100 reports per second
};

// Enable WebSocket debugging if RT_MOUSE_WS_DEBUG is set
if (process.env.RT_MOUSE_WS_DEBUG) {
    wsDebug.enabled = true;
}

// Serve static files from public directory
app.use(express.static('public'));

// Create HTTP server
const server = app.listen(port, () => {
    debug(`Server running at http://localhost:${port}`);
    debug(`Using serial port: ${serialPortPath}`);
});

// Create WebSocket server
const wss = new WebSocketServer({ server });

// Serial port configuration
const serialPort = new SerialPort({
    path: serialPortPath,
    baudRate: 9600,
    dataBits: 8,
    stopBits: 1,
    parity: 'odd',
    autoOpen: false
});

// Track last received character time for timing info
let lastCharTime = Date.now();

// Handle serial port data
serialPort.on('data', (data) => {
    const now = Date.now();
    const timeSinceLastChar = now - lastCharTime;
    lastCharTime = now;

    // Process each byte
    for (const byte of data) {
        const cmd = byte;
        const cmdName = MOUSE_CMD_NAMES[cmd] || 'UNKNOWN';
        const result = handleMouseCommand(cmd);
        if (result) {
            debug(`Received ${cmdName} (0x${cmd.toString(16).toUpperCase()}), sent ${result.map(b => '0x' + b.toString(16).toUpperCase()).join(' ')}`);
        } else {
            debug(`Received ${cmdName} (0x${cmd.toString(16).toUpperCase()}), no response`);
        }
    }
});

function handleMouseCommand(cmd) {
    const now = Date.now();

    switch (cmd) {
        case MOUSE_CMD.RESET:
            // Send reset acknowledgment (4 bytes)
            const resetAck = MOUSE_RESPONSE.RESET_ACK;
            serialPort.write(Buffer.from(resetAck), (err) => {
                if (err) {
                    debug('Error sending reset acknowledgment:', err);
                }
            });
            mouseState.initialized = false;
            mouseState.enabled = false;
            mouseState.lastCommand = null;  // Reset clears any pending command
            mouseState.lastCommandTime = now;
            return resetAck;

        case MOUSE_CMD.READ_CONFIG:
            // Send configured state (4 bytes)
            const configResp = MOUSE_RESPONSE.CONFIGURED;
            serialPort.write(Buffer.from(configResp), (err) => {
                if (err) {
                    debug('Error sending configured state:', err);
                }
            });
            mouseState.initialized = true;
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return configResp;

        case MOUSE_CMD.ENABLE:
            mouseState.enabled = true;
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.DISABLE:
            mouseState.enabled = false;
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.WRAP_ON:
            mouseState.wrapMode = true;
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.WRAP_OFF:
            mouseState.wrapMode = false;
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.SET_SCALE_EXP:
            mouseState.scaling = 'exponential';
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.SET_SCALE_LIN:
            mouseState.scaling = 'linear';
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.READ_STATUS:
            // Send status report with current configuration (4 bytes)
            const status = [
                MOUSE_RESPONSE.STATUS_REPORT,  // Status report magic number
                (mouseState.enabled ? 0x00 : 0x20) | // Bit 5: 0=enabled, 1=disabled
                (mouseState.scaling === 'exponential' ? 0x10 : 0x00) | // Bit 4: 0=linear, 1=exponential
                (mouseState.mode === 'remote' ? 0x08 : 0x00) | // Bit 3: 0=stream, 1=remote
                0x04, // Bit 2: Always 1
                mouseState.resolution, // Resolution setting
                mouseState.sampleRate  // Sample rate setting
            ];
            serialPort.write(Buffer.from(status), (err) => {
                if (err) {
                    debug('Error sending status report:', err);
                }
            });
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return status;

        case MOUSE_CMD.SET_RATE:
            // The rate byte will come in the next byte
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.SET_MODE:
            // The mode byte will come in the next byte
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        case MOUSE_CMD.SET_RESOLUTION:
            // The resolution byte will come in the next byte
            mouseState.lastCommand = cmd;
            mouseState.lastCommandTime = now;
            return null;

        default:
            // Handle parameter bytes for commands that expect them
            if (mouseState.lastCommand === MOUSE_CMD.SET_RATE) {
                const cmdName = 'SET_RATE_PARAM';
                debug(`Received ${cmdName} (0x${cmd.toString(16).toUpperCase()}) for SET_RATE command`);
                mouseState.sampleRate = cmd;
                mouseState.lastCommand = null;
                return null;
            }
            else if (mouseState.lastCommand === MOUSE_CMD.SET_MODE) {
                const cmdName = 'SET_MODE_PARAM';
                debug(`Received ${cmdName} (0x${cmd.toString(16).toUpperCase()}) for SET_MODE command`);
                mouseState.mode = (cmd === MOUSE_MODE.REMOTE) ? 'remote' : 'stream';
                mouseState.lastCommand = null;
                return null;
            }
            else if (mouseState.lastCommand === MOUSE_CMD.SET_RESOLUTION) {
                const cmdName = 'SET_RESOLUTION_PARAM';
                debug(`Received ${cmdName} (0x${cmd.toString(16).toUpperCase()}) for SET_RESOLUTION command`);
                mouseState.resolution = cmd;
                mouseState.lastCommand = null;
                return null;
            }
            else {
                debug(`Unknown command: 0x${cmd.toString(16).toUpperCase()}`);
                return null;
            }
    }
}

// Handle serial port errors
serialPort.on('error', (err) => {
    debug('Serial port error:', err);
});

// Handle WebSocket connections
wss.on('connection', (ws) => {
    debug('New WebSocket connection');

    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            wsDebug('Received:', data);

            // Update button states
            if (data.type === 'buttons') {
                const oldState = {
                    left: mouseState.leftButton,
                    middle: mouseState.middleButton,
                    right: mouseState.rightButton
                };

                // Update button states based on state value
                // state is a bitmask: bit 0 = left, bit 1 = middle, bit 2 = right
                mouseState.leftButton = (data.state & 0x01) !== 0;
                mouseState.middleButton = (data.state & 0x02) !== 0;
                mouseState.rightButton = (data.state & 0x04) !== 0;

                wsDebug(`Button state change:`, {
                    state: data.state.toString(2).padStart(3, '0'),
                    before: oldState,
                    after: {
                        left: mouseState.leftButton,
                        middle: mouseState.middleButton,
                        right: mouseState.rightButton
                    }
                });
            }

            // Calculate movement deltas if this is a move event
            if (data.type === 'move') {
                // Client is already sending relative movements
                // Note: In RT PC protocol:
                // - Positive X means right movement
                // - Positive Y means down movement (invert client's Y)
                // For debugging: clamp all movements to +/-10
                const dx = data.x / 5;
                const dy = -data.y / 5;

                wsDebug('Movement details:', {
                    rawDeltas: { dx, dy }
                });

                // Convert to two's complement format
                // For negative values: invert all bits and add 1
                // For positive values: just use the value as is
                const byte3 = dx < 0 ? ((~Math.abs(dx) & 0xFF) + 1) & 0xFF : dx & 0xFF;  // X movement (7 bits)
                const byte4 = dy < 0 ? ((~Math.abs(dy) & 0xFF) + 1) & 0xFF : dy & 0xFF;  // Y movement (7 bits)

                const byte1 = MOUSE_RESPONSE.DATA_REPORT;  // Magic number (0x0b)
                const byte2 = ((mouseState.rightButton ? 0x80 : 0x00) |  // Right button (bit 7)
                               (mouseState.middleButton ? 0x40 : 0x00) | // Middle button (bit 6)
                               (mouseState.leftButton ? 0x20 : 0x00) |   // Left button (bit 5)
                               0x00 |                                    // Always 0 (bit 4)
                               0x00 |                                    // Reset bit (bit 3, normally 0)
                    (dx < 0 ? 0x04 : 0) |                                    // X sign bit (bit 2)
                    (dy < 0 ? 0x02 : 0));                                    // Y sign bit (bit 1)

                // Decode the packet for logging
                const statusBits = {
                    rightButton: (byte2 & 0x80) !== 0,    // Bit 7
                    middleButton: (byte2 & 0x40) !== 0,   // Bit 6
                    leftButton: (byte2 & 0x20) !== 0,     // Bit 5
                    reserved: (byte2 & 0x10) !== 0,       // Bit 4 (always 0)
                    reset: (byte2 & 0x08) !== 0,         // Bit 3 (reset bit)
                    xSign: (byte2 & 0x04) !== 0,         // Bit 2 (always 1)
                    ySign: (byte2 & 0x02) !== 0,         // Bit 1 (always 1)
                    unused: (byte2 & 0x01) !== 0         // Bit 0 (always 0)
                };

                // Format buttons as a string (e.g. "LMR" for all buttons, "L" for left only)
                const buttons = [
                    statusBits.leftButton ? 'L' : '',
                    statusBits.middleButton ? 'M' : '',
                    statusBits.rightButton ? 'R' : ''
                ].join('');

                // Format movement with direction
                // Note: In RT PC protocol:
                // - Values are in two's complement format
                // - Sign bits are always 1
                const xDir = dx < 0 ? '←' : '→';
                const yDir = dy < 0 ? '↑' : '↓';
                const xMove = `${xDir}${Math.abs(dx)}`;
                const yMove = `${yDir}${Math.abs(dy)}`;

                serialDebug(`Serial: ${byte1.toString(16).padStart(2, '0')} ${byte2.toString(16).padStart(2, '0')} ${byte3.toString(16).padStart(2, '0')} ${byte4.toString(16).padStart(2, '0')} | Buttons:${buttons || 'none'} Move:${xMove} ${yMove} (xsign:${(byte2 & 0x04) ? 1 : 0} ysign:${(byte2 & 0x02) ? 1 : 0})`);

                // Convert mouse data to RT PC protocol format (4 bytes)
                const report = [
                    byte1,                        // Data report magic number (0x0b)
                    byte2,                        // Status byte (sign bits always 1)
                    byte3,                        // X movement (two's complement)
                    byte4                         // Y movement (two's complement)
                ];

                serialPort.write(Buffer.from(report), (err) => {
                    if (err) {
                        debug('Error sending mouse data:', err);
                    }
                });
            } else {
                // For non-move events, just send a report with zero movement
                const report = [
                    MOUSE_RESPONSE.DATA_REPORT,
                    ((mouseState.rightButton ? 0x80 : 0x00) |
                     (mouseState.middleButton ? 0x40 : 0x00) |
                     (mouseState.leftButton ? 0x20 : 0x00)),
                    0x00,  // No X movement
                    0x00   // No Y movement
                ];

                serialPort.write(Buffer.from(report), (err) => {
                    if (err) {
                        debug('Error sending mouse data:', err);
                    }
                });
            }
        } catch (err) {
            debug('Error processing mouse data:', err);
        }
    });

    // Reset mouse state when connection is closed
    ws.on('close', () => {
        debug('WebSocket connection closed');
        wsDebug('Resetting mouse state');
        mouseState.lastX = 0;
        mouseState.lastY = 0;
        mouseState.leftButton = false;
        mouseState.middleButton = false;
        mouseState.rightButton = false;
    });
});

// Open serial port
serialPort.open((err) => {
    if (err) {
        debug('Error opening serial port:', err);
        process.exit(1);
    }
    debug('Serial port opened');
});

// Handle process termination
process.on('SIGINT', () => {
    debug('Closing serial port...');
    serialPort.close(() => {
        debug('Serial port closed');
        process.exit(0);
    });
});
