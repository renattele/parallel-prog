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
    }                                                           \
    while (0)

// Размеры матриц (поддерживается любой K, N; N можно не кратно 8)
#define M 256
#define K 256
#define N 256

// -----------------------------
// Скалярное умножение C = A*B
// A: MxK, B: KxN, C: MxN (row-major)
// -----------------------------
static void matmul_scalar(const float* A, const float* B, float* C)
{
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; ++k) {
                sum += A[i*(size_t)K + k] * B[k*(size_t)N + j];
            }
            C[i*(size_t)N + j] = sum;
        }
    }
}

// -------------------------------------------
// AVX2: векторизация по столбцам (j) пачками по 8
// используем _mm256_broadcast_ss + _mm256_fmadd_ps,
// а если FMA недоступно — компилятор разложит на add+mul
// -------------------------------------------
static void matmul_avx2(const float* A, const float* B, float* C)
{
    const size_t J8 = N & ~(size_t)7; // кратная 8 часть N
    for (size_t i = 0; i < M; ++i) {
        // векторная часть по 8 столбцов
        for (size_t j = 0; j < J8; j += 8) {
            __m256 cvec = _mm256_setzero_ps();
            const float* arow = A + i*(size_t)K;
            for (size_t k = 0; k < K; ++k) {
                __m256 bvec = _mm256_loadu_ps(B + k*(size_t)N + j);
                __m256 aval = _mm256_broadcast_ss(arow + k);
                // FMA: cvec += aval * bvec
                #ifdef __FMA__
                cvec = _mm256_fmadd_ps(aval, bvec, cvec);
                #else
                cvec = _mm256_add_ps(cvec, _mm256_mul_ps(aval, bvec));
                #endif
            }
            _mm256_storeu_ps(C + i*(size_t)N + j, cvec);
        }
        // скалярный хвост, если N не кратно 8
        for (size_t j = J8; j < N; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; ++k) {
                sum += A[i*(size_t)K + k] * B[k*(size_t)N + j];
            }
            C[i*(size_t)N + j] = sum;
        }
    }
}

int main(void)
{
    float *A = (float*)malloc((size_t)M*K*sizeof(float));
    float *B = (float*)malloc((size_t)K*N*sizeof(float));
    float *C = (float*)malloc((size_t)M*N*sizeof(float));
    if (!A || !B || !C) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }

    // инициализируем детерминированно, чтобы компилятор не выкинул вычисления
    for (size_t i = 0; i < (size_t)M*K; ++i) A[i] = (float)((i % 71) - 35) * 0.031f;
    for (size_t i = 0; i < (size_t)K*N; ++i) B[i] = (float)((i % 53) - 26) * 0.027f;

    BENCH(matmul_scalar(A, B, C), 100);
    BENCH(matmul_avx2(A, B, C), 100);

    free(A); free(B); free(C);
    return 0;
}
