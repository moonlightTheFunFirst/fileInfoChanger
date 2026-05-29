@echo off
setlocal

set "ROOT=%~dp0"
set "MINGW_DIR=D:\msys64\mingw64"
set "BUILD_DIR=%ROOT%build-release"
set "DEPLOY_DIR=%ROOT%deploy\fileInfoChanger"

set "PATH=%MINGW_DIR%\bin;%PATH%"

where cmake >nul 2>nul
if errorlevel 1 (
    echo cmake was not found.
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo ninja was not found.
    exit /b 1
)

where windeployqt >nul 2>nul
if errorlevel 1 (
    echo windeployqt was not found.
    exit /b 1
)

cmake -S "%ROOT%." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

if exist "%DEPLOY_DIR%" rmdir /S /Q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"
if errorlevel 1 exit /b %errorlevel%

copy /Y "%BUILD_DIR%\fileInfoChanger.exe" "%DEPLOY_DIR%\fileInfoChanger.exe" >nul
if errorlevel 1 exit /b %errorlevel%

if exist "%ROOT%fileInfoChanger.ini" (
    copy /Y "%ROOT%fileInfoChanger.ini" "%DEPLOY_DIR%\fileInfoChanger.ini" >nul
    if errorlevel 1 exit /b %errorlevel%
)

windeployqt --release --compiler-runtime "%DEPLOY_DIR%\fileInfoChanger.exe"
if errorlevel 1 exit /b %errorlevel%

if not exist "%DEPLOY_DIR%\Qt6Core.dll" (
    echo Qt6Core.dll was not deployed.
    exit /b 1
)

if not exist "%DEPLOY_DIR%\platforms\qwindows.dll" (
    echo platforms\qwindows.dll was not deployed.
    exit /b 1
)

echo Deployment completed:
echo %DEPLOY_DIR%

endlocal
