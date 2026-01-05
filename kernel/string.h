#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int strcmp(const char* a, const char* b);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
char* strcpy(char* dest, const char* src);
void* memset(void* s, int c, size_t n);
size_t strlen(const char* str);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
char* strrchr(const char* s, int c);
void* memmove(void* dest, const void* src, size_t n);

void itoa(int value, char* str, int base);

#endif