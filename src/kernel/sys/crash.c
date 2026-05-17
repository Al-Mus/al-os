#include "crash.h"

void crash_system(void) {
    // __asm__ __volatile__("cli");
    volatile int zero = 0;
    volatile int crash = 10 / zero; // Процессор должен споткнуться здесь!
    (void)crash;
}
