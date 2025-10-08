#define _POSIX_C_SOURCE 200809L

#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH(func, iter_count)                                 \
    do                                                          \
    {                                                           \
        struct timespec start, end;                             \
        clock_gettime(CLOCK_MONOTONIC, &start);                 \
        for (size_t _it = 0; _it < (iter_count); _it++)         \
        {                                                       \
            func;                                               \
        }                                                       \
        clock_gettime(CLOCK_MONOTONIC, &end);                   \
        long seconds = end.tv_sec - start.tv_sec;               \
        long nanoseconds = end.tv_nsec - start.tv_nsec;         \
        double elapsed_ns = 1.0e9 * seconds + nanoseconds;      \
        printf("elapsed_ns: %f\n", elapsed_ns / (iter_count));  \
    } while (0)

#define N 32768   // длина массива


static void poly_scalar(const float* x, float* y,
                        float a, float b, float c, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        y[i] = a * x[i] * x[i] + b * x[i] + c;
    }
}

static void poly_avx2(const float* x, float* y,
                      float a, float b, float c, size_t n)
{
    __m256 va = _mm256_set1_ps(a);
    __m256 vb = _mm256_set1_ps(b);
    __m256 vc = _mm256_set1_ps(c);

    size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vx2 = _mm256_mul_ps(vx, vx);              // x^2
        __m256 term1 = _mm256_mul_ps(va, vx2);           // a*x^2
        __m256 term2 = _mm256_mul_ps(vb, vx);            // b*x
        __m256 vy = _mm256_add_ps(term1, term2);
        vy = _mm256_add_ps(vy, vc);                      // + c
        _mm256_storeu_ps(y + i, vy);
    }
    // хвост, если n не кратно 8
    for (; i < n; ++i) {
        y[i] = a * x[i] * x[i] + b * x[i] + c;
    }
}

int main(void)
{
    float *x = (float*)malloc(N * sizeof(float));
    float *y = (float*)malloc(N * sizeof(float));
    if (!x || !y) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }

    // инициализация массива x
    for (size_t i = 0; i < N; ++i) {
        x[i] = (float)(i % 1000) * 0.001f;
    }

    float a = 2.0f, b = -1.5f, c = 0.5f;

    BENCH(poly_scalar(x, y, a, b, c, N), 1000);
    BENCH(poly_avx2(x, y, a, b, c, N), 1000);

    free(x);
    free(y);
    return 0;
}
