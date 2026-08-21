#ifndef DJY_TEST_STRING_H
#define DJY_TEST_STRING_H
#include <stddef.h>
void *memcpy(void *destination, const void *source, size_t count);
void *memset(void *destination, int value, size_t count);
size_t strlen(const char *text);
#endif
