#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "tasks.h"
#include "../core/util.h"

struct matmul_ctx {
    float *a;
    float *b;
    float *c;
    size_t n;
};

static void fill_matrix(float *data, size_t n, unsigned int seed)
{
    srand(seed);
    for (size_t i = 0; i < n * n; i++)
        data[i] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

static void matmul_scalar(const float *a, const float *b, float *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < n; k++)
                sum += a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    }
}

static void matmul_row(void *arg)
{
    struct iter_data *iter = arg;
    struct matmul_ctx *ctx = iter->ctx;
    size_t i = (size_t)iter->i;
    size_t n = ctx->n;

    for (size_t j = 0; j < n; j++) {
        float sum = 0.0f;
        for (size_t k = 0; k < n; k++)
            sum += ctx->a[i * n + k] * ctx->b[k * n + j];
        ctx->c[i * n + j] = sum;
    }
}

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

int main(void)
{
    size_t sizes[] = {200, 400, 600};
    size_t workers[] = {1, 2, 4, 8};
    size_t size_count = sizeof(sizes) / sizeof(sizes[0]);
    size_t worker_count = sizeof(workers) / sizeof(workers[0]);

    for (size_t si = 0; si < size_count; si++) {
        size_t n = sizes[si];
        float *a = newarr(float, n * n);
        float *b = newarr(float, n * n);
        float *c = newarr(float, n * n);

        fill_matrix(a, n, (unsigned int)(n + 1));
        fill_matrix(b, n, (unsigned int)(n + 2));

        double start = get_time_ms();
        matmul_scalar(a, b, c, n);
        double t1 = get_time_ms() - start;

        struct matmul_ctx ctx = { a, b, c, n };

        for (size_t wi = 0; wi < worker_count; wi++) {
            size_t w = workers[wi];
            start = get_time_ms();
            parallel_for_n(0, 1, (int)n, matmul_row, &ctx, w);
            double tw = get_time_ms() - start;
            double speedup = t1 / tw;
            printf("%zu %zu %.4f\n", n, w, speedup);
        }

        free(a);
        free(b);
        free(c);
    }

    return 0;
}
