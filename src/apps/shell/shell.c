#include "../../kernel/drivers/vga/vga.h"
#include "shell.h"
#include "../../kernel/drivers/keyboard/keyboard.h"
#include "../../kernel/commands/execute_commands.h"
#include "../../kernel/kernel.h"
#include "../../kernel/fs/memory_fs/fs.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/drivers/vga/colors.h"

static int exit_status = 0;

void show_prompt(void) {
    if (exit_status != 0) {
        char exit_str[16];
        itoa(exit_status, exit_str, 10);
        vga_print_color(exit_str, LIGHT_RED);
        vga_print(" ");
    }

    vga_print_color(user, LIGHT_GREEN);
    vga_print_color("@al-os", LIGHT_CYAN);
    vga_print_color(current_path, WHITE);
    vga_print_color("> ", LIGHT_GREY);
}


void shell_main_loop(void) {
    char cmd[MAX_CMD_LEN];

    while (1)
    {
        show_prompt();
        keyboard_read_line(cmd, MAX_CMD_LEN);

        if (cmd[0] == '\0' || cmd[0] == '\n')
            continue;

        keyboard_history_add(cmd);

        char* current_cmd = cmd;
        while (current_cmd != NULL && *current_cmd != '\0') {

            char* next_separator = strstr(current_cmd, "&&");

            if (next_separator) {
                *next_separator = '\0';
                exit_status = execute_command(current_cmd);
                current_cmd = next_separator + 2;
            } else {
                exit_status = execute_command(current_cmd);
                current_cmd = NULL;
            }
        }
    }
}
