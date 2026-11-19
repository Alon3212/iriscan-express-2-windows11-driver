# ColorUSBScanner.ds — TWAIN Data Source para Plustek OpticSlim M12 / IRIScan Express 2

**Hardware:** USB `VID_07B3&PID_0412`, chipset **GT-6816**  
**DS:** 32 bits, `C:\Windows\twain_32\Color USB Scanner\ColorUSBScanner.ds`  
**Cliente de prueba:** NAPS2 7.x / 8.x de 32 bits

---

## Estado actual (importante)

El **escaneo real está implementado** para la ruta principal: **color 8-bit,
300 dpi, A4 (210x297 mm)**, sheet-fed y line mode CIS. Está portado de SANE
`gt68xx` en `src/gt68xx_proto.c`:

- Firmware `cism216.fw` en bloques de 64 bytes, verificación y arranque `0x69`.
- `get_id`, `stop_scan` (comando small `0x41`), lámpara `0x25`.
- `move_paper` `0x82`, espera de motor `0x17`.
- `setup_scan` `0x20` con geometría M12/A4.
- AFE `0x22`, exposición `0x76`, `start_scan` `0x43`, `read_scanned_data` `0x35`.
- Lectura bulk de 3 planos BGR por línea (`gt68xx_line_reader` BGR line mode).
- Gamma 2.0 equivalente a SANE.

**No he podido probarlo contra el hardware físico en este entorno** (no hay
WinUSB/libusb activo aquí). La integración TWAIN sí está validada con NAPS2
7.5.3 y una imagen sintética; el PDF de prueba se genera correctamente.

Lo que falta / limitaciones:

1. **Calibración de sombreado**: el M12 tiene `GT68XX_FLAG_HAS_CALIBRATE`.
   Ahora se escanea sin `gt68xx_sheetfed_scanner_calibrate`, así que puede
   haber bandas o dominante de color. El siguiente paso es portar el
   calibrador y guardar la calibración.
2. Probar con el escáner real y ajustar offset/exposure/AFE si la imagen
   sale oscura, cortada o con el papel mal posicionado.
3. Soportar resoluciones/modos distintos de color 300 A4 (la geometría está
   calculada para A4 y line mode color).
4. Validar el orden de canales y el área exacta con una página de prueba.

El **modo de prueba** (`COLORUSBSCANNER_TEST_PATTERN=1`) sigue disponible y no
toca USB.

Ver `PORTING_GT68XX.md`.

---

## Requisitos

- Windows 10/11 **x64** (el DS es de 32 bits; NAPS2 debe ser de 32 bits).
- **NAPS2 7.x/8.x de 32 bits** (el `NAPS2.exe` que hay en `C:\Program Files\NAPS2`
  es PE32, así que sirve).
- **libusb-1.0.dll de 32 bits** (MSYS2 `mingw-w64-i686-libusb`, o un build
  oficial de libusb). No sirve la DLL de 64 bits.
- **Zadig** para enlazar el escáner a **WinUSB** (o libusbK):
  <https://zadig.akeo.ie/>
- Compilador:
  - MinGW-w64 i686 (`i686-w64-mingw32-gcc`), o
  - Visual Studio 2022 con la plataforma **Win32**.
- Firmware `cism216.fw` (ya está en
  `..\pkg\cab_extract\cism216.fw`).

### Sobre Zadig

El DS usa libusb, así que el escáner **no puede estar enlazado a `usbscan.sys`**
(el driver de Plustek). Con Zadig:

1. Opciones → List All Devices.
2. Selecciona `07B3 0412` ("600dpi USB Scanner").
3. Driver: **WinUSB** (o libusbK).
4. Install Driver.
5. Desconecta/reconecta.

Para volver atrás, en Zadig elige "Restore Default Driver" o reinstala el INF
de Plustek.

---

## Compilar

### MinGW-w64 (32 bits)

```bat
cmake -S . -B build -G "MinGW Makefiles" ^
      -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake
cmake --build build
```

o, sin CMake:

```bat
make -f Makefile.mingw
```

El resultado es `ColorUSBScanner.ds`.

### Visual Studio 2022 (Win32)

```bat
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A Win32
cmake --build build-vs --config Release
```

El `.ds` queda en `build-vs\Release\ColorUSBScanner.ds`.

> Si CMake dice que `CMAKE_SIZEOF_VOID_P` es 8, estás compilando en x64.
> Usa `-A Win32` o el toolchain i686.

---

## Instalar

1. Copia `libusb-1.0.dll` (32 bits) a la raíz de este proyecto o a
   `third_party\libusb\`.
2. Ejecuta **como Administrador**:

```bat
install.cmd
```

El script copia a `C:\Windows\twain_32\Color USB Scanner\`:

```
ColorUSBScanner.ds
libusb-1.0.dll     (si lo encuentra)
cism216.fw         (si lo encuentra)
```

No hace falta registrar nada en el registro: el DSM de TWAIN enumera los `.ds`
de `twain_32` (incluidas subcarpetas, como hace el DS de Canon).

---

## Probar sin hardware (modo de prueba)

Antes de conectar el escáner, valida el DS con una imagen sintética:

```bat
set COLORUSBSCANNER_TEST_PATTERN=1
"C:\Program Files\NAPS2\NAPS2.exe"
```

En NAPS2:

1. Nuevo perfil → **TWAIN**.
2. Debe aparecer **"Color USB Scanner"**.
3. Escanea: obtendrás una imagen de prueba (marco negro, cuadrícula, cruz y
   gradiente) al DPI seleccionado.

### Validación realizada

Con el DS instalado en `C:\Windows\twain_32\Color USB Scanner\ColorUSBScanner.ds`
y `COLORUSBSCANNER_TEST_PATTERN=1`, NAPS2 7.5.3 (32 bits) mostró
"Color USB Scanner" y guardó:

- Gris 100 dpi: 827x1169, PDF de ~36 KB.
- Color 300 dpi: 2480x3508, PDF de ~243 KB
  (`test_color_300.pdf`).

Nota: NAPS2 imprime una `ArgumentNullException` no fatal después de
"Scanned page 1", pero el PDF se genera correctamente. Es un detalle del
procesado de imagen de NAPS2 con este DS; no afecta a la transferencia TWAIN.

Si NAPS2 no lo enumera:

- Comprueba que `ColorUSBScanner.ds` es de 32 bits
  (`dumpbin /headers` → `machine (x86)`).
- Comprueba que `libusb-1.0.dll` de 32 bits está junto al `.ds`.
- Prueba a copiar el `.ds` también a `C:\Windows\twain_32\` (algunos DSM solo
  miran el nivel superior).
- Revisa el visor de eventos o usa `OutputDebugString`/DebugView.

## Probar con hardware

1. Enlaza el escáner a **WinUSB** con Zadig y desconecta/reconecta.
2. Copia `libusb-1.0.dll` de 32 bits y `cism216.fw` junto al `.ds`
   (`install.cmd` lo hace si están en el proyecto).
3. **No** definas `COLORUSBSCANNER_TEST_PATTERN`.
4. En NAPS2, perfil TWAIN "Color USB Scanner", **Color**, **300 dpi**, A4.
   También puedes ejecutar `scan_real.cmd`.
5. Al escanear, el DS:
   - sube `cism216.fw` y arranca el firmware (`0x69`);
   - enciende la lámpara, mueve el papel a `abs_y0=70`;
   - configura `0x20` para 2480x3508, color 8-bit;
   - lee 3508 líneas de 3 planos BGR por bulk IN;
   - devuelve RGB 24-bit a NAPS2.

Si falla, el mensaje de `gt68xx_proto_last_error()` se registra con
`COLORUSBSCANNER_DEBUG=1` en `%TEMP%\ColorUSBScanner.log`. Los puntos más
probables son: driver WinUSB no aplicado, `libusb-1.0.dll` de 64 bits, falta
`cism216.fw`, o papel no detectado/colocado.

---

## Estructura

```
twain_ds/
├─ CMakeLists.txt
├─ Makefile.mingw
├─ toolchain-mingw32.cmake
├─ install.cmd
├─ scan_real.cmd
├─ test_pattern.cmd
├─ README.md
├─ PORTING_GT68XX.md
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
│  └─ ColorUSBScanner-test.ds  binario de prueba (test pattern + ruta real)
└─ reference/             fuentes SANE gt68xx de consulta (no se compilan)
```

---

## Limitaciones y avisos

- **No es un driver WIA**: no aparecerá en la app "Escáner" de Windows. Es un
  Data Source TWAIN para NAPS2/IrfanView/Photoshop, etc.
- El DS es de **32 bits** a propósito: NAPS2 7.x es de 32 bits y el TWAIN de
  64 bits es minoritario.
- El modo de prueba no toca el hardware. El modo real está implementado pero
  **no probado en hardware físico** en este entorno; falta validar alimentación
  de papel, orden BGR y área.
- **Sin calibración de sombreado**: puede haber bandas/dominante de color.
- **Seguridad del hardware:** no envíes comandos de motor/firmware inventados.
  Un comando incorrecto puede dejar el motor bloqueado. Ten el USB a mano.
- **Licencia:** si portas código de SANE `gt68xx` (GPL-2.0-or-later), el DS es
  una obra derivada y debe distribuirse bajo GPL-2.0-or-later con atribución.
  Si no quieres GPL, reimplementa el protocolo desde la especificación/trazas
  sin copiar código.
- Sin garantía. Prueba en un equipo/VM de pruebas.
