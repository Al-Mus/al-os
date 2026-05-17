#include "string.h"

/*
    Сравнивает две строки лексикографически
    Возвращает 0 если равны, <0 если a < b, >0 если a > b
 */
int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        i++;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

/*
    Копирует до n символов из src в dest
    Если src короче n — дополняет '\0'
 */
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

/*
    Конкатенирует строку src в конец dest (сложение строк)
 */
char* strcat(char* dest, const char* src) {
    size_t len = 0;
    while (dest[len]) len++;
    size_t i = 0;
    while (src[i]) { dest[len + i] = src[i]; i++; }
    dest[len + i] = '\0';
    return dest;
}

/*
    Копирует n байт из src в dest
    Не учитывает перекрытие памяти
 */
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

/*
    Сравнивает n байт двух блоков памяти
    Возвращает 0 если равны, иначе разницу первого отличия
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = s1;
    const unsigned char* b = s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

/*
    Копирует строку src в dest до '\0'
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

/*
    Заполняет первые n байт памяти s символом с
 */
void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

/*
    Копирует n байт из src в dest
    Учитывает перекрытие памяти
 */
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

/*
    Преобразует целое число value в строку str в системе счисления base
 */
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

/*
    Сравнивает n байт двух блоков памяти
    Возвращает 0 если равны, иначе разницу первого отличия
 */
int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == 0 || b[i] == 0)
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

/*
    Ищет символ c в строке s
    Возвращает указатель на первый найденный символ, или NULL, если не найден
 */
char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    if (*s == (char)c) return (char*)s;
    return NULL;
}

/*
    Ищет подстроку needle в строке haystack
    Возвращает указатель на первый найденный символ, или NULL, если не найден
 */
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return NULL;
}

/*
    Ищет последний символ c в строке s
    Возвращает указатель на последний найденный символ, или NULL, если не найден
 */
char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (*s == (char)c) last = s;
    return (char*)last;
}

/*
    Возвращает длину строки str
 */
size_t strlen(const char *str) {
    const char *s = str;
    while (*s) s++;
    return (size_t)(s - str);
}

int atoi(const char* s) {
    if (!s) return 0;

    int sign = 1;
    int result = 0;

    // пропуск пробелов
    while (*s == ' ' || *s == '\t')
        s++;

    // знак
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // цифры
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return result * sign;
}
