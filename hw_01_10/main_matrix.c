#define _POSIX_C_SOURCE 200810L

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(__FMA__)
#define FMADD(acc, data, coeff) acc = _mm256_fmadd_ps((data), (coeff), (acc))
#else
#define FMADD(acc, data, coeff) acc = _mm256_add_ps((acc), _mm256_mul_ps((data), (coeff)))
#endif

// Простой бенчмарк: многократно выполняет выражение и печатает среднее время.
#define BENCH(label, expr, iter_count)                                     \
    do                                                                     \
    {                                                                      \
        struct timespec start, end;                                        \
        clock_gettime(CLOCK_MONOTONIC, &start);                            \
        volatile uintptr_t bench_sink = 0;                                 \
        for (size_t bench_i = 0; bench_i < (iter_count); bench_i++)        \
        {                                                                  \
            (expr);                                                        \
            bench_sink ^= (uintptr_t)(bench_i + 1);                        \
        }                                                                  \
        clock_gettime(CLOCK_MONOTONIC, &end);                              \
        long seconds = end.tv_sec - start.tv_sec;                          \
        long nanoseconds = end.tv_nsec - start.tv_nsec;                    \
        double elapsed_ns = 1.0e9 * seconds + nanoseconds;                 \
        printf("%s elapsed_ns: %f\n", (label), elapsed_ns / (iter_count)); \
        (void)bench_sink;                                                  \
    } while (0)

static inline size_t idx(size_t row, size_t col, size_t width)
{
    return row * width + col; // смещение в матрице в постричном порядке
}

// Скалярная свёртка: вложенные циклы перемножают фильтр и входное изображение.
static void conv2d_scalar(const float *image,
                          size_t image_rows,
                          size_t image_cols,
                          const float *filter,
                          size_t filter_rows,
                          size_t filter_cols,
                          float *output)
{
    const size_t out_rows = image_rows - filter_rows + 1;
    const size_t out_cols = image_cols - filter_cols + 1;

    for (size_t r = 0; r < out_rows; r++)
    {
        for (size_t c = 0; c < out_cols; c++)
        {
            float sum = 0.0f;
            for (size_t fr = 0; fr < filter_rows; fr++)
            {
                for (size_t fc = 0; fc < filter_cols; fc++)
                {
                    sum += image[idx(r + fr, c + fc, image_cols)] *
                           filter[idx(fr, fc, filter_cols)];
                }
            }
            output[idx(r, c, out_cols)] = sum;
        }
    }
}

// Векторная свёртка на AVX2: считаем сразу восемь соседних пикселей с помощью FMA.
static void conv2d_avx2(const float *image,
                        size_t image_rows,
                        size_t image_cols,
                        const float *filter,
                        size_t filter_rows,
                        size_t filter_cols,
                        float *output)
{
    const size_t out_rows = image_rows - filter_rows + 1;
    const size_t out_cols = image_cols - filter_cols + 1;
    const size_t simd_width = 8;
    const size_t simd_cols = out_cols & ~(simd_width - 1U);

    for (size_t r = 0; r < out_rows; r++)
    {
        size_t c = 0;
        for (; c < simd_cols; c += simd_width)
        {
            __m256 acc = _mm256_setzero_ps();
            for (size_t fr = 0; fr < filter_rows; fr++)
            {
                const float *image_row = image + idx(r + fr, c, image_cols);
                const float *filter_row = filter + idx(fr, 0, filter_cols);
                for (size_t fc = 0; fc < filter_cols; fc++)
                {
                    __m256 pixels = _mm256_loadu_ps(image_row + fc);
                    __m256 coeffs = _mm256_set1_ps(filter_row[fc]);
                    FMADD(acc, pixels, coeffs);
                }
            }
            _mm256_storeu_ps(output + idx(r, c, out_cols), acc);
        }

        for (; c < out_cols; c++)
        {
            float sum = 0.0f;
            for (size_t fr = 0; fr < filter_rows; fr++)
            {
                for (size_t fc = 0; fc < filter_cols; fc++)
                {
                    sum += image[idx(r + fr, c + fc, image_cols)] *
                           filter[idx(fr, fc, filter_cols)];
                }
            }
            output[idx(r, c, out_cols)] = sum;
        }
    }
}

// Заполняем матрицу детерминированными псевдослучайными значениями из диапазона [-1, 1].
static void init_matrix(float *matrix, size_t rows, size_t cols, unsigned int seed)
{
    srand(seed);
    for (size_t i = 0; i < rows * cols; i++)
    {
        double value = (double)rand() / (double)RAND_MAX;
        matrix[i] = (float)(value * 2.0 - 1.0);
    }
}

static int compare_outputs(const float *a, const float *b, size_t len)
{
    const float eps = 1e-4f;
    for (size_t i = 0; i < len; i++)
    {
        float diff = a[i] - b[i];
        if (diff < -eps || diff > eps)
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    // Размеры тестового изображения и фильтра.
    const size_t image_rows = 256;
    const size_t image_cols = 256;
    const size_t filter_rows = 5;
    const size_t filter_cols = 5;

    const size_t out_rows = image_rows - filter_rows + 1;
    const size_t out_cols = image_cols - filter_cols + 1;
    const size_t out_len = out_rows * out_cols;

    float *image = malloc(image_rows * image_cols * sizeof(float));
    float *filter = malloc(filter_rows * filter_cols * sizeof(float));
    float *out_baseline = malloc(out_len * sizeof(float));
    float *out_simd = malloc(out_len * sizeof(float));

    if (image == NULL || filter == NULL || out_baseline == NULL || out_simd == NULL)
    {
        fprintf(stderr, "allocation failed\n");
        free(image);
        free(filter);
        free(out_baseline);
        free(out_simd);
        return EXIT_FAILURE;
    }

    init_matrix(image, image_rows, image_cols, 1);
    init_matrix(filter, filter_rows, filter_cols, 2);

    conv2d_scalar(image, image_rows, image_cols, filter, filter_rows, filter_cols, out_baseline);
    conv2d_avx2(image, image_rows, image_cols, filter, filter_rows, filter_cols, out_simd);

    if (!compare_outputs(out_baseline, out_simd, out_len))
    {
        fprintf(stderr, "mismatch between scalar and vector outputs\n");
        free(image);
        free(filter);
        free(out_baseline);
        free(out_simd);
        return EXIT_FAILURE;
    }

    BENCH("scalar",
          conv2d_scalar(image, image_rows, image_cols, filter, filter_rows, filter_cols, out_baseline),
          100);
    BENCH("vector",
          conv2d_avx2(image, image_rows, image_cols, filter, filter_rows, filter_cols, out_simd),
          100);

    free(image);
    free(filter);
    free(out_baseline);
    free(out_simd);
    return EXIT_SUCCESS;
}
