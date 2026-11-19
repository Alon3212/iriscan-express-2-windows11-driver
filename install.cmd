@echo off
setlocal EnableExtensions

rem Instala ColorUSBScanner.ds en C:\Windows\twain_32\Color USB Scanner
rem Ejecutar como Administrador.

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: ejecuta este script como Administrador.
    exit /b 1
)

set "HERE=%~dp0"
set "DEST=C:\Windows\twain_32\Color USB Scanner"
set "DS=%HERE%ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%build\ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%build-vs\Release\ColorUSBScanner.ds"
if not exist "%DS%" set "DS=%HERE%prebuilt\ColorUSBScanner-test.ds"

if not exist "%DS%" (
    echo ERROR: no se encontro ColorUSBScanner.ds.
    echo Compila primero con CMake o con Makefile.mingw.
    exit /b 1
)

if not exist "%DEST%" mkdir "%DEST%"

rem Si existe el Data Source antiguo de Plustek, se aparta para que NAPS2
rem no muestre dos entradas con el mismo nombre.
if exist "%DEST%\Color USB Scanner.ds" (
    move /y "%DEST%\Color USB Scanner.ds" "%DEST%\Color USB Scanner.ds.plustek.bak" >nul
)

copy /y "%DS%" "%DEST%\ColorUSBScanner.ds" >nul
if errorlevel 1 (
    echo ERROR: no se pudo copiar el Data Source.
    exit /b 1
)

rem libusb-1.0.dll de 32 bits: debe estar junto al .ds.
if exist "%HERE%libusb-1.0.dll" (
    copy /y "%HERE%libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
) else if exist "%HERE%third_party\libusb\libusb-1.0.dll" (
    copy /y "%HERE%third_party\libusb\libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
) else (
    echo AVISO: no se encontro libusb-1.0.dll de 32 bits.
    echo Copiala manualmente en "%DEST%".
)

rem Firmware del GT-6816.
if exist "%HERE%cism216.fw" (
    copy /y "%HERE%cism216.fw" "%DEST%\cism216.fw" >nul
) else if exist "%HERE%..\pkg\cab_extract\cism216.fw" (
    copy /y "%HERE%..\pkg\cab_extract\cism216.fw" "%DEST%\cism216.fw" >nul
) else (
    echo AVISO: no se encontro cism216.fw; copialo manualmente en "%DEST%".
)

echo.
echo Instalado en: %DEST%
echo.
echo Recuerda:
echo   1. Enlaza el escaner 07B3:0412 a WinUSB/libusbK con Zadig.
echo   2. Prueba primero con COLORUSBSCANNER_TEST_PATTERN=1 y NAPS2 32 bits.
echo   3. Para escaneo real, porta el protocolo gt68xx en gt68xx_proto.c.
exit /b 0
