#ifndef BENCH_H
#define BENCH_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#define BENCH(label, iter_count, expr_scalar, expr_vector)                    \
    do                                                                        \
    {                                                                         \
        size_t bench_iter = (iter_count);                                     \
        struct timespec bench_start, bench_end;                               \
        clock_gettime(CLOCK_MONOTONIC, &bench_start);                         \
        for (size_t bench_i = 0; bench_i < bench_iter; bench_i++)             \
        {                                                                     \
            expr_scalar;                                                      \
        }                                                                     \
        clock_gettime(CLOCK_MONOTONIC, &bench_end);                           \
        long bench_sec = bench_end.tv_sec - bench_start.tv_sec;               \
        long bench_nsec = bench_end.tv_nsec - bench_start.tv_nsec;            \
        if (bench_nsec < 0)                                                   \
        {                                                                     \
            bench_nsec += 1000000000L;                                        \
            bench_sec -= 1;                                                   \
        }                                                                     \
        double bench_scalar_ns = 1.0e9 * (double)bench_sec + (double)bench_nsec; \
        bench_scalar_ns /= (double)bench_iter;                                \
        clock_gettime(CLOCK_MONOTONIC, &bench_start);                         \
        for (size_t bench_j = 0; bench_j < bench_iter; bench_j++)             \
        {                                                                     \
            expr_vector;                                                      \
        }                                                                     \
        clock_gettime(CLOCK_MONOTONIC, &bench_end);                           \
        bench_sec = bench_end.tv_sec - bench_start.tv_sec;                    \
        bench_nsec = bench_end.tv_nsec - bench_start.tv_nsec;                 \
        if (bench_nsec < 0)                                                   \
        {                                                                     \
            bench_nsec += 1000000000L;                                        \
            bench_sec -= 1;                                                   \
        }                                                                     \
        double bench_vector_ns = 1.0e9 * (double)bench_sec + (double)bench_nsec; \
        bench_vector_ns /= (double)bench_iter;                                \
        printf("%zu %.6f %.6f\n", (size_t)(label),                            \
               bench_scalar_ns / 1.0e6, bench_vector_ns / 1.0e6);             \
    } while (false)

#endif
