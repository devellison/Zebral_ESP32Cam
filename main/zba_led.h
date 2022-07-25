#ifndef ZEBRAL_ESP32CAM_ZBA_LED_H_
#define ZEBRAL_ESP32CAM_ZBA_LED_H_
#include <stdbool.h>
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  DECLARE_ZBA_MODULE(zba_led);

  /// Initializes led module
  zba_err_t zba_led_init();

  /// Deinits led module
  zba_err_t zba_led_deinit();

  /// Turn the white LED on the top of the board on/off
  zba_err_t zba_led_light(bool on);

  /// Blink the white LED on the top of the board
  zba_err_t zba_led_light_blink();

  /// The different types of LEDs we have right now
  typedef enum zba_led_type
  {
    LED_STRIP_RGB,   ///< 3 pixels per LED - R, G, B
    LED_STRIP_RGBW,  ///< 4 pixels per LED - R, G, B, W
    LED_STRIP_UV3,   ///< 3 pixels per LED, all UV.
  } zba_led_type_t;

  int pixels_per_led(zba_led_type_t led_type);

  /// So... For the LED strips, I want to chain them and use 1 pin to control them all.
  /// But I'm using assorted types - some 3 LED (RGB, UV), and some 4 (RGBW), so addressing
  /// gets a bit odd and most of the standard libs I looked at weren't quite right for it.
  ///
  /// My terminology may be a bit different than normal - since we have LED packages that have
  /// differing numbers of channels / colors, I'm calling each individual channel a "pixel", and the
  /// composite the LED.  Hence, on an RGBW LED, you have 1 LED there, with 4 pixels.
  ///
  /// So on the first strip, if it's RGBW, the second LED would be LED index 1 and Pixel index 4.
  ///
  /// I'm using a double-buffer for the pixels.  The first buffer, in data, is what to use when
  /// manually setting pixels.
  ///
  /// When you've set the pixels up the way you like, call flip() to copy them to the output buffer
  /// and have them sent to the LED strip.  If you want to adjust brightness or similar, you can
  /// install a filter that operates on the flip() operation.
  ///
  /// Alternatively, you can set an animator call up that gets called every cycle. Animators
  /// can write directly to data_buf, either generating their own pixels or using data as a basis
  /// for animation.
  typedef struct zba_led_seg
  {
    // these must be set for each strip sent to zba_led_strip_init
    const char* name;         ///< Name of the strip
    zba_led_type_t led_type;  ///< Type of strip (RGB, RGBW, UV3, etc.)
    size_t num_leds;          ///< Number of LEDs on strip

    // these are set by zba_led_strip_init and should be zero initially (but doesn't matter much)
    size_t num_pixels;         ///< Num pixels on strip (num_leds * pix per led)
    size_t first_pixel_index;  ///< First pixel index of strip
    size_t first_led_index;    ///< First led index of strip
    uint8_t* data;             ///< Use this to draw normally outside of the refresh task.
                               ///< When done, call flip() to display it.
    uint8_t* data_buf;         ///< Use this to draw if in an animator/within the update task.
  } zba_led_seg_t;

  /// Used to set config when we want init to be separate
  zba_err_t zba_led_strip_cfg(zba_led_seg_t* strips, size_t strip_count);

  /// Initializes the LED strip
  /// Note: does not make a copy of strips and expects it to be static or stay around!
  zba_err_t zba_led_strip_init(zba_led_seg_t* strips, size_t strip_count);

  /// Deinitializes the strip
  zba_err_t zba_led_strip_deinit();

  /// Clears the strip data (you need to call flip() to realize the blank strip afterwards)
  zba_err_t zba_led_strip_clear();

  /// Sets a pixel to data.  Call flip after setting any pixels you want to display it.
  /// \param seg_name - name of the segment in table.
  /// \param led_index - index within the segment. If -1, sets all pixels in that segment.
  /// \param r - 2nd pixel (red if rgbw)
  /// \param g - 1st pixel
  /// \param b - 3rd pixel
  /// \param w - 4th pixel
  zba_err_t zba_led_strip_set_led(const char* seg_name, int led_index, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t w);

  /// Get a pixel from data
  zba_err_t zba_led_strip_get_led(const char* seg_name, int led_index, zba_led_type_t* led_type,
                                  uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w);

  // Retrieve an initialized segment by name. If null, gets the first one.
  zba_led_seg_t* zba_led_strip_get_segment(const char* seg_name);

  // Retrieve number of segments.
  size_t zba_led_strip_get_num_segments();

  /// Once you're done setting the LEDs, call flip() to make the buffer active.
  zba_err_t zba_led_strip_flip();

  /// Animator callback definition
  typedef bool (*ZBA_LED_ANIMATOR_CB)(zba_led_seg_t* segment);

  /// Set an animator callback - it'll be called by the refresh task prior to refresh
  /// Animators should use the data_buf member of the segment directly to modify pixels.
  /// No need to call flip() in an animator.
  /// Animators should also manage their own delay times beyond the 280us reset.
  void zba_led_strip_set_animator(ZBA_LED_ANIMATOR_CB animator);

  /// Filter callback definition
  typedef bool (*ZBA_LED_FILTER_CB)(uint8_t* dest, uint8_t* source, size_t length);

  /// Set a filter - this can perform filters on the LED data w/o affecting the memory
  /// during the flip.  Could be useful for brightness or similar adjustments.
  /// Filters do NOT affect animators - only runtime buffer changes that get flipped.
  void zba_led_strip_set_filter(ZBA_LED_FILTER_CB filter);

  // GRBWPixel_t for RGBW led strips
  typedef struct
  {
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;
  } GRBWPixel_t;

  /// HSVPixel_t for hue conversion
  typedef struct
  {
    uint8_t h;
    uint8_t s;
    uint8_t v;
  } HSVPixel_t;

  /// GRBPixel_t for RGB led strips
  typedef struct
  {
    uint8_t g;
    uint8_t r;
    uint8_t b;
  } GRBPixel_t;

  // Hue is 0-180, like OpenCV's.
  HSVPixel_t rgbw2hsv(GRBWPixel_t rgbw);
  GRBWPixel_t hsv2rgbw(HSVPixel_t hsv);

  // Cycles the hue on whatever's been put in memory
  bool zba_hue_cycle_animator(zba_led_seg_t* segment);

  // Chases with whatever's been put in memory
  bool zba_chase_animator(zba_led_seg_t* segment);

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_LED_H_
