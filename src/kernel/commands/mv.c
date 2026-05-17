#include "../utils/string.h"
#include "../kernel.h"
#include "../fs/memory_fs/fs.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"


void cmd_mv(const char* args) {
    if (!args) { vga_print_color("Usage: mv <src> <dest>\n", LIGHT_RED); return; }
    char* dest = strchr(args, ' ');
    if (!dest) { vga_print_color("Usage: mv <src> <dest>\n", LIGHT_RED); return; }
    *dest = '\0'; dest++;
    while (*dest == ' ') dest++;

    fs_node* src = resolve_path(args, fs_current);
    if (!src) { vga_print_color("Source not found\n", LIGHT_RED); return; }
    const char* new_name = dest;
    char* slash = strrchr(dest, '/');
    if (slash) new_name = slash + 1;
    strcpy(src->name, new_name);
}
