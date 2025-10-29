#define _POSIX_C_SOURCE 200809L

#include "conv_util.h"

#include <stdlib.h>
#include "../core/util.h"

void conv_image_init(struct conv_image *conv_image)
{
    conv_image->width = 0;
    conv_image->height = 0;
    conv_image->r = nullptr;
    conv_image->g = nullptr;
    conv_image->b = nullptr;
}

void conv_image_free(struct conv_image *conv_image)
{
    conv_image->width = 0;
    conv_image->height = 0;
    free(conv_image->r);
    free(conv_image->g);
    free(conv_image->b);
}

void read_jpeg(const char *filename, struct conv_image *conv_image)
{
    FILE *file = fopen(filename, "rb");
    if (file == nullptr)
    {
        perror("error opening jpeg file");
        exit(EXIT_FAILURE);
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    conv_image->width = cinfo.output_width;
    conv_image->height = cinfo.output_height;
    int channels = cinfo.output_components;

    if (channels != 3)
    {
        fprintf(stderr, "error: jpeg file is not in rgb format.\n");
        exit(EXIT_FAILURE);
    }

    size_t image_size = conv_image->width * conv_image->height;
    conv_image->r = newarr(unsigned char, image_size);
    conv_image->g = newarr(unsigned char, image_size);
    conv_image->b = newarr(unsigned char, image_size);

    unsigned char *row = newarr(unsigned char, channels * conv_image->width);

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, &row, 1);

        for (int x = 0; x < conv_image->width; x++)
        {
            conv_image->r[(cinfo.output_scanline - 1) * conv_image->width + x] = row[x * channels];
            conv_image->g[(cinfo.output_scanline - 1) * conv_image->width + x] = row[x * channels + 1];
            conv_image->b[(cinfo.output_scanline - 1) * conv_image->width + x] = row[x * channels + 2];
        }
    }

    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);
}

void save_jpeg(const char *filename, struct conv_image *conv_image, int quality)
{
    FILE *file = fopen(filename, "wb");
    if (file == nullptr)
    {
        perror("error opening jpeg file");
        exit(EXIT_FAILURE);
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_stdio_dest(&cinfo, file);

    const int channels = 3;

    cinfo.image_width = conv_image->width;
    cinfo.image_height = conv_image->height;
    cinfo.input_components = channels;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    unsigned char *row = newarr(unsigned char, channels * conv_image->width);

    while (cinfo.next_scanline < cinfo.image_height)
    {
        for (int x = 0; x < conv_image->width; x++)
        {
            row[x * channels] = conv_image->r[cinfo.next_scanline * conv_image->width + x];
            row[x * channels + 1] = conv_image->g[cinfo.next_scanline * conv_image->width + x];
            row[x * channels + 2] = conv_image->b[cinfo.next_scanline * conv_image->width + x];
        }

        jpeg_write_scanlines(&cinfo, &row, 1);
    }

    free(row);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(file);
}

void conv_mat_init(struct conv_mat *conv_mat)
{
    conv_mat->rows = 0;
    conv_mat->cols = 0;
    conv_mat->values = nullptr;
}

void conv_mat_free(struct conv_mat *conv_mat)
{
    conv_mat->rows = 0;
    conv_mat->cols = 0;
    free(conv_mat->values);
}

void read_conv_mat(const char *filename, struct conv_mat *conv_mat)
{
    FILE *file = fopen(filename, "r");
    if (file == nullptr)
    {
        perror("failed to open file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%u %u", &conv_mat->rows, &conv_mat->cols);

    conv_mat->values = newarr(float, conv_mat->rows * conv_mat->cols);

    for (int i = 0; i < conv_mat->rows; i++)
    {
        for (int j = 0; j < conv_mat->cols; j++)
        {
            fscanf(file, "%f", &conv_mat->values[i * conv_mat->cols + j]);
        }
    }

    fclose(file);
}
