@echo off
setlocal EnableExtensions

rem Desinstala ColorUSBScanner.ds y restaura el Data Source antiguo de Plustek
rem si install.cmd lo habia apartado. Ejecutar como Administrador.

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: ejecuta este script como Administrador.
    exit /b 1
)

set "DEST=%SystemRoot%\twain_32\Color USB Scanner"

echo.
echo Desinstalando ColorUSBScanner...
echo.

if exist "%DEST%\ColorUSBScanner.ds" (
    del /f /q "%DEST%\ColorUSBScanner.ds"
    if errorlevel 1 (
        echo AVISO: no se pudo borrar "%DEST%\ColorUSBScanner.ds".
        echo Cierra NAPS2 y cualquier aplicacion TWAIN y vuelve a intentarlo.
    ) else (
        echo Eliminado: "%DEST%\ColorUSBScanner.ds"
    )
) else (
    echo No existe "%DEST%\ColorUSBScanner.ds".
)

rem Si el usuario copio el DS al nivel superior de twain_32 (fallback del README).
if exist "%SystemRoot%\twain_32\ColorUSBScanner.ds" (
    del /f /q "%SystemRoot%\twain_32\ColorUSBScanner.ds"
    echo Eliminado tambien: "%SystemRoot%\twain_32\ColorUSBScanner.ds"
)

rem Restaurar el DS original de Plustek si se hizo copia de seguridad.
if exist "%DEST%\Color USB Scanner.ds.plustek.bak" (
    if exist "%DEST%\Color USB Scanner.ds" (
        echo AVISO: ya existe "%DEST%\Color USB Scanner.ds";
        echo se conserva y el backup queda como .plustek.bak.
    ) else (
        move /y "%DEST%\Color USB Scanner.ds.plustek.bak" "%DEST%\Color USB Scanner.ds" >nul
        if errorlevel 1 (
            echo AVISO: no se pudo restaurar el Data Source original.
        ) else (
            echo Restaurado: "%DEST%\Color USB Scanner.ds"
        )
    )
)

rem Opcional: borrar tambien las dependencias copiadas por install.cmd.
if /I "%~1"=="/deps" (
    if exist "%DEST%\libusb-1.0.dll" del /f /q "%DEST%\libusb-1.0.dll"
    if exist "%DEST%\cism216.fw" del /f /q "%DEST%\cism216.fw"
    echo Dependencias eliminadas de "%DEST%".
) else (
    echo.
    echo Nota: libusb-1.0.dll y cism216.fw se dejan en la carpeta.
    echo Para borrarlos tambien: uninstall.cmd /deps
)

echo.
echo === Restaurar el driver del escaner (Zadig) ===
echo.
echo Este script NO puede restaurar con seguridad un driver de kernel.
echo Sigue estos pasos:
echo.
echo   1. Abre Zadig 2.9 como Administrador.
echo   2. Options -^> List All Devices.
echo   3. Selecciona 07B3 0412.
echo   4. Pulsa "Restore Default Driver" (o reinstala el INF de Plustek).
echo   5. Desconecta y reconecta el escaner.
echo   6. Si usabas la app "Escaner" de Windows, reinstala el software/WIA
echo      original de Plustek.
echo.
echo Despues, cierra y reabre NAPS2.
echo.

rem Abrir Zadig si esta en una ruta habitual, para facilitar la restauracion.
set "ZADIG="
if exist "%ProgramFiles%\Zadig\zadig.exe" set "ZADIG=%ProgramFiles%\Zadig\zadig.exe"
if not defined ZADIG if exist "%ProgramFiles(x86)%\Zadig\zadig.exe" set "ZADIG=%ProgramFiles(x86)%\Zadig\zadig.exe"
if defined ZADIG (
    echo Abriendo Zadig...
    start "" "%ZADIG%"
) else (
    echo No se encontro zadig.exe en Program Files. Abrelo manualmente.
)

pause
exit /b 0
