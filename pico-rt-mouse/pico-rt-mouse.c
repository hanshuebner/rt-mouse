#include <stdio.h>
#include <pico/stdio.h>
#include <bsp/board.h>
#include <tusb.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <stdint.h>
#include <string.h>

// UART1 configuration for RT mouse protocol
#define RT_UART_ID uart1
#define RT_UART_TX_PIN 8
#define RT_UART_RX_PIN 9
#define RT_UART_BAUD 9600
#define RT_UART_DATA_BITS 8
#define RT_UART_STOP_BITS 1
#define RT_UART_PARITY UART_PARITY_ODD

// RT mouse protocol constants
#define RT_MOUSE_DATA_REPORT 0x0b
#define RT_MOUSE_STATUS_REPORT 0x61
#define RT_MOUSE_RESET_ACK 0xff
#define RT_MOUSE_CONFIGURED 0x20

// RT mouse protocol commands
#define MOUSE_CMD_RESET 0x01
#define MOUSE_CMD_READ_CONFIG 0x06
#define MOUSE_CMD_ENABLE 0x08
#define MOUSE_CMD_DISABLE 0x09
#define MOUSE_CMD_READ_DATA 0x0b
#define MOUSE_CMD_WRAP_ON 0x0e
#define MOUSE_CMD_WRAP_OFF 0x0f
#define MOUSE_CMD_SET_SCALE_EXP 0x78
#define MOUSE_CMD_SET_SCALE_LIN 0x6c
#define MOUSE_CMD_READ_STATUS 0x73
#define MOUSE_CMD_SET_RATE 0x8a
#define MOUSE_CMD_SET_MODE 0x8d
#define MOUSE_CMD_SET_RESOLUTION 0x89

// Mouse state
struct MouseState {
    bool initialized;
    uint8_t last_command;
    bool enabled;
    bool wrap_mode;
    char scaling; // 'l' = linear, 'e' = exponential
    uint8_t resolution;
    uint8_t sample_rate;
    char mode; // 's' = stream, 'r' = remote
    bool left_button;
    bool middle_button;
    bool right_button;
};

static struct MouseState mouse_state = {
    .initialized = false,
    .last_command = 0,
    .enabled = true,
    .wrap_mode = false,
    .scaling = 'l',
    .resolution = 100,
    .sample_rate = 100,
    .mode = 's',
    .left_button = false,
    .middle_button = false,
    .right_button = false
};

// UART1 initialization
void init_rt_uart() {
    uart_init(RT_UART_ID, RT_UART_BAUD);
    uart_set_format(RT_UART_ID, RT_UART_DATA_BITS, RT_UART_STOP_BITS, RT_UART_PARITY);
    uart_set_hw_flow(RT_UART_ID, false, false);
    uart_set_fifo_enabled(RT_UART_ID, true);
    gpio_set_function(RT_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RT_UART_RX_PIN, GPIO_FUNC_UART);
}

// Send 4-byte RT mouse report over UART1
void send_mouse_report_uart(const uint8_t report[4]) {
    uart_write_blocking(RT_UART_ID, report, 4);
}

// Send status report over UART1
void send_status_report_uart() {
    uint8_t status[4] = {
        RT_MOUSE_STATUS_REPORT,
        (mouse_state.enabled ? 0x00 : 0x20) |
        (mouse_state.scaling == 'e' ? 0x10 : 0x00) |
        (mouse_state.mode == 'r' ? 0x08 : 0x00) |
        0x04,
        mouse_state.resolution,
        mouse_state.sample_rate
    };
    send_mouse_report_uart(status);
}

// Send reset ack over UART1
void send_reset_ack_uart() {
    uint8_t ack[4] = {RT_MOUSE_RESET_ACK, 0x08, 0x00, 0x00};
    send_mouse_report_uart(ack);
}

// Send config response over UART1
void send_configured_uart() {
    uint8_t conf[4] = {RT_MOUSE_CONFIGURED, 0x00, 0x00, 0x00};
    send_mouse_report_uart(conf);
}

// Translate TinyUSB mouse report to RT PC format and send over UART1
void send_rt_mouse_data(const hid_mouse_report_t *report) {
    printf("sending report.\n");
    uint8_t status = 0;
    if (report->buttons & 0x01) status |= 0x20; // left
    if (report->buttons & 0x02) status |= 0x80; // right
    if (report->buttons & 0x04) status |= 0x40; // middle
    if (report->x < 0) status |= 0x04;
    if (report->y < 0) status |= 0x02;
    uint8_t out[4] = {
        RT_MOUSE_DATA_REPORT,
        status,
        (uint8_t)report->x,
        (uint8_t)report->y
    };
    send_mouse_report_uart(out);
}

// Handle RT mouse protocol commands from host
void handle_rt_mouse_command(uint8_t cmd) {
    switch (cmd) {
        case MOUSE_CMD_RESET:
            send_reset_ack_uart();
            mouse_state.initialized = false;
            mouse_state.enabled = false;
            mouse_state.last_command = 0;
            break;
        case MOUSE_CMD_READ_CONFIG:
            send_configured_uart();
            mouse_state.initialized = true;
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_ENABLE:
            mouse_state.enabled = true;
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_DISABLE:
            mouse_state.enabled = false;
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_WRAP_ON:
            mouse_state.wrap_mode = true;
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_WRAP_OFF:
            mouse_state.wrap_mode = false;
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_SET_SCALE_EXP:
            mouse_state.scaling = 'e';
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_SET_SCALE_LIN:
            mouse_state.scaling = 'l';
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_READ_STATUS:
            send_status_report_uart();
            mouse_state.last_command = cmd;
            break;
        case MOUSE_CMD_SET_RATE:
        case MOUSE_CMD_SET_MODE:
        case MOUSE_CMD_SET_RESOLUTION:
            mouse_state.last_command = cmd;
            break;
        default:
            // Handle parameter bytes for SET_RATE, SET_MODE, SET_RESOLUTION
            switch (mouse_state.last_command) {
                case MOUSE_CMD_SET_RATE:
                    mouse_state.sample_rate = cmd;
                    break;
                case MOUSE_CMD_SET_MODE:
                    mouse_state.mode = (cmd == 0x03) ? 'r' : 's';
                    break;
                case MOUSE_CMD_SET_RESOLUTION:
                    mouse_state.resolution = cmd;
                    break;
                default:
                    break;
            }
            mouse_state.last_command = 0;
            break;
    }
}

// Poll UART1 for incoming RT mouse protocol commands
void poll_rt_mouse_uart() {
    while (uart_is_readable(RT_UART_ID)) {
        uint8_t cmd = uart_getc(RT_UART_ID);
        handle_rt_mouse_command(cmd);
    }
}

// TinyUSB callback: device mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    printf("HID device mounted: Address %d, Instance %d\n", dev_addr, instance);
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        printf("Mouse detected, requesting reports...\n");
        tuh_hid_receive_report(dev_addr, instance);
    }
}

// TinyUSB callback: device unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID device unmounted: Address %d, Instance %d\n", dev_addr, instance);
}

// TinyUSB callback: report received
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE && len >= sizeof(hid_mouse_report_t)) {
        if (mouse_state.enabled) {
            send_rt_mouse_data((const hid_mouse_report_t *)report);
        }
    }
    // Request the next report
    tuh_hid_receive_report(dev_addr, instance);
}

int main(void) {
    stdio_init_all(); // UART0 for debug
    board_init();
    tuh_init(BOARD_TUH_RHPORT);
    board_init_after_tusb();
    init_rt_uart();
    printf("pico-rt-mouse running (RT mouse protocol on UART1)\n");
    while (1) {
        tuh_task();
        poll_rt_mouse_uart();
    }
    return 0;
} 