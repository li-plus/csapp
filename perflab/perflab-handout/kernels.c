/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "bovik",              /* Team name */

    "Harry Q. Bovik",     /* First member full name */
    "bovik@nowhere.edu",  /* First member email address */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) 
{
    const int block_size = 8;
    for (int bi = 0; bi < dim; bi += block_size) {
        for (int bj = 0; bj < dim; bj += block_size) {
            for (int i = bi; i < bi + block_size; i++) {
                for (int j = bj; j < bj + block_size; j++) {
	                dst[RIDX(dim - 1 - j, i, dim)] = src[RIDX(i, j, dim)];
                }
            }
        }
    }
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

typedef struct {
    unsigned red;
    unsigned green;
    unsigned blue;
} pixel32_t;

static void pixel32_init(pixel32_t *pixel32) {
    pixel32->red = pixel32->green = pixel32->blue = 0;
}

static void pixel32_add_pixel(pixel32_t *self, const pixel *other) {
    self->red += other->red;
    self->green += other->green;
    self->blue += other->blue;
}

static void pixel32_add(pixel32_t *self, const pixel32_t *other) {
    self->red += other->red;
    self->green += other->green;
    self->blue += other->blue;
}

static void pixel32_sub(pixel32_t *self, const pixel32_t *other) {
    self->red -= other->red;
    self->green -= other->green;
    self->blue -= other->blue;
}

static void pixel32_to_pixel(const pixel32_t *self, pixel *other, unsigned scale) {
    other->red = self->red / scale;
    other->green = self->green / scale;
    other->blue = self->blue / scale;
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) 
{
    pixel32_t sum, left, mid, right;
    pixel32_init(&sum);
    pixel32_init(&left);
    pixel32_init(&mid);

    // top left corner
    pixel32_add_pixel(&left, &src[RIDX(0, 0, dim)]);
    pixel32_add_pixel(&mid, &src[RIDX(0, 1, dim)]);
    pixel32_add_pixel(&left, &src[RIDX(1, 0, dim)]);
    pixel32_add_pixel(&mid, &src[RIDX(1, 1, dim)]);
    pixel32_add(&sum, &left);
    pixel32_add(&sum, &mid);
    pixel32_to_pixel(&sum, &dst[RIDX(0, 0, dim)], 4);

    // top edge
    for (int j = 1; j < dim - 1; j++) {
        pixel32_init(&right);
        pixel32_add_pixel(&right, &src[RIDX(0, j + 1, dim)]);
        pixel32_add_pixel(&right, &src[RIDX(1, j + 1, dim)]);
        pixel32_add(&sum, &right);
        pixel32_to_pixel(&sum, &dst[RIDX(0, j, dim)], 6);
        pixel32_sub(&sum, &left);
        left = mid;
        mid = right;
    }

    // top right corner
    pixel32_to_pixel(&sum, &dst[RIDX(0, dim - 1, dim)], 4);

    for (int i = 1; i < dim - 1; i++) {
        pixel32_init(&sum);
        pixel32_init(&left);
        pixel32_init(&mid);

        // left edge
        pixel32_add_pixel(&left, &src[RIDX(i - 1, 0, dim)]);
        pixel32_add_pixel(&mid, &src[RIDX(i - 1, 1, dim)]);
        pixel32_add_pixel(&left, &src[RIDX(i, 0, dim)]);
        pixel32_add_pixel(&mid, &src[RIDX(i, 1, dim)]);
        pixel32_add_pixel(&left, &src[RIDX(i + 1, 0, dim)]);
        pixel32_add_pixel(&mid, &src[RIDX(i + 1, 1, dim)]);
        pixel32_add(&sum, &left);
        pixel32_add(&sum, &mid);
        pixel32_to_pixel(&sum, &dst[RIDX(i, 0, dim)], 6);

        // middle pixels
        for (int j = 1; j < dim - 1; j++) {
            pixel32_init(&right);
            pixel32_add_pixel(&right, &src[RIDX(i - 1, j + 1, dim)]);
            pixel32_add_pixel(&right, &src[RIDX(i, j + 1, dim)]);
            pixel32_add_pixel(&right, &src[RIDX(i + 1, j + 1, dim)]);
            pixel32_add(&sum, &right);
            pixel32_to_pixel(&sum, &dst[RIDX(i, j, dim)], 9);
            pixel32_sub(&sum, &left);
            left = mid;
            mid = right;
        }

        // right edge
        pixel32_to_pixel(&sum, &dst[RIDX(i, dim - 1, dim)], 6);
    }

    pixel32_init(&sum);
    pixel32_init(&left);
    pixel32_init(&mid);

    // bottom left corner
    pixel32_add_pixel(&left, &src[RIDX(dim - 2, 0, dim)]);
    pixel32_add_pixel(&mid, &src[RIDX(dim - 2, 1, dim)]);
    pixel32_add_pixel(&left, &src[RIDX(dim - 1, 0, dim)]);
    pixel32_add_pixel(&mid, &src[RIDX(dim - 1, 1, dim)]);
    pixel32_add(&sum, &left);
    pixel32_add(&sum, &mid);
    pixel32_to_pixel(&sum, &dst[RIDX(dim - 1, 0, dim)], 4);

    // bottom edge
    for (int j = 1; j < dim - 1; j++) {
        pixel32_init(&right);
        pixel32_add_pixel(&right, &src[RIDX(dim - 2, j + 1, dim)]);
        pixel32_add_pixel(&right, &src[RIDX(dim - 1, j + 1, dim)]);
        pixel32_add(&sum, &right);
        pixel32_to_pixel(&sum, &dst[RIDX(dim - 1, j, dim)], 6);
        pixel32_sub(&sum, &left);
        left = mid;
        mid = right;
    }

    // bottom right corner
    pixel32_to_pixel(&sum, &dst[RIDX(dim - 1, dim - 1, dim)], 4);
}

/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}

