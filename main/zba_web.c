// todo - wait for video task shutdown on deinit...
#include "zba_web.h"
#include <esp_http_server.h>
#include <string.h>
#include "zba_auth.h"
#include "zba_binaries.h"
#include "zba_camera.h"
#include "zba_commands.h"
#include "zba_html.h"
#include "zba_priority.h"

DEFINE_ZBA_MODULE(zba_web);

// {TODO} Add authentication and/or SSL

/// Web module state
typedef struct
{
  httpd_handle_t page_server;   ///< Each server handles a connection at a time.
  httpd_handle_t video_server;  ///< So... need separate ones for the root pages, video, and
  volatile bool run_server;
} zba_web_state_t;
static zba_web_state_t web_state = {.page_server = NULL, .video_server = NULL, .run_server = false};

static const char kBoundary[] = "\r\n--ZEBRAL_IMAGE_CHUNK\r\n";
static const int kBoundaryLen = sizeof(kBoundary);
static const int kWebStack    = 8192;

/// URI handler forward declares
esp_err_t index_handler(httpd_req_t *req);
esp_err_t video_handler(httpd_req_t *req);
esp_err_t image_handler(httpd_req_t *req);
esp_err_t favicon_handler(httpd_req_t *req);
esp_err_t logo_handler(httpd_req_t *req);
esp_err_t command_handler(httpd_req_t *req);
// clang-format off

/// Table of URI handlers to set up
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/",      .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL},
    {.uri = "/command",.method=HTTP_GET, .handler = command_handler,.user_ctx = NULL},
    {.uri = "/image", .method = HTTP_GET, .handler = image_handler,.user_ctx=NULL},
    {.uri = "/favicon.png", .method = HTTP_GET, .handler = favicon_handler,.user_ctx=NULL},
   { .uri = "/zebral_logo.svg", .method = HTTP_GET, .handler = logo_handler, .user_ctx = NULL}
};
static const httpd_uri_t video_uri = {.uri = "/video", .method = HTTP_GET, .handler = video_handler, .user_ctx = NULL};



static const size_t num_uri_handlers = sizeof(uri_handlers) / sizeof(httpd_uri_t);
// clang-format on

zba_err_t zba_web_init()
{
  zba_err_t init_err      = ZBA_OK;
  httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
  config.stack_size       = kWebStack;
  config.task_priority    = ZBA_HTTPD_PRIORITY;
  config.max_uri_handlers = num_uri_handlers;
  esp_err_t esp_err;
  size_t i;

  web_state.run_server = true;
  if (ESP_OK == (esp_err = httpd_start(&web_state.page_server, &config)))
  {
    // Set up all the table-defined handlers
    for (i = 0; i < num_uri_handlers; ++i)
    {
      httpd_register_uri_handler(web_state.page_server, &uri_handlers[i]);
    }

    config.server_port++;
    config.ctrl_port++;
    config.max_uri_handlers = 1;
    if (ESP_OK == (esp_err = httpd_start(&web_state.video_server, &config)))
    {
      httpd_register_uri_handler(web_state.video_server, &video_uri);
    }
    else
    {
      ZBA_ERR("Error registering video server.");
    }
  }
  else
  {
    ZBA_ERR("Error registering uri server.");
  }

  if (esp_err != ESP_OK)
  {
    web_state.run_server = false;
    ZBA_ERR("Failed starting httpd. ESP: %d", esp_err);
    zba_web_deinit();
    init_err = ZBA_WEB_INIT_FAILED;
  }

  ZBA_SET_INIT(zba_web, init_err);
  return init_err;
}

zba_err_t zba_web_deinit()
{
  size_t i;
  zba_err_t deinit_error = ZBA_OK;

  if (web_state.run_server)
  {
    web_state.run_server = false;
    if (web_state.page_server != NULL)
    {
      for (i = 0; i < num_uri_handlers; ++i)
      {
        httpd_unregister_uri_handler(web_state.page_server, uri_handlers[i].uri,
                                     uri_handlers[i].method);
      }
      if (ESP_OK != httpd_stop(web_state.page_server))
      {
        deinit_error = ZBA_WEB_DEINIT_FAILED;
      }
    }
    if (web_state.video_server != NULL)
    {
      httpd_unregister_uri_handler(web_state.video_server, video_uri.uri, video_uri.method);
      if (ESP_OK != httpd_stop(web_state.video_server))
      {
        deinit_error = ZBA_WEB_DEINIT_FAILED;
      }
    }
  }

  // On successful deinit, set module status back to not initialized
  // otherwise set it to the error.
  ZBA_SET_DEINIT(zba_web, deinit_error);

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
  return httpd_resp_sendstr(req, kIndexHtml);
}

esp_err_t command_handler(httpd_req_t *req)
{
  char query[256] = {0};
  size_t buf_len  = 256;
  // Check authorization. Bail if not authorized.
  if (ZBA_OK != zba_auth_digest_check_web(req))
  {
    return ESP_OK;
  }

  httpd_req_get_url_query_str(req, query, buf_len - 1);
  ZBA_LOG("Got command: %s", query);

  zba_commands_process_web(query, req);

  return ESP_OK;
}

esp_err_t send_and_release_image_chunked(httpd_req_t *req, camera_fb_t **framePtr)
{
  char buf[128]      = {0};
  size_t bufLen      = 0;
  esp_err_t res      = ESP_OK;
  camera_fb_t *frame = (*framePtr);
  if (!frame) return ESP_FAIL;

  if (frame->format == PIXFORMAT_JPEG)
  {
    bufLen =
        snprintf(buf, sizeof(buf),
                 "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %ld.%06ld\r\n\r\n",
                 frame->len, frame->timestamp.tv_sec, frame->timestamp.tv_usec);

    if (ESP_OK == (res = httpd_resp_send_chunk(req, buf, bufLen)))
    {
      res = httpd_resp_send_chunk(req, (const char *)frame->buf, frame->len);
    }
  }
  else
  {
    // Try converting non-jpegs to bitmaps
    uint8_t *bmp             = NULL;
    size_t bmp_len           = 0;
    struct timeval timestamp = frame->timestamp;
    bool converted           = frame2bmp(frame, &bmp, &bmp_len);

    // Release frame now, since sending will take a bit.
    zba_camera_release_frame(frame);
    *framePtr = NULL;

    if (!converted)
    {
      ZBA_ERR("Failed converting frame to bitmap");
      res = ESP_FAIL;
    }
    else
    {
      bufLen = snprintf(buf, sizeof(buf),
                        "Content-Type: image/x-windows-bmp\r\nContent-Length: %u\r\nX-Timestamp: "
                        "%ld.%06ld\r\n\r\n",
                        bmp_len, timestamp.tv_sec, timestamp.tv_usec);

      if (ESP_OK == (res = httpd_resp_send_chunk(req, buf, bufLen)))
      {
        res = httpd_resp_send_chunk(req, (const char *)bmp, bmp_len);
      }
    }

    if (bmp)
    {
      free(bmp);
    }
  }

  if (frame)
  {
    zba_camera_release_frame(frame);
    *framePtr = NULL;
  }

  return res;
}

esp_err_t video_handler(httpd_req_t *req)
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

  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=ZEBRAL_IMAGE_CHUNK");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "30");

  int retries = 0;
  while (web_state.run_server)
  {
    // check for res change.
    if (zba_camera_need_restart())
    {
      zba_camera_deinit();
      zba_camera_init();
    }

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

    // Send chunk boundary
    if (ESP_OK != (res = httpd_resp_send_chunk(req, kBoundary, kBoundaryLen)))
    {
      break;
    }

    // Send chunk and free it.  We free it in the subroutine
    // to give the camera extra time for the next frame.
    if (ESP_OK != (res = send_and_release_image_chunked(req, &frame)))
    {
      break;
    }
  }

  if (res != ESP_OK)
  {
    httpd_resp_send_500(req);
  }

  // Make sure frame is released
  if (frame)
  {
    zba_camera_release_frame(frame);
    frame = NULL;
  }

  return res;
}

esp_err_t send_and_release_image(httpd_req_t *req, camera_fb_t **framePtr)
{
  esp_err_t res      = ESP_OK;
  camera_fb_t *frame = *framePtr;
  if (!frame) return ESP_FAIL;

  if (frame->format == PIXFORMAT_JPEG)
  {
    if (ESP_OK == (res = httpd_resp_set_type(req, "image/jpeg")))
    {
      if (ESP_OK ==
          (res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=image.jpg")))
      {
        res = httpd_resp_send(req, (const char *)frame->buf, frame->len);
      }
    }
  }
  else
  {
    uint8_t *buf   = NULL;
    size_t buf_len = 0;
    bool converted = frame2bmp(frame, &buf, &buf_len);
    zba_camera_release_frame(frame);

    for (;;)
    {
      if (!converted)
      {
        ZBA_ERR("Failed converting frame to bitmap");
        res = ESP_FAIL;
        break;
      }

      if (ESP_OK != (res = httpd_resp_set_type(req, "image/x-windows-bmp")))
      {
        break;
      }

      if (ESP_OK !=
          (res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=image.bmp")))
      {
        break;
      }

      if (ESP_OK != (res = httpd_resp_send(req, (const char *)buf, buf_len)))
      {
        break;
      }
      break;
    }

    if (buf) free(buf);
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
  }
  else
  {
    res = send_and_release_image(req, &frame);
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
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  return httpd_resp_send(req, (const char *)favicon_16x16, sizeof(favicon_16x16));
}

esp_err_t logo_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "image/svg+xml");
  return httpd_resp_sendstr(req, kZebralLogo);
}
