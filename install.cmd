@echo off
setlocal EnableExtensions

rem Instala ColorUSBScanner.ds en %SystemRoot%\twain_32\Color USB Scanner
rem Ejecutar como Administrador.

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: ejecuta este script como Administrador.
    exit /b 1
)

set "HERE=%~dp0"
set "DEST=%SystemRoot%\twain_32\Color USB Scanner"
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
set "HAVE_LIBUSB=0"
if exist "%HERE%libusb-1.0.dll" (
    copy /y "%HERE%libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
    set "HAVE_LIBUSB=1"
) else if exist "%HERE%third_party\libusb\libusb-1.0.dll" (
    copy /y "%HERE%third_party\libusb\libusb-1.0.dll" "%DEST%\libusb-1.0.dll" >nul
    set "HAVE_LIBUSB=1"
) else (
    echo AVISO: no se encontro libusb-1.0.dll de 32 bits.
    echo El modo de prueba funcionara, pero el escaneo real NO.
    echo Copia libusb-1.0.dll (32 bits, probado con 1.0.30) en:
    echo   "%HERE%" o "%HERE%third_party\libusb\"
)

rem Firmware del GT-6816.
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
    echo AVISO: no se encontro cism216.fw.
    echo El modo de prueba funcionara, pero el escaneo real NO.
    echo Extrae cism216.fw (8192 bytes) de un instalador oficial de Plustek
    echo y copialo en "%HERE%firmware\" o junto al .ds.
)

echo.
echo Instalado en: %DEST%
echo.
if "%HAVE_LIBUSB%%HAVE_FW%"=="11" (
    echo Dependencias de hardware: OK.
) else (
    echo Dependencias de hardware: INCOMPLETAS.
    echo El modo de prueba funciona; para escaneo real faltan las dependencias
    echo indicadas arriba.
)
echo.
echo Recuerda:
echo   1. Enlaza el escaner 07B3:0412 a WinUSB/libusbK con Zadig 2.9.
echo   2. Prueba primero con COLORUSBSCANNER_TEST_PATTERN=1 y NAPS2 32 bits.
echo   3. Para escaneo real: libusb-1.0.dll 32-bit + cism216.fw + scan_real.cmd.
echo   4. Para desinstalar: uninstall.cmd
exit /b 0
