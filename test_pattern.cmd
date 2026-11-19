@echo off
setlocal
set "NAPS2=C:\Program Files\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: no se encontro "%NAPS2%".
    echo Ajusta la ruta o instala NAPS2 de 32 bits.
    exit /b 1
)

rem Fuerza el modo de imagen sintetica (no toca el hardware).
set COLORUSBSCANNER_TEST_PATTERN=1

"%NAPS2%" --driver twain --device "Color USB Scanner" --dpi 100 --bitdepth gray ^
          -o "%~dp0test_pattern.pdf" -v

rem NAPS2 puede devolver errorlevel 1 por una excepcion no fatal de su
rem procesador de imagen; lo importante es que el PDF se haya creado.
if exist "%~dp0test_pattern.pdf" (
    echo.
    echo OK: se genero "%~dp0test_pattern.pdf".
) else (
    echo.
    echo Fallo la prueba. Revisa que ColorUSBScanner.ds este en:
    echo   C:\Windows\twain_32\Color USB Scanner\
    echo y que libusb-1.0.dll de 32 bits este en la misma carpeta.
    exit /b 1
)
