SET(ZBA_SRC
  "zba_config.c"   
  "zba_util.c" 
  "zba_commands.c" 
  "zba_stream.c"
  "zba_wifi.c"
  "zba_camera.c"
  "zba_web.c"
  "zba_led.c"
  "zba_pins.c"
  "zba_sd.c"
  "zba_binaries.c"
  "zba_auth.c"
  "zba_vision.c"
  "zba_imgproc.c"
)
idf_component_register(SRCS  "main.c" ${ZBA_SRC} INCLUDE_DIRS ".")