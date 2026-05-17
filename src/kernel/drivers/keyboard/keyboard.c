#include "keyboard.h"
#include "../../utils/ports.h"
#include "../../utils/string.h"
#include "../vga/vga.h"
#include "../../kernel.h"
#include "../vga/colors.h"

#include <stdbool.h>

#define HISTORY_SIZE 10
#define HISTORY_ENTRY_LEN 128
#define INPUT_BUFFER_SIZE 256


static char input_buffer[INPUT_BUFFER_SIZE];
static int input_len = 0;
static int cursor_pos = 0;

static bool extended_code = false;
static bool ctrl_pressed = false;
static bool shift_pressed = false;
static bool numlock_on = false;
static volatile bool sigint_received = false;

// --- КРУГОВОЙ БУФЕР ДЛЯ ПРЕРЫВАНИЙ ---
#define KBD_RING_BUFFER_SIZE 128
static char kbd_ring_buffer[KBD_RING_BUFFER_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;

// Записать символ в буфер (вызывается внутри прерывания)
static void kbd_ring_push(char c) {
    int next = (kbd_head + 1) % KBD_RING_BUFFER_SIZE;
    if (next != kbd_tail) { // Если буфер не переполнен
        kbd_ring_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

// Проверить, есть ли символы
int keyboard_has_key(void) {
    return kbd_head != kbd_tail;
}

// Забрать символ из буфера (вызывается в Си-коде шелла)
char keyboard_getc_from_buffer(void) {
    if (!keyboard_has_key()) return 0;
    char c = kbd_ring_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_RING_BUFFER_SIZE;
    return c;
}


void keyboard_history_add(const char* cmd) {
    if (!cmd || !cmd[0] || cmd[0] == '\n') return;
    if (history_count > 0 && strcmp(history[history_count-1], cmd) == 0)
        return;

    if (history_count == HISTORY_SIZE) {
        for (int i = 0; i < HISTORY_SIZE - 1; i++)
            strcpy(history[i], history[i+1]);
        history_count--;
    }

    strncpy(history[history_count], cmd, HISTORY_ENTRY_LEN-1);
    history[history_count][HISTORY_ENTRY_LEN-1] = '\0';
    history_count++;
    history_nav = history_count;
}

const char* keyboard_history_prev(void) {
    if (history_count == 0) return NULL;
    if (history_nav <= 0) history_nav = history_count;
    history_nav--;
    return history[history_nav];
}

const char* keyboard_history_next(void) {
    if (history_count == 0) return NULL;
    if (history_nav >= history_count - 1) {
        history_nav = history_count;
        return NULL;
    }
    history_nav++;
    return history[history_nav];
}

void keyboard_history_reset_nav(void) {
    history_nav = history_count;
}

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x29] = '`',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' '
};

static const char keymap_shift[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?'
};

static const char numpad_on[128] = {
    [0x47] = '7', [0x48] = '8', [0x49] = '9',
    [0x4B] = '4', [0x4C] = '5', [0x4D] = '6',
    [0x4F] = '1', [0x50] = '2', [0x51] = '3',
    [0x52] = '0', [0x53] = '.',
    [0x4E] = '+', [0x4A] = '-', [0x37] = '*', [0x35] = '/', [0x1C] = '\n'
};

// int keyboard_has_key(void) {
//     return (inb(KBD_STATUS) & 1) != 0;
// }


// Эту функцию мы вызываем ИЗ НАШЕГО ИСПРАВЛЕННОГО isr.c при IRQ_KEYBOARD
void keyboard_handler(void) {
    // Проверяем, есть ли данные в контроллере (на всякий случай)
    if ((inb(KBD_STATUS) & 1) == 0) return;

    unsigned char sc = inb(KBD_DATA);

    // Логика обработки расширенного кода 0xE0
    if (sc == 0xE0) {
        extended_code = true;
        return;
    }

    if (extended_code) {
        extended_code = false;
        if (sc & 0x80) {
            if ((sc & 0x7F) == 0x1D) ctrl_pressed = false;
            return;
        }
        if (sc == 0x1D) { ctrl_pressed = true; return; }

        // Твои стрелочки и спец-клавиши
        if (sc == 0x4B) kbd_ring_push(-1);
        if (sc == 0x4D) kbd_ring_push(-3);
        if (sc == 0x48) kbd_ring_push(-2);
        if (sc == 0x50) kbd_ring_push(-4);
        if (sc == 0x53) kbd_ring_push(-5);
        if (sc == 0x47) kbd_ring_push(-7);
        if (sc == 0x4F) kbd_ring_push(-8);
        return;
    }

    // Обычные скан-коды (отпускание)
    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36) shift_pressed = false;
        if (sc == 0x1D) ctrl_pressed = false;
        return;
    }

    // Нажатие модификаторов
    if (sc == 0x2A || sc == 0x36) { shift_pressed = true; return; }
    if (sc == 0x1D) { ctrl_pressed = true; return; }
    if (sc == 0x45) { numlock_on = !numlock_on; return; }

    if (numlock_on && numpad_on[sc]) {
        kbd_ring_push(numpad_on[sc]);
        return;
    }

    // Обычные символы
    if (sc < 128 && keymap[sc]) {
        char ch = shift_pressed ? keymap_shift[sc] : keymap[sc];
        if (ctrl_pressed) {
            if (ch >= 'a' && ch <= 'z') {
                if (ch == 'c') sigint_received = true;
                kbd_ring_push((char)(ch - 'a' + 1));
                return;
            }
            if (ch >= 'A' && ch <= 'Z') {
                if (ch == 'C') sigint_received = true;
                kbd_ring_push((char)(ch - 'A' + 1));
                return;
            }
        }
        kbd_ring_push(ch);
    }
}


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

        if (ext == 0x4B) return -1;
        if (ext == 0x4D) return -3;
        if (ext == 0x48) return -2;
        if (ext == 0x50) return -4;
        if (ext == 0x53) return -5;
        if (ext == 0x47) return -7;
        if (ext == 0x4F) return -8;

        return 0;
    }

    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36) shift_pressed = false;
        if (sc == 0x1D) ctrl_pressed = false;
        return 0;
    }

    if (sc == 0x2A || sc == 0x36) { shift_pressed = true; return 0; }
    if (sc == 0x1D) { ctrl_pressed = true; return 0; }

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
    if (sigint_received) {
        sigint_received = false;
        return 1;
    }
    return 0;
}

void keyboard_poll(void) {
    while (inb(KBD_STATUS) & 1) {
        unsigned char sc = inb(KBD_DATA);
        if (sc == 0xE0) {
            if (inb(KBD_STATUS) & 1) inb(KBD_DATA);
            continue;
        }
    }
}

static uint16_t input_start;
static int displayed_len = 0;

static void redraw_input_line(void) {
    vga_set_cursor(input_start);
    int to_print = (input_len > displayed_len) ? input_len : displayed_len;
    for (int i = 0; i < to_print; i++) {
        if (i < input_len) {
            vga_putc(input_buffer[i]);
        } else {
            vga_putc(' ');
        }
    }
    displayed_len = input_len;
    vga_set_cursor(input_start + cursor_pos);
}

static void clear_current_input(void) {
    input_len = 0;
    cursor_pos = 0;
    input_buffer[0] = '\0';
}

void keyboard_read_line(char* buffer, int max_len) {
    if (max_len <= 1) return;

        input_start = vga_get_cursor();
        displayed_len = 0;
        clear_current_input();
        keyboard_history_reset_nav();

        while (1) {
            // ЭНЕРГОЭФФЕКТИВНОЕ ОЖИДАНИЕ ПРЕРЫВАНИЯ
            while (!keyboard_has_key()) {
                __asm__ __volatile__("hlt"); // Спим, пока прерывание клавиатуры не положит символ в буфер!
            }

            // Забираем символ из буфера
            char c = keyboard_getc_from_buffer();
            if (c == 0) continue;

        if (c == '\n') {
            input_buffer[input_len] = '\0';
            strncpy(buffer, input_buffer, max_len);
            buffer[max_len - 1] = '\0';
            vga_putc('\n');
            keyboard_history_add(buffer);
            return;
        }

        if (c == '\x03') {
            vga_print_color("^C\n", LIGHT_RED);
            sigint_received = true;
            buffer[0] = '\0';
            return;
        }

        if (c == '\b') {
            if (cursor_pos > 0) {
                cursor_pos--;
                for (int i = cursor_pos; i < input_len - 1; i++) {
                    input_buffer[i] = input_buffer[i + 1];
                }
                input_len--;
                redraw_input_line();
            }
            continue;
        }

        if (c == -5) {
            if (cursor_pos < input_len) {
                for (int i = cursor_pos; i < input_len - 1; i++) {
                    input_buffer[i] = input_buffer[i + 1];
                }
                input_len--;
                redraw_input_line();
            }
            continue;
        }

        if (c < 0) {
            switch (c) {
                case -1:
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        vga_set_cursor(input_start + cursor_pos);
                    }
                    break;

                case -3:
                    if (cursor_pos < input_len) {
                        cursor_pos++;
                        vga_set_cursor(input_start + cursor_pos);
                    }
                    break;

                case -2:
                    {
                        const char* h = keyboard_history_prev();
                        if (h) {
                            strncpy(input_buffer, h, INPUT_BUFFER_SIZE - 1);
                            input_buffer[INPUT_BUFFER_SIZE - 1] = '\0';
                            input_len = strlen(input_buffer);
                            cursor_pos = input_len;
                            redraw_input_line();
                        }
                    }
                    break;

                case -4:
                    {
                        const char* h = keyboard_history_next();
                        if (h) {
                            strncpy(input_buffer, h, INPUT_BUFFER_SIZE - 1);
                            input_buffer[INPUT_BUFFER_SIZE - 1] = '\0';
                            input_len = strlen(input_buffer);
                            cursor_pos = input_len;
                        } else {
                            clear_current_input();
                        }
                        redraw_input_line();
                    }
                    break;

                case -7:
                    cursor_pos = 0;
                    vga_set_cursor(input_start + cursor_pos);
                    break;

                case -8:
                    cursor_pos = input_len;
                    vga_set_cursor(input_start + cursor_pos);
                    break;

                default:
                    break;
            }
            continue;
        }

        if ((c >= 32 && c <= 126) && input_len < max_len - 1) {
            for (int i = input_len; i > cursor_pos; i--) {
                input_buffer[i] = input_buffer[i - 1];
            }
            input_buffer[cursor_pos] = c;
            input_len++;
            cursor_pos++;
            redraw_input_line();
        }
    }
}
