#ifndef _CONV_UTIL_H
#define _CONV_UTIL_H

#include <stdio.h>
#include <jpeglib.h>

struct conv_image
{
    unsigned int width;
    unsigned int height;
    unsigned char *r;
    unsigned char *g;
    unsigned char *b;
};

void conv_image_init(struct conv_image *conv_image);
void conv_image_free(struct conv_image *conv_image);

void read_jpeg(const char *filename, struct conv_image *conv_image);
void save_jpeg(const char *filename, struct conv_image *conv_image, int quality);

struct conv_mat
{
    unsigned int rows;
    unsigned int cols;
    float *values;
};

void conv_mat_init(struct conv_mat *conv_mat);
void conv_mat_free(struct conv_mat *conv_mat);

void read_conv_mat(const char *filename, struct conv_mat *conv_mat);

#endif // _CONV_UTIL_H
