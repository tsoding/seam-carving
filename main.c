#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>

#include "stb_image.h"
#include "stb_image_write.h"
#define NOB_IMPLEMENTATION
#include "nob.h"

#define GENERATE_GIF

#ifdef GENERATE_GIF
#include "gifenc.h"
#endif

typedef struct {
    uint8_t *items;
    size_t count;
    size_t capacity;
} GifPixels;

typedef struct {
    uint32_t *pixels;
    int width, height, stride;
} Img;

#define IMG_AT(img, row, col) (img).pixels[(row)*(img).stride + (col)]

typedef struct {
    float *items;
    int width, height, stride;
} Mat;

#define MAT_AT(mat, row, col) (mat).items[(row)*(mat).stride + (col)]
#define MAT_WITHIN(mat, row, col) \
    (0 <= (col) && (col) < (mat).width && 0 <= (row) && (row) < (mat).height)

static Mat mat_alloc(int width, int height)
{
    Mat mat = {0};
    mat.items = malloc(sizeof(float)*width*height);
    assert(mat.items != NULL);
    mat.width = width;
    mat.height = height;
    mat.stride = width;
    return mat;
}

// https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
static float rgb_to_lum(uint32_t rgb)
{
    float r = ((rgb >> (8*0)) & 0xFF)/255.0;
    float g = ((rgb >> (8*1)) & 0xFF)/255.0;
    float b = ((rgb >> (8*2)) & 0xFF)/255.0;
    return 0.2126*r + 0.7152*g + 0.0722*b;
}

static void luminance(Img img, Mat lum)
{
    assert(img.width == lum.width);
    assert(img.height == lum.height);
    for (int y = 0; y < lum.height; ++y) {
        for (int x = 0; x < lum.width; ++x) {
            MAT_AT(lum, y, x) = rgb_to_lum(IMG_AT(img, y, x));
        }
    }
}

static float sobel_filter_at(Mat mat, int cx, int cy)
{
    static float gx[3][3] = {
        {1.0, 0.0, -1.0},
        {2.0, 0.0, -2.0},
        {1.0, 0.0, -1.0},
    };

    static float gy[3][3] = {
        {1.0, 2.0, 1.0},
        {0.0, 0.0, 0.0},
        {-1.0, -2.0, -1.0},
    };

    float sx = 0.0;
    float sy = 0.0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            float c = MAT_WITHIN(mat, y, x) ? MAT_AT(mat, y, x) : 0.0;
            sx += c*gx[dy + 1][dx + 1];
            sy += c*gy[dy + 1][dx + 1];
        }
    }
    // NOTE: Apparently sqrtf does not make that much difference perceptually.
    // Yet it is slightly faster without it.
    //return sqrtf(sx*sx + sy*sy);
    return sx*sx + sy*sy;
}

static void sobel_filter(Mat mat, Mat grad)
{
    assert(mat.width == grad.width);
    assert(mat.height == grad.height);

    for (int cy = 0; cy < mat.height; ++cy) {
        for (int cx = 0; cx < mat.width; ++cx) {
            MAT_AT(grad, cy, cx) = sobel_filter_at(mat, cx, cy);
        }
    }
}

static void grad_to_dp(Mat grad, Mat dp)
{
    assert(grad.width == dp.width);
    assert(grad.height == dp.height);

    for (int x = 0; x < grad.width; ++x) {
        MAT_AT(dp, 0, x) = MAT_AT(grad, 0, x);
    }
    for (int y = 1; y < grad.height; ++y) {
        for (int cx = 0; cx < grad.width; ++cx) {
            float m = FLT_MAX;
            for (int dx = -1; dx <= 1; ++dx) {
                int x = cx + dx;
                float value = 0 <= x && x < grad.width ? MAT_AT(dp, y - 1, x) : FLT_MAX;
                if (value < m) m = value;
            }
            MAT_AT(dp, y, cx) = MAT_AT(grad, y, cx) + m;
        }
    }
}

static long dist2(long r1, long r2, long g1, long g2, long b1, long b2)
{
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s <input> <output>\n", program);
}

static void img_remove_column_at_row(Img img, int row, int column)
{
    uint32_t *pixel_row = &IMG_AT(img, row, 0);
    memmove(pixel_row + column, pixel_row + column + 1, (img.width - column - 1)*sizeof(uint32_t));
}

static void mat_remove_column_at_row(Mat mat, int row, int column)
{
    float *pixel_row = &MAT_AT(mat, row, 0);
    memmove(pixel_row + column, pixel_row + column + 1, (mat.width - column - 1)*sizeof(float));
}

static void compute_seam(Mat dp, int *seam)
{
    int y = dp.height - 1;
    seam[y] = 0;
    for (int x = 1; x < dp.width; ++x) {
        if (MAT_AT(dp, y, x) < MAT_AT(dp, y, seam[y])) {
            seam[y] = x;
        }
    }

    for (y = dp.height - 2; y >= 0; --y) {
        seam[y] = seam[y+1];
        for (int dx = -1; dx <= 1; ++dx) {
            int x = seam[y+1] + dx;
            if (0 <= x && x < dp.width && MAT_AT(dp, y, x) < MAT_AT(dp, y, seam[y])) {
                seam[y] = x;
            }
        }
    }
}

void markout_sobel_patches(Mat grad, int *seam)
{
    for (int cy = 0; cy < grad.height; ++cy) {
        int cx = seam[cy];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int x = cx + dx;
                int y = cy + dy;
                if (MAT_WITHIN(grad, y, x)) {
                    *(uint32_t*)&MAT_AT(grad, y, x) = 0xFFFFFFFF;
                }
            }
        }
    }
}

static void create_gif_palette(Img src, uint8_t *palette, int palette_size)
{
    // black - used for background
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;

    // red - used for seam
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;

    for (int i = 2; i < palette_size; i++) {
        int h = rand() % src.height;
        int w = rand() % src.width;

        uint32_t pixel = IMG_AT(src, h, w);

        palette[i * 3 + 0] = (pixel >> 0) & 0xFF;
        palette[i * 3 + 1] = (pixel >> 8) & 0xFF;
        palette[i * 3 + 2] = (pixel >> 16) & 0xFF;
    }

    GifPixels pixels[palette_size];

    for (int i = 0; i < palette_size; i++) {
        pixels[i].count = 0;
        pixels[i].capacity = 0;
        pixels[i].items = NULL;
    }

    // 10 is hard-coded number of iterations of K-Means
    for (int i = 0; i < 10; i++) {
        for (int y = 0; y < src.height; y++) {
            for (int x = 0; x < src.width; x++) {
                uint32_t pixel = IMG_AT(src, y, x);

                int r = (pixel >> 0) & 0xFF;
                int g = (pixel >> 8) & 0xFF;
                int b = (pixel >> 16) & 0xFF;

                int palette_idx = 0;
                for (int j = 1; j < palette_size; j++) {
                    if (dist2(r, palette[j * 3 + 0], g, palette[j * 3 + 1], b, palette[j * 3 + 2]) <
                        dist2(r, palette[palette_idx * 3 + 0], g, palette[palette_idx * 3 + 1], b, palette[palette_idx * 3 + 2])) {
                        palette_idx = j;
                    }
                }

                uint8_t new_vals[3] = {r, g, b};
                nob_da_append_many(pixels + palette_idx, new_vals, 3);
            }
        }

        for (int j = 0; j < palette_size; j++) {
            float r_sum = 0;
            float g_sum = 0;
            float b_sum = 0;

            for (size_t k = 0; k < pixels[j].count; k += 3) {
                r_sum += pixels[j].items[k + 0];
                g_sum += pixels[j].items[k + 1];
                b_sum += pixels[j].items[k + 2];
            }

            if (j != 0 && j != 1 && pixels[j].count != 0) {
                palette[j * 3 + 0] = r_sum / (pixels[j].count / 3);
                palette[j * 3 + 1] = g_sum / (pixels[j].count / 3);
                palette[j * 3 + 2] = b_sum / (pixels[j].count / 3);
            }

            pixels[j].count = 0;
        }
    }

    for (int i = 0; i < palette_size; i++) {
        nob_da_free(pixels[i]);
    }
}

static void add_frame(ge_GIF *gif, Img img, uint8_t *palette, int palette_size, int *seam)
{
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            uint32_t pixel = IMG_AT(img, y, x);

            int r = (pixel >> 0) & 0xFF;
            int g = (pixel >> 8) & 0xFF;
            int b = (pixel >> 16) & 0xFF;

            int palette_idx = 0;
            for (int i = 1; i < palette_size; i++) {
                if (dist2(r, palette[i * 3 + 0], g, palette[i * 3 + 1], b, palette[i * 3 + 2]) <
                    dist2(r, palette[palette_idx * 3 + 0], g, palette[palette_idx * 3 + 1], b, palette[palette_idx * 3 + 2])) {
                    palette_idx = i;
                }
            }

            gif->frame[y * img.stride + x] = palette_idx;
        }

        for (int x = img.width; x < img.stride; ++x) {
            gif->frame[y * img.stride + x] = 0;
        }
    }

    for (int y = 0; y < img.height; ++y) {
        gif->frame[y * img.stride + seam[y]] = 1;
    }

    ge_add_frame(gif, 10);
}

int main(int argc, char **argv)
{
    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        usage(program);
        fprintf(stderr, "ERROR: no input file is provided\n");
        return 1;
    }
    const char *file_path = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        usage(program);
        fprintf(stderr, "ERROR: no output file is provided\n");
        return 1;
    }
    const char *out_file_path = nob_shift_args(&argc, &argv);

    int width_, height_;
    uint32_t *pixels_ = (uint32_t*)stbi_load(file_path, &width_, &height_, NULL, 4);
    if (pixels_ == NULL) {
        fprintf(stderr, "ERROR: could not read %s\n", file_path);
        return 1;
    }
    Img img = {
        .pixels = pixels_,
        .width = width_,
        .height = height_,
        .stride = width_,
    };

    Mat lum = mat_alloc(width_, height_);
    Mat grad = mat_alloc(width_, height_);
    Mat dp = mat_alloc(width_, height_);
    int *seam = malloc(sizeof(*seam)*height_);

    int seams_to_remove = img.width * 2 / 3;

    luminance(img, lum);
    sobel_filter(lum, grad);

#ifdef GENERATE_GIF
    int palette_size = 64;
    uint8_t *palette = (uint8_t *)malloc(3 * palette_size * sizeof(uint8_t));
    create_gif_palette(img, palette, palette_size);

    ge_GIF *gif = ge_new_gif(
        "images/seams.gif",
        width_,
        height_,
        palette,
        (int)ceil(log2(palette_size)),
        -1,
        0);
#endif

    for (int i = 0; i < seams_to_remove; ++i) {
        grad_to_dp(grad, dp);
        compute_seam(dp, seam);

#ifdef GENERATE_GIF
        add_frame(gif, img, palette, palette_size, seam);
#endif

        markout_sobel_patches(grad, seam);

        for (int cy = 0; cy < img.height; ++cy) {
            int cx = seam[cy];
            img_remove_column_at_row(img, cy, cx);
            mat_remove_column_at_row(lum, cy, cx);
            mat_remove_column_at_row(grad, cy, cx);
        }

        img.width -= 1;
        lum.width -= 1;
        grad.width -= 1;
        dp.width -= 1;

        for (int cy = 0; cy < grad.height; ++cy) {
            for (int cx = seam[cy]; cx < grad.width && *(uint32_t*)&MAT_AT(grad, cy, cx) == 0xFFFFFFFF; ++cx) {
                MAT_AT(grad, cy, cx) = sobel_filter_at(lum, cx, cy);
            }
            for (int cx = seam[cy] - 1; cx >= 0 && *(uint32_t*)&MAT_AT(grad, cy, cx) == 0xFFFFFFFF; --cx) {
                MAT_AT(grad, cy, cx) = sobel_filter_at(lum, cx, cy);
            }
        }
    }

#ifdef GENERATE_GIF
    ge_close_gif(gif);

    free(palette);
#endif

    if (!stbi_write_png(out_file_path, img.width, img.height, 4, img.pixels, img.stride*sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file %s\n", out_file_path);
        return 1;
    }
    printf("OK: generated %s\n", out_file_path);

    return 0;
}
