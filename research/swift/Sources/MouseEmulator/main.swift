import Foundation
import Cocoa
import SwiftSerial

// Debug logging levels
enum DebugLevel {
    case none
    case general
    case serial
    case all
}

// Debug logging functions
func debug(_ message: String, level: DebugLevel = .general) {
    if currentDebugLevel == .all || currentDebugLevel == level {
        let timestamp = String(format: "%.3f", Date().timeIntervalSince1970)
        print("[\(timestamp)] [DEBUG] \(message)")
    }
}

func serialDebug(_ message: String) {
    debug(message, level: .serial)
}

// Global debug level, set by command line
var currentDebugLevel: DebugLevel = .none

// Mouse state struct to replace tuple
private struct MouseState {
    var initialized: Bool = false
    var lastCommand: UInt8? = nil
    var lastCommandTime: Date = Date()
    var enabled: Bool = true
    var mode: String = "stream" // 'stream' or 'remote'
    var scaling: String = "linear" // 'linear' or 'exponential'
    var resolution: UInt8 = 100 // counts per inch
    var sampleRate: UInt8 = 100 // reports per second
    var wrapMode: Bool = false
    var leftButton: Bool = false
    var middleButton: Bool = false
    var rightButton: Bool = false
}

class MouseMonitor: NSObject {
    let serialPort: SerialPort
    var lastX: CGFloat = 0
    var lastY: CGFloat = 0
    var isInitialized = false
    private var eventMonitor: Any? = nil
    private var captureWindow: NSWindow? = nil
    
    // Rate limiting state
    private var reportTimer: Timer? = nil
    private var currentCycleState = (
        dx: 0,  // Movement in current cycle
        dy: 0,  // Movement in current cycle
        buttons: (left: false, middle: false, right: false)  // Current button state
    )
    private var lastReportTime = Date()
    private let reportRate: TimeInterval = 1.0 / 10.0  // 10 reports per second
    private let reportQueue = DispatchQueue(label: "com.rt-mouse.report")
    private var isProcessingReport = false
    
    // Mouse protocol constants from mouseio.h (matching JavaScript version)
    private let MOUSE_CMD = (
        RESET: UInt8(0x01),           // returns a reset response
        READ_CONFIG: UInt8(0x06),     // returns one byte = 0x20
        ENABLE: UInt8(0x08),          // no response
        DISABLE: UInt8(0x09),         // no response
        READ_DATA: UInt8(0x0b),       // returns a data report
        WRAP_ON: UInt8(0x0e),         // no response; enters wrap mode
        WRAP_OFF: UInt8(0x0f),        // no response; leaves wrap mode
        SET_SCALE_EXP: UInt8(0x78),   // no response
        SET_SCALE_LIN: UInt8(0x6c),   // no response
        READ_STATUS: UInt8(0x73),     // returns a status report
        SET_RATE: UInt8(0x8a),        // no response; rate byte must follow
        SET_MODE: UInt8(0x8d),        // no response; mode byte must follow
        SET_RESOLUTION: UInt8(0x89)   // no response; resolution byte must follow
    )
    
    private let MOUSE_RESPONSE = (
        RESET_ACK: [UInt8(0xff), UInt8(0x08), UInt8(0x00), UInt8(0x00)],  // Reset acknowledgment
        RESET_ACK2: [UInt8(0xff), UInt8(0x04), UInt8(0x00), UInt8(0x00)], // Reset acknowledgment for optical
        CONFIGURED: [UInt8(0x20), UInt8(0x00), UInt8(0x00), UInt8(0x00)], // Configuration response
        DATA_REPORT: UInt8(0x0b),     // Magic number for data reports
        STATUS_REPORT: UInt8(0x61)    // Magic number for status reports
    )
    
    // Mouse state using the struct
    private var mouseState = MouseState()
    
    // Serial port write queue and buffer
    private let writeQueue = DispatchQueue(label: "com.rt-mouse.serial.write")
    private var writeBuffer = [UInt8]()
    private let writeBufferLock = NSLock()
    private var isWriting = false
    
    // Helper function to clamp a Double to Int8 range
    private func clampToInt8(_ value: Double) -> Int8 {
        if value > Double(Int8.max) {
            return Int8.max
        } else if value < Double(Int8.min) {
            return Int8.min
        } else {
            return Int8(value)
        }
    }
    
    init(serialPortPath: String) {
        // Configure serial port
        serialPort = SerialPort(path: serialPortPath)
        
        // Initialize properties before super.init
        super.init()
        
        do {
            debug("Opening serial port...")
            try serialPort.openPort()
            debug("Setting serial port settings...")
            serialPort.setSettings(
                receiveRate: .baud9600,
                transmitRate: .baud9600,
                minimumBytesToRead: 1,
                timeout: 100,
                parityType: .odd,
                sendTwoStopBits: false,
                dataBitsSize: .bits8,
                useHardwareFlowControl: false,
                useSoftwareFlowControl: false,
                processOutput: false
            )
            debug("Serial port opened and configured successfully")
            
            // Start reading thread for host commands after super.init
            debug("Starting serial port read thread...")
            DispatchQueue.global(qos: .userInitiated).async { [weak self] in
                self?.readHostCommands()
            }
            
            // Start write queue processing
            debug("Starting write queue...")
            startWriteQueue()
            
            // Create transparent window for event capture
            setupCaptureWindow()
            
            // Start report timer
            startReportTimer()
            
        } catch {
            debug("Error opening serial port: \(error)")
            exit(1)
        }
        
        debug("Mouse monitoring started. Press Ctrl+C to exit.")
    }
    
    private func setupCaptureWindow() {
        // Create a transparent window that covers the entire screen
        let screenFrame = NSScreen.main?.frame ?? .zero
        captureWindow = NSWindow(
            contentRect: screenFrame,
            styleMask: [.borderless],
            backing: .buffered,
            defer: false
        )
        
        // Make the window transparent but NOT click-through
        captureWindow?.backgroundColor = .clear
        captureWindow?.isOpaque = false
        captureWindow?.level = .floating
        captureWindow?.ignoresMouseEvents = false  // Changed to false to capture events
        captureWindow?.collectionBehavior = [.canJoinAllSpaces, .stationary]
        
        // Create a transparent view to receive events
        let captureView = MouseCaptureView()
        captureView.mouseMonitor = self
        captureWindow?.contentView = captureView
        
        // Make the window key window to receive keyboard events
        captureWindow?.makeKeyAndOrderFront(nil)
        
        // Set up global monitor for all events
        eventMonitor = NSEvent.addGlobalMonitorForEvents(matching: [.mouseMoved, .leftMouseDown, .leftMouseUp, .rightMouseDown, .rightMouseUp, .otherMouseDown, .otherMouseUp, .leftMouseDragged, .rightMouseDragged, .otherMouseDragged, .keyDown, .keyUp, .flagsChanged]) { [weak self] event in
            if event.type == .keyDown && event.keyCode == 53 {  // 53 is the keyCode for Escape
                debug("Escape key pressed, exiting...")
                NSApplication.shared.terminate(nil)
                return
            }
            
            self?.handleMouseEvent(event)
        }
        
        // Also set up local monitor to ensure we get all events
        let localMonitor = NSEvent.addLocalMonitorForEvents(matching: [.mouseMoved, .leftMouseDown, .leftMouseUp, .rightMouseDown, .rightMouseUp, .otherMouseDown, .otherMouseUp, .leftMouseDragged, .rightMouseDragged, .otherMouseDragged, .keyDown, .keyUp, .flagsChanged]) { [weak self] event in
            if event.type == .keyDown && event.keyCode == 53 {  // 53 is the keyCode for Escape
                debug("Escape key pressed, exiting...")
                NSApplication.shared.terminate(nil)
                return nil  // Consume the event
            }
            
            self?.handleMouseEvent(event)
            return event  // Don't consume the event, let it propagate
        }
        
        // Store both monitors
        eventMonitors = [eventMonitor, localMonitor]
    }
    
    deinit {
        reportTimer?.invalidate()
        if let monitors = eventMonitors {
            for monitor in monitors {
                if let globalMonitor = monitor as? AnyObject {
                    NSEvent.removeMonitor(globalMonitor)
                }
            }
        }
        captureWindow?.close()
    }
    
    private var eventMonitors: [Any]? = nil
    
    private func startWriteQueue() {
        // Process write buffer every ~10ms (roughly matching 9600 baud rate for 4 bytes)
        Timer.scheduledTimer(withTimeInterval: 0.01, repeats: true) { [weak self] _ in
            self?.processWriteBuffer()
        }
    }
    
    private func processWriteBuffer() {
        writeBufferLock.lock()
        defer { writeBufferLock.unlock() }
        
        guard !writeBuffer.isEmpty && !isWriting else { return }
        isWriting = true
        
        // Take up to 4 bytes from the buffer (one complete mouse report)
        let bytesToWrite = Array(writeBuffer.prefix(4))
        writeBuffer.removeFirst(min(4, writeBuffer.count))
        
        debug("Processing write buffer: \(bytesToWrite.count) bytes, \(writeBuffer.count) bytes remaining")
        
        writeQueue.async { [weak self] in
            guard let self = self else { return }
            
            do {
                // Create a mutable copy of the bytes
                var mutableBytes = bytesToWrite
                try mutableBytes.withUnsafeMutableBufferPointer { buffer in
                    let bytesWritten = try self.serialPort.writeBytes(from: buffer.baseAddress!, size: buffer.count)
                    debug("Wrote \(bytesWritten) bytes to serial port")
                }
            } catch {
                debug("Error writing to serial port: \(error)")
            }
            
            self.writeBufferLock.lock()
            self.isWriting = false
            self.writeBufferLock.unlock()
        }
    }
    
    private func queueWrite(_ bytes: [UInt8]) {
        writeBufferLock.lock()
        writeBuffer.append(contentsOf: bytes)
        writeBufferLock.unlock()
    }
    
    private func handleHostCommand(_ command: UInt8) {
        let cmdName = MOUSE_CMD_NAMES[command] ?? "UNKNOWN"
        serialDebug("Received command: \(cmdName) (0x\(String(format: "%02x", command)))")
        
        switch command {
        case MOUSE_CMD.RESET:
            queueWrite(MOUSE_RESPONSE.RESET_ACK)
            serialDebug("Sent RESET_ACK response")
            mouseState.initialized = false
            mouseState.enabled = false
            mouseState.lastCommand = nil
            
        case MOUSE_CMD.READ_CONFIG:
            queueWrite(MOUSE_RESPONSE.CONFIGURED)
            serialDebug("Sent CONFIGURED response")
            mouseState.initialized = true
            mouseState.lastCommand = command
            
        case MOUSE_CMD.ENABLE:
            serialDebug("Mouse enabled")
            mouseState.enabled = true
            mouseState.lastCommand = command
            
        case MOUSE_CMD.DISABLE:
            serialDebug("Mouse disabled")
            mouseState.enabled = false
            mouseState.lastCommand = command
            
        case MOUSE_CMD.WRAP_ON:
            serialDebug("Wrap mode enabled")
            mouseState.wrapMode = true
            mouseState.lastCommand = command
            
        case MOUSE_CMD.WRAP_OFF:
            serialDebug("Wrap mode disabled")
            mouseState.wrapMode = false
            mouseState.lastCommand = command
            
        case MOUSE_CMD.SET_SCALE_EXP:
            serialDebug("Scale set to exponential")
            mouseState.scaling = "exponential"
            mouseState.lastCommand = command
            
        case MOUSE_CMD.SET_SCALE_LIN:
            serialDebug("Scale set to linear")
            mouseState.scaling = "linear"
            mouseState.lastCommand = command
            
        case MOUSE_CMD.READ_STATUS:
            let status: [UInt8] = [
                MOUSE_RESPONSE.STATUS_REPORT,
                (mouseState.enabled ? UInt8(0x00) : UInt8(0x20)) |
                (mouseState.scaling == "exponential" ? UInt8(0x10) : UInt8(0x00)) |
                (mouseState.mode == "remote" ? UInt8(0x08) : UInt8(0x00)) |
                UInt8(0x04),
                mouseState.resolution,
                mouseState.sampleRate
            ]
            queueWrite(status)
            serialDebug("Sent status report: enabled=\(mouseState.enabled), scaling=\(mouseState.scaling), mode=\(mouseState.mode), resolution=\(mouseState.resolution), rate=\(mouseState.sampleRate)")
            mouseState.lastCommand = command
            
        case MOUSE_CMD.SET_RATE, MOUSE_CMD.SET_MODE, MOUSE_CMD.SET_RESOLUTION:
            serialDebug("Waiting for parameter byte for command \(cmdName)")
            mouseState.lastCommand = command
            
        default:
            // Handle parameter bytes for commands that expect them
            if let lastCmd = mouseState.lastCommand {
                switch lastCmd {
                case MOUSE_CMD.SET_RATE:
                    mouseState.sampleRate = command
                    serialDebug("Set sample rate to \(command) reports/sec")
                case MOUSE_CMD.SET_MODE:
                    mouseState.mode = command == 0x03 ? "remote" : "stream"
                    serialDebug("Set mode to \(mouseState.mode)")
                case MOUSE_CMD.SET_RESOLUTION:
                    mouseState.resolution = command
                    serialDebug("Set resolution to \(command) counts/inch")
                default:
                    break
                }
                mouseState.lastCommand = nil
            } else {
                serialDebug("Unknown command: 0x\(String(format: "%02x", command))")
            }
        }
    }
    
    private func processPendingState() {
        // Skip if we're already processing a report
        guard !isProcessingReport else {
            debug("Skipping report processing - previous report still in progress (isProcessingReport=\(isProcessingReport))")
            return
        }
        
        let workItem = DispatchWorkItem { [weak self] in
            guard let self = self else { return }
            
            let cycleStartTime = Date()
            self.isProcessingReport = true
            defer { 
                self.isProcessingReport = false
                // Reset cycle state after processing
                self.currentCycleState.dx = 0
                self.currentCycleState.dy = 0
                let cycleEndTime = Date()
                debug("Cycle completed in \(String(format: "%.3f", cycleEndTime.timeIntervalSince(cycleStartTime)))s")
            }
            
            // Get current mouse button state
            let mouseButtons = NSEvent.pressedMouseButtons
            
            // Get modifier keys
            let modifiers = NSEvent.modifierFlags
            let ctrlPressed = modifiers.contains(.control)
            let altPressed = modifiers.contains(.option)
            
            // Map physical buttons to logical buttons based on modifiers
            // Left button (bit 0) can be modified by ctrl/alt
            // Right button (bit 1) and middle button (bit 2) are used directly
            let leftButton = (mouseButtons & (1 << 0)) != 0 && !ctrlPressed && !altPressed
            let middleButton = ((mouseButtons & (1 << 0)) != 0 && ctrlPressed && !altPressed) || (mouseButtons & (1 << 2)) != 0
            let rightButton = ((mouseButtons & (1 << 0)) != 0 && altPressed) || (mouseButtons & (1 << 1)) != 0
            
            // Check if button states have changed
            let oldButtons = (self.mouseState.leftButton, self.mouseState.middleButton, self.mouseState.rightButton)
            let newButtons = (leftButton, middleButton, rightButton)
            let buttonsChanged = oldButtons != newButtons
            
            if buttonsChanged {
                debug("Button state changed: old=(\(oldButtons.0),\(oldButtons.1),\(oldButtons.2)), new=(\(newButtons.0),\(newButtons.1),\(newButtons.2))")
                debug("Physical buttons: \(String(format: "%x", mouseButtons)), modifiers: ctrl=\(ctrlPressed), alt=\(altPressed)")
                
                // Update both mouse state and current cycle state
                self.mouseState.leftButton = leftButton
                self.mouseState.middleButton = middleButton
                self.mouseState.rightButton = rightButton
                self.currentCycleState.buttons = (left: leftButton, middle: middleButton, right: rightButton)
            }
            
            // Calculate time since last report
            let now = Date()
            let timeSinceLastReport = now.timeIntervalSince(self.lastReportTime)
            
            // Always process movement if there is any, regardless of button state
            let hasMovement = self.currentCycleState.dx != 0 || self.currentCycleState.dy != 0
            let needsReport = buttonsChanged || hasMovement
            
            if needsReport {
                if hasMovement {
                    // Calculate movement per second for debugging
                    let dxPerSecond = Double(self.currentCycleState.dx) / timeSinceLastReport
                    let dyPerSecond = Double(self.currentCycleState.dy) / timeSinceLastReport
                    
                    debug("Processing report: cycle dx=\(self.currentCycleState.dx), dy=\(self.currentCycleState.dy), " +
                          "rate: \(Int(dxPerSecond))/s, \(Int(dyPerSecond))/s, " +
                          "time since last report: \(String(format: "%.3f", timeSinceLastReport))s, " +
                          "cycle start: \(String(format: "%.3f", cycleStartTime.timeIntervalSince1970))")
                    
                    // Handle large movements by sending multiple reports if needed
                    var remainingDx = self.currentCycleState.dx
                    var remainingDy = self.currentCycleState.dy
                    var reportCount = 0
                    
                    while remainingDx != 0 || remainingDy != 0 {
                        let reportStartTime = Date()
                        reportCount += 1
                        
                        // Calculate this report's movement, clamping to Int8 range
                        let rawDx = Double(remainingDx)
                        let rawDy = Double(remainingDy)
                        let reportDx = self.clampToInt8(rawDx)
                        let reportDy = self.clampToInt8(rawDy)
                        
                        debug("Report \(reportCount) movement calculation: " +
                              "raw dx=\(rawDx), dy=\(rawDy), " +
                              "clamped dx=\(reportDx), dy=\(reportDy), " +
                              "remaining dx=\(remainingDx), dy=\(remainingDy)")
                        
                        // Send the report with current button states
                        self.sendMouseState(
                            x: reportDx,
                            y: reportDy,
                            leftButton: self.mouseState.leftButton,
                            middleButton: self.mouseState.middleButton,
                            rightButton: self.mouseState.rightButton
                        )
                        
                        // Update remaining movement, being careful with signs
                        remainingDx -= Int(reportDx)
                        remainingDy -= Int(reportDy)
                        
                        let reportTime = Date().timeIntervalSince(reportStartTime)
                        debug("Report \(reportCount) sent: dx=\(reportDx), dy=\(reportDy), " +
                              "remaining dx=\(remainingDx), dy=\(remainingDy), " +
                              "report time: \(String(format: "%.3f", reportTime))s")
                        
                        // If we still have movement to report, add a small delay
                        if remainingDx != 0 || remainingDy != 0 {
                            Thread.sleep(forTimeInterval: 0.001)  // 1ms delay between reports
                        }
                    }
                    
                    debug("Movement complete: sent \(reportCount) reports, " +
                          "total time: \(String(format: "%.3f", Date().timeIntervalSince(cycleStartTime)))s")
                } else if buttonsChanged {
                    // Send a report for button changes only
                    debug("Sending button-only report")
                    self.sendMouseState(
                        x: 0,
                        y: 0,
                        leftButton: self.mouseState.leftButton,
                        middleButton: self.mouseState.middleButton,
                        rightButton: self.mouseState.rightButton
                    )
                }
                
                // Update last report time
                self.lastReportTime = now
            } else {
                debug("No changes to report in this cycle " +
                      "(time since last report: \(String(format: "%.3f", timeSinceLastReport))s)")
            }
        }
        reportQueue.sync(execute: workItem)
    }
    
    private func sendMouseState(x: Int8, y: Int8, leftButton: Bool? = nil, middleButton: Bool? = nil, rightButton: Bool? = nil) {
        // Create status byte with button states
        let statusByte: UInt8 = ((rightButton ?? mouseState.rightButton) ? 0x80 : 0x00) |
                               ((middleButton ?? mouseState.middleButton) ? 0x40 : 0x00) |
                               ((leftButton ?? mouseState.leftButton) ? 0x20 : 0x00) |
                               0x00 |                                    // Always 0 (bit 4)
                               0x00 |                                    // Reset bit (bit 3, normally 0)
                               (x < 0 ? 0x04 : 0) |                      // X sign bit (bit 2)
                               (y < 0 ? 0x02 : 0)                        // Y sign bit (bit 1)
        
        // Convert to two's complement format
        let byte3 = UInt8(bitPattern: x)  // X movement
        let byte4 = UInt8(bitPattern: y)  // Y movement
        
        // Create the complete mouse report
        let report: [UInt8] = [
            MOUSE_RESPONSE.DATA_REPORT,  // Data report magic number
            statusByte,                  // Status byte with button states
            byte3,                       // X movement
            byte4                        // Y movement
        ]
        
        // Format buttons as a string (e.g. "LMR" for all buttons, "L" for left only)
        let buttons = [
            (leftButton ?? mouseState.leftButton) ? "L" : "",
            (middleButton ?? mouseState.middleButton) ? "M" : "",
            (rightButton ?? mouseState.rightButton) ? "R" : ""
        ].joined()
        
        // Format movement with direction
        let xDir = x < 0 ? "←" : "→"
        let yDir = y < 0 ? "↑" : "↓"
        let xMove = "\(xDir)\(x == Int8.min ? 127 : abs(x))"  // Handle Int8.min case
        let yMove = "\(yDir)\(y == Int8.min ? 127 : abs(y))"  // Handle Int8.min case
        
        // Use serialDebug for serial port messages
        serialDebug("Serial: \(String(format: "%02x", report[0])) \(String(format: "%02x", report[1])) \(String(format: "%02x", report[2])) \(String(format: "%02x", report[3])) | Buttons:\(buttons.isEmpty ? "none" : buttons) Move:\(xMove) \(yMove) (xsign:\((statusByte & 0x04) != 0 ? 1 : 0) ysign:\((statusByte & 0x02) != 0 ? 1 : 0))")
        
        queueWrite(report)
    }
    
    private func startReportTimer() {
        reportTimer = Timer.scheduledTimer(withTimeInterval: reportRate, repeats: true) { [weak self] _ in
            self?.processPendingState()
        }
    }
    
    func handleMouseEvent(_ event: NSEvent) {
        // We still need this function to be present for the MouseCaptureView,
        // but we'll only handle mouse movement events
        if !mouseState.enabled {
            debug("Mouse is disabled, ignoring event")
            return
        }
        
        let eventTime = Date()
        let workItem = DispatchWorkItem { [weak self] in
            guard let self = self else { return }
            
            // Handle all types of mouse movement events
            switch event.type {
            case .mouseMoved, .leftMouseDragged, .rightMouseDragged, .otherMouseDragged:
                // Add to current cycle's movement
                let dx = Int(event.deltaX)
                let dy = Int(-event.deltaY)  // Inverted Y-axis
                
                // Debug the raw event values
                debug("Mouse event: type=\(event.type), deltaX=\(event.deltaX), deltaY=\(event.deltaY), " +
                      "inverted dy=\(dy), buttons=\(String(format: "%x", NSEvent.pressedMouseButtons))")
                
                self.currentCycleState.dx += dx
                self.currentCycleState.dy += dy
                
                // Log detailed movement info with timing
                let timeSinceLastReport = eventTime.timeIntervalSince(self.lastReportTime)
                debug("Movement event: raw dx=\(event.deltaX), dy=\(event.deltaY), " +
                      "cycle dx=\(self.currentCycleState.dx), dy=\(self.currentCycleState.dy), " +
                      "time since last report: \(String(format: "%.3f", timeSinceLastReport))s, " +
                      "event time: \(String(format: "%.3f", eventTime.timeIntervalSince1970))")
                
            default:
                // Ignore all other events since we're polling for button states
                return
            }
        }
        reportQueue.sync(execute: workItem)
    }
    
    private func readHostCommands() {
        debug("Serial port read thread started")
        var buffer = [UInt8](repeating: 0, count: 1)
        while true {
            do {
                let bytesRead = try serialPort.readBytes(into: &buffer, size: 1)
                if bytesRead > 0 {
                    debug("Read \(bytesRead) bytes from serial port")
                    handleHostCommand(buffer[0])
                }
            } catch {
                debug("Error reading from serial port: \(error)")
                // Try to reconnect after a delay
                DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
                    debug("Attempting to reconnect serial port...")
                    self?.reconnect()
                }
                break
            }
        }
    }
    
    func reconnect() {
        do {
            try serialPort.openPort()
            serialPort.setSettings(
                receiveRate: .baud9600,
                transmitRate: .baud9600,
                minimumBytesToRead: 1,
                timeout: 100,
                parityType: .odd,  // Changed to odd parity
                sendTwoStopBits: false,
                dataBitsSize: .bits8,
                useHardwareFlowControl: false,
                useSoftwareFlowControl: false,
                processOutput: false
            )
            mouseState.initialized = false
            mouseState.enabled = false
            print("Serial port reconnected and configured")
            
            // Restart reading thread
            DispatchQueue.global(qos: .userInitiated).async { [weak self] in
                self?.readHostCommands()
            }
        } catch {
            print("Error reconnecting: \(error)")
            // Try again in 5 seconds
            DispatchQueue.main.asyncAfter(deadline: .now() + 5) { [weak self] in
                self?.reconnect()
            }
        }
    }
}

// Custom view to capture mouse events
class MouseCaptureView: NSView {
    weak var mouseMonitor: MouseMonitor?
    
    // Make the view accept all mouse events
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool {
        return true
    }
    
    // Make the view accept keyboard events
    override var acceptsFirstResponder: Bool {
        return true
    }
    
    // Make sure we can track mouse events
    override func mouseDown(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func mouseDragged(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func mouseUp(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func rightMouseDown(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func rightMouseDragged(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func rightMouseUp(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func otherMouseDown(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func otherMouseDragged(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func otherMouseUp(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func mouseMoved(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    // Handle keyboard events
    override func keyDown(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func keyUp(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
    
    override func flagsChanged(with event: NSEvent) {
        mouseMonitor?.handleMouseEvent(event)
    }
}

// Command names for logging
private let MOUSE_CMD_NAMES: [UInt8: String] = [
    0x01: "RESET",
    0x06: "READ_CONFIG",
    0x08: "ENABLE",
    0x09: "DISABLE",
    0x0b: "READ_DATA",
    0x0e: "WRAP_ON",
    0x0f: "WRAP_OFF",
    0x78: "SET_SCALE_EXP",
    0x6c: "SET_SCALE_LIN",
    0x73: "READ_STATUS",
    0x8a: "SET_RATE",
    0x8d: "SET_MODE",
    0x89: "SET_RESOLUTION"
]

class AppDelegate: NSObject, NSApplicationDelegate {
    var mouseMonitor: MouseMonitor?
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Parse command line arguments
        let args = CommandLine.arguments
        
        // Set debug level based on command line argument
        if args.contains("--debug-all") {
            currentDebugLevel = .all
        } else if args.contains("--debug-serial") {
            currentDebugLevel = .serial
        } else if args.contains("--debug") {
            currentDebugLevel = .general
        }
        
        // Get serial port path from command line arguments
        let serialPortArg = args.first { arg in
            !arg.hasPrefix("--") && arg != args[0]  // Skip program name and debug flags
        }
        
        guard let serialPortPath = serialPortArg else {
            print("Error: Serial port path is required")
            print("Usage: \(args[0]) [--debug|--debug-serial|--debug-all] <serial-port-path>")
            print("Example: \(args[0]) --debug-serial /dev/tty.usbserial-1234")
            exit(1)
        }
        
        debug("Using serial port: \(serialPortPath)")
        if currentDebugLevel != .none {
            debug("Debug logging enabled: \(currentDebugLevel)")
        }
        
        // Create the mouse monitor
        mouseMonitor = MouseMonitor(serialPortPath: serialPortPath)
    }
}

// Create the application
let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate

// Run the application
app.run() 