#include "zba_rtsp.h"

#include <CRtspSession.h>
#include <CStreamer.h>
#include <string.h>

#include "zba_priority.h"
#include "zba_util.h"
#include "zba_web.h"
#include "zba_wifi.h"

DEFINE_ZBA_MODULE(zba_rtsp);

void zba_rtsp_task(void *context);
void client_worker(SOCKET client);

#define ZBA_RTSP_PORT 8554

typedef struct
{
  bool exiting;
  size_t stackSize;
  TaskHandle_t rtspTask;  ///< Capture task
  CStreamer *streamer;
} rtsp_state_t;

static rtsp_state_t rtsp_state = {
    .exiting = false, .stackSize = 8192, .rtspTask = 0, .streamer = 0};

zba_err_t zba_rtsp_init()
{
  zba_err_t result = ZBA_OK;

  rtsp_state.exiting = false;
  xTaskCreate(zba_rtsp_task, "RTSP", rtsp_state.stackSize, NULL, ZBA_RTSP_PRIORITY,
              &rtsp_state.rtspTask);

  ZBA_MODULE_INITIALIZED(zba_rtsp) = result;
  return result;
}

zba_err_t zba_rtsp_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  rtsp_state.exiting     = true;
  if (rtsp_state.rtspTask)
  {
    while (eTaskGetState(rtsp_state.rtspTask) != eDeleted)
    {
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    rtsp_state.rtspTask = NULL;
  }

  ZBA_MODULE_INITIALIZED(zba_rtsp) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

class ZBAStreamer : public CStreamer
{
 public:
  ZBAStreamer(SOCKET client) : CStreamer(client, zba_camera_get_width(), zba_camera_get_height()) {}

  virtual void streamImage(uint32_t curMsec) override
  {
    camera_fb_t *frame = zba_camera_capture_frame();
    if (frame)
    {
      streamFrame(frame->buf, frame->len, curMsec);
      zba_camera_release_frame(frame);
    }
    else
    {
      ZBA_LOG("Failed to get frame for RTSP.");
    }
  }
};

void zba_rtsp_task(void *context)
{
  SOCKET server_socket;
  SOCKET client_socket;
  sockaddr_in server_addr;
  sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  ZBA_LOG("Starting RTSP server");

  server_addr.sin_family      = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port        = htons(ZBA_RTSP_PORT);
  server_socket               = socket(AF_INET, SOCK_STREAM, 0);

  int enable = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
  {
    printf("setsockopt(SO_REUSEADDR) failed! errno=%d\n", errno);
  }

  // bind our server socket to the RTSP port and listen for a client connection
  if (bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    printf("bind failed! errno=%d\n", errno);
  }

  if (listen(server_socket, 5) != 0)
  {
    printf("listen failed! errno=%d\n", errno);
  }

  printf("\n\nrtsp://%s:8554/mjpeg/1\n\n", zba_wifi_get_ip_addr());

  // loop forever to accept client connections
  while (!rtsp_state.exiting)
  {
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    printf("Client connected from: %s\n", inet_ntoa(client_addr.sin_addr));
    client_worker(client_socket);
  }
  ZBA_LOG("Exiting RTSP server.");
  close(server_socket);
  vTaskDelete(rtsp_state.rtspTask);
}

void client_worker(SOCKET client)
{
  ZBAStreamer *streamer = new ZBAStreamer(client);
  CRtspSession *session = new CRtspSession(client, streamer);

  unsigned long lastFrameTime      = 0;
  unsigned long expectedFps        = 30;
  const unsigned long msecPerFrame = (1000 / expectedFps);

  while ((session->m_stopped == false) && (!rtsp_state.exiting))
  {
    session->handleRequests(0);

    unsigned long now = zba_now_ms();
    if ((now > (lastFrameTime + msecPerFrame)) || (now < lastFrameTime))
    {
      session->broadcastCurrentFrame(now);
      lastFrameTime = now;
    }
    else
    {
      vTaskDelay(10);
    }
  }

  // shut ourselves down
  delete streamer;
  delete session;
}