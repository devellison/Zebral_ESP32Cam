#include "zba_imgproc.h"
#include <stdint.h>

#include "zba_util.h"

void zba_imgproc_rgb565_to_gray(uint16_t* input, size_t width, size_t height, uint8_t* output)
{
  uint8_t* dst  = output;
  uint16_t* src = input;

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      uint16_t pixel = *src;
      uint16_t gray =
          (uint16_t)(0.299 * RGB565_R(pixel) + 0.587 * RGB565_G(pixel) + 0.114 * RGB565_B(pixel));
      *dst = (uint8_t)gray;
      ++dst;
      ++src;
    }
  }
}

// clang-format off
void zba_imgproc_mean_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  static int8_t kernel[9] = {1, 1, 1, 
                             1, 1, 1, 
                             1, 1, 1};
  return zba_imgproc_convolve3x3_rgb565(input, width, height, output, kernel, 9, POST_DIV);
}

void zba_imgproc_gaussian_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  static int8_t kernel[9] = {1, 2, 1, 
                             2, 4, 2, 
                             1, 2, 1};
  return zba_imgproc_convolve3x3_rgb565(input, width, height, output, kernel, 16, POST_DIV);
}

void zba_imgproc_edgex_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  static int8_t kernel[9] = {-1,  0,  1, 
                             -2,  0,  2, 
                             -1,  0,  1};
  return zba_imgproc_convolve3x3_rgb565(input, width, height, output, kernel, 1, POST_SATURATE);
}

void zba_imgproc_edgey_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  static int8_t kernel[9] = {-1, -2, -1, 
                              0,  0,  0, 
                              1,  2,  1};
  return zba_imgproc_convolve3x3_rgb565(input, width, height, output, kernel, 1, POST_SATURATE);
}
// clang-format on
void zba_imgproc_convolve3x3_gray(uint8_t* input, size_t width, size_t height, uint8_t* output,
                                  int8_t* kernel, int8_t divisor, zba_convolve_flags_t flags)
{
  uint8_t* dst = output;
  uint8_t* src = input;

  if ((divisor == 0) || (!(flags & POST_DIV))) divisor = 1;

  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 1; x < width - 1; ++x)
    {
      int32_t accum = 0;

      for (int i = -1; i < 2; ++i)
      {
        int krow = (i + 1) * 3;
        int irow = (y + i) * width;
        for (int j = -1; j < 2; ++j)
        {
          accum += src[irow + (x + j)] * kernel[krow + j + 1];
        }
      }

      if (flags & POST_ABS) accum = abs(accum);
      if (flags & POST_DIV) accum = (accum + divisor / 2) / divisor;
      if (flags & POST_SATURATE) accum = ZBA_CLAMP(0, 255, accum);

      *dst = accum;
      dst++;
    }
  }
}

void zba_imgproc_convolve3x3_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output,
                                    int8_t* kernel, int8_t divisor, zba_convolve_flags_t flags)
{
  uint16_t* dst = output;
  uint16_t* src = input;

  if ((divisor == 0) || (!(flags & POST_DIV))) divisor = 1;

  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 1; x < width - 1; ++x)
    {
      int32_t accum[3] = {0};

      for (int i = -1; i < 2; ++i)
      {
        int krow = (i + 1) * 3;
        int irow = (y + i) * width;
        for (int j = -1; j < 2; ++j)
        {
          int8_t k       = kernel[krow + j + 1];
          uint16_t pixel = src[irow + (x + j)];
          accum[0] += ((int32_t)RGB565_R(pixel)) * k;
          accum[1] += ((int32_t)RGB565_G(pixel)) * k;
          accum[2] += ((int32_t)RGB565_B(pixel)) * k;
        }
      }
      if (flags & POST_ABS)
      {
        accum[0] = abs(accum[0]);
        accum[1] = abs(accum[1]);
        accum[2] = abs(accum[2]);
      }
      if (flags & POST_DIV)
      {
        accum[0] = (accum[0] + divisor / 2) / divisor;
        accum[1] = (accum[1] + divisor / 2) / divisor;
        accum[2] = (accum[2] + divisor / 2) / divisor;
      }
      if (flags & POST_SATURATE)
      {
        accum[0] = ZBA_CLAMP(0, 255, accum[0]);
        accum[1] = ZBA_CLAMP(0, 255, accum[1]);
        accum[2] = ZBA_CLAMP(0, 255, accum[2]);
      }
      *dst = RGB565(accum[0], accum[1], accum[2]);
      dst++;
    }
  }
}

void zba_imgproc_dilate_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  uint16_t* dst = output;
  uint16_t* src = input;

  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 1; x < width - 1; ++x)
    {
      uint8_t vals_a[3];
      uint16_t pixel = src[y * width + x];
      vals_a[0]      = RGB565_R(pixel);
      vals_a[1]      = RGB565_G(pixel);
      vals_a[2]      = RGB565_B(pixel);

      for (int i = -1; i < 2; ++i)
      {
        int icol = (y + i) * width;
        for (int j = -1; j < 2; ++j)
        {
          if (j == 0 && i == 0) continue;

          pixel = src[icol + (x + j)];

          vals_a[0] = ZBA_MAX_BYTE(vals_a[0], RGB565_R(pixel));
          vals_a[1] = ZBA_MAX_BYTE(vals_a[1], RGB565_R(pixel));
          vals_a[2] = ZBA_MAX_BYTE(vals_a[2], RGB565_R(pixel));
        }
      }
      dst[y * width + x] = RGB565(vals_a[0], vals_a[1], vals_a[2]);
    }
  }
}

void zba_imgproc_erode_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output)
{
  uint16_t* dst = output;
  uint16_t* src = input;

  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 1; x < width - 1; ++x)
    {
      uint8_t vals_a[3];
      uint16_t pixel = src[y * width + x];
      vals_a[0]      = RGB565_R(pixel);
      vals_a[1]      = RGB565_G(pixel);
      vals_a[2]      = RGB565_B(pixel);

      for (int i = -1; i < 2; ++i)
      {
        int icol = (y + i) * width;
        for (int j = -1; j < 2; ++j)
        {
          if (j == 0 && i == 0) continue;

          pixel = src[icol + (x + j)];

          vals_a[0] = ZBA_MIN_BYTE(vals_a[0], RGB565_R(pixel));
          vals_a[1] = ZBA_MIN_BYTE(vals_a[1], RGB565_R(pixel));
          vals_a[2] = ZBA_MIN_BYTE(vals_a[2], RGB565_R(pixel));
        }
      }
      dst[y * width + x] = RGB565(vals_a[0], vals_a[1], vals_a[2]);
    }
  }
}
