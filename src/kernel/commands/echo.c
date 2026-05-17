#include "../kernel.h"
#include "../drivers/vga/vga.h"
#include "string.h"
#include "../fs/memory_fs/fs.h"
#include "all_commands.h"

void cmd_echo(const char* args) {
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
