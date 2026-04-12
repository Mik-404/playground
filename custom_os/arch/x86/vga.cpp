#include "arch/x86/vga.h"
#include "mm/paging.h"

static const uint64_t VGA_WIDTH = 80;
static const uint64_t VGA_HEIGHT = 25;

static uint64_t vga_row;
static uint64_t vga_column;
static volatile uint16_t* vga_buffer;

extern "C" {

inline uint64_t vga_index(uint64_t row, uint64_t col) {
    return row * VGA_WIDTH + col;
}

NO_KASAN void vga_init(void) {
    vga_row = 0;
    vga_column = 0;
    uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_buffer = (volatile uint16_t*)PHYS_TO_VIRT((uint16_t*)0xb8000);
    for (uint64_t row = 0; row < VGA_HEIGHT; row++) {
        for (uint64_t col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[vga_index(row, col)] = vga_entry(' ', color);
        }
    }
}

NO_KASAN void vga_putentryat(char c, uint8_t color, uint64_t row, uint64_t col) {
    vga_buffer[vga_index(row, col)] = vga_entry(c, color);
}

NO_KASAN static void vga_newline_and_return() {
    vga_row++;
    if (vga_row == VGA_HEIGHT) {
        for (uint64_t row = 0; row < VGA_HEIGHT - 1; row++) {
            for (uint64_t col = 0; col < VGA_WIDTH; col++) {
                vga_buffer[vga_index(row, col)] = vga_buffer[vga_index(row + 1, col)];
            }
        }
        for (uint64_t col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[vga_index(VGA_HEIGHT - 1, col)] = 0;
        }
        vga_row = VGA_HEIGHT - 1;
    }
    vga_column = 0;
}

void vga_putchar_color(char c, uint8_t color) {
    switch (c) {
    case '\n':
        vga_newline_and_return();
        break;

    case '\r':
        vga_column = 0;
        break;

    default:
        vga_putentryat(c, color, vga_row, vga_column);
        vga_column++;
        if (vga_column == VGA_WIDTH) {
            vga_newline_and_return();
        }
    }
}

void vga_write(const char* data, size_t size, uint8_t color) {
    for (uint64_t i = 0; i < size; i++) {
        vga_putchar_color(data[i], color);
    }
}

void vga_writestring(const char* data) {
    vga_write(data, strlen(data), vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void vga_writestring_color(const char* data, uint8_t color) {
    vga_write(data, strlen(data), color);
}

}
