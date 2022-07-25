#ifndef ZEBRAL_ESP32CAM_ZBA_IMGPROC_H_
#define ZEBRAL_ESP32CAM_ZBA_IMGPROC_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// expected bit format
// rrrrr ggg | ggg bbbbbb
// but the bytes are swapped.... so g is spread out
// ggg bbbbb | rrrrr ggg
// Also, we want the macros to just get the 8-bit values 0-255 (lower bits zeroed),
// then take 8-bit values and convert them back into 565
#define RGB565_B(x)     (((x)&0x1f00) >> 5)
#define RGB565_R(x)     ((x)&0xf8)
#define RGB565_G(x)     ((((x)&0xe000) >> 11) | (((x)&0x07) << 5))
#define RGB565(r, g, b) ((r & 0xf8) | ((g & 0xfc) >> 5) | ((b & 0xf8) << 5) | ((g & 0x1c) << 11))

  typedef enum
  {
    POST_NONE     = 0,
    POST_DIV      = 0x01,
    POST_ABS      = 0x02,
    POST_SATURATE = 0x04
  } zba_convolve_flags_t;

  void zba_imgproc_rgb565_to_gray(uint16_t* input, size_t width, size_t height, uint8_t* output);

  // Convolution functions
  void zba_imgproc_mean_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);
  void zba_imgproc_gaussian_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);
  void zba_imgproc_edgex_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);
  void zba_imgproc_edgey_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);

  // Base convolution
  void zba_imgproc_convolve3x3_rgb565(uint16_t* input, size_t width, size_t height,
                                      uint16_t* output, int8_t* kernel, int8_t divisor,
                                      zba_convolve_flags_t flags);

  // Morphology functions
  void zba_imgproc_dilate_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);
  void zba_imgproc_erode_rgb565(uint16_t* input, size_t width, size_t height, uint16_t* output);
#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_IMGPROC_H_