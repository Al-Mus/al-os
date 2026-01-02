#include "string.h"

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        i++;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    size_t len = 0;
    while (dest[len]) len++;
    size_t i = 0;
    while (src[i]) { dest[len + i] = src[i]; i++; }
    dest[len + i] = '\0';
    return dest;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = s1;
    const unsigned char* b = s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}