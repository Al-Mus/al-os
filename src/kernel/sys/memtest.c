#include "../drivers/vga/vga.h"
#include "../utils/string.h"
#include "memtest.h"
#include "../drivers/vga/colors.h"


void memtest(void) {
    extern char end;
    uint32_t start_addr = (uint32_t)&end + 0x1000;
    uint32_t test_size = 0x100000;

    vga_print_color("Simple memory test...\n", YELLOW);
    vga_print("Testing range: 0x");
    char buf[16];
    itoa(start_addr, buf, 16);
    vga_print(buf);
    vga_print(" - 0x");
    itoa(start_addr + test_size, buf, 16);
    vga_print(buf);
    vga_print("\n");

    uint8_t *ptr = (uint8_t*)start_addr;
    int errors = 0;

    vga_print("Writing 0xAA... ");
    for (uint32_t i = 0; i < test_size; i++) {
        ptr[i] = 0xAA;
    }
    vga_print_color("OK\n", 0x0A);

    vga_print("Reading 0xAA...  ");
    for (uint32_t i = 0; i < test_size; i++) {
        if (ptr[i] != 0xAA) {
            errors++;
            if (errors <= 5) {
                vga_print_color("Error at 0x", LIGHT_RED);
                itoa((uint32_t)&ptr[i], buf, 16);
                vga_print(buf);
                vga_print(" (");
                itoa(ptr[i], buf, 16);
                vga_print(buf);
                vga_print(")\n");
            }
        }
    }
    if (errors == 0) vga_print_color("OK - all correct\n", 0x0A);
    else {
        vga_print_color("Errors found: ", LIGHT_RED);
        itoa(errors, buf, 10);
        vga_print(buf);
        vga_print("\n");
    }

    vga_print("\n");
}
