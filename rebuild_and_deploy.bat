@echo off
setlocal

set "ROOT=%~dp0"
set "MINGW_DIR=F:\msys64\mingw64"
set "QT_PLUGIN_DIR=%MINGW_DIR%\share\qt6\plugins"
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

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"

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

for %%D in (
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Svg.dll
    Qt6Widgets.dll
    libb2-1.dll
    libbrotlicommon.dll
    libbrotlidec.dll
    libbz2-1.dll
    libdouble-conversion.dll
    libfreetype-6.dll
    libgcc_s_seh-1.dll
    libglib-2.0-0.dll
    libgraphite2.dll
    libharfbuzz-0.dll
    libiconv-2.dll
    libicudt78.dll
    libicuin78.dll
    libicuuc78.dll
    libintl-8.dll
    libmd4c.dll
    libpcre2-16-0.dll
    libpcre2-8-0.dll
    libpng16-16.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    libzstd.dll
    zlib1.dll
) do (
    if not exist "%MINGW_DIR%\bin\%%D" (
        echo %%D was not found in %MINGW_DIR%\bin.
        exit /b 1
    )

    copy /Y "%MINGW_DIR%\bin\%%D" "%DEPLOY_DIR%\%%D" >nul
    if errorlevel 1 exit /b %errorlevel%
)

mkdir "%DEPLOY_DIR%\platforms"
if errorlevel 1 exit /b %errorlevel%
copy /Y "%QT_PLUGIN_DIR%\platforms\qwindows.dll" "%DEPLOY_DIR%\platforms\qwindows.dll" >nul
if errorlevel 1 exit /b %errorlevel%

mkdir "%DEPLOY_DIR%\iconengines"
if errorlevel 1 exit /b %errorlevel%
copy /Y "%QT_PLUGIN_DIR%\iconengines\qsvgicon.dll" "%DEPLOY_DIR%\iconengines\qsvgicon.dll" >nul
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
