set(ZBA_SRC
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
    "zba_html.c"
    "zba_i2c.c"
    "zba_rtsp.cpp"
)
SET(MICRORTSP_DIR "${CMAKE_SOURCE_DIR}/components/Micro-RTSP/src")

SET(MICRORTSP_SRC
  "${MICRORTSP_DIR}/CRtspSession.cpp"
  "${MICRORTSP_DIR}/CStreamer.cpp"  
)



idf_component_register(SRCS "main.c" ${ZBA_SRC} ${MICRORTSP_SRC} INCLUDE_DIRS "." "${MICRORTSP_DIR}")

