#include "execute_commands.h"
#include "../../apps/nano/nano.h"
#include "../../apps/fm/fm.h"
#include "../../apps/screensaver/screensaver.h"
#include "../sys/panic.h"
#include "../drivers/net/rtl8139.h"
#include "../drivers/vga/vga.h"
#include "../drivers/pci/pci.h"
#include "../fs/memory_fs/fs.h"
#include "../../apps/shell/fat_shell.h"
#include "../utils/string.h"
#include "../fs/fat/fat.h"
#include "../kernel.h"
#include "../drivers/speaker/beep.h"
#include "../sys/power/power.h"
#include "../drivers/time/time.h"
#include "../drivers/vga/colors.h"
#include <stddef.h>

#include "all_commands.h"

typedef int (*cmd_handler_t)(char* args);

typedef struct {
    const char* name;
    cmd_handler_t handler;
} command_t;

/* ========================================================================== */
/* Статическая обёртка для сложных или инлайн команд                         */
/* ========================================================================== */

static int execute_cmd_help(char* args)      { cmd_help(args); return 0; }
static int execute_cmd_clear(char* args)     { (void)args; vga_clear(); return 0; }
static int execute_cmd_echo(char* args)      { cmd_echo(args); return 0; }
static int execute_cmd_calc(char* args)      { cmd_calc(args); return 0; }
static int execute_cmd_sysinfo(char* args)   { (void)args; cmd_sysinfo(); return 0; }
static int execute_cmd_slowfetch(char* args) { (void)args; cmd_slowfetch(); return 0; }
static int execute_cmd_uptime(char* args)    { (void)args; uptime_cmd(); return 0; }
static int execute_cmd_meminfo(char* args)   { (void)args; meminfo_cmd(); return 0; }
static int execute_cmd_time(char* args)      { (void)args; time_cmd(); return 0; }
static int execute_cmd_aarch(char* args)     { (void)args; cmd_aarch(); return 0; }
static int execute_cmd_reboot(char* args)    { (void)args; do_reboot(); return 0; }
static int execute_cmd_poweroff(char* args)  { (void)args; do_poweroff(); return 0; }
static int execute_cmd_whoami(char* args)    { (void)args; cmd_whoami(); return 0; }
static int execute_cmd_date(char* args)      { cmd_date(args); return 0; }
static int execute_cmd_colorbar(char* args)  { (void)args; cmd_colorbar(); return 0; }
static int execute_cmd_memtest(char* args)   { (void)args; cmd_memtest(); return 0; }
static int execute_cmd_history(char* args)   { (void)args; cmd_history(); return 0; }
static int execute_cmd_mkrootfs(char* args)  { cmd_mkrootfs(args); return 0; }
static int execute_cmd_crash(char* args)     { (void)args; cmd_crash(); return 0; }

static int execute_cmd_chusr(char* args) {
    if (args[0]) strncpy(user, args, 31);
    user[31] = 0;
    return 0;
}

static int execute_cmd_beep(char* args) {
    int f = 1000, ms = 300;
    if (args[0]) { f = 2000; ms = 500; }
    beep_pit(f, ms);
    return 0;
}

static int execute_cmd_panic(char* args) {
    if (args && args[0]) {
        panic("Shell", args, __func__);
    } else {
        panic("Shell", "User requested panic", __func__);
    }
    return 0;
}

// Команды RAM-FS
static int execute_cmd_ls(char* args)    { fs_list(args[0] ? args : NULL); return 0; }
static int execute_cmd_cd(char* args)    { if (args[0]) return fs_cd(args); return 0; }
static int execute_cmd_pwd(char* args)   { (void)args; fs_pwd(); return 0; }
static int execute_cmd_rm(char* args)    { if (args[0]) fs_rm(args); return 0; }
static int execute_cmd_touch(char* args) { if (args[0]) fs_touch(args); return 0; }
static int execute_cmd_cp(char* args)    { cmd_cp(args); return 0; }
static int execute_cmd_mv(char* args)    { cmd_mv(args); return 0; }
static int execute_cmd_tree(char* args)  { (void)args; cmd_tree(NULL, 0); return 0; }
static int execute_cmd_cat(char* args)   { if (args[0]) fs_cat(args); return 0; }

static int execute_cmd_mkdir(char* args) {
    if (args[0]) return fs_mkdir(args);
    vga_print_color("Usage: mkdir <name>\n", LIGHT_RED);
    return 1;
}

static int execute_cmd_write(char* args) {
    if (!args[0]) {
        vga_print_color("Usage: write <file> <text>\n", LIGHT_RED);
        return 1;
    }
    char* text = strchr(args, ' ');
    if (text) {
        *text = 0;
        text++;
        while (*text == ' ') text++; // Пропускаем лишние пробелы перед текстом
        fs_write(args, text);
    } else {
        vga_print_color("Usage: write <file> <text>\n", LIGHT_RED);
    }
    return 0;
}

// Внешние программы
static int execute_cmd_fm(char* args)          { (void)args; fm_run(); return 0; }
static int execute_cmd_screensaver(char* args) { (void)args; screensaver_run(); return 0; }

static int execute_cmd_nano(char* args) {
    if (args[0]) nano_edit(args);
    else vga_print_color("Usage: nano <file>\n", LIGHT_RED);
    return 0;
}

// Команды диска и FAT
static int execute_cmd_disks(char* args)    { (void)args; cmd_disks(); return 0; }
static int execute_cmd_umount(char* args)   { (void)args; fat_unmount(); vga_print_color("Unmounted\n", 0x0A); return 0; }
static int execute_cmd_fatls(char* args)    { fat_ls(args[0] ? args : NULL); return 0; }
static int execute_cmd_fatpwd(char* args)   { (void)args; fat_pwd(); return 0; }
static int execute_cmd_fatwrite(char* args) { (void)args; cmd_fatwrite(); return 0; }
static int execute_cmd_fatinfo(char* args)  { (void)args; fat_info(); return 0; }
static int execute_cmd_fat(char* args)      { (void)args; fat_shell(); return 0; }

static int execute_cmd_mount(char* args) {
    int drive = 0;
    if (args[0]) drive = args[0] - '0';
    if (fat_mount(drive) == 0) {
        vga_print_color("Mounted ", 0x0A);
        vga_print_color(fat_get_type_str(), YELLOW);
        vga_print_color(" filesystem\n", 0x0A);
    }
    return 0;
}

static int execute_cmd_fatcd(char* args) {
    if (args[0]) fat_cd(args);
    else fat_cd("/");
    return 0;
}

static int execute_cmd_fatcat(char* args) {
    if (args[0]) fat_cat(args);
    else vga_print_color("Usage: fatcat <file>\n", LIGHT_RED);
    return 0;
}

static int execute_cmd_fatmkdir(char* args) {
    if (args[0]) fat_mkdir(args);
    else vga_print_color("Usage: fatmkdir <name>\n", LIGHT_RED);
    return 0;
}

static int execute_cmd_fatrm(char* args) {
    if (args[0]) fat_rm(args);
    else vga_print_color("Usage: fatrm <name>\n", LIGHT_RED);
    return 0;
}

static int execute_cmd_fattouch(char* args) {
    if (args[0]) fat_touch(args);
    else vga_print_color("Usage: fattouch <name>\n", LIGHT_RED);
    return 0;
}

// Интернет
static int execute_cmd_ping(char* args) {
    extern void ping_cmd(char* args); // Прототип, если его нет в all_commands.h
    ping_cmd(args);
    return 0;
}

static int execute_cmd_pci(char* args) {
    (void)args;
    pci_scan_bus();
    return 0;
}

static int execute_cmd_netstat(char* args) {
    (void)args;
    vga_print_color("Checking network buffer...\n", LIGHT_CYAN);
    rtl8139_receive();
    return 0;
}

static int execute_cmd_net_test(char* args) {
    (void)args;
    net_test();
    return 0;
}

/* ========================================================================== */
/* Таблица команд                              */
/* ========================================================================== */

static const command_t commands[] = {
    {"help",        execute_cmd_help},
    {"clear",       execute_cmd_clear},
    {"echo",        execute_cmd_echo},
    {"calc",        execute_cmd_calc},
    {"chusr",       execute_cmd_chusr},
    {"beep",        execute_cmd_beep},
    {"crash",       execute_cmd_crash},
    {"sysinfo",     execute_cmd_sysinfo},
    {"slowfetch",   execute_cmd_slowfetch},
    {"uptime",      execute_cmd_uptime},
    {"meminfo",     execute_cmd_meminfo},
    {"time",        execute_cmd_time},
    {"aarch",       execute_cmd_aarch},
    {"reboot",      execute_cmd_reboot},
    {"poweroff",    execute_cmd_poweroff},
    {"shutdown",    execute_cmd_poweroff},
    {"whoami",      execute_cmd_whoami},
    {"date",        execute_cmd_date},
    {"colorbar",    execute_cmd_colorbar},
    {"memtest",     execute_cmd_memtest},
    {"panic",       execute_cmd_panic},
    {"history",     execute_cmd_history},
    {"mkrootfs",    execute_cmd_mkrootfs},

    // Команды RAM-FS
    {"ls",          execute_cmd_ls},
    {"cd",          execute_cmd_cd},
    {"pwd",         execute_cmd_pwd},
    {"mkdir",       execute_cmd_mkdir},
    {"rm",          execute_cmd_rm},
    {"touch",       execute_cmd_touch},
    {"write",       execute_cmd_write},
    {"cat",         execute_cmd_cat},
    {"cp",          execute_cmd_cp},
    {"mv",          execute_cmd_mv},
    {"tree",        execute_cmd_tree},

    // Программы
    {"nano",        execute_cmd_nano},
    {"fm",          execute_cmd_fm},
    {"ss",          execute_cmd_screensaver},
    {"screensaver", execute_cmd_screensaver},

    // Диски и FAT
    {"disks",       execute_cmd_disks},
    {"mount",       execute_cmd_mount},
    {"umount",      execute_cmd_umount},
    {"fatls",       execute_cmd_fatls},
    {"fatcd",       execute_cmd_fatcd},
    {"fatpwd",      execute_cmd_fatpwd},
    {"fatcat",      execute_cmd_fatcat},
    {"fatmkdir",    execute_cmd_fatmkdir},
    {"fatrm",       execute_cmd_fatrm},
    {"fattouch",    execute_cmd_fattouch},
    {"fatwrite",    execute_cmd_fatwrite},
    {"fatinfo",     execute_cmd_fatinfo},
    {"fat",         execute_cmd_fat},

    // Интернет
    {"pci",         execute_cmd_pci},
    {"netstat",     execute_cmd_netstat},
    {"net_test",    execute_cmd_net_test},
    {"ping",        execute_cmd_ping},
};

#define COMMANDS_COUNT (sizeof(commands) / sizeof(commands[0]))

int execute_command(char* cmd) {
    while (*cmd == ' ') cmd++;
    if (!*cmd) return 0;

    char* space = strchr(cmd, ' ');
    char* args = "";
    if (space) {
        *space = '\0';
        args = space + 1;
        while (*args == ' ') args++;
        rtrim_spaces(args);
    }

    for (size_t i = 0; i < COMMANDS_COUNT; i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            return commands[i].handler(args);
        }
    }

    vga_print_color("Command not found\n", LIGHT_RED);
    return 127;
}
