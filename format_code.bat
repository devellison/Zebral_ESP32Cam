@echo off

pushd "%~dp0"
echo Formatting Code....
clang-format -i main\*.c || goto error
clang-format -i main\*.h || goto error

echo Formatting CMake...

cmake-format -i main\CMakeLists.txt  || goto error

echo "Reformatted everything."
exit /b 0
popd
:error
echo "An error occurred!"
popd
exit /b 1


