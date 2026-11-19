@echo off
setlocal EnableExtensions
set "NAPS2=%ProgramFiles%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" set "NAPS2=%ProgramFiles(x86)%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: NAPS2.Console.exe was not found.
    echo Adjust the 32-bit NAPS2 path in this script.
    exit /b 1
)

rem Force synthetic-image mode (does not touch the hardware).
set "COLORUSBSCANNER_TEST_PATTERN=1"

if exist "%~dp0test_pattern.pdf" del /f /q "%~dp0test_pattern.pdf"

"%NAPS2%" --driver twain --device "Color USB Scanner" --dpi 100 --bitdepth gray ^
          -o "%~dp0test_pattern.pdf" -v

rem NAPS2 may return errorlevel 1 because of a non-fatal exception in its
rem image processor; what matters is that the PDF was created.
if exist "%~dp0test_pattern.pdf" (
    echo.
    echo OK: "%~dp0test_pattern.pdf" was created.
) else (
    echo.
    echo Test failed. Check that ColorUSBScanner.ds is in:
    echo   %SystemRoot%\twain_32\Color USB Scanner\
    echo and that NAPS2 is 32-bit.
    exit /b 1
)
