@echo off
setlocal EnableExtensions

rem Installs ColorUSBScanner.ds into %SystemRoot%\twain_32\Color USB Scanner
rem Run this script as Administrator.

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: run this script as Administrator.
    exit /b 1
)

set "HERE=%~dp0"
set "DEST=%SystemRoot%\twain_32\Color USB Scanner"
set "DS=%HERE%ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%build\ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%build-vs\Release\ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%prebuilt\ColorUSBScanner-test.ds"

if not exist "%DS%" (
    echo ERROR: ColorUSBScanner.ds was not found.
    echo Build it first with CMake or Makefile.mingw.
    exit /b 1
)

if not exist "%DEST%" mkdir "%DEST%"

rem If the old Plustek Data Source exists, move it aside so NAPS2 does not
rem show two entries with the same name.
if exist "%DEST%\Color USB Scanner.ds" (
    move /y "%DEST%\Color USB Scanner.ds" "%DEST%\Color USB Scanner.ds.plustek.bak" >nul
)

copy /y "%DS%" "%DEST%\ColorUSBScanner.ds" >nul
if errorlevel 1 (
    echo ERROR: could not copy the Data Source.
    exit /b 1
)

rem 32-bit libusb-1.0.dll: it must be next to the .ds.
set "HAVE_LIBUSB=0"
if exist "%HERE%libusb-1.0.dll" (
    copy /y "%HERE%libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
    set "HAVE_LIBUSB=1"
) else if exist "%HERE%third_party\libusb\libusb-1.0.dll" (
    copy /y "%HERE%third_party\libusb\libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
    set "HAVE_LIBUSB=1"
) else (
    echo WARNING: 32-bit libusb-1.0.dll was not found.
    echo Test mode will work, but real scanning will NOT.
    echo Copy libusb-1.0.dll (32-bit, tested with 1.0.30) to:
    echo   "%HERE%" or "%HERE%third_party\libusb\"
)

rem GT-6816 firmware.
set "HAVE_FW=0"
if exist "%HERE%cism216.fw" (
    copy /y "%HERE%cism216.fw" "%DEST%\cism216.fw" >nul
    set "HAVE_FW=1"
) else if exist "%HERE%firmware\cism216.fw" (
    copy /y "%HERE%firmware\cism216.fw" "%DEST%\cism216.fw" >nul
    set "HAVE_FW=1"
) else if exist "%HERE%..\pkg\cab_extract\cism216.fw" (
    copy /y "%HERE%..\pkg\cab_extract\cism216.fw" "%DEST%\cism216.fw" >nul
    set "HAVE_FW=1"
) else (
    echo WARNING: cism216.fw was not found.
    echo Test mode will work, but real scanning will NOT.
    echo Extract cism216.fw (8192 bytes) from an official Plustek installer
    echo and copy it to "%HERE%firmware\" or next to the .ds.
)

echo.
echo Installed to: %DEST%
echo.
if "%HAVE_LIBUSB%%HAVE_FW%"=="11" (
    echo Hardware dependencies: OK.
) else (
    echo Hardware dependencies: INCOMPLETE.
    echo Test mode works; real scanning is missing the dependencies listed above.
)
echo.
echo Remember:
echo   1. Bind scanner 07B3:0412 to WinUSB/libusbK with Zadig 2.9.
echo   2. Test first with COLORUSBSCANNER_TEST_PATTERN=1 and NAPS2 32-bit.
echo   3. For real scanning: libusb-1.0.dll 32-bit + cism216.fw + scan_real.cmd.
echo   4. To uninstall: uninstall.cmd
exit /b 0
