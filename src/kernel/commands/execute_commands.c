#include "execute_commands.h"
#include "../../apps/nano/nano.h"
#include "../../apps/fm/fm.h"
#include "../../apps/screensaver/screensaver.h"
#include "../sys/panic.h"
#include "../drivers/vga/vga.h"
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

    //Команды shell
    if (strcmp(cmd, "help") == 0)       { cmd_help(args); return 0; }
    else if (strcmp(cmd, "clear") == 0) { vga_clear(); return 0; }
    else if (strcmp(cmd, "echo") == 0) cmd_echo(args);
    else if (strcmp(cmd, "calc") == 0) cmd_calc(args);
    else if (strcmp(cmd, "chusr") == 0) { if (args[0]) strncpy(user, args, 31); user[31] = 0; }
    else if (strcmp(cmd, "beep") == 0) { int f = 1000, ms = 300; if (args[0]) { f = 2000; ms = 500; } beep_pit(f, ms); }
    else if (strcmp(cmd, "crash") == 0) cmd_crash();
    else if (strcmp(cmd, "sysinfo") == 0) cmd_sysinfo();
    else if (strcmp(cmd, "slowfetch") == 0) cmd_slowfetch();
    else if (strcmp(cmd, "uptime") == 0) uptime_cmd();
    else if (strcmp(cmd, "meminfo") == 0) meminfo_cmd();
    else if (strcmp(cmd, "time") == 0) time_cmd();
    else if (strcmp(cmd, "aarch") == 0) cmd_aarch();
    else if (strcmp(cmd, "reboot") == 0) do_reboot();
    else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) do_poweroff();
    else if (strcmp(cmd, "whoami") == 0)    cmd_whoami();
    else if (strcmp(cmd, "date") == 0)      cmd_date(args);
    else if (strcmp(cmd, "colorbar") == 0)  cmd_colorbar();
    else if (strcmp(cmd, "memtest") == 0)   cmd_memtest();
    else if (strcmp(cmd, "panic") == 0) { if (args && args[0]) { panic("Shell", args, __func__); } else { panic("Shell", "User requested panic", __func__); } }
    else if (strcmp(cmd, "history") == 0) cmd_history();
    else if (strcmp(cmd, "mkrootfs") == 0)  cmd_mkrootfs(args);


    // Команды ram-fs
    else if (strcmp(cmd, "ls") == 0)    { fs_list(args[0] ? args : NULL); return 0; }
    else if (strcmp(cmd, "cd") == 0)    { if (args[0]) return fs_cd(args); return 0; }
    else if (strcmp(cmd, "pwd") == 0)   { fs_pwd(); return 0; }
    else if (strcmp(cmd, "mkdir") == 0) { if (args[0]) return fs_mkdir(args); vga_print_color("Usage: mkdir <name>\n", LIGHT_RED); return 1; }
    else if (strcmp(cmd, "rm") == 0) { if (args[0]) fs_rm(args); }
    else if (strcmp(cmd, "touch") == 0) { if (args[0]) fs_touch(args); }
    else if (strcmp(cmd, "write") == 0) { char* text = strchr(args, ' '); if (text) { *text = 0; text++; fs_write(args, text); } }
    else if (strcmp(cmd, "cat") == 0) { if (args[0]) fs_cat(args); }
    else if (strcmp(cmd, "cp") == 0) cmd_cp(args);
    else if (strcmp(cmd, "mv") == 0) cmd_mv(args);
    else if (strcmp(cmd, "tree") == 0) cmd_tree(NULL, 0);


    // Команды программы
    else if (strcmp(cmd, "nano") == 0) { if (args[0]) nano_edit(args); else vga_print_color("Usage: nano <file>\n", LIGHT_RED); }
    else if (strcmp(cmd, "fm") == 0) { fm_run(); return 0; }
    else if (strcmp(cmd, "ss") == 0 || strcmp(cmd, "screensaver") == 0) { screensaver_run(); return 0; }


    // Команды с диском
    else if (strcmp(cmd, "disks") == 0) cmd_disks();
    else if (strcmp(cmd, "mount") == 0) { int drive = 0; if (args[0]) drive = args[0] - '0'; if (fat_mount(drive) == 0) { vga_print_color("Mounted ", 0x0A); vga_print_color(fat_get_type_str(), YELLOW); vga_print_color(" filesystem\n", 0x0A); } }
    else if (strcmp(cmd, "umount") == 0) { fat_unmount(); vga_print_color("Unmounted\n", 0x0A); }
    else if (strcmp(cmd, "fatls") == 0) fat_ls(args[0] ? args : NULL);
    else if (strcmp(cmd, "fatcd") == 0) { if (args[0]) fat_cd(args); else fat_cd("/"); }
    else if (strcmp(cmd, "fatpwd") == 0) fat_pwd();
    else if (strcmp(cmd, "fatcat") == 0) { if (args[0]) fat_cat(args); else vga_print_color("Usage: fatcat <file>\n", LIGHT_RED); }
    else if (strcmp(cmd, "fatmkdir") == 0) { if (args[0]) fat_mkdir(args); else vga_print_color("Usage: fatmkdir <name>\n", LIGHT_RED); }
    else if (strcmp(cmd, "fatrm") == 0) { if (args[0]) fat_rm(args); else vga_print_color("Usage: fatrm <name>\n", LIGHT_RED); }
    else if (strcmp(cmd, "fattouch") == 0) { if (args[0]) fat_touch(args); else vga_print_color("Usage: fattouch <name>\n", LIGHT_RED); }
    else if (strcmp(cmd, "fatwrite") == 0) cmd_fatwrite();
    else if (strcmp(cmd, "fatinfo") == 0) fat_info();
    else if (strcmp(cmd, "fat") == 0) fat_shell();


    else { vga_print_color("Command not found\n", LIGHT_RED); return 127; }

    return 0;
}
