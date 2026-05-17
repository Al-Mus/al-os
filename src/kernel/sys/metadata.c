#include "metadata.h"
#include "../drivers/vga/vga.h"

// void load_data() {

// }
const char* VERSION = "0.5.1";

const char* RELEASE_NOTES = "v0.5.1 - New command mkrootfs! And update date";

const char* AL_OS_LOGO[] = {
    "     _    _         ___  ____  ",
    "    / \\  | |       / _ \\/ ___| ",
    "   / _ \\ | |      | | | \\___ \\ ",
    "  / ___ \\| |___   | |_| |___) |",
    " /_/   \\_\\_____|   \\___/|____/ ",
    0 //
};

void print_logo() {
    for (int i = 0; i < 5; i++) {
        vga_print_color(AL_OS_LOGO[i], 0x0B);
        vga_print_color("\n", 0x00);
    }
}
