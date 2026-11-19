@echo off
setlocal EnableExtensions
set "NAPS2=%ProgramFiles%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" set "NAPS2=%ProgramFiles(x86)%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: no se encontro NAPS2.Console.exe.
    echo Ajusta la ruta de NAPS2 32 bits en este script.
    exit /b 1
)

rem Debe abrir el hardware real: limpiar cualquier modo de prueba heredado.
set "COLORUSBSCANNER_TEST_PATTERN="
set "COLORUSBSCANNER_DEBUG=1"

rem Evitar que un PDF antiguo haga parecer exitoso un escaneo fallido.
if exist "%~dp0real_scan.pdf" del /f /q "%~dp0real_scan.pdf"

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
    echo   - Zadig 2.9: 07B3:0412 enlazado a WinUSB/libusbK.
    echo   - libusb-1.0.dll de 32 bits junto a ColorUSBScanner.ds.
    echo   - cism216.fw (8192 bytes) junto a ColorUSBScanner.ds.
    echo   - %%TEMP%%\ColorUSBScanner.log con COLORUSBSCANNER_DEBUG=1.
    exit /b 1
)
