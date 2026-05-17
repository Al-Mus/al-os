#include <stddef.h>
#include "kernel.h"
#include "drivers/vga/vga.h"
#include "drivers/time/time.h"
#include "sys/init.h"
#include "../apps/shell/shell.h"
#include "drivers/vga/colors.h"
#include "arch/i686/gdt/gdt.h"
#include "arch/i686/idt/idt.h"
#include "arch/i686/pic/pic.h"
#include "arch/i686/timer/timer.h"



char user[32] = "root";
long boot_seconds = 0;

void tests()
{

}

void kernel_main(void)
{
    pic_remap(32, 40);
    init_gdt();
    init_idt();
    init_timer(100);
    __asm__ __volatile__("sti");
    // tests();

    init_system_base();

    shell_main_loop();
    vga_print_color("Shell exited.", LIGHT_RED);
}
