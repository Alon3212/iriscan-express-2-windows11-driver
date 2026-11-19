# Validation Log

## Real Scan Confirmed

| Field | Value |
|---|---|
| OS | Windows 11 |
| Application | NAPS2 7.5.3, 32-bit, TWAIN driver |
| Scanner | Plustek OpticSlim M12 / IRIScan Express 2 |
| USB | `07B3:0412` |
| Chipset | GT-6816 |
| Firmware | `cism216.fw`, 8192 bytes |
| libusb | 1.0.30, 32-bit, MinGW32 |
| Zadig | 2.9 |
| Mode | Color, 8 bits per channel |
| Resolution | 300 dpi |
| Area | A4 (210 × 297 mm) |
| Image | 2480 × 3508 pixels |
| Result | Legible page |

The actual scanned image is not included in the repository for privacy reasons. This record documents the exact test configuration.

## TWAIN Test Without Hardware

With `COLORUSBSCANNER_TEST_PATTERN=1`:

- Grayscale 100 dpi: 827 × 1169, PDF ~36 KB (`test_pattern.pdf`).
- Color 300 dpi: 2480 × 3508, PDF ~243 KB (`test_color_300.pdf`).

NAPS2 7.5.3 printed a non-fatal `ArgumentNullException` after
"Scanned page 1"; the PDF was generated correctly. This is a behavior of
NAPS2's image processing with this DS, not a failure of the TWAIN transfer.

## Reproducing the Test

1. Install the DS and the dependencies described in the README.
2. Bind the scanner to WinUSB with Zadig.
3. Run:

```bat
scan_real.cmd
```

4. Check that `real_scan.pdf` exists and that the page is legible.
5. If it fails, check `%TEMP%\ColorUSBScanner.log`.
