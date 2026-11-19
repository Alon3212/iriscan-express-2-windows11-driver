@echo off
setlocal
set "NAPS2=C:\Program Files\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: no se encontro "%NAPS2%".
    exit /b 1
)

rem NO definir COLORUSBSCANNER_TEST_PATTERN: debe abrir el hardware real.
set COLORUSBSCANNER_DEBUG=1

"%NAPS2%" --driver twain --device "Color USB Scanner" --dpi 300 --bitdepth color ^
          -o "%~dp0real_scan.pdf" -v

rem NAPS2 puede devolver 1 por su ArgumentNullException no fatal.
if exist "%~dp0real_scan.pdf" (
    echo.
    echo OK: se genero "%~dp0real_scan.pdf".
    echo Si la imagen sale mal, revisa %%TEMP%%\ColorUSBScanner.log.
) else (
    echo.
    echo Fallo el escaneo real. Revisa:
    echo   - Zadig: 07B3:0412 enlazado a WinUSB/libusbK.
    echo   - libusb-1.0.dll de 32 bits junto a ColorUSBScanner.ds.
    echo   - cism216.fw junto a ColorUSBScanner.ds.
    echo   - %%TEMP%%\ColorUSBScanner.log con COLORUSBSCANNER_DEBUG=1.
    exit /b 1
)
