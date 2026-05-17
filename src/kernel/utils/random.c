#include "random.h"
#include "ports.h"

static unsigned int rand_seed = 12345;

unsigned int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

int rand_range(int min, int max) {
    return min + (rand() % (max - min + 1));
}

void rand_init(void) {
    outb(0x43, 0x00);
    unsigned char lo = inb(0x40);
    unsigned char hi = inb(0x40);
    rand_seed = (hi << 8) | lo;
    rand_seed ^= rand_seed << 13;
    if (rand_seed == 0) rand_seed = 12345;
}
