# Registro de validación

## Prueba real confirmada

| Dato | Valor |
|---|---|
| Sistema | Windows 11 |
| Aplicación | NAPS2 7.5.3, 32 bits, driver TWAIN |
| Escáner | Plustek OpticSlim M12 / IRIScan Express 2 |
| USB | `07B3:0412` |
| Chipset | GT-6816 |
| Firmware | `cism216.fw`, 8192 bytes |
| libusb | 1.0.30, 32 bits, MinGW32 |
| Zadig | 2.9 |
| Modo | Color, 8 bits por canal |
| Resolución | 300 dpi |
| Área | A4 (210 × 297 mm) |
| Imagen | 2480 × 3508 píxeles |
| Resultado | Página legible |

La imagen real no se incluye en el repositorio por privacidad. Esta ficha
documenta la configuración exacta de la prueba.

## Prueba TWAIN sin hardware

Con `COLORUSBSCANNER_TEST_PATTERN=1`:

- Gris 100 dpi: 827 × 1169, PDF de ~36 KB (`test_pattern.pdf`).
- Color 300 dpi: 2480 × 3508, PDF de ~243 KB (`test_color_300.pdf`).

En NAPS2 7.5.3 apareció una `ArgumentNullException` no fatal después de
“Scanned page 1”; el PDF se generó correctamente. Es un comportamiento del
procesado de imagen de NAPS2 con este DS, no un fallo de la transferencia
TWAIN.

## Reproducir la prueba

1. Instala el DS y las dependencias descritas en el README.
2. Enlaza el escáner a WinUSB con Zadig.
3. Ejecuta:

```bat
scan_real.cmd
```

4. Comprueba que `real_scan.pdf` existe y que la página es legible.
5. Si falla, revisa `%TEMP%\ColorUSBScanner.log`.
