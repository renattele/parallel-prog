#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>

[[noreturn]] void die(const char *message);

void *safe_alloc(size_t count, size_t element_size, bool zeroing);
void *safe_realloc(void *memory, size_t count, size_t element_size);

#define new(T) ((T *)safe_alloc(1, sizeof(T), true))
#define newarr(T, count) ((T *)safe_alloc((count), sizeof(T), false))
#define resize(memory, T, count) ((T *)safe_realloc((memory), (count), sizeof(T)))

#endif // UTIL_H
