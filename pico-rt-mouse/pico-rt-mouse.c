#include <stdio.h>
#include <pico/stdio.h>
#include <bsp/board.h>
#include <tusb.h>
#include <hardware/gpio.h>
#include <stdint.h>

// GPIO pin definitions (same as pico-ascii-keyboard)
#define GPIO_FIRST_BIT 12
#define BIT_COUNT 10
#define GPIO_DATA_MASK (0xFF << GPIO_FIRST_BIT)
#define GPIO_STB_MASK (1 << (GPIO_FIRST_BIT + 8))
#define GPIO_AKD_MASK (1 << (GPIO_FIRST_BIT + 9))

// RT PC mouse protocol constants
#define RT_MOUSE_DATA_REPORT 0x0b

// Using TinyUSB's hid_mouse_report_t definition from hid.h

uint32_t current_output = 0;

// Output a 4-byte RT PC mouse report on the GPIO bus
void send_mouse_report(const uint8_t report[4]) {
    for (int i = 0; i < 4; ++i) {
        current_output &= ~GPIO_DATA_MASK;
        current_output |= (report[i] << GPIO_FIRST_BIT);
        gpio_put_all(current_output);
        sleep_us(10);
        current_output |= GPIO_STB_MASK | GPIO_AKD_MASK;
        gpio_put_all(current_output);
        sleep_us(11);
        current_output &= ~GPIO_STB_MASK;
        gpio_put_all(current_output);
        sleep_ms(1);
    }
}

// Initialize GPIO pins for output
void init_mouse_port() {
    for (int i = GPIO_FIRST_BIT; i < GPIO_FIRST_BIT + BIT_COUNT; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_pull_down(i);
    }
}

// Translate TinyUSB mouse report to RT PC format and send
void process_mouse_report(const hid_mouse_report_t *report) {
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

    printf("Mouse report: %02x %02x %02x %02x\n", out[0], out[1], out[2], out[3]);
    send_mouse_report(out);
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
        process_mouse_report((const hid_mouse_report_t *)report);
    }
    // Request the next report
    tuh_hid_receive_report(dev_addr, instance);
}

int main(void) {
    stdio_init_all();
    board_init();
    tuh_init(BOARD_TUH_RHPORT);
    board_init_after_tusb();
    init_mouse_port();

    printf("pico-rt-mouse running\n");

    while (1) {
        tuh_task();
        // Add any additional timing or repeat logic if needed
    }
    return 0;
} 