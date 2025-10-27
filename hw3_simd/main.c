#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

#include "../core/util.h"
#include "../core/bench.h"

static void fill_text(char *data, size_t len, unsigned int seed)
{
    srand(seed);
    for (size_t i = 0; i < len; i++)
        data[i] = (char)('a' + rand() % 26);
}

static long search_scalar(const char *haystack, size_t hay_len, const char *needle, size_t needle_len)
{
    if (needle_len == 0)
        return 0;
    if (needle_len > hay_len)
        return -1;
    size_t limit = hay_len - needle_len + 1;
    for (size_t i = 0; i < limit; i++)
    {
        size_t j = 0;
        for (; j < needle_len; j++)
            if (haystack[i + j] != needle[j])
                break;
        if (j == needle_len)
            return (long)i;
    }
    return -1;
}

static long search_avx2(const char *haystack, size_t hay_len, const char *needle, size_t needle_len)
{
    if (needle_len == 0)
        return 0;
    if (needle_len > hay_len)
        return -1;
    __m256i first = _mm256_set1_epi8((char)needle[0]);
    size_t limit = hay_len - needle_len + 1;
    size_t i = 0;
    const size_t step = 32;
    while (i + step <= hay_len)
    {
        __m256i block = _mm256_loadu_si256((const __m256i *)(haystack + i));
        __m256i cmp = _mm256_cmpeq_epi8(block, first);
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(cmp);
        while (mask)
        {
            unsigned bit = (unsigned)__builtin_ctz(mask);
            size_t pos = i + bit;
            if (pos < limit && memcmp(haystack + pos, needle, needle_len) == 0)
                return (long)pos;
            mask &= mask - 1;
        }
        i += step;
    }
    for (; i < limit; i++)
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return (long)i;
    return -1;
}

static volatile long search_sink = 0;

static void search_scalar_run(const char *haystack, size_t hay_len, const char *needle, size_t needle_len)
{
    search_sink = search_scalar(haystack, hay_len, needle, needle_len);
}

static void search_avx2_run(const char *haystack, size_t hay_len, const char *needle, size_t needle_len)
{
    search_sink = search_avx2(haystack, hay_len, needle, needle_len);
}

int main(void)
{
    size_t sizes[] = {4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288};
    size_t needle_len = 32;
    size_t repeats = 10;
    size_t inner = 100;
    for (size_t idx = 0; idx < sizeof(sizes) / sizeof(sizes[0]); idx++)
    {
        size_t hay_len = sizes[idx];
        char *haystack = newarr(char, hay_len);
        char *needle = newarr(char, needle_len);
        fill_text(needle, needle_len, (unsigned int)(hay_len + 1));
        fill_text(haystack, hay_len, (unsigned int)(hay_len + 3));
        size_t insert_pos = hay_len > needle_len ? hay_len / 2 : 0;
        if (insert_pos + needle_len > hay_len)
            insert_pos = hay_len - needle_len;
        memcpy(haystack + insert_pos, needle, needle_len);
        long s = search_scalar(haystack, hay_len, needle, needle_len);
        long v = search_avx2(haystack, hay_len, needle, needle_len);
        if (s != v || s < 0)
        {
            fprintf(stderr, "mismatch at size %zu\n", hay_len);
            free(haystack);
            free(needle);
            return EXIT_FAILURE;
        }
        size_t total_iters = repeats * inner;
        BENCH(hay_len, total_iters, search_scalar_run(haystack, hay_len, needle, needle_len),
              search_avx2_run(haystack, hay_len, needle, needle_len));
        free(haystack);
        free(needle);
    }
    return EXIT_SUCCESS;
}
