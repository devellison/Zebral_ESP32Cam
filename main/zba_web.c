#include "zba_web.h"
#include <esp_http_server.h>
#include <string.h>
#include "zba_auth.h"
#include "zba_binaries.h"
#include "zba_camera.h"
#include "zba_priority.h"

DEFINE_ZBA_MODULE(zba_web);

// {TODO} Add authentication and/or SSL

/// Web module state
typedef struct
{
  httpd_handle_t http_server;
  volatile bool run_server;
} zba_web_state_t;
static zba_web_state_t web_state = {.http_server = NULL, .run_server = false};

/// Main page
static const char kIndexHtml[] =
    "<html>"
    "<head>"
    "  <style>"
    "    img{height:100vh;width:100%;object-fit:contain;image-rendering: pixelated;}"
    "    body {background-color:#000000;}"
    "  </style>"
    "  <title>Zebral ESP32-CAM</title>"
    "  <link rel=\"ico\" type=\"image/png\" href=\"/favicon.png\"/>"
    "  <link rel=\"shortcut icon\" href=\"/favicon.png\" type=\"image/png\"/>"
    "</head>"
    "<body>"
    "  <div><image src=\"video\"></image></div>"
    "</body>"
    "</html>";

static const int kIndexHtmlLen = sizeof(kIndexHtml);
static const char kBoundary[]  = "\r\n--ZEBRAL_IMAGE_CHUNK\r\n";
static const int kBoundaryLen  = sizeof(kBoundary);

/// URI handler forward declares
esp_err_t index_handler(httpd_req_t *req);
esp_err_t video_handler(httpd_req_t *req);
esp_err_t image_handler(httpd_req_t *req);
esp_err_t favicon_handler(httpd_req_t *req);
// clang-format off

/// Table of URI handlers to set up
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/",      .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL},
    {.uri = "/video", .method = HTTP_GET, .handler = video_handler, .user_ctx = NULL},
    {.uri = "/favicon.png", .method = HTTP_GET, .handler = favicon_handler,.user_ctx=NULL},
    {.uri = "/image", .method = HTTP_GET, .handler = image_handler,.user_ctx=NULL}

};

static const size_t num_uri_handlers = sizeof(uri_handlers) / sizeof(httpd_uri_t);
// clang-format on

zba_err_t zba_web_init()
{
  zba_err_t init_err      = ZBA_OK;
  httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
  config.stack_size       = 8192;
  config.task_priority    = ZBA_HTTPD_PRIORITY;
  config.max_uri_handlers = num_uri_handlers;
  esp_err_t esp_err;
  size_t i;

  web_state.run_server = true;
  if (ESP_OK == (esp_err = httpd_start(&web_state.http_server, &config)))
  {
    // Set up all the table-defined handlers
    for (i = 0; i < num_uri_handlers; ++i)
    {
      httpd_register_uri_handler(web_state.http_server, &uri_handlers[i]);
    }
  }
  else
  {
    web_state.run_server = false;
    ZBA_ERR("Failed starting httpd. ESP: %d", esp_err);
    zba_web_deinit();
    init_err = ZBA_WEB_INIT_FAILED;
  }

  ZBA_MODULE_INITIALIZED(zba_web) = init_err;
  return init_err;
}

zba_err_t zba_web_deinit()
{
  size_t i;
  zba_err_t deinit_error = ZBA_OK;

  if (web_state.run_server)
  {
    web_state.run_server = false;
    if (web_state.http_server != NULL)
    {
      for (i = 0; i < num_uri_handlers; ++i)
      {
        httpd_unregister_uri_handler(web_state.http_server, uri_handlers[i].uri,
                                     uri_handlers[i].method);
      }
      if (ESP_OK != httpd_stop(web_state.http_server))
      {
        deinit_error = ZBA_WEB_DEINIT_FAILED;
      }
    }
  }

  // On successful deinit, set module status back to not initialized
  // otherwise set it to the error.
  ZBA_MODULE_INITIALIZED(zba_web) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

esp_err_t index_handler(httpd_req_t *req)
{
  // Check authorization. Bail if not authorized.
  if (ZBA_OK != zba_auth_digest_check_web(req))
  {
    return ESP_OK;
  }

  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "utf-8");
  return httpd_resp_send(req, kIndexHtml, kIndexHtmlLen);
}

esp_err_t video_handler(httpd_req_t *req)
{
  char buf[128]      = {0};
  size_t bufLen      = 0;
  camera_fb_t *frame = NULL;
  esp_err_t res      = ESP_OK;

  // Check authorization. Bail if not authorized.
  // zba_auth_basic_check_web challenges the client, so next request may be authenticated.
  // zba_auth_digest_check_web is more secure, otherwise same thing.
  if (ZBA_OK != zba_auth_digest_check_web(req))
  {
    return ESP_OK;
  }

  if (ESP_OK !=
      (res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=ZEBRAL_IMAGE_CHUNK")))
  {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "30");
  int retries = 0;

  while (web_state.run_server)
  {
    // Retry up to 3 consecutive times if we fail to get a frame.
    frame = zba_camera_capture_frame();
    if (!frame)
    {
      ZBA_LOG("Failed to get frame. Retry: %d", retries);
      retries++;
      if (retries >= 3)
      {
        res = ESP_FAIL;
        break;
      }
      continue;
    }
    retries = 0;

    if (ESP_OK != (res = httpd_resp_send_chunk(req, kBoundary, kBoundaryLen)))
    {
      break;
    }

    if (frame->format == PIXFORMAT_JPEG)
    {
      bufLen = snprintf(
          buf, sizeof(buf),
          "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %ld.%06ld\r\n\r\n",
          frame->len, frame->timestamp.tv_sec, frame->timestamp.tv_usec);

      if (ESP_OK != (res = httpd_resp_send_chunk(req, buf, bufLen)))
      {
        break;
      }

      if (ESP_OK != (res = httpd_resp_send_chunk(req, (const char *)frame->buf, frame->len)))
      {
        break;
      }
    }
    else
    {
      // Try converting non-jpegs to bitmaps
      uint8_t *bmp             = NULL;
      size_t bmp_len           = 0;
      struct timeval timestamp = frame->timestamp;
      ZBA_LOG("Frame: %d,%d (%d) %p (%u)", frame->width, frame->height, (int)frame->format,
              frame->buf, frame->len);
      bool converted = frame2bmp(frame, &bmp, &bmp_len);
      // Release frame now, since sending will take a bit.
      zba_camera_release_frame(frame);
      frame = NULL;

      if (!converted)
      {
        ZBA_ERR("Failed converting frame to bitmap");
        break;
      }

      bufLen = snprintf(buf, sizeof(buf),
                        "Content-Type: image/x-windows-bmp\r\nContent-Length: %u\r\nX-Timestamp: "
                        "%ld.%06ld\r\n\r\n",
                        bmp_len, timestamp.tv_sec, timestamp.tv_usec);

      if (ESP_OK != (res = httpd_resp_send_chunk(req, buf, bufLen)))
      {
        break;
      }

      if (ESP_OK != (res = httpd_resp_send_chunk(req, (const char *)bmp, bmp_len)))
      {
        break;
      }

      free(bmp);
    }

    if (frame)
    {
      zba_camera_release_frame(frame);
      frame = NULL;
    }
  }

  if (res != ESP_OK)
  {
    httpd_resp_send_500(req);
  }

  if (frame)
  {
    zba_camera_release_frame(frame);
    frame = NULL;
  }

  return res;
}

esp_err_t image_handler(httpd_req_t *req)
{
  camera_fb_t *frame = NULL;
  esp_err_t res      = ESP_OK;

  // Check authorization. Bail if not authorized.
  // zba_auth_basic_check_web challenges the client, so next request may be authenticated.
  // zba_auth_digest_check_web is more secure, otherwise same thing.
  if (ZBA_OK != zba_auth_digest_check_web(req))
  {
    return ESP_OK;
  }

  for (;;)
  {
    // Retry up to 3 consecutive times if we fail to get a frame.
    int retries = 3;
    while (retries && !frame)
    {
      frame = zba_camera_capture_frame();
      // SO... this is a hack. Even with camera grab latest, I still get a stale frame
      // when doing onesies. That's no good. So measure once, capture twice.
      if (frame && (retries == 3))
      {
        zba_camera_release_frame(frame);
        frame = zba_camera_capture_frame();
      }

      retries--;
    }

    if (!frame)
    {
      res = ESP_FAIL;
      break;
    }

    if (frame->format == PIXFORMAT_JPEG)
    {
      if (ESP_OK != (res = httpd_resp_set_type(req, "image/jpeg")))
      {
        break;
      }

      if (ESP_OK !=
          (res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=image.jpg")))
      {
        break;
      }

      if (ESP_OK != (res = httpd_resp_send(req, (const char *)frame->buf, frame->len)))
      {
        break;
      }
    }
    else
    {
      uint8_t *buf   = NULL;
      size_t buf_len = 0;
      ZBA_LOG("Frame: %d,%d (%d) %p (%u)", frame->width, frame->height, (int)frame->format,
              frame->buf, frame->len);
      bool converted = frame2bmp(frame, &buf, &buf_len);
      zba_camera_release_frame(frame);

      if (!converted)
      {
        ZBA_ERR("Failed converting frame to bitmap");
        break;
      }

      res = httpd_resp_set_type(req, "image/x-windows-bmp") ||
            httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=image.bmp") ||
            httpd_resp_send(req, (const char *)buf, buf_len);
      free(buf);
    }

    break;
  }

  if (ESP_OK != res)
  {
    httpd_resp_send_500(req);
  }

  if (frame)
  {
    zba_camera_release_frame(frame);
    frame = NULL;
  }

  return res;
}

esp_err_t favicon_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "image/png");
  return httpd_resp_send(req, (const char *)favicon_16x16, sizeof(favicon_16x16));
}
