#define _POSIX_C_SOURCE 200810L

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIMD_WIDTH 32

// Скалярный поиск подстроки: сдвигаем образец по тексту и побайтно сравниваем.
static const char *find_substring_scalar(const char *text,
                                         size_t text_len,
                                         const char *pattern,
                                         size_t pattern_len)
{
    if (pattern_len == 0)
    {
        return text;
    }

    if (text_len < pattern_len)
    {
        return NULL;
    }

    const size_t limit = text_len - pattern_len;
    const char first = pattern[0];

    for (size_t i = 0; i <= limit; i++)
    {
        if (text[i] == first &&
            memcmp(text + i + 1, pattern + 1, pattern_len - 1) == 0)
        {
            return text + i;
        }
    }

    return NULL;
}

// Векторный поиск подстроки на AVX2: параллельно проверяем совпадение первого символа.
static const char *find_substring_avx2(const char *text,
                                       size_t text_len,
                                       const char *pattern,
                                       size_t pattern_len)
{
    if (pattern_len == 0)
    {
        return text;
    }

    if (text_len < pattern_len)
    {
        return NULL;
    }

    const size_t limit = text_len - pattern_len;
    const char first = pattern[0];
    const __m256i vfirst = _mm256_set1_epi8(first);
    size_t i = 0;

    if (text_len >= SIMD_WIDTH)
    {
        size_t safe_end = text_len - SIMD_WIDTH;
        size_t simd_end = safe_end < limit ? safe_end : limit;

        for (; i <= simd_end; i += SIMD_WIDTH)
        {
            __m256i chunk = _mm256_loadu_si256((const __m256i *)(text + i));
            __m256i cmp = _mm256_cmpeq_epi8(chunk, vfirst);
            unsigned int mask = (unsigned int)_mm256_movemask_epi8(cmp);

            while (mask != 0U)
            {
                unsigned int bit = (unsigned int)__builtin_ctz(mask);
                size_t pos = i + bit;

                if (pos > limit)
                {
                    break;
                }

                if (memcmp(text + pos, pattern, pattern_len) == 0)
                {
                    return text + pos;
                }

                mask &= mask - 1U;
            }
        }
    }

    for (; i <= limit; i++)
    {
        if (text[i] == first &&
            memcmp(text + i, pattern, pattern_len) == 0)
        {
            return text + i;
        }
    }

    return NULL;
}

// Вспомогательный бенчмарк: много раз выполняет выражение и выводит среднее время.
#define BENCH(label, expr, iter_count)                                     \
    do                                                                     \
    {                                                                      \
        struct timespec start, end;                                        \
        clock_gettime(CLOCK_MONOTONIC, &start);                            \
        volatile uintptr_t bench_sink = 0;                                 \
        for (size_t bench_i = 0; bench_i < (iter_count); bench_i++)        \
        {                                                                  \
            const char *bench_tmp = (expr);                                \
            bench_sink ^= (uintptr_t)bench_tmp;                            \
        }                                                                  \
        clock_gettime(CLOCK_MONOTONIC, &end);                              \
        long seconds = end.tv_sec - start.tv_sec;                          \
        long nanoseconds = end.tv_nsec - start.tv_nsec;                    \
        double elapsed_ns = 1.0e9 * seconds + nanoseconds;                 \
        printf("%s elapsed_ns: %f\n", (label), elapsed_ns / (iter_count)); \
        (void)bench_sink;                                                  \
    } while (0)

int main(void)
{
    const size_t text_len = 1 << 20;
    const size_t pattern_len = 32;

    char *text = malloc(text_len);
    char *pattern = malloc(pattern_len);

    if (text == NULL || pattern == NULL)
    {
        free(text);
        free(pattern);
        return EXIT_FAILURE;
    }

    srand(42);

    for (size_t i = 0; i < text_len; i++)
    {
        text[i] = (char)('A' + rand() % 26);
    }

    for (size_t i = 0; i < pattern_len; i++)
    {
        pattern[i] = (char)('a' + (i % 26));
    }

    const size_t match_pos = text_len / 2;
    // Гарантируем хотя бы одно совпадение в середине текста.
    memcpy(text + match_pos, pattern, pattern_len);

    const char *scalar_match = find_substring_scalar(text, text_len, pattern, pattern_len);
    const char *vector_match = find_substring_avx2(text, text_len, pattern, pattern_len);

    if (scalar_match != vector_match)
    {
        fprintf(stderr, "mismatch detected\n");
        free(text);
        free(pattern);
        return EXIT_FAILURE;
    }

    // Замеряем время работы обеих реализаций.
    BENCH("scalar", find_substring_scalar(text, text_len, pattern, pattern_len), 10000);
    BENCH("vector", find_substring_avx2(text, text_len, pattern, pattern_len), 10000);

    free(text);
    free(pattern);

    return EXIT_SUCCESS;
}
