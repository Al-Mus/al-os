#include "../drivers/vga/vga.h"
#include "../fs/memory_fs/fs.h"
#include "all_commands.h"


void cmd_tree(fs_node* node, int depth) {
    if (!node) node = fs_current;
    for (int i = 0; i < node->child_count; i++) {
        for (int d = 0; d < depth; d++) vga_print("  ");
        vga_print_color(node->children[i]->name, node->children[i]->type == FS_DIR ? 0x09 : 0x0F);
        if (node->children[i]->type == FS_DIR) vga_print_color("/", 0x09);
        vga_putc('\n');
        if (node->children[i]->type == FS_DIR) cmd_tree(node->children[i], depth + 1);
    }
}
