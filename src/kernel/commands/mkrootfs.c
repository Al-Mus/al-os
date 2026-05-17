#include "all_commands.h"
#include "../fs/fat/fat.h"
#include "../drivers/vga/vga.h"
#include "../drivers/vga/colors.h"
#include "../utils/string.h"

void cmd_mkrootfs(const char*) {
    if (!fat_is_mounted()) {
        vga_print_color("Error: FAT disk is not mounted. Use 'mount <drive>' first.\n", LIGHT_RED);
        return;
    }

    vga_print_color("Creating root filesystem structure...\n", LIGHT_CYAN);

    const char* dirs[] = {
        "/bin",
        "/etc",
        "/home",
        "/usr",
        "/tmp",
        NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        vga_print_color("Creating dir  ", WHITE);
        vga_print_color(dirs[i], YELLOW);
        vga_print_color("... ", WHITE);

        if (fat_mkdir(dirs[i]) == 0) {
            vga_print_color("[ OK ]\n", LIGHT_GREEN);
        } else {
            vga_print_color("[ EXISTS / FAIL ]\n", LIGHT_RED);
        }
    }

    const char* localtime_path = "/etc/localtime";
    const char* localtime_content = "+0";
    vga_print_color("Creating file ", WHITE);
    vga_print_color(localtime_path, YELLOW);
    vga_print_color("... ", WHITE);

    fat_touch(localtime_path);
    if (fat_write(localtime_path, localtime_content, strlen(localtime_content)) >= 0) {
        vga_print_color("[ OK ]\n", LIGHT_GREEN);
    } else {
        vga_print_color("[ FAIL ]\n", LIGHT_RED);
    }

    const char* os_release_path = "/etc/os-release";
    const char* os_release_content =
    "NAME=\"AL-OS\"\n"
    "PRETTY_NAME=\"Al-OS\"\n"
    "ID=al-os\n"
    "BUILD_ID=v0.5.1\n"
    "HOME_URL=\"https://github.com/Al-Mus/al-os\"\n"
    "LOGO=al-os-logo\n";

    vga_print_color("Creating file ", WHITE);
    vga_print_color(os_release_path, YELLOW);
    vga_print_color("... ", WHITE);

    fat_touch(os_release_path);
    if (fat_write(os_release_path, os_release_content, strlen(os_release_content)) >= 0) {
        vga_print_color("[ OK ]\n", LIGHT_GREEN);
    } else {
        vga_print_color("[ FAIL ]\n", LIGHT_RED);
    }

    vga_print_color("Rootfs structure created successfully!\n", LIGHT_CYAN);
}
