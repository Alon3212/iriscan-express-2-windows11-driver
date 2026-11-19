# ColorUSBScanner.ds — Data Source TWAIN para Plustek OpticSlim M12 / IRIScan Express 2

- **Hardware probado:** USB `VID_07B3&PID_0412`, chipset **GT-6816**.
- **DS:** 32 bits, `%SystemRoot%\twain_32\Color USB Scanner\ColorUSBScanner.ds`.
- **Cliente probado:** NAPS2 7.5.3 **32 bits** (TWAIN).
- **Estado:** escaneo real confirmado en Windows 11: **A4, 300 dpi, color, 2480×3508**.

---

## Inicio rápido

Para usar el escáner en otro PC:

1. Consigue `libusb-1.0.dll` **de 32 bits** (probado con **libusb 1.0.30**, build MinGW32).
2. Consigue `cism216.fw` (8192 bytes) extrayéndolo de un instalador oficial de Plustek/IRIScan. **No se redistribuye** en este repositorio.
3. Coloca ambos junto a este README, en `third_party\libusb\` o en `firmware\`.
4. Ejecuta `install.cmd` **como Administrador**.
5. Con **Zadig 2.9**, enlaza `07B3:0412` a **WinUSB**.
6. Abre NAPS2 **32 bits**, crea un perfil **TWAIN**, selecciona **Color USB Scanner**, **Color**, **300 dpi**, A4.
7. Comprueba con `scan_real.cmd` o escanea desde NAPS2.

Si solo quieres validar la instalación TWAIN sin hardware:

```bat
set COLORUSBSCANNER_TEST_PATTERN=1
test_pattern.cmd
```

---

## Estado y validación real

Probado con:

- **Windows 11**
- **NAPS2 7.5.3 de 32 bits**, driver TWAIN
- USB **`07B3:0412`**
- **libusb 1.0.30** (32 bits, MinGW32)
- **Zadig 2.9**
- Firmware `cism216.fw` de 8192 bytes
- Resultado: página **A4**, **300 dpi**, **color**, imagen **2480×3508**, legible.

La imagen real de prueba no se publica por privacidad. El registro de la prueba y los datos técnicos están en [`docs/VALIDATION.md`](docs/VALIDATION.md).

También está validada la integración TWAIN sin hardware con una imagen sintética:

- Gris 100 dpi: 827×1169, PDF de ~36 KB.
- Color 300 dpi: 2480×3508, PDF de ~243 KB (`test_color_300.pdf`).

> NAPS2 7.5.3 puede imprimir una `ArgumentNullException` no fatal después de
> “Scanned page 1”. En la prueba el PDF se generó correctamente; el problema
> está en el procesado de imagen de NAPS2, no en la transferencia TWAIN.

**Importante:** el escaneo real está probado con **color 300 dpi y A4**. Otras
resoluciones y modos no están validados todavía.

---

## Hardware y compatibilidad

- **Soportado/probado:** `07B3:0412` (Plustek OpticSlim M12; unidad reportada también como IRIScan Express 2).
- **No soportado por el código actual:** `07B3:045F`. En la tabla de SANE ese ID corresponde a `iriscan_express_2_model`; este DS solo busca `0412`. Si tu unidad es `045F`, no funcionará sin añadir ese ID/modelo.
- El M12 es **sheet-fed** (alimentación hoja a hoja), **CIS**, sin dúplex.
- El DS es **32 bits**: úsalo desde NAPS2 32 bits u otra aplicación TWAIN de 32 bits. Las aplicaciones de 64 bits no cargan un `.ds` de 32 bits.

---

## Requisitos y dependencias

### 1. libusb 1.0.30 de 32 bits

El DS carga `libusb-1.0.dll` de forma dinámica desde su propia carpeta. No se
linkea contra `libusb-1.0.lib`.

- Versión probada: **libusb 1.0.30**, build **MinGW32**.
- Descarga oficial: <https://github.com/libusb/libusb/releases>
- Alternativa MSYS2: paquete `mingw-w64-i686-libusb` (versión 1.0.x de 32 bits).
- Debe ser **32 bits**. Una DLL de 64 bits no se puede cargar en el DS.
- Si tu build de MinGW32 depende de `libwinpthread-1.dll` u otras DLLs de
  runtime, cópialas también junto al `.ds`.

Colócala en una de estas rutas:

```
libusb-1.0.dll
third_party\libusb\libusb-1.0.dll
```

`install.cmd` la copia a `%SystemRoot%\twain_32\Color USB Scanner\` si la
encuentra. Si no aparece, el modo de prueba seguirá funcionando, pero el
escaneo real fallará al abrir USB.

### 2. Firmware `cism216.fw`

El firmware **no se redistribuye** en este repositorio: es firmware propietario
de Plustek. Debes extraerlo de un instalador oficial de Plustek/IRIScan que
corresponda a tu escáner.

- Nombre: `cism216.fw`
- Tamaño esperado: **8192 bytes**
- SHA-256 de la copia usada en la prueba:
  `9730A258DC7152184A9C4FB7C8D12ABB3B4DE537276F4686525BEB6A72F98C15`
- Colócalo en la raíz del proyecto, en `firmware\` o deja que `install.cmd` lo
  busque en `..\pkg\cab_extract\` si conservas esa estructura.

Ver [`firmware/README.md`](firmware/README.md) para instrucciones y aviso legal.

### 3. Zadig 2.9 y WinUSB

El DS usa libusb, así que el escáner **no puede estar enlazado a `usbscan.sys`**
(el driver de Plustek). Con **Zadig 2.9**:

1. Abre Zadig como Administrador.
2. **Options → List All Devices**.
3. Selecciona `07B3 0412` (“600dpi USB Scanner”).
4. En el cuadro de driver elige **WinUSB** (o libusbK).
5. Pulsa **Replace Driver** / **Install Driver**.
6. Desconecta y reconecta el escáner.

Para volver al driver original, usa **Zadig → Restore Default Driver** o
reinstala el INF de Plustek. Ver [Desinstalar y restaurar](#desinstalar-y-restaurar).

> Zadig sustituye el driver del dispositivo. Esto puede hacer que deje de
> funcionar la app “Escáner” de Windows/WIA y el software original de Plustek.
> Hazlo solo si aceptas ese cambio y ten a mano el instalador original.

### 4. Compilador (solo si compilas desde fuente)

- **MinGW-w64 i686** (`i686-w64-mingw32-gcc`), o
- **Visual Studio 2022** con plataforma **Win32** (no verificado en esta revisión).

El proyecto incluye las cabeceras `twain.h` y `libusb.h`; no necesitas el SDK
de TWAIN ni linkear libusb.

---

## Compilar

### MinGW-w64 de 32 bits

```bat
cmake -S . -B build -G "MinGW Makefiles" ^
      -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake
cmake --build build
```

o, sin CMake:

```bat
make -f Makefile.mingw
```

El resultado es `ColorUSBScanner.ds` (PE32).

> En `cmd.exe` la continuación de línea es `^`. Si usas PowerShell, usa una
> sola línea o el carácter de continuación de PowerShell.

### Visual Studio 2022 (Win32)

```bat
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A Win32
cmake --build build-vs --config Release
```

El `.ds` queda en `build-vs\Release\ColorUSBScanner.ds`.

> La ruta de Visual Studio no está verificada en la prueba original. Si vas a
> publicar un binario, comprueba con `dumpbin /exports` que aparece exactamente
> `DS_Entry` y que el PE es `machine (x86)`.

Si CMake dice que `CMAKE_SIZEOF_VOID_P` es 8, estás compilando en x64. Usa
`-A Win32` o el toolchain i686.

---

## Instalar

1. Copia `libusb-1.0.dll` (32 bits) a la raíz del proyecto o a
   `third_party\libusb\`.
2. Copia `cism216.fw` a la raíz o a `firmware\`.
3. Ejecuta **como Administrador**:

```bat
install.cmd
```

El script copia a `%SystemRoot%\twain_32\Color USB Scanner\`:

```
ColorUSBScanner.ds
libusb-1.0.dll     (si lo encuentra)
cism216.fw         (si lo encuentra)
```

Si existe el Data Source antiguo `Color USB Scanner.ds` de Plustek, lo aparta
como `Color USB Scanner.ds.plustek.bak` para que NAPS2 no muestre dos entradas
con el mismo nombre.

No hace falta registrar nada en el registro: el DSM de TWAIN enumera los `.ds`
de `twain_32` (incluidas subcarpetas, como hace el DS de Canon). Cierra y
reabre NAPS2 después de instalar.

Si tu aplicación TWAIN no enumera el DS, copia también
`ColorUSBScanner.ds` a `%SystemRoot%\twain_32\` (algunos DSM solo miran el
nivel superior).

---

## Probar sin hardware (modo de prueba)

Antes de conectar el escáner, valida el DS con una imagen sintética:

```bat
set COLORUSBSCANNER_TEST_PATTERN=1
test_pattern.cmd
```

O abre NAPS2 y escanea: obtendrás una imagen de prueba (marco negro,
cuadrícula, cruz y gradiente) al DPI seleccionado.

El modo de prueba **no toca USB**: puedes usarlo sin Zadig, sin libusb y sin
firmware.

---

## Probar con hardware

1. Enlaza el escáner a **WinUSB** con Zadig 2.9 y desconecta/reconecta.
2. Copia `libusb-1.0.dll` de 32 bits y `cism216.fw` junto al `.ds`.
3. **No** definas `COLORUSBSCANNER_TEST_PATTERN`.
4. En NAPS2: perfil TWAIN **Color USB Scanner**, **Color**, **300 dpi**, A4.
   También puedes ejecutar:

```bat
scan_real.cmd
```

o manualmente:

```bat
set "COLORUSBSCANNER_TEST_PATTERN="
set COLORUSBSCANNER_DEBUG=1
"C:\Program Files\NAPS2\NAPS2.Console.exe" ^
  --driver twain --device "Color USB Scanner" ^
  --dpi 300 --bitdepth color -o real_scan.pdf -v
```

Al escanear, el DS:

- sube `cism216.fw` y arranca el firmware (`0x69`);
- enciende la lámpara y mueve el papel a `abs_y0=70`;
- configura `0x20` para 2480×3508, color 8-bit;
- lee 3508 líneas de 3 planos BGR por bulk IN;
- devuelve RGB 24-bit a NAPS2.

El resultado esperado es un PDF A4 a 300 dpi y 2480×3508 píxeles.

### Log de errores

Con `COLORUSBSCANNER_DEBUG=1`, el DS escribe en:

```
%TEMP%\ColorUSBScanner.log
```

Los errores más probables:

- `OPENDS usb_open fallo`: el escáner no está en WinUSB, o `libusb-1.0.dll`
  es de 64 bits / no está junto al `.ds`.
- `No se pudo leer el firmware`: falta `cism216.fw` o la ruta es incorrecta.
- `El firmware arranco pero el escaner no responde a get_id`: firmware
  incorrecto o USB en mal estado.
- `Fallo move_paper` / timeout de motor: papel mal insertado o sensor.
- `El escaner no esta listo para transferir (0x35)`: no llegó a iniciar el
  motor/scan.

---

## Problemas conocidos y limitaciones

- **Sin calibración de sombreado**: no se ha portado
  `gt68xx_sheetfed_scanner_calibrate()`. La imagen puede tener bandas
  verticales, polvo o dominante de color. Es el siguiente paso para calidad de
  producción.
- **Solo 300 dpi color A4 validado**. Otras resoluciones y el modo gris no
  están confirmados en hardware.
- **`07B3:045F` no soportado** por el código actual (solo busca `0412`).
- **DS de 32 bits**: no aparece en aplicaciones TWAIN de 64 bits.
- **No es un driver WIA**: no aparece en la app “Escáner” de Windows. Es un
  Data Source TWAIN para NAPS2, IrfanView, Photoshop, etc.
- **DS sin firmar**: SmartScreen o el antivirus pueden avisar. Verifica el
  SHA-256 del binario y no ejecutes releases de terceros.
- **Zadig cambia el driver del dispositivo**: puede romper WIA/Plustek; usa
  `uninstall.cmd` y restaura el driver original si es necesario.

---

## Desinstalar y restaurar

Ejecuta **como Administrador**:

```bat
uninstall.cmd
```

El script:

- elimina `ColorUSBScanner.ds` de `%SystemRoot%\twain_32\Color USB Scanner\`;
- restaura `Color USB Scanner.ds.plustek.bak` como `Color USB Scanner.ds` si
  existe;
- deja `libusb-1.0.dll` y `cism216.fw` en la carpeta (puedes borrarlos a mano).

Después, para devolver el escáner al driver original:

1. Abre **Zadig 2.9** como Administrador.
2. **Options → List All Devices**.
3. Selecciona `07B3 0412`.
4. Pulsa **Restore Default Driver** (o reinstala el INF de Plustek).
5. Desconecta/reconecta el escáner.
6. Si usabas la app “Escáner” de Windows, reinstala el software/WIA de Plustek.

`uninstall.cmd` no puede automatizar con seguridad la restauración del driver
de kernel; Zadig o el INF original son la vía recomendada.

---

## Estructura del repositorio

```
.
├─ CMakeLists.txt
├─ Makefile.mingw
├─ toolchain-mingw32.cmake
├─ install.cmd
├─ uninstall.cmd
├─ scan_real.cmd
├─ test_pattern.cmd
├─ README.md
├─ LICENSE
├─ NOTICE.md
├─ PORTING_GT68XX.md
├─ docs/
│  └─ VALIDATION.md
├─ firmware/
│  └─ README.md          (no incluye cism216.fw)
├─ src/
│  ├─ twain_ds.c          DS_Entry, identidad, capacidades, transferencia
│  ├─ libusb_dyn.c/.h     carga dinámica de libusb-1.0.dll
│  ├─ gt68xx_usb.c/.h     open/claim/reset, endpoints, bulk/control
│  ├─ gt68xx_proto.c/.h   firmware, init, motor, setup y escaneo real
│  └─ ColorUSBScanner.def exporta DS_Entry
├─ third_party/
│  ├─ twain.h             cabecera TWAIN (TWAIN Working Group)
│  └─ libusb/libusb.h     cabecera libusb-1.0
├─ prebuilt/
│  └─ ColorUSBScanner-test.ds  binario de conveniencia (ver abajo)
└─ reference/             fuentes SANE gt68xx de consulta (no se compilan)
```

### Sobre `prebuilt/ColorUSBScanner-test.ds`

Es un binario de conveniencia para probar TWAIN sin compilar. No es un release
oficial y puede no corresponder al último commit. Para uso real, compila desde
fuente o usa un release con SHA-256 publicado.

---

## Seguridad

- El `.ds` **no está firmado**. Windows SmartScreen o un antivirus pueden
  bloquearlo; no desactives la protección del sistema para evitarlo. Compila
  desde fuente o verifica el hash del release.
- El firmware se sube al escáner al abrir el DS. Un firmware incorrecto o
  interrumpido puede dejar el dispositivo en mal estado; no desconectes
  durante la subida.
- `install.cmd` y `uninstall.cmd` modifican `%SystemRoot%\twain_32` y el driver
  del dispositivo. Ejecútalos solo como Administrador y con una copia de
  seguridad del driver original.
- No redistribuyas `cism216.fw` sin permiso de Plustek.

---

## Licencia y atribución

El código de `src/` es una obra derivada del backend SANE `gt68xx` y se
distribuye bajo **GPL-2.0-or-later**. El texto completo de la GPL v2 está en
[`LICENSE`](LICENSE). Atribución y detalles en [`NOTICE.md`](NOTICE.md).

- SANE `gt68xx`: <https://gitlab.com/sane-project/backends>
- `third_party/twain.h`: TWAIN Working Group.
- `third_party/libusb/libusb.h`: libusb, LGPL-2.1-or-later.
- `cism216.fw`: firmware propietario de Plustek; **no incluido**.

Si no puedes aceptar la GPL, debes reimplementar el protocolo desde cero sin
copiar código de SANE.

---

## Sin garantía

Este software se proporciona “AS IS”, sin garantía de ningún tipo. El uso de
comandos de firmware/motor sobre el hardware es bajo tu responsabilidad. Ten el
USB a mano y prueba primero en un equipo/VM de pruebas.
