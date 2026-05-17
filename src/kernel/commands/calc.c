#include "../drivers/vga/vga.h"
#include "../utils/string.h"
#include "../drivers/vga/colors.h"


int parse_expr(const char* s, int* a, char* op, int* b) {
    int i = 0;
    while (s[i] == ' ') i++;
    int sign = 1;
    if (s[i] == '-') { sign = -1; i++; }
    *a = 0;
    while (s[i] >= '0' && s[i] <= '9') { *a = *a * 10 + (s[i++] - '0'); }
    *a *= sign;
    while (s[i] == ' ') i++;
    *op = s[i++];
    while (s[i] == ' ') i++;
    sign = 1;
    if (s[i] == '-') { sign = -1; i++; }
    *b = 0;
    while (s[i] >= '0' && s[i] <= '9') { *b = *b * 10 + (s[i++] - '0'); }
    *b *= sign;
    return 1;
}

void cmd_calc(const char* expr) {
    if (!expr || !expr[0]) {
        vga_print_color("Usage: calc 2+2\n", LIGHT_RED);
        return;
    }
    int a, b;
    char op;
    parse_expr(expr, &a, &op, &b);
    int res = 0;
    switch (op) {
        case '+': res = a + b; break;
        case '-': res = a - b; break;
        case '*': res = a * b; break;
        case '/':
            if (b == 0) {
                vga_print_color("Division by zero\n", LIGHT_RED);
                return;
            }
            res = a / b;
            break;
        default:
            vga_print_color("Invalid operator\n", LIGHT_RED);
            return;
    }
    char buf[32];
    itoa(res, buf, 10);
    vga_print_color("Result: ", YELLOW);
    vga_print_color(buf, LIGHT_GREEN);
    vga_putc('\n');
}
