#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void vga_putc(char c);
void vga_print(const char* str);
void vga_print_color(const char* str, uint8_t color);
void vga_clear(void);
void vga_put_at(char c, uint8_t color, uint16_t pos);
void vga_set_cursor(uint16_t pos);
void vga_move_cursor_back(void);
void vga_move_cursor_forward(void);
uint16_t vga_get_cursor(void);

extern unsigned char vga_color;

#endif