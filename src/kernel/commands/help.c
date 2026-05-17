#include "../utils/string.h"
#include "../drivers/vga/vga.h"
#include "../kernel.h"
#include "all_commands.h"
#include "../drivers/vga/colors.h"



const struct { const char* cmd; const char* desc; } help_table[] = {
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
    {"shutdown", "Shutdown machine"},
    {"whoami",   "Display current user name"},
    {"date",     "Show current date and time"},
    {"colorbar", "Display VGA color palette"},
    {"memtest",  "Simple memory write/read test"},
    {"nano", "Simple text editor"},
    {"panic", "Trigger kernel panic"},
    {"fm", "Launch file manager"},
    {"screensaver", "Launch screensaver"},
    {"mount", "Mount FAT disk (mount 0)"},
    {"umount", "Unmount FAT disk"},
    {"fatls", "List FAT directory"},
    {"fatcd", "Change FAT directory"},
    {"fatpwd", "Show FAT current path"},
    {"fatcat", "Show FAT file contents"},
    {"fatmkdir", "Create FAT directory"},
    {"fatrm", "Remove FAT file/dir"},
    {"fattouch", "Create FAT file"},
    {"fatwrite", "Write to FAT file"},
    {"fatinfo", "Show FAT info"},
    {"disks", "Show detected disks"},
    {"fat", "Enter FAT shell mode"},
};


void cmd_help(const char* arg) {
    int cmd_count = (int)(sizeof(help_table)/sizeof(help_table[0]));

    if (arg && arg[0]) {
        for (int i = 0; i < cmd_count; i++) {
            if (strcmp(help_table[i].cmd, arg) == 0) {
                vga_print_color(help_table[i].cmd, YELLOW); vga_print_color(" - ", 0x0F); vga_print_color(help_table[i].desc, 0x0F); vga_putc('\n');
                return;
            }
        }
        vga_print_color("No help for that command\n", LIGHT_RED);
        return;
    }

    vga_print_color("Available commands:\n", YELLOW);

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
