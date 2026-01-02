#include "keyboard.h"
#include "ports.h"
#include "string.h"
#include "vga.h"
#include <stdbool.h>

#define HISTORY_SIZE 16
#define HISTORY_ENTRY_LEN 128

static char history[HISTORY_SIZE][HISTORY_ENTRY_LEN];
static int history_count = 0;
static int history_nav = 0;
void keyboard_history_add(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;
    if (history_count > 0 && strcmp(history[(history_count-1) % HISTORY_SIZE], cmd) == 0) return;
    if (history_count < HISTORY_SIZE) {
        strncpy(history[history_count], cmd, HISTORY_ENTRY_LEN-1);
        history[history_count][HISTORY_ENTRY_LEN-1] = '\0';
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_SIZE-1; i++) strcpy(history[i], history[i+1]);
        strncpy(history[HISTORY_SIZE-1], cmd, HISTORY_ENTRY_LEN-1);
        history[HISTORY_SIZE-1][HISTORY_ENTRY_LEN-1] = '\0';
    }
}

const char* keyboard_history_prev(void) {
    if (history_count == 0) return NULL;
    if (history_nav == 0) history_nav = history_count - 1;
    else if (history_nav > 0) history_nav--;
    return history[history_nav];
}

const char* keyboard_history_next(void) {
    if (history_count == 0) return NULL;
    if (history_nav >= (history_count - 1)) { history_nav = history_count; return NULL; }
    history_nav++;
    return history[history_nav];
}

void keyboard_history_reset_nav(void) {
    history_nav = history_count;
}

static bool ctrl_pressed = false;
static bool shift_pressed = false;
static bool numlock_on = false;
static volatile bool sigint_received = false;

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' '
};

static const char keymap_shift[128] = {
/* 0x00 */ 0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
/* 0x10 */ 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
/* 0x20 */ 'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
/* 0x30 */ 'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
/* 0x40 */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x50 */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x60 */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x70 */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

static const char numpad_on[128] = {
    [0x47] = '7', [0x48] = '8', [0x49] = '9',
    [0x4B] = '4', [0x4C] = '5', [0x4D] = '6',
    [0x4F] = '1', [0x50] = '2', [0x51] = '3',
    [0x52] = '0', [0x53] = '.',
    [0x4E] = '+', [0x4A] = '-', [0x37] = '*', [0x35] = '/', [0x1C] = '\n'
};

char keyboard_read_char(void) {
    while ((inb(KBD_STATUS) & 1) == 0);
    unsigned char sc = inb(KBD_DATA);

    if (sc == 0xE0) {
        while ((inb(KBD_STATUS) & 1) == 0);
        unsigned char ext = inb(KBD_DATA);

        if (ext & 0x80) {
            if ((ext & 0x7F) == 0x1D) ctrl_pressed = false;
            return 0;
        }

        if (ext == 0x1D) { ctrl_pressed = true; return 0; }

            if (ext == 0x4B) return -1;  // Left
        if (ext == 0x4D) return -3;  // Right
        if (ext == 0x48) return -2;  // Up (history)
        if (ext == 0x50) return -4;  // Down (history)
        if (ext == 0x53) return -5;  // Delete
        if (ext == 0x52) return -6;  // Insert
        if (ext == 0x47) return -7;  // Home
        if (ext == 0x4F) return -8;  // End

        return 0;
    }

    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36) shift_pressed = false;
        if (sc == 0x1D) ctrl_pressed = false; 
        return 0;
    }

    if (sc == 0x2A || sc == 0x36) {
        shift_pressed = true;
        return 0;
    }

    if (sc == 0x1D) {
        ctrl_pressed = true;
        return 0;
    }

    if (sc == 0x45) {
        numlock_on = !numlock_on;
        return 0;
    }

    if (numlock_on && numpad_on[sc]) return numpad_on[sc];

    if (sc < 128 && keymap[sc]) {
        char ch = shift_pressed ? keymap_shift[sc] : keymap[sc];
        if (ctrl_pressed) {
            if (ch >= 'a' && ch <= 'z') {
                if (ch == 'c') sigint_received = true;
                return (char)(ch - 'a' + 1);
            }
            if (ch >= 'A' && ch <= 'Z') {
                if (ch == 'C') sigint_received = true;
                return (char)(ch - 'A' + 1);
            }
        }
        return ch;
    }

    return 0;
}

int keyboard_sigint_check(void) {
    if (sigint_received) { sigint_received = false; return 1; }
    return 0;
}

void keyboard_poll(void) {
    while (inb(KBD_STATUS) & 1) {
        unsigned char sc = inb(KBD_DATA);
        if (sc == 0xE0) {
            if (!(inb(KBD_STATUS) & 1)) break;
            unsigned char ext = inb(KBD_DATA);
            if (ext & 0x80) { if ((ext & 0x7F) == 0x1D) ctrl_pressed = false; continue; }
            if (ext == 0x1D) { ctrl_pressed = true; continue; }
            continue;
        }
        if (sc & 0x80) {
            sc &= 0x7F;
            if (sc == 0x2A || sc == 0x36) shift_pressed = false;
            if (sc == 0x1D) ctrl_pressed = false;
            continue;
        }
        if (sc == 0x2A || sc == 0x36) { shift_pressed = true; continue; }
        if (sc == 0x1D) { ctrl_pressed = true; continue; }

        if (numlock_on && numpad_on[sc]) continue;
        if (sc < 128 && keymap[sc]) {
            char ch = shift_pressed ? keymap_shift[sc] : keymap[sc];
            if (ctrl_pressed && (ch == 'c' || ch == 'C')) { sigint_received = true; }
        }
    }
}

void vga_clear_line_from_cursor() {
    uint16_t cur = vga_get_cursor();
    int x = cur % VGA_WIDTH;
    int y = cur / VGA_WIDTH;
    for (int i = x; i < VGA_WIDTH; i++)
        vga_put_at(' ', 0x07, y * VGA_WIDTH + i);
}

void keyboard_read_line(char* buffer, int max_len) {
    int len = 0;
    int pos = 0;
    buffer[0] = '\0';

    keyboard_history_reset_nav();

    while (1) {
        char c = keyboard_read_char();

        if (c == '\n') {
            buffer[len] = '\0';
            vga_putc('\n');
            break;
        } else if (c == '\x03') {
            buffer[0] = '\0';
            vga_print_color("^C\n", 0x0C);
            sigint_received = true;
            break;
        } else if (c == '\b') {
                    if (pos > 0) {
                        pos--;
                        len--;
                        for (int i = pos; i < len; i++) {
                            buffer[i] = buffer[i + 1];
                        }
                        buffer[len] = '\0';

                        uint16_t cur = vga_get_cursor();
                        vga_set_cursor(cur - 1);
                        vga_clear_line_from_cursor();
                        for (int i = pos; i < len; i++) {
                            vga_putc(buffer[i]);
                        }
                        vga_putc(' ');
                        vga_set_cursor(cur - 1 - (len - pos));
                    }
        } else if (c < 0) {
            uint16_t cur = vga_get_cursor();
            if (c == -1 && pos > 0) {
                pos--;
                vga_set_cursor(cur - 1);
            } else if (c == -3 && pos < len) {
                pos++;
                vga_set_cursor(cur + 1);
            }
        } else if (c != 0 && len < max_len - 1) {
            for (int i = len; i > pos; i--) buffer[i] = buffer[i - 1];
            buffer[pos] = c;
            len++; pos++;

            uint16_t cur = vga_get_cursor();
            vga_putc(c);
            for (int i = pos; i < len; i++) vga_putc(buffer[i]);
            vga_putc(' ');
            vga_set_cursor(cur + 1);
        } else if (c < 0) {
            uint16_t cur = vga_get_cursor();
            uint16_t line_start = cur - pos;

            if (c == -2) {
                const char* h = keyboard_history_prev();
                uint16_t cur2 = vga_get_cursor();
                uint16_t line_start2 = cur2 - pos;
                vga_set_cursor(line_start2);
                for (int i = 0; i < len; i++) vga_putc(' ');
                vga_set_cursor(line_start2);
                if (h) {
                    int hlen = 0; while (h[hlen]) hlen++;
                    if (hlen >= max_len) hlen = max_len - 1;
                    for (int i = 0; i < hlen; i++) { buffer[i] = h[i]; vga_putc(buffer[i]); }
                    len = hlen; pos = len;
                    buffer[len] = '\0';
                } else {
                    len = 0; pos = 0; buffer[0] = '\0';
                }
            }
            else if (c == -4) {
                const char* h = keyboard_history_next();
                uint16_t cur2 = vga_get_cursor();
                uint16_t line_start2 = cur2 - pos;
                vga_set_cursor(line_start2);
                for (int i = 0; i < len; i++) vga_putc(' ');
                vga_set_cursor(line_start2);
                if (h) {
                    int hlen = 0; while (h[hlen]) hlen++;
                    if (hlen >= max_len) hlen = max_len - 1;
                    for (int i = 0; i < hlen; i++) { buffer[i] = h[i]; vga_putc(buffer[i]); }
                    len = hlen; pos = len;
                    buffer[len] = '\0';
                } else {
                    len = 0; pos = 0; buffer[0] = '\0';
                }
            }
            else if (c == -1 && pos > 0) { pos--; vga_set_cursor(cur - 1); }
            else if (c == -3 && pos < len) { pos++; vga_set_cursor(cur + 1); }
            else if (c == -5 && pos < len) {
                len--;
                for (int i = pos; i < len; i++) buffer[i] = buffer[i + 1];
                buffer[len] = '\0';
                vga_clear_line_from_cursor();
                for (int i = pos; i < len; i++) vga_putc(buffer[i]);
                vga_putc(' ');
                vga_set_cursor(cur);
            }
            else if (c == -7) {
                vga_set_cursor(line_start);
                pos = 0;
            }
            else if (c == -8) {
                vga_set_cursor(line_start + len);
                pos = len;
            }
        }
    }
}

void keyboard_init() {
}