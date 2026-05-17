#include "../../kernel/drivers/vga/vga.h"
#include "../../kernel/fs/fat/fat.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/kernel.h"
#include "../../kernel/drivers/keyboard/keyboard.h"
#include "../../kernel/exec/elf.h"
#include "fat_shell.h"
#include "../../kernel/drivers/vga/colors.h"



void fat_shell(void) {
    if (!fat_is_mounted()) {
        vga_print_color("No FAT filesystem mounted. Use 'mount 0' first.\n", LIGHT_RED);
        return;
    }

    vga_print_color("Entering FAT shell. Type 'exit' to return.\n", 0x0A);

    char cmd[MAX_CMD_LEN];

    while (1) {
        vga_print_color("fat:", 0x0B);
        vga_print_color(fat_get_current_path(), YELLOW);
        vga_print_color("> ", 0x07);

        keyboard_read_line(cmd, MAX_CMD_LEN);

        if (cmd[0] == '\0' || cmd[0] == '\n') continue;

        char* space = strchr(cmd, ' ');
        char* args = "";
        if (space) {
            *space = '\0';
            args = space + 1;
            while (*args == ' ') args++;
        }

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            vga_print_color("Exiting FAT shell\n", 0x0A);
            break;
        }
        else if (strcmp(cmd, "help") == 0) {
            vga_print_color("FAT Shell Commands:\n", YELLOW);
            vga_print_color("  ls [path]       - List directory\n", 0x0F);
            vga_print_color("  cd [path]       - Change directory\n", 0x0F);
            vga_print_color("  pwd             - Print working directory\n", 0x0F);
            vga_print_color("  cat <file>      - Display file contents\n", 0x0F);
            vga_print_color("  mkdir <name>    - Create directory\n", 0x0F);
            vga_print_color("  touch <name>    - Create empty file\n", 0x0F);
            vga_print_color("  rm <name>       - Remove file/directory\n", 0x0F);
            vga_print_color("  write <f> <txt> - Write text to file\n", 0x0F);
            vga_print_color("  exec <file>     - Execute ELF program\n", 0x0F);
            vga_print_color("  info            - Filesystem info\n", 0x0F);
            vga_print_color("  clear           - Clear screen\n", 0x0F);
            vga_print_color("  exit            - Exit FAT shell\n", 0x0F);
        }
        else if (strcmp(cmd, "ls") == 0) {
            fat_ls(args[0] ? args : NULL);
        }
        else if (strcmp(cmd, "cd") == 0) {
            if (args[0]) fat_cd(args);
            else fat_cd("/");
        }
        else if (strcmp(cmd, "pwd") == 0) {
            fat_pwd();
        }
        else if (strcmp(cmd, "cat") == 0) {
            if (args[0]) fat_cat(args);
            else vga_print_color("Usage: cat <file>\n", LIGHT_RED);
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            if (args[0]) fat_mkdir(args);
            else vga_print_color("Usage: mkdir <name>\n", LIGHT_RED);
        }
        else if (strcmp(cmd, "touch") == 0) {
            if (args[0]) fat_touch(args);
            else vga_print_color("Usage: touch <name>\n", LIGHT_RED);
        }
        else if (strcmp(cmd, "rm") == 0) {
            if (args[0]) fat_rm(args);
            else vga_print_color("Usage: rm <name>\n", LIGHT_RED);
        }
        else if (strcmp(cmd, "write") == 0) {
            char* text = strchr(args, ' ');
            if (text) {
                *text = '\0';
                text++;
                if (fat_write(args, text, strlen(text)) == 0) {
                    vga_print_color("Written\n", 0x0A);
                }
            } else {
                vga_print_color("Usage: write <file> <text>\n", LIGHT_RED);
            }
        }
        else if (strcmp(cmd, "exec") == 0 || strcmp(cmd, "run") == 0 || strcmp(cmd, "./") == 0) {
            if (args[0]) {
                elf_exec(args);
            } else {
                vga_print_color("Usage: exec <file.elf>\n", LIGHT_RED);
            }
        }
        else if (strcmp(cmd, "info") == 0) {
            fat_info();
        }
        else if (strcmp(cmd, "clear") == 0) {
            vga_clear();
        }
        else {
            if (fat_exists(cmd)) {
                elf_exec(cmd);
            } else {
                vga_print_color("Unknown command. Type 'help' for list.\n", LIGHT_RED);
            }
        }
    }
}
