@echo off
SET ERROR=1
SET PORT=COM3
IF NOT "%1" == "" (
     SET PORT=%1
)
idf.py build || ( goto exit_tag )
idf.py -p %PORT% flash || ( goto exit_tag )

SET ERROR=0
:exit_tag