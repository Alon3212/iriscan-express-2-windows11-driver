@echo off
setlocal EnableExtensions
set "NAPS2=%ProgramFiles%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" set "NAPS2=%ProgramFiles(x86)%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: NAPS2.Console.exe was not found.
    echo Adjust the 32-bit NAPS2 path in this script.
    exit /b 1
)

rem Must open the real hardware: clear any inherited test mode.
set "COLORUSBSCANNER_TEST_PATTERN="
set "COLORUSBSCANNER_DEBUG=1"

rem Prevent an old PDF from making a failed scan look successful.
if exist "%~dp0real_scan.pdf" del /f /q "%~dp0real_scan.pdf"

"%NAPS2%" --driver twain --device "Color USB Scanner" --dpi 300 --bitdepth color ^
          -o "%~dp0real_scan.pdf" -v

rem NAPS2 may return 1 because of its non-fatal ArgumentNullException.
if exist "%~dp0real_scan.pdf" (
    echo.
    echo OK: "%~dp0real_scan.pdf" was created.
    echo If the image looks wrong, check %%TEMP%%\ColorUSBScanner.log.
) else (
    echo.
    echo Real scan failed. Check:
    echo   - Zadig 2.9: 07B3:0412 bound to WinUSB/libusbK.
    echo   - 32-bit libusb-1.0.dll next to ColorUSBScanner.ds.
    echo   - cism216.fw (8192 bytes) next to ColorUSBScanner.ds.
    echo   - %%TEMP%%\ColorUSBScanner.log with COLORUSBSCANNER_DEBUG=1.
    exit /b 1
)
