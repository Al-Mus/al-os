#ifndef KERNEL_H
#define KERNEL_H

#include "../apps/shell/history.h"

// === Глобальные переменные ===
extern int history_count;
extern int history_nav;
extern char history[HISTORY_SIZE][HISTORY_LEN];
extern char user[32];
extern long boot_seconds;

// === Функции ===
void keyboard_history_add(const char* cmd);

#endif
