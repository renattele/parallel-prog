#define _POSIX_C_SOURCE 200810L

#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void add(const float *a,
         const float *b,
         float *result,
         size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        result[i] = a[i] + b[i];
    }
}

void vadd(const float *a,
          const float *b,
          float *result,
          size_t len)
{
    for (size_t i = 0; i < len; i += 8)
    {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);

        __m256 vresult = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(result + i, vresult);
    }
}

#define BENCH(func, iter_count)                                 \
    do                                                          \
    {                                                           \
                                                                \
        struct timespec start, end;                             \
        clock_gettime(CLOCK_MONOTONIC, &start);                 \
        for (size_t i = 0; i < iter_count; i++)                 \
        {                                                       \
            func;                                               \
        }                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                   \
        long seconds = end.tv_sec - start.tv_sec;               \
        long nanoseconds = end.tv_nsec - start.tv_nsec;         \
        double elapsed_ns = 1.0e9 * seconds + nanoseconds;      \
        printf("elapsed_ns: %f\n", elapsed_ns / iter_count);    \
    }                                                           \
    while (false)

#define N 32

int main()
{
    float *a = malloc(N * sizeof(int));
    float *b = malloc(N * sizeof(int));
    float *result = malloc(N * sizeof(int));

    for (size_t i = 0; i < N; i++)
    {
        a[i] = i;
        b[i] = i;
    }

    BENCH(add(a, b, result, N), 10'000);
    BENCH(vadd(a, b, result, N), 10'000);

    return 0;
}