#include "vga.h"

unsigned char vga_color = 0x07;
static uint16_t* const vga_buffer = (uint16_t*)0xB8000;
static uint16_t cursor_pos = 0;

void vga_put_at(char c, uint8_t color, uint16_t pos) {
    vga_buffer[pos] = ((uint16_t)color << 8) | c;
}

void vga_set_cursor(uint16_t pos) {
    cursor_pos = pos;
    uint8_t low = pos & 0xFF;
    uint8_t high = (pos >> 8) & 0xFF;

    __asm__ volatile ("outb %0, %1" : : "a"( (unsigned char)0x0F ), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"( low ), "Nd"(0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"( (unsigned char)0x0E ), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"( high ), "Nd"(0x3D5));
}

uint16_t vga_get_cursor(void) {
    return cursor_pos;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_pos += VGA_WIDTH - (cursor_pos % VGA_WIDTH);
    } else {
        vga_put_at(c, vga_color, cursor_pos++);
    }
    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(y-1)*VGA_WIDTH + x] = vga_buffer[y*VGA_WIDTH + x];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_put_at(' ', vga_color, (VGA_HEIGHT-1)*VGA_WIDTH + x);
        }
        cursor_pos -= VGA_WIDTH;
    }
    vga_set_cursor(cursor_pos);
}

void vga_print(const char* str) {
    for (int i = 0; str[i]; i++)
        vga_putc(str[i]);
}

void vga_print_color(const char* str, uint8_t color) {
    uint8_t old_color = vga_color;
    vga_color = color;
    vga_print(str);
    vga_color = old_color;
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH*VGA_HEIGHT; i++)
        vga_put_at(' ', vga_color, i);
    cursor_pos = 0;
    vga_set_cursor(cursor_pos);
}

void vga_move_cursor_back(void) {
    uint16_t pos = vga_get_cursor();
    if (pos > 0) {
        vga_set_cursor(pos - 1);
    }
}

void vga_move_cursor_forward(void) {
    uint16_t pos = vga_get_cursor();
    vga_set_cursor(pos + 1);
}