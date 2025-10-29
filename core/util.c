#define _POSIX_C_SOURCE 200809L

#include "util.h"

#include <stdckdint.h>
#include <stdlib.h>
#include <stdio.h>

void die(const char *message)
{
    if (message != NULL)
        fprintf(stderr, "error: %s\n", message);

    exit(EXIT_FAILURE);
}

void *safe_alloc(size_t count, size_t element_size, bool zeroing)
{
    size_t memory_size;
    if (ckd_mul(&memory_size, count, element_size))
        die("memory allocation error: size overflow");

    void *memory = zeroing
        ? calloc(count, element_size)
        : malloc(memory_size);

    if (memory == NULL)
        die("memory allocation error: out of memory");

    return memory;
}

void *safe_realloc(void *memory, size_t count, size_t element_size)
{
    size_t memory_size;
    if (ckd_mul(&memory_size, count, element_size))
        die("memory allocation error: size overflow");

    memory = realloc(memory, memory_size);
    if (memory == NULL)
        die("memory allocation error: out of memory");

    return memory;
}
