#ifndef ZEBRAL_ESP32CAM_ZBA_PRIORITY_H_
#define ZEBRAL_ESP32CAM_ZBA_PRIORITY_H_

// Priorities for the various tasks if we want them to play well together
// Higher values are higher priority
#define ZBA_STREAM_PRIORITY         (tskIDLE_PRIORITY + 2)
#define ZBA_HTTPD_PRIORITY          (tskIDLE_PRIORITY + 3)
#define ZBA_LED_UPDATE_PRIORITY     (tskIDLE_PRIORITY + 4)
#define ZBA_CAMERA_LOC_CAP_PRIORITY (tskIDLE_PRIORITY + 5)

// This one is in cam_hal.c and set by config, but here
// for reference. Actual camera task priority
#define ZBA_CAMERA_HAL_PRIORITY (configMAX_PRIORITIES - 2)

#endif  // ZEBRAL_ESP32CAM_ZBA_PRIORITY_H_