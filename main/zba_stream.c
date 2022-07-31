#include "zba_stream.h"
#include <driver/uart.h>
#include <esp_vfs.h>
#include <esp_vfs_dev.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/unistd.h>
#include "zba_commands.h"
#include "zba_priority.h"

DEFINE_ZBA_MODULE(zba_stream);

typedef struct
{
  int fd;  ///< File descriptor for uart
  volatile bool exiting;
  zba_cmd_stream_t cmd_stream;
  TaskHandle_t readerTask;
} stream_state_t;

/// Some commands may tax this a bit.
/// e.g. listing files recursively on SD
static const size_t kStreamStackSize = 8192;

static const char *kStreamName     = "/dev/uart/0";
static stream_state_t stream_state = {
    .fd = ZBA_INVALID_FD, .cmd_stream = {0}, .readerTask = 0, .exiting = false};

void stream_reader_task(void *context);

zba_err_t zba_stream_init(bool init_commands)
{
  zba_err_t result = ZBA_OK;
  if (ZBA_INVALID_FD != stream_state.fd)
  {
    ZBA_LOG("Streams already initialized");
    return result;
  }

  for (;;)
  {
    ZBA_LOG("Initializing Stream");
    if (ZBA_INVALID_FD == (stream_state.fd = open(kStreamName, O_RDWR)))
    {
      ZBA_ERR("Could not open stream %s", kStreamName);
      result = ZBA_STREAM_INIT_FAILED;
      break;
    }

    if (init_commands)
    {
      ZBA_LOG("Init commands for stream");
      zba_commands_stream_init(&stream_state.cmd_stream, stream_state.fd);
    }

    // Create reader task
    ZBA_LOG("Creating stream reader task");
    stream_state.exiting = false;
    xTaskCreate(&stream_reader_task, "stream_reader_task", kStreamStackSize, &stream_state,
                ZBA_STREAM_PRIORITY, &stream_state.readerTask);
    break;
  }

  ZBA_SET_INIT(zba_stream, result);
  return result;
}

zba_err_t zba_stream_deinit()
{
  zba_err_t deinit_error = ZBA_OK;

  if (stream_state.fd)
  {
    stream_state.exiting = true;
    // Wait for reader task to exit
    ZBA_LOG("Waiting for reader exit.");
    if (stream_state.readerTask)
    {
      while (eTaskGetState(stream_state.readerTask) != eDeleted)
      {
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
      stream_state.readerTask = 0;
    }
    close(stream_state.fd);
    stream_state.fd      = ZBA_INVALID_FD;
    stream_state.exiting = false;
  }

  ZBA_MODULE_INITIALIZED(zba_stream) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

void on_byte_read(char new_char, stream_state_t *ss)
{
  // if (!ss->cmd_stream)
  switch (new_char)
  {
    case 0:
    case '\r':
    case '\n':
      ss->cmd_stream.buffer[ss->cmd_stream.bufPos] = 0;
      if (ss->cmd_stream.bufPos > 0)
      {
        zba_commands_process(ss->cmd_stream.buffer, &ss->cmd_stream);
        ss->cmd_stream.bufPos = 0;
      }
      break;

    default:
      ss->cmd_stream.buffer[ss->cmd_stream.bufPos] = new_char;
      ss->cmd_stream.bufPos++;
      if (ss->cmd_stream.bufPos >= sizeof(ss->cmd_stream.buffer) - 1)
      {
        ZBA_LOG("Stream buffer overflow! Resetting buffer.");
        // ss->cmd_stream.source->flush();
        ss->cmd_stream.bufPos = 0;
      }
      break;
  }
}

void stream_reader_task(void *context)
{
  stream_state_t *ss = (stream_state_t *)context;
  char nextByte;
  int amountRead;

  ZBA_LOG("Read task started.");
  while (!ss->exiting)
  {
    amountRead = uart_read_bytes(UART_NUM_0, &nextByte, 1, 50 / portTICK_RATE_MS);
    if (amountRead)
    {
      on_byte_read(nextByte, ss);
    }
  }
  ZBA_LOG("Reader task exiting.");
  vTaskDelete(ss->readerTask);
}
