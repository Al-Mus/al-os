#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KBD_STATUS 0x64
#define KBD_DATA   0x60

char keyboard_read_char(void);
void keyboard_init(void);
void keyboard_read_line(char* buffer, int max_len);
int keyboard_sigint_check(void);
void keyboard_poll(void);

void keyboard_history_add(const char* cmd);
const char* keyboard_history_prev(void);
const char* keyboard_history_next(void);
void keyboard_history_reset_nav(void);

#endif