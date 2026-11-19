# Porting the SANE `gt68xx` backend to the TWAIN Data Source

This document maps the SANE code to the `ColorUSBScanner.ds` implementation and
explains what remains for production quality (shading calibration).

Port status (2026-11-19):

- [x] 32-bit TWAIN DS, `DS_Entry`, identity and enumeration in NAPS2.
- [x] `DAT_ENTRYPOINT` (TWAIN 2.x), `MSG_XFERREADY` and transfer through
  `DAT_IMAGEMEMXFER`; validated with NAPS2 7.5.3 and a synthetic image.
- [x] USB open with libusb and endpoint discovery.
- [x] `cism216.fw` upload (64-byte blocks, verification and `0x69` boot).
- [x] Initialization: `get_id` `0x2e`, `stop_scan` `0x41`, lamp `0x25`.
- [x] Motor/positioning: `move_paper` `0x82`, `is_moving` `0x17`.
- [x] `setup_scan` `0x20`, AFE `0x22`, exposure `0x76`, `start_scan` `0x43`,
  `read_scanned_data` `0x35`.
- [x] Bulk read in BGR line mode (3 planes of `scan_bpl`) and gamma 2.0;
  8-bit color path, 300 dpi, A4.
- [ ] Shading calibration (`gt68xx_sheetfed_scanner_calibrate`).
- [ ] Hardware testing and offset/exposure/AFE tuning.

Reference source: <https://gitlab.com/sane-project/backends/-/tree/master/backend>
Relevant files (local copy in `reference/`):

- `gt68xx.c` — SANE layer, open, options, `download_firmware_file()`.
- `gt68xx_low.c` — USB transport: `gt68xx_device_memory_write/read`,
  `gt68xx_device_req`, `gt68xx_device_download_firmware`, raw read.
- `gt68xx_generic.c` — common commands: `setup_scan`, `set_afe`,
  `set_exposure_time`, `start_scan`, `read_scanned_data`, `move_paper`.
- `gt68xx_mid.c` — line reader and conversion of the 3 CIS planes.
- `gt68xx_high.c` — geometry, calibration, gamma.
- `gt68xx_devices.c` — model table and per-chipset `command_set`.
- `gt68xx_gt6816.c` — GT-6816-specific commands, including
  `gt6816_download_firmware()`.

---

## 1. M12 model and command set

In `gt68xx_devices.c`, ID `07b3:0412` points to
`plustek_opticslim_m12_model`:

```c
{0x07b3, 0x0412, &plustek_opticslim_m12_model},
```

The model uses `mustek_gt6816_sheetfed_command_set`. Key parameters:

- Firmware: `cism216.fw`.
- Maximum optical resolution: 600 dpi; motor: 1200 dpi.
- X resolutions: 600, 400, 300, 200, 150, 100, 50.
- Y resolutions: 1200, 600, 400, 300, 200, 150, 100, 50.
- Depths: 16/8-bit gray and 16/8-bit color.
- Area: start (0.0, 1.5) mm; size 218.0 x 299.0 mm.
- CIS, sheet-fed, with calibration.
- Default offset/gain: `{0x24, 0x0a, 0x23, 0x0f, 0x23, 0x0b}`.
- Default exposure: `{0x157, 0x157, 0x157}`.
- Default gamma: 2.0.
- Flags: `NO_POWER_STATUS | SHEET_FED | OFFSET_INV | HAS_CALIBRATE`.

`iriscan_express_2_model` is equivalent (same `command_set`, same values except
offset/gain `{0x22,0x0c,0x22,0x0a,0x24,0x09}`). The DS can use the M12
parameters for both.

---

## 2. GT-6816 command set

`mustek_gt6816_sheetfed_command_set`:

| Field | Value | Purpose |
|---|---:|---|
| `request_type` | `0x40` | Vendor control OUT |
| `request` | `0x01` | bRequest |
| `memory_read_value` | `0x200C` | wValue for memory read |
| `memory_write_value` | `0x200B` | wValue for memory write |
| `send_cmd_value` | `0x2010` | wValue to send a 64-byte command |
| `send_cmd_index` | `0x3F40` | wIndex for the command |
| `recv_res_value` | `0x2011` | wValue to read the response |
| `recv_res_index` | `0x3F00` | wIndex for the response |
| `send_small_cmd_value` | `0x2012` | "small" command (8 bytes) |
| `recv_small_res_value` | `0x2013` | "small" response |

The implementation in `gt68xx_low.c`:

```c
/* memory write */
sanei_usb_control_msg(fd, 0x40, 0x01, 0x200B, addr, size, data);

/* memory read */
sanei_usb_control_msg(fd, 0xC0, 0x01, 0x200C, addr, size, data);

/* normal command */
sanei_usb_control_msg(fd, 0x40, 0x01, 0x2010, 0x3F40, 64, cmd);
sanei_usb_control_msg(fd, 0xC0, 0x01, 0x2011, 0x3F00, 64, res);
```

`gt68xx_usb.c` already exposes `gt68xx_usb_control()` and
`gt68xx_proto.c` implements these helpers.

---

## 3. Firmware upload (already implemented)

`gt6816_download_firmware()` in `gt68xx_gt6816.c`:

1. For `addr = 0; addr < size; addr += 64`:
   - Copy 64 firmware bytes (the last block is zero-padded).
   - `memory_write(addr, 64, block)`.
   - `memory_read(addr, 64, check)`.
   - Compare `block` and `check`; if they differ, return an I/O error.
2. Send the 64-byte boot command:
   ```
   0x69, 0x01, addr_lo, addr_hi, 0, ..., 0
   ```
   through `gt68xx_device_req()`.
3. `addr` is the final address (size rounded up to 64).

`gt68xx_proto.c` implements exactly this in `gt68xx_upload_firmware()`, with
`cism216.fw` (8192 bytes = 128 blocks).

**Status:** firmware upload is ported. If the device is bound to WinUSB,
`gt68xx_proto_init()` should load the firmware and boot it.

---

## 4. Current implementation and remaining work

### 4.1 Motor, setup and read (implemented)

`gt68xx_proto.c` implements the `sane_start` sequence for the M12:

1. `stop_scan` small `0x41` and lamp `0x25`.
2. `move_paper` `0x82` with `abs_y0=70` (A4, y0=1.5 mm + offset 1.5 mm) and
   wait for `is_moving` `0x17`.
3. `setup_scan` `0x20`:
   - 300 dpi, 8-bit, color (`color_mode_code=0x84`).
   - `abs_x0=0`, `abs_y0=70`, `abs_xs=4992`, `abs_ys=7016`.
   - `scan_xs=2496`, `scan_bpl=2496`, `scan_ys=10524` (3 CIS lines).
   - `motor_mode_1=0x01` (mbs=1, mds=1, mas=0), `motor_mode_2=0x00`.
4. `set_exposure` `0x76` (`0x157` per channel) and `set_afe` `0x22`
   (`{0x24,0x0a,0x23,0x0f,0x23,0x0b}`).
5. `start_scan` `0x43`, poll `read_scanned_data` `0x35`.
6. Bulk read of `scan_bpl*3 = 7488` bytes per RGB line.
   - First plane: B; second: G; third: R (`COLOR_ORDER_BGR`).
   - The 16 padding pixels are discarded (2496 - 2480).
7. `stop_scan`, `paperfeed` `0x83` (eject the sheet) and lamp off.

The output image is 2480x3508 (A4 at 300 dpi), 8-bit RGB, with gamma 2.0.

### 4.2 Remaining work

- **Shading calibration:** `gt68xx_sheetfed_scanner_calibrate()` is not
  ported. Without it, vertical bands, dust and color casts may appear. This is
  the next step for production quality.
- **Physical testing:** verify that `move_paper` feeds the sheet, that BGR
  order is correct, and that the area is not shifted.
- **Other modes:** native gray, 16-bit, other resolutions and areas other than
  A4. The current path converts gray from RGB capture, but the geometry is
  designed for A4.
- **Duplex / feeder:** the M12 is a simple sheet-fed scanner; there is no
  duplex.

### 4.3 Geometry

- `base_xdpi=600`, `base_ydpi=600`, `optical_ydpi=1200`.
- Physical model area: 218 x 299 mm; A4 uses 210 x 297 mm, start y=1.5 mm.
- The model uses `SHEET_FED` and `HAS_CALIBRATE`; the port applies the model's
  default AFE/exposure values, but does not run auto-AFE or shading
  calibration.

---

## 5. How to test the port

1. **SANE on Linux/WSL** with the same hardware and `cism216.fw`:
   ```bash
   scanimage -L
   scanimage --mode Color --resolution 300 -o sane_test.pnm
   ```
   Use it to obtain a reference image for comparison.
2. **USBPcap/Wireshark** on Windows while VueScan scans:
   - Filter `usb.idVendor == 0x07b3 && usb.idProduct == 0x0412`.
   - Capture the bulk endpoints.
   - Compare the sequence with `gt68xx_device_req()`/`memory_write/read`.
3. **DS in real mode**:
   - Bind to WinUSB with Zadig.
   - Copy 32-bit `libusb-1.0.dll` and `cism216.fw` next to the `.ds`.
   - Build, install and run NAPS2 32-bit in color, 300 dpi, A4.
   - `gt68xx_proto_init()` loads the firmware and `gt68xx_proto_scan()` runs
     motor + real image read.

---

## 6. License

The SANE `gt68xx` backend is **GPL-2.0-or-later**. If you copy/port its code,
`ColorUSBScanner.ds` is a derivative work and must be distributed under the
same license, with attribution to the SANE project and the backend authors.

If you need a non-GPL license, you must reimplement the protocol from the
specification and traces without copying SANE code. Firmware upload and USB
commands are technical facts; the concrete implementation is what is subject
to copyright.
