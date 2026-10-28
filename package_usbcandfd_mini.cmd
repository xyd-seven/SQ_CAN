@echo off
setlocal EnableExtensions

rem Create a clean distributable directory after build_and_run_usbcandfd_mini.cmd.
set "ROOT=%~dp0"
set "BUILD=%ROOT%build_qt_mingw81_32_usbcandfd_mini\release"
set "DIST=%ROOT%dist_usbcandfd_mini"

if not exist "%BUILD%\CAN.exe" (
    echo [ERROR] CAN.exe not found: %BUILD%\CAN.exe
    echo Run build_and_run_usbcandfd_mini.cmd first.
    goto :failed
)

if exist "%DIST%" rmdir /S /Q "%DIST%"
mkdir "%DIST%"

for %%F in (CAN.exe zlgcan.dll Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll Qt5SerialPort.dll Qt5Svg.dll libEGL.dll libGLESv2.dll opengl32sw.dll D3DCompiler_47.dll libgcc_s_dw2-1.dll libstdc++-6.dll libwinpthread-1.dll msvcr120.dll msvcp120.dll) do (
    if exist "%BUILD%\%%F" copy /Y "%BUILD%\%%F" "%DIST%\%%F" >nul
)

set "HAS_PLATFORM=0"
for %%D in (platforms imageformats iconengines styles translations) do (
    if exist "%BUILD%\%%D" (
        if "%%D"=="platforms" set "HAS_PLATFORM=1"
        xcopy /E /I /Y "%BUILD%\%%D" "%DIST%\%%D" >nul
    )
)
if "%HAS_PLATFORM%"=="0" (
    echo [ERROR] Qt platform plugin directory not found: %BUILD%\platforms
    goto :failed
)

if exist "%BUILD%\kerneldlls" (
    xcopy /E /I /Y "%BUILD%\kerneldlls" "%DIST%\kerneldlls" >nul
) else (
    echo [ERROR] ZLG kernel runtime directory not found: %BUILD%\kerneldlls
    goto :failed
)

>"%DIST%\启动CAN工具.cmd" echo @echo off
>>"%DIST%\启动CAN工具.cmd" echo cd /d "%%~dp0"
>>"%DIST%\启动CAN工具.cmd" echo start "" "%%~dp0CAN.exe"

echo Package created: %DIST%
echo Copy the entire directory to the target computer.
echo.
pause
endlocal
exit /b 0

:failed
echo.
echo Packaging failed. Check the message above.
pause
endlocal
exit /b 1
