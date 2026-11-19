# Portar el backend SANE `gt68xx` al Data Source TWAIN

Este documento mapea el código de SANE con el esqueleto de `ColorUSBScanner.ds`
y explica qué falta para escanear de verdad.

Estado del port (19/11/2026):

- [x] DS TWAIN de 32 bits, `DS_Entry`, identidad y enumeración en NAPS2.
- [x] `DAT_ENTRYPOINT` (TWAIN 2.x), `MSG_XFERREADY` y transferencia por
  `DAT_IMAGEMEMXFER`; validado con NAPS2 7.5.3 y una imagen sintética.
- [x] Apertura USB con libusb y descubrimiento de endpoints.
- [x] Subida de `cism216.fw` (bloques de 64 bytes, verificación y arranque
  `0x69`).
- [x] Inicialización: `get_id` `0x2e`, `stop_scan` `0x41`, lámpara `0x25`.
- [x] Motor/posicionamiento: `move_paper` `0x82`, `is_moving` `0x17`.
- [x] `setup_scan` `0x20`, AFE `0x22`, exposición `0x76`, `start_scan` `0x43`,
  `read_scanned_data` `0x35`.
- [x] Lectura bulk en line mode BGR (3 planos de `scan_bpl`) y gamma 2.0;
  ruta color 8-bit, 300 dpi, A4.
- [ ] Calibración de sombreado (`gt68xx_sheetfed_scanner_calibrate`).
- [ ] Pruebas con el hardware y ajuste de offsets/exposure/AFE.

Fuente de referencia: <https://gitlab.com/sane-project/backends/-/tree/master/backend>
Archivos relevantes (copia local en `reference/`):

- `gt68xx.c` — capa SANE, apertura, opciones, `download_firmware_file()`.
- `gt68xx_low.c` — transporte USB: `gt68xx_device_memory_write/read`,
  `gt68xx_device_req`, `gt68xx_device_download_firmware`, lectura raw.
- `gt68xx_generic.c` — comandos comunes: `setup_scan`, `set_afe`,
  `set_exposure_time`, `start_scan`, `read_scanned_data`, `move_paper`.
- `gt68xx_mid.c` — line reader y conversión de los 3 planos CIS.
- `gt68xx_high.c` — geometría, calibración, gamma.
- `gt68xx_devices.c` — tabla de modelos y `command_set` por chipset.
- `gt68xx_gt6816.c` — comandos específicos del GT-6816, incluido
  `gt6816_download_firmware()`.

---

## 1. Modelo M12 y command set

En `gt68xx_devices.c`, el ID `07b3:0412` apunta a
`plustek_opticslim_m12_model`:

```c
{0x07b3, 0x0412, &plustek_opticslim_m12_model},
```

El modelo usa `mustek_gt6816_sheetfed_command_set`. Parámetros clave:

- Firmware: `cism216.fw`.
- Resolución óptica máxima: 600 dpi; motor: 1200 dpi.
- Resoluciones X: 600, 400, 300, 200, 150, 100, 50.
- Resoluciones Y: 1200, 600, 400, 300, 200, 150, 100, 50.
- Profundidades: 16/8 bits gris y 16/8 bits color.
- Área: inicio (0.0, 1.5) mm; tamaño 218.0 x 299.0 mm.
- Es CIS, sheet-fed, con calibración.
- Offset/gain por defecto: `{0x24, 0x0a, 0x23, 0x0f, 0x23, 0x0b}`.
- Exposure por defecto: `{0x157, 0x157, 0x157}`.
- Gamma por defecto: 2.0.
- Flags: `NO_POWER_STATUS | SHEET_FED | OFFSET_INV | HAS_CALIBRATE`.

`iriscan_express_2_model` es equivalente (mismo `command_set`, mismos valores
salvo offset/gain `{0x22,0x0c,0x22,0x0a,0x24,0x09}`). El DS puede usar los
parámetros del M12 para ambos.

---

## 2. Command set del GT-6816

`mustek_gt6816_sheetfed_command_set`:

| Campo | Valor | Uso |
|---|---:|---|
| `request_type` | `0x40` | Control OUT vendor |
| `request` | `0x01` | bRequest |
| `memory_read_value` | `0x200C` | wValue para leer memoria |
| `memory_write_value` | `0x200B` | wValue para escribir memoria |
| `send_cmd_value` | `0x2010` | wValue para enviar comando de 8 bytes |
| `send_cmd_index` | `0x3F40` | wIndex del comando |
| `recv_res_value` | `0x2011` | wValue para leer respuesta |
| `recv_res_index` | `0x3F00` | wIndex de la respuesta |
| `send_small_cmd_value` | `0x2012` | Comando "small" (8 bytes) |
| `recv_small_res_value` | `0x2013` | Respuesta "small" |

La implementación de `gt68xx_low.c`:

```c
/* memory write */
sanei_usb_control_msg(fd, 0x40, 0x01, 0x200B, addr, size, data);

/* memory read */
sanei_usb_control_msg(fd, 0xC0, 0x01, 0x200C, addr, size, data);

/* comando normal */
sanei_usb_control_msg(fd, 0x40, 0x01, 0x2010, 0x3F40, 64, cmd);
sanei_usb_control_msg(fd, 0xC0, 0x01, 0x2011, 0x3F00, 64, res);
```

`gt68xx_usb.c` ya expone `gt68xx_usb_control()` y
`gt68xx_proto.c` implementa estos helpers.

---

## 3. Subida de firmware (ya implementada)

`gt6816_download_firmware()` en `gt68xx_gt6816.c`:

1. Para `addr = 0; addr < size; addr += 64`:
   - Copia 64 bytes del firmware (el último bloque se rellena con ceros).
   - `memory_write(addr, 64, block)`.
   - `memory_read(addr, 64, check)`.
   - Compara `block` y `check`; si difieren, error de E/S.
2. Envía el comando de arranque de 64 bytes:
   ```
   0x69, 0x01, addr_lo, addr_hi, 0, ..., 0
   ```
   con `gt68xx_device_req()`.
3. `addr` es la dirección final (tamaño redondeado a 64).

`gt68xx_proto.c` implementa exactamente esto en
`gt68xx_upload_firmware()`, con `cism216.fw` (8192 bytes = 128 bloques).

**Estado:** la subida de firmware está portada. Si el dispositivo está en
WinUSB, `gt68xx_proto_init()` debería cargar el firmware y arrancarlo.

---

## 4. Implementación actual y lo que falta

### 4.1 Motor, setup y lectura (implementado)

`gt68xx_proto.c` implementa la secuencia de `sane_start` para el M12:

1. `stop_scan` small `0x41` y lámpara `0x25`.
2. `move_paper` `0x82` con `abs_y0=70` (A4, y0=1.5 mm + offset 1.5 mm) y
   espera `is_moving` `0x17`.
3. `setup_scan` `0x20`:
   - 300 dpi, 8 bits, color (`color_mode_code=0x84`).
   - `abs_x0=0`, `abs_y0=70`, `abs_xs=4992`, `abs_ys=7016`.
   - `scan_xs=2496`, `scan_bpl=2496`, `scan_ys=10524` (3 líneas CIS).
   - `motor_mode_1=0x01` (mbs=1, mds=1, mas=0), `motor_mode_2=0x00`.
4. `set_exposure` `0x76` (`0x157` por canal) y `set_afe` `0x22`
   (`{0x24,0x0a,0x23,0x0f,0x23,0x0b}`).
5. `start_scan` `0x43`, sondeo `read_scanned_data` `0x35`.
6. Lectura bulk de `scan_bpl*3 = 7488` bytes por línea RGB.
   - Primer plano: B; segundo: G; tercero: R (`COLOR_ORDER_BGR`).
   - Se descartan los 16 píxeles de padding (2496 - 2480).
7. `stop_scan`, `paperfeed` `0x83` (expulsar la hoja) y lámpara off.

La imagen de salida es 2480x3508 (A4 a 300 dpi), RGB de 8 bits, con gamma 2.0.

### 4.2 Lo que falta

- **Calibración de sombreado**: `gt68xx_sheetfed_scanner_calibrate()` no está
  portada. Sin ella puede haber bandas verticales, polvo y dominante de color.
  Es el siguiente paso para calidad de producción.
- **Prueba física**: validar que `move_paper` alimenta la hoja, que el orden
  BGR es correcto y que el área no sale desplazada.
- **Otros modos**: gris nativo, 16 bits, otras resoluciones y área distinta de
  A4. La ruta actual convierte gris desde la Captura RGB, pero la geometría
  está pensada para A4.
- **Doble cara / alimentador**: el M12 es sheet-fed simple; no hay dúplex.

### 4.3 Geometría

- `base_xdpi=600`, `base_ydpi=600`, `optical_ydpi=1200`.
- Área física del modelo: 218 x 299 mm; para A4 se usa 210 x 297 mm,
  inicio y=1.5 mm.
- El modelo usa `SHEET_FED` y `HAS_CALIBRATE`; el port aplica los valores
  AFE/exposición por defecto del modelo, pero no ejecuta el auto-AFE ni la
  calibración de sombreado.

---

## 5. Cómo probar el port

1. **SANE en Linux/WSL** con el mismo hardware y `cism216.fw`:
   ```bash
   scanimage -L
   scanimage --mode Color --resolution 300 -o sane_test.pnm
   ```
   Sirve para obtener una imagen de referencia y comparar.
2. **USBPcap/Wireshark** en Windows mientras VueScan escanea:
   - Filtro `usb.idVendor == 0x07b3 && usb.idProduct == 0x0412`.
   - Captura los endpoints bulk.
   - Compara la secuencia con `gt68xx_device_req()`/`memory_write/read`.
3. **DS en modo real**:
   - Enlaza a WinUSB con Zadig.
   - Copia `libusb-1.0.dll` de 32 bits y `cism216.fw` junto al `.ds`.
   - Compila, instala y ejecuta NAPS2 32 bits en color, 300 dpi, A4.
   - `gt68xx_proto_init()` carga el firmware y `gt68xx_proto_scan()` ejecuta
     motor + lectura real.

---

## 6. Licencia

El backend SANE `gt68xx` es **GPL-2.0-or-later**. Si copias/portas su código,
`ColorUSBScanner.ds` es una obra derivada y debe distribuirse bajo la misma
licencia, con atribución al proyecto SANE y a los autores del backend.

Si necesitas una licencia no-GPL, debes reimplementar el protocolo desde la
especificación y las trazas, sin copiar código de SANE. La subida de firmware
y los comandos USB son hechos técnicos; la implementación concreta es la que
está sujeta a copyright.
