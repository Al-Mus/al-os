#include <stddef.h>
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "ports.h"
#include "string.h"

#define MAX_CMD_LEN 256
char user[32] = "root";

char* strchr(const char* s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return NULL;
}

char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (*s == (char)c) last = s;
    return (char*)last;
}

void itoa(int value, char* str, int base) {
    char* ptr = str, *ptr1 = str;
    if (base < 2 || base > 36) { *str = '\0'; return; }
    if (value == 0) { *ptr++ = '0'; *ptr = '\0'; return; }
    int sign = 0;
    if (value < 0 && base == 10) { sign = 1; value = -value; }
    while (value != 0) {
        int digit = value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= base;
    }
    if (sign) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) { char t = *ptr; *ptr-- = *ptr1; *ptr1++ = t; }
}

/* --- Parse expr (простой, слева направо) --- */
static int parse_expr(const char* s, int* a, char* op, int* b) {
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

/* --- Beep --- */
static void beep_pit(unsigned int frequency, unsigned int duration_ms) {
    if (frequency == 0) return;
    unsigned int div = 1193180 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, (div >> 8) & 0xFF);
    unsigned char tmp = inb(0x61);
    outb(0x61, tmp | 3);
    for (volatile unsigned int i = 0; i < duration_ms * 10000; i++);
    outb(0x61, tmp & 0xFC);
}

/* --- Slow print (Brokem) --- */
static void slowprint_line(const char* str, uint8_t color, unsigned int delay) {
    uint8_t old = vga_color;
    vga_color = color;
    for (int i = 0; str[i]; i++) {
        vga_putc(str[i]);
        for (volatile unsigned int j = 0; j < delay; j++) {
            if ((j & 0xFFF) == 0) {
                keyboard_poll();
                if (keyboard_sigint_check()) {
                    vga_print_color("\nOperation cancelled\n", 0x0C);
                    vga_color = old;
                    return;
                }
            }
        }
    }
    vga_putc('\n');
    vga_color = old;
}

/* --- Prompt --- */
static void show_prompt(void) {
    vga_print_color(user, 0x0A);
    vga_print_color("@al-os", 0x0B);
    vga_print_color(current_path, 0x0F);
    vga_print_color("> ", 0x07);
}

/* --- Commands --- */
static const struct { const char* cmd; const char* desc; } help_table[] = {
    {"help", "Show this help or help <cmd>"},
    {"clear", "Clear screen"},
    {"ls", "List in-memory FS"},
    {"cd", "Change directory"},
    {"pwd", "Show current path"},
    {"mkdir", "Create directory"},
    {"rm", "Remove file/dir"},
    {"touch", "Create file in memory FS"},
    {"write", "Write to in-memory file"},
    {"cat", "Show in-memory file"},
    {"echo", "Echo or redirect"},
    {"cp", "Copy file"},
    {"mv", "Move/Rename file"},
    {"tree", "Show tree"},
    {"calc", "Simple calculator"},
    {"chusr", "Change user"},
    {"beep", "Play beep"},
    {"sysinfo", "Show system info"},
    {"slowfetch", "Animated banner"},
    {"aarch", "Show architecture info"},
    {"uptime", "Show uptime"},
    {"meminfo", "Kernel memory info"},
    {"time", "Show RTC time"},
    {"reboot", "Reboot machine"},
    {"shutdown", "Shutdown machine"}
};

static void cmd_help(const char* arg) {
    int cmd_count = (int)(sizeof(help_table)/sizeof(help_table[0]));

    if (arg && arg[0]) {
        for (int i = 0; i < cmd_count; i++) {
            if (strcmp(help_table[i].cmd, arg) == 0) {
                vga_print_color(help_table[i].cmd, 0x0E); vga_print_color(" - ", 0x0F); vga_print_color(help_table[i].desc, 0x0F); vga_putc('\n');
                return;
            }
        }
        vga_print_color("No help for that command\n", 0x0C);
        return;
    }

    vga_print_color("Available commands:\n", 0x0E);

    const char* names[64];
    for (int i = 0; i < cmd_count && i < (int)(sizeof(names)/sizeof(names[0])); i++) names[i] = help_table[i].cmd;

    for (int i = 0; i < cmd_count - 1; i++) {
        int min = i;
        for (int j = i + 1; j < cmd_count; j++) if (strcmp(names[j], names[min]) < 0) min = j;
        if (min != i) {
            const char* t = names[i]; names[i] = names[min]; names[min] = t;
        }
    }

    int maxlen = 0;
    for (int i = 0; i < cmd_count; i++) { int l = 0; while (names[i][l]) l++; if (l > maxlen) maxlen = l; }
    int cols = 4;
    int colw = maxlen + 4; if (colw > 28) colw = 28;

    for (int i = 0; i < cmd_count; i++) {
        char buf[40]; int l = 0;
        while (names[i][l] && l < 30) { buf[l] = names[i][l]; l++; }
        buf[l] = '\0';
        int p;
        for (p = l; p < colw && p < (int)sizeof(buf) - 1; p++) buf[p] = ' ';
        buf[p] = '\0';
        vga_print_color(buf, 0x0A);
        if ((i % cols) == cols - 1) vga_putc('\n');
    }
    if (cmd_count % cols) vga_putc('\n');
}

static void cmd_sysinfo(void) {
    vga_print_color("=== AL-OS ===\n", 0x0D);
    vga_print_color("Arch: i686\nBuild: v0.3", 0x0F);
}

static int bcd2bin(int v) { return (v & 0x0F) + ((v >> 4) * 10); }

typedef struct { int sec, min, hour, day, month, year; } rtc_time;

static void rtc_read(rtc_time* t) {
    unsigned char status;
    do { outb(0x70, 0x0A); status = inb(0x71); } while (status & 0x80);
    outb(0x70, 0x00); unsigned char sec = inb(0x71);
    outb(0x70, 0x02); unsigned char min = inb(0x71);
    outb(0x70, 0x04); unsigned char hour = inb(0x71);
    outb(0x70, 0x07); unsigned char day = inb(0x71);
    outb(0x70, 0x08); unsigned char month = inb(0x71);
    outb(0x70, 0x09); unsigned char year = inb(0x71);
    outb(0x70, 0x0B); unsigned char statusB = inb(0x71);

    if (!(statusB & 0x04)) {
        t->sec = bcd2bin(sec);
        t->min = bcd2bin(min);
        t->hour = bcd2bin(hour);
        t->day = bcd2bin(day);
        t->month = bcd2bin(month);
        t->year = bcd2bin(year) + 2000;
    } else {
        t->sec = sec; t->min = min; t->hour = hour; t->day = day; t->month = month; t->year = 2000 + year;
    }
}

static long days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y-399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1;
    unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
    return era * 146097 + (long)doe - 719468;
}

static long time_to_seconds(const rtc_time* t) {
    long days = days_from_civil(t->year, t->month, t->day);
    return days * 86400 + t->hour * 3600 + t->min * 60 + t->sec;
}

static long boot_seconds = 0;


static void cmd_slowfetch(void) {
    slowprint_line(" __________ ", 0x0B, 80000);
    slowprint_line(" ", 0x0B, 80000);
    slowprint_line("    Soon    ", 0x0B, 80000);
    slowprint_line(" ", 0x0B, 80000);
    slowprint_line(" __________ ", 0x07, 50000);
    cmd_sysinfo();
}

static void cmd_calc(const char* expr) {
    if (!expr || !expr[0]) {
        vga_print_color("Usage: calc 2+2\n", 0x0C);
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
                vga_print_color("Division by zero\n", 0x0C);
                return;
            }
            res = a / b;
            break;
        default:
            vga_print_color("Invalid operator\n", 0x0C);
            return;
    }
    char buf[32];
    itoa(res, buf, 10);
    vga_print_color("Result: ", 0x0E);
    vga_print_color(buf, 0x0A);
    vga_putc('\n');
}

static void cmd_echo(const char* args) {
    if (!args || !args[0]) {
        vga_putc('\n');
        return;
    }

    char* append_ptr = strstr(args, ">>");
    char* overwrite_ptr = strstr(args, ">");
    char* redirect = NULL;
    int append = 0;

    if (append_ptr) { redirect = append_ptr; append = 1; }
    else if (overwrite_ptr && (overwrite_ptr[1] != '>' || !overwrite_ptr[1])) {
        redirect = overwrite_ptr;
        append = 0;
    }

    if (redirect) {
        *redirect = '\0';
        char* filename = redirect + (append ? 2 : 1);
        while (*filename == ' ') filename++;

        char text[512];
        strncpy(text, args, 511);
        text[511] = '\0';

        fs_node* node = resolve_path(filename, fs_current);
        if (append && node && node->type == FS_FILE) {
            strcat(node->content, text);
        } else {
            fs_touch(filename);
            node = resolve_path(filename, fs_current);
            if (node && node->type == FS_FILE) {
                strcpy(node->content, text);
            }
        }
    } else {
        vga_print_color(args, 0x0F);
        vga_putc('\n');
    }
}

static void cmd_cp(const char* args) {
    if (!args) { vga_print_color("Usage: cp <src> <dest>\n", 0x0C); return; }
    char* dest = strchr(args, ' ');
    if (!dest) { vga_print_color("Usage: cp <src> <dest>\n", 0x0C); return; }
    *dest = '\0'; dest++;
    while (*dest == ' ') dest++;

    fs_node* src = resolve_path(args, fs_current);
    if (!src || src->type != FS_FILE) { vga_print_color("Source not file\n", 0x0C); return; }
    fs_touch(dest);
    fs_write(dest, src->content);
}

static void cmd_mv(const char* args) {
    if (!args) { vga_print_color("Usage: mv <src> <dest>\n", 0x0C); return; }
    char* dest = strchr(args, ' ');
    if (!dest) { vga_print_color("Usage: mv <src> <dest>\n", 0x0C); return; }
    *dest = '\0'; dest++;
    while (*dest == ' ') dest++;

    fs_node* src = resolve_path(args, fs_current);
    if (!src) { vga_print_color("Source not found\n", 0x0C); return; }
    const char* new_name = dest;
    char* slash = strrchr(dest, '/');
    if (slash) new_name = slash + 1;
    strcpy(src->name, new_name);
}

static void cmd_tree(fs_node* node, int depth) {
    if (!node) node = fs_current;
    for (int i = 0; i < node->child_count; i++) {
        for (int d = 0; d < depth; d++) vga_print("  ");
        vga_print_color(node->children[i]->name, node->children[i]->type == FS_DIR ? 0x09 : 0x0F);
        if (node->children[i]->type == FS_DIR) vga_print_color("/", 0x09);
        vga_putc('\n');
        if (node->children[i]->type == FS_DIR) cmd_tree(node->children[i], depth + 1);
    }
}

/* --- Kernel main --- */
void kernel_main(void) {
    vga_clear();
    vga_print_color("Welcome to AL-OS!\nType 'help' for commands.\n\n", 0x0A);

    fs_init();
    fs_cd("/home");

    int __cmdcnt = (int)(sizeof(help_table) / sizeof(help_table[0]));
    for (int __i = 0; __i < __cmdcnt; __i++) {
        char __path[64]; __path[0] = '\0';
        strcpy(__path, "/bin/");
        int k = 0; while (help_table[__i].cmd[k] && k < 31) { __path[5 + k] = help_table[__i].cmd[k]; k++; }
        __path[5 + k] = '\0';
        if (fs_touch(__path) != 0) break;
    }

    rtc_time now; rtc_read(&now); boot_seconds = time_to_seconds(&now);

    char cmd[MAX_CMD_LEN];
    while (1) {
        show_prompt();
        keyboard_read_line(cmd, MAX_CMD_LEN);
        if (cmd[0] == 0) continue;
        keyboard_history_add(cmd);

        char* space = strchr(cmd, ' ');
        char* args = "";
        if (space) { *space = 0; args = space + 1; }

        if (strcmp(cmd, "help") == 0) cmd_help(args);
        else if (strcmp(cmd, "clear") == 0) vga_clear();
        else if (strcmp(cmd, "ls") == 0) fs_list(args[0] ? args : NULL);
        else if (strcmp(cmd, "cd") == 0) { if (args[0]) fs_cd(args); }
        else if (strcmp(cmd, "pwd") == 0) fs_pwd();
        else if (strcmp(cmd, "mkdir") == 0) { if (args[0]) fs_mkdir(args); }
        else if (strcmp(cmd, "rm") == 0) { if (args[0]) fs_rm(args); }
        else if (strcmp(cmd, "touch") == 0) { if (args[0]) fs_touch(args); }
        else if (strcmp(cmd, "write") == 0) {
            char* text = strchr(args, ' ');
            if (text) { *text = 0; text++; fs_write(args, text); }
        }
        else if (strcmp(cmd, "cat") == 0) { if (args[0]) fs_cat(args); }
        else if (strcmp(cmd, "echo") == 0) cmd_echo(args);
        else if (strcmp(cmd, "cp") == 0) cmd_cp(args);
        else if (strcmp(cmd, "mv") == 0) cmd_mv(args);
        else if (strcmp(cmd, "tree") == 0) cmd_tree(NULL, 0);
        else if (strcmp(cmd, "calc") == 0) cmd_calc(args);
        else if (strcmp(cmd, "chusr") == 0) { if (args[0]) strncpy(user, args, 31); user[31] = 0; }
        else if (strcmp(cmd, "beep") == 0) {
            int f = 1000, ms = 300;
            if (args[0]) {
                f = 2000; ms = 500;
            }
            beep_pit(f, ms);
        }
        else if (strcmp(cmd, "sysinfo") == 0) cmd_sysinfo();
        else if (strcmp(cmd, "slowfetch") == 0) cmd_slowfetch();
        else if (strcmp(cmd, "uptime") == 0) {
            rtc_time now; rtc_read(&now);
            long now_s = time_to_seconds(&now);
            long diff = now_s - boot_seconds;
            if (diff < 0) diff = 0;
            int days = diff / 86400; diff %= 86400;
            int hours = diff / 3600; diff %= 3600;
            int mins = diff / 60; int secs = diff % 60;
            char buf[64];
            itoa(days, buf, 10); vga_print_color(buf, 0x0E); vga_print_color(" days ", 0x0F);
            itoa(hours, buf, 10); vga_print_color(buf, 0x0E); vga_print_color(":", 0x0F);
            itoa(mins, buf, 10); vga_print_color(buf, 0x0E); vga_print_color(":", 0x0F);
            itoa(secs, buf, 10); vga_print_color(buf, 0x0E); vga_putc('\n');
        }
        else if (strcmp(cmd, "meminfo") == 0) {
            extern char _start; extern char end;
            char buf[32];
            itoa((int)&_start, buf, 16); vga_print_color("kernel start: 0x", 0x0E); vga_print_color(buf, 0x0F); vga_putc('\n');
            itoa((int)&end, buf, 16); vga_print_color("kernel end: 0x", 0x0E); vga_print_color(buf, 0x0F); vga_putc('\n');
            int ksize = (int)&end - (int)&_start; itoa(ksize, buf, 10); vga_print_color("size: ", 0x0E); vga_print_color(buf, 0x0F); vga_putc('\n');
        }
        else if (strcmp(cmd, "time") == 0) {
            rtc_time now; rtc_read(&now);
            char buf[32];
            itoa(now.hour, buf, 10); vga_print_color(buf, 0x0E); vga_print_color(":", 0x0F);
            itoa(now.min, buf, 10); vga_print_color(buf, 0x0E); vga_print_color(":", 0x0F);
            itoa(now.sec, buf, 10); vga_print_color(buf, 0x0E); vga_putc('\n');
        }
        else if (strcmp(cmd, "aarch") == 0) {
            vga_print_color("Architecture: i686\n", 0x0E);
            vga_print_color("Mode: 32-bit\n", 0x0F);
            vga_print_color("Endianness: little\n", 0x0F);
        }
        else if (strcmp(cmd, "reboot") == 0) {
            vga_print_color("Rebooting...", 0x0C);
            for (volatile int i = 0; i < 100000000; i++) {
            }
            asm volatile ("ljmp $0, $0" ::: "memory");
            for(;;) asm("hlt");
        }
        else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "poweroff") == 0) {
            vga_print_color("SHUTTING DOWN...", 0x0C);
            for (volatile int i = 0; i < 100000000; i++);
            outw(0x604, 0x2000);
            outw(0xB004, 0x2000);
            outb(0x64, 0xFE);
            for(;;) asm("hlt");
        }
        else vga_print_color("Command not found\n", 0x0C);
    }
}