#include "history.h"
#include "../../kernel/drivers/vga/vga.h"
#include "../../kernel/kernel.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/drivers/vga/colors.h"


char history[HISTORY_SIZE][HISTORY_LEN] = {0};
int history_count = 0;
int history_nav = -1;
void print(const char *str);


void print_history(void) {
    if (history_count == 0) {
        vga_print_color("History is empty\n", YELLOW);
        return;
    }
    char num[8];
    for (int i = 0; i < history_count; i++) {
        if (!history[i][0]) continue;
        itoa(i + 1, num, 10);
        vga_print_color(num, 0x08);
        vga_print_color("  ", 0x08);
        vga_print_color(history[i], 0x0F);
        vga_putc('\n');
    }
}
