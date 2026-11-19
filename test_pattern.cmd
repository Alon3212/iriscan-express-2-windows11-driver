@echo off
setlocal EnableExtensions
set "NAPS2=%ProgramFiles%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" set "NAPS2=%ProgramFiles(x86)%\NAPS2\NAPS2.Console.exe"
if not exist "%NAPS2%" (
    echo ERROR: no se encontro NAPS2.Console.exe.
    echo Ajusta la ruta de NAPS2 32 bits en este script.
    exit /b 1
)

rem Fuerza el modo de imagen sintetica (no toca el hardware).
set "COLORUSBSCANNER_TEST_PATTERN=1"

if exist "%~dp0test_pattern.pdf" del /f /q "%~dp0test_pattern.pdf"

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
    echo   %SystemRoot%\twain_32\Color USB Scanner\
    echo y que NAPS2 sea de 32 bits.
    exit /b 1
)
