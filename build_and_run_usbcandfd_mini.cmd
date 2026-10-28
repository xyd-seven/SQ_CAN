@echo off
setlocal EnableExtensions

rem Build and run SQ_CAN_ZLG for USBCANFD-100U-mini.
rem The Qt and MinGW paths below match the development environment.

set "ROOT=%~dp0"
set "QT_ROOT=D:\QT5.15.2\5.15.2\mingw81_32"
set "MINGW_ROOT=D:\QT5.15.2\Tools\mingw810_32"
set "QMAKE=%QT_ROOT%\bin\qmake.exe"
set "MAKE=%MINGW_ROOT%\bin\mingw32-make.exe"
set "WDEPLOY=%QT_ROOT%\bin\windeployqt.exe"
set "ZCANPRO_ROOT=E:\ZCANPRO"
set "PRO=%ROOT%qt-example_32\CAN\CAN.pro"
set "CAN_DIR=%ROOT%qt-example_32\CAN"
set "BUILD=%ROOT%build_qt_mingw81_32_usbcandfd_mini"
set "APP=%BUILD%\release"
set "EXE=%APP%\CAN.exe"

if not exist "%QMAKE%" (
    echo [ERROR] qmake not found: %QMAKE%
    exit /b 1
)
if not exist "%MAKE%" (
    echo [ERROR] mingw32-make not found: %MAKE%
    exit /b 1
)
if not exist "%PRO%" (
    echo [ERROR] project file not found: %PRO%
    exit /b 1
)
if not exist "%CAN_DIR%\libzlgcan_mingw.a" (
    echo [ERROR] MinGW import library not found: %CAN_DIR%\libzlgcan_mingw.a
    echo Generate it from zlgcan.dll before building.
    exit /b 1
)

set "PATH=%MINGW_ROOT%\bin;%QT_ROOT%\bin;%PATH%"
if not exist "%BUILD%" mkdir "%BUILD%"

echo [1/4] Running qmake...
pushd "%BUILD%"
"%QMAKE%" "%PRO%" -spec win32-g++
if errorlevel 1 (
    popd
    echo [ERROR] qmake failed.
    exit /b 1
)

echo [2/4] Building...
"%MAKE%" -j2
if errorlevel 1 (
    popd
    echo [ERROR] compilation failed.
    exit /b 1
)
popd

if not exist "%EXE%" (
    echo [ERROR] executable was not generated: %EXE%
    exit /b 1
)

echo [3/4] Deploying Qt and ZLG runtime files...
if exist "%WDEPLOY%" (
    "%WDEPLOY%" --release --compiler-runtime "%EXE%"
    if errorlevel 1 (
        echo [WARN] windeployqt returned an error; continuing with existing files.
    )
)
rem Some Qt installations cannot locate their platform plugin from windeployqt.
rem Copy the required runtime files explicitly as a fallback.
for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll Qt5SerialPort.dll Qt5Svg.dll libEGL.dll libGLESv2.dll opengl32sw.dll D3DCompiler_47.dll) do (
    if exist "%QT_ROOT%\bin\%%F" copy /Y "%QT_ROOT%\bin\%%F" "%APP%\%%F" >nul
)
for %%F in (libgcc_s_dw2-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_ROOT%\bin\%%F" copy /Y "%MINGW_ROOT%\bin\%%F" "%APP%\%%F" >nul
)
for %%D in (platforms imageformats iconengines styles) do (
    if exist "%QT_ROOT%\plugins\%%D" (
        if not exist "%APP%\%%D" mkdir "%APP%\%%D"
        xcopy /E /I /Y "%QT_ROOT%\plugins\%%D" "%APP%\%%D" >nul
    )
)
if exist "%QT_ROOT%\translations" (
    if not exist "%APP%\translations" mkdir "%APP%\translations"
    xcopy /E /I /Y "%QT_ROOT%\translations" "%APP%\translations" >nul
)
copy /Y "%CAN_DIR%\zlgcan.dll" "%APP%\zlgcan.dll" >nul
if exist "%CAN_DIR%\kerneldlls" (
    xcopy /E /I /Y "%CAN_DIR%\kerneldlls" "%APP%\kerneldlls" >nul
)
if exist "%ZCANPRO_ROOT%\msvcr120.dll" copy /Y "%ZCANPRO_ROOT%\msvcr120.dll" "%APP%\msvcr120.dll" >nul
if exist "%ZCANPRO_ROOT%\msvcp120.dll" copy /Y "%ZCANPRO_ROOT%\msvcp120.dll" "%APP%\msvcp120.dll" >nul

echo [4/4] Starting application...
pushd "%APP%"
start "SQ_CAN_ZLG" "%EXE%"
popd
echo Application started: %EXE%
endlocal
