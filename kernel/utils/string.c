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

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void itoa(int value, char* str, int base) {
    char* ptr = str, *ptr1 = str;
    if (base < 2 || base > 36) { *str = '\0'; return; }
    if (value == 0) { *ptr++ = '0'; *ptr = '\0'; return; }
    int sign = 0;
    if (value < 0 && base == 10) { sign = 1; value = -value; }
    while (value != 0) {
        int digit = value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= base;
    }
    if (sign) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) { char t = *ptr; *ptr-- = *ptr1; *ptr1++ = t; }
}

int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == 0 || b[i] == 0)
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    if (*s == (char)c) return (char*)s;
    return NULL;
}