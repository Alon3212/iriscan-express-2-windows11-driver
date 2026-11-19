@echo off
setlocal EnableExtensions

rem Uninstalls ColorUSBScanner.ds and restores the old Plustek Data Source
rem if install.cmd moved it aside. Run this script as Administrator.

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: run this script as Administrator.
    exit /b 1
)

set "DEST=%SystemRoot%\twain_32\Color USB Scanner"

echo.
echo Uninstalling ColorUSBScanner...
echo.

if exist "%DEST%\ColorUSBScanner.ds" (
    del /f /q "%DEST%\ColorUSBScanner.ds"
    if errorlevel 1 (
        echo WARNING: could not delete "%DEST%\ColorUSBScanner.ds".
        echo Close NAPS2 and any TWAIN application, then try again.
    ) else (
        echo Deleted: "%DEST%\ColorUSBScanner.ds"
    )
) else (
    echo "%DEST%\ColorUSBScanner.ds" does not exist.
)

rem If the user copied the DS to the top level of twain_32 (README fallback).
if exist "%SystemRoot%\twain_32\ColorUSBScanner.ds" (
    del /f /q "%SystemRoot%\twain_32\ColorUSBScanner.ds"
    echo Also deleted: "%SystemRoot%\twain_32\ColorUSBScanner.ds"
)

rem Restore the original Plustek DS if a backup was made.
if exist "%DEST%\Color USB Scanner.ds.plustek.bak" (
    if exist "%DEST%\Color USB Scanner.ds" (
        echo WARNING: "%DEST%\Color USB Scanner.ds" already exists;
        echo it is kept and the backup remains as .plustek.bak.
    ) else (
        move /y "%DEST%\Color USB Scanner.ds.plustek.bak" "%DEST%\Color USB Scanner.ds" >nul
        if errorlevel 1 (
            echo WARNING: could not restore the original Data Source.
        ) else (
            echo Restored: "%DEST%\Color USB Scanner.ds"
        )
    )
)

rem Optional: also delete the dependencies copied by install.cmd.
if /I "%~1"=="/deps" (
    if exist "%DEST%\libusb-1.0.dll" del /f /q "%DEST%\libusb-1.0.dll"
    if exist "%DEST%\cism216.fw" del /f /q "%DEST%\cism216.fw"
    echo Dependencies deleted from "%DEST%".
) else (
    echo.
    echo Note: libusb-1.0.dll and cism216.fw are left in the folder.
    echo To delete them too: uninstall.cmd /deps
)

echo.
echo === Restore the scanner driver (Zadig) ===
echo.
echo This script CANNOT safely restore a kernel driver.
echo Follow these steps:
echo.
echo   1. Open Zadig 2.9 as Administrator.
echo   2. Options -^> List All Devices.
echo   3. Select 07B3 0412.
echo   4. Click "Restore Default Driver" (or reinstall the Plustek INF).
echo   5. Unplug and reconnect the scanner.
echo   6. If you used the Windows "Scanner" app, reinstall the original
echo      Plustek software/WIA driver.
echo.
echo Then close and reopen NAPS2.
echo.

rem Open Zadig if it is in a common location, to make restoration easier.
set "ZADIG="
if exist "%ProgramFiles%\Zadig\zadig.exe" set "ZADIG=%ProgramFiles%\Zadig\zadig.exe"
if not defined ZADIG if exist "%ProgramFiles(x86)%\Zadig\zadig.exe" set "ZADIG=%ProgramFiles(x86)%\Zadig\zadig.exe"
if defined ZADIG (
    echo Opening Zadig...
    start "" "%ZADIG%"
) else (
    echo zadig.exe was not found in Program Files. Open it manually.
)

pause
exit /b 0
