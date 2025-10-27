#include <immintrin.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../core/util.h"
#include "../core/bench.h"

static void fill_matrix(float *data, size_t n, unsigned int seed)
{
    srand(seed);
    size_t total = n * n;
    for (size_t i = 0; i < total; i++)
    {
        float r = (float)rand() / (float)RAND_MAX;
        data[i] = r * 2.0f - 1.0f;
    }
}

static void transpose(const float *src, float *dst, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = 0; j < n; j++)
        {
            dst[j * n + i] = src[i * n + j];
        }
    }
}

static void matmul_scalar(const float *a, const float *b, float *c, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = 0; j < n; j++)
        {
            float sum = 0.0f;
            for (size_t k = 0; k < n; k++)
            {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

static float hsum256_ps(__m256 v)
{
    __m128 low = _mm256_castps256_ps128(v);
    __m128 high = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(low, high);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

static void matmul_avx(const float *a, const float *bt, float *c, size_t n)
{
    const size_t step = 8;
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = 0; j < n; j++)
        {
            __m256 acc = _mm256_setzero_ps();
            size_t k = 0;
            for (; k + step <= n; k += step)
            {
                __m256 va = _mm256_loadu_ps(a + i * n + k);
                __m256 vb = _mm256_loadu_ps(bt + j * n + k);
                acc = _mm256_add_ps(acc, _mm256_mul_ps(va, vb));
            }
            float sum = hsum256_ps(acc);
            for (; k < n; k++)
            {
                sum += a[i * n + k] * bt[j * n + k];
            }
            c[i * n + j] = sum;
        }
    }
}

static int compare(const float *a, const float *b, size_t n)
{
    size_t total = n * n;
    for (size_t i = 0; i < total; i++)
    {
        float d = fabsf(a[i] - b[i]);
        if (d > 1e-3f)
            return 0;
    }
    return 1;
}

static void matmul_scalar_run(const float *a, const float *b, float *c, size_t n)
{
    memset(c, 0, n * n * sizeof(float));
    matmul_scalar(a, b, c, n);
}

static void matmul_avx_run(const float *a, const float *bt, float *c, size_t n)
{
    memset(c, 0, n * n * sizeof(float));
    matmul_avx(a, bt, c, n);
}

int main(void)
{
    size_t sizes[] = {128, 192, 256, 320, 384, 448, 512};
    size_t size_count = sizeof(sizes) / sizeof(sizes[0]);
    size_t repeats = 5;

    for (size_t idx = 0; idx < size_count; idx++)
    {
        size_t n = sizes[idx];
        float *a = newarr(float, n * n);
        float *b = newarr(float, n * n);
        float *bt = newarr(float, n * n);
        float *c_scalar = newarr(float, n * n);
        float *c_avx = newarr(float, n * n);

        fill_matrix(a, n, (unsigned int)(n + 1));
        fill_matrix(b, n, (unsigned int)(n + 2));
        transpose(b, bt, n);

        matmul_scalar_run(a, b, c_scalar, n);
        matmul_avx_run(a, bt, c_avx, n);
        if (!compare(c_scalar, c_avx, n))
        {
            fprintf(stderr, "mismatch at size %zu\n", n);
            free(a);
            free(b);
            free(bt);
            free(c_scalar);
            free(c_avx);
            return EXIT_FAILURE;
        }

        BENCH(n, repeats, matmul_scalar_run(a, b, c_scalar, n), matmul_avx_run(a, bt, c_avx, n));

        free(a);
        free(b);
        free(bt);
        free(c_scalar);
        free(c_avx);
    }

    return EXIT_SUCCESS;
}
