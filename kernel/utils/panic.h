#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>

__attribute__((noreturn))
void panic(const char* module, const char* reason, const char* func_name);

#endif