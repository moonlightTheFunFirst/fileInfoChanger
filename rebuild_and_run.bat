@echo off
setlocal

set "PATH=D:\msys64\mingw64\bin;%PATH%"

cmake -S . -B build -G Ninja
if errorlevel 1 exit /b %errorlevel%

cmake --build build
if errorlevel 1 exit /b %errorlevel%

copy /Y fileInfoChanger.ini build\fileInfoChanger.ini >nul

build\fileInfoChanger.exe
