#include <stdio.h>
#include <immintrin.h>
#include <jpeglib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "conv_util.h"
#include "../core/bench.h"

static void fill(float *data, size_t count, unsigned int seed)
{
    srand(seed);
    for (size_t i = 0; i < count; i++)
        data[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

static void conv_scalar(const float *src, size_t h, size_t w, const float *kernel, size_t kh, size_t kw, float *dst)
{
    size_t oh = h - kh + 1;
    size_t ow = w - kw + 1;
    for (size_t i = 0; i < oh; i++)
        for (size_t j = 0; j < ow; j++)
        {
            float sum = 0.0f;
            for (size_t ki = 0; ki < kh; ki++)
                for (size_t kj = 0; kj < kw; kj++)
                    sum += src[(i + ki) * w + (j + kj)] * kernel[ki * kw + kj];
            dst[i * ow + j] = sum;
        }
}

static inline __m256 fmadd(__m256 a, __m256 b, __m256 acc)
{
    return _mm256_add_ps(_mm256_mul_ps(a, b), acc);
}

static void conv_avx(const float *src, size_t h, size_t w, const float *kernel, size_t kh, size_t kw, float *dst)
{
    size_t oh = h - kh + 1;
    size_t ow = w - kw + 1;
    for (size_t i = 0; i < oh; i++)
    {
        size_t j = 0;
        for (; j + 8 <= ow; j += 8)
        {
            __m256 acc = _mm256_setzero_ps();
            for (size_t ki = 0; ki < kh; ki++)
            {
                size_t base = (i + ki) * w + j;
                for (size_t kj = 0; kj < kw; kj++)
                {
                    __m256 pixels = _mm256_loadu_ps(src + base + kj);
                    __m256 kval = _mm256_set1_ps(kernel[ki * kw + kj]);
                    acc = fmadd(pixels, kval, acc);
                }
            }
            _mm256_storeu_ps(dst + i * ow + j, acc);
        }
        for (; j < ow; j++)
        {
            float sum = 0.0f;
            for (size_t ki = 0; ki < kh; ki++)
                for (size_t kj = 0; kj < kw; kj++)
                    sum += src[(i + ki) * w + (j + kj)] * kernel[ki * kw + kj];
            dst[i * ow + j] = sum;
        }
    }
}

static int similar(const float *a, const float *b, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (fabsf(a[i] - b[i]) > 1e-3f)
            return 0;
    return 1;
}

static void conv_scalar_run(const float *src, size_t h, size_t w, const float *kernel, size_t kh, size_t kw, float *dst)
{
    conv_scalar(src, h, w, kernel, kh, kw, dst);
}

static void conv_avx_run(const float *src, size_t h, size_t w, const float *kernel, size_t kh, size_t kw, float *dst)
{
    conv_avx(src, h, w, kernel, kh, kw, dst);
}

static float clip(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

int main(void)
{
    size_t sizes[] = {256, 512, 768, 1024, 1280, 1536, 1792, 2048};
    size_t repeats = 5;
    float kernel[] = {
        0.0625f, 0.1250f, 0.0625f,
        0.1250f, 0.2500f, 0.1250f,
        0.0625f, 0.1250f, 0.0625f};
    for (size_t n = 0; n < sizeof(sizes) / sizeof(sizes[0]); n++)
    {
        size_t side = sizes[n];
        size_t total = side * side;
        size_t out_side = side - 2;
        size_t out_total = out_side * out_side;
        float *src = malloc(total * sizeof(float));
        float *dst_s = malloc(out_total * sizeof(float));
        float *dst_v = malloc(out_total * sizeof(float));
        fill(src, total, (unsigned int)(side + 1));
        BENCH(side, repeats, conv_scalar_run(src, side, side, kernel, 3, 3, dst_s),
              conv_avx_run(src, side, side, kernel, 3, 3, dst_v));
        if (!similar(dst_s, dst_v, out_total))
            fprintf(stderr, "mismatch %zu\n", side);
        free(src);
        free(dst_s);
        free(dst_v);
    }
    struct conv_image input;
    conv_image_init(&input);
    read_jpeg("input.jpg", &input);
    size_t ih = input.height;
    size_t iw = input.width;
    size_t total = ih * iw;
    float *image = malloc(total * sizeof(float));
    for (size_t idx = 0; idx < total; idx++)
        image[idx] = (input.r[idx] + input.g[idx] + input.b[idx]) / (3.0f * 255.0f);
    size_t oh = ih - 2;
    size_t ow = iw - 2;
    float *dst_s = malloc(oh * ow * sizeof(float));
    float *dst_v = malloc(oh * ow * sizeof(float));
    conv_scalar(image, ih, iw, kernel, 3, 3, dst_s);
    conv_avx(image, ih, iw, kernel, 3, 3, dst_v);
    struct conv_image output;
    conv_image_init(&output);
    output.width = (unsigned int)ow;
    output.height = (unsigned int)oh;
    size_t out_total = ow * oh;
    output.r = malloc(out_total);
    output.g = malloc(out_total);
    output.b = malloc(out_total);
    for (size_t idx = 0; idx < out_total; idx++)
    {
        unsigned char v = (unsigned char)lrintf(clip(dst_v[idx]) * 255.0f);
        output.r[idx] = v;
        output.g[idx] = v;
        output.b[idx] = v;
    }
    save_jpeg("output.jpg", &output, 90);
    conv_image_free(&input);
    conv_image_free(&output);
    free(image);
    free(dst_s);
    free(dst_v);
    return 0;
}
