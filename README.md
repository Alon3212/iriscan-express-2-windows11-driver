# ColorUSBScanner.ds — TWAIN Data Source for Plustek OpticSlim M12 / IRIScan Express 2

- **Tested hardware:** USB `VID_07B3&PID_0412`, **GT-6816** chipset.
- **DS:** 32-bit, `%SystemRoot%\twain_32\Color USB Scanner\ColorUSBScanner.ds`.
- **Tested client:** NAPS2 7.5.3 **32-bit** (TWAIN).
- **Status:** Real scan confirmed on Windows 11: **A4, 300 dpi, color, 2480×3508**.

---

## Quick Start

To use the scanner on another PC:

1. Get `libusb-1.0.dll` **32-bit** (tested with **libusb 1.0.30**, MinGW32 build).
2. Get `cism216.fw` (8192 bytes) by extracting it from an official Plustek/IRIScan installer. **Not redistributed** in this repository.
3. Place both files next to this README, in `third_party\libusb\` or in `firmware\`.
4. Run `install.cmd` **as Administrator**.
5. With **Zadig 2.9**, bind `07B3:0412` to **WinUSB**.
6. Open NAPS2 **32-bit**, create a **TWAIN** profile, select **Color USB Scanner**, **Color**, **300 dpi**, A4.
7. Test with `scan_real.cmd` or scan from NAPS2.

If you only want to validate the TWAIN installation without hardware:

```bat
set COLORUSBSCANNER_TEST_PATTERN=1
test_pattern.cmd
```

---

## Status and Real Validation

Tested with:

- **Windows 11**
- **NAPS2 7.5.3 32-bit**, TWAIN driver
- USB **`07B3:0412`**
- **libusb 1.0.30** (32-bit, MinGW32)
- **Zadig 2.9**
- Firmware `cism216.fw` 8192 bytes
- Result: **A4** page, **300 dpi**, **color**, image **2480×3508**, legible.

The actual test image is not published for privacy reasons. The test log and technical data are in [`docs/VALIDATION.md`](docs/VALIDATION.md).

TWAIN integration without hardware has also been validated using a synthetic image:

- Grayscale 100 dpi: 827×1169, PDF ~36 KB.
- Color 300 dpi: 2480×3508, PDF ~243 KB (`test_color_300.pdf`).

> NAPS2 7.5.3 may print a non-fatal `ArgumentNullException` after
> "Scanned page 1". In testing the PDF was generated correctly; the issue
> is in NAPS2's image processing, not in the TWAIN transfer.

**Important:** real scanning has only been tested with **color 300 dpi and A4**. Other
resolutions and modes have not yet been validated on hardware.

---

## Hardware and Compatibility

- **Supported/tested:** `07B3:0412` (Plustek OpticSlim M12; unit also reported as IRIScan Express 2).
- **Not supported by current code:** `07B3:045F`. In the SANE table that ID corresponds to `iriscan_express_2_model`; this DS only searches for `0412`. If your unit is `045F`, it will not work without adding that ID/model.
- The M12 is **sheet-fed**, **CIS**, no duplex.
- The DS is **32-bit**: use it from NAPS2 32-bit or another 32-bit TWAIN application. 64-bit applications cannot load a 32-bit `.ds`.

---

## Requirements and Dependencies

### 1. libusb 1.0.30 32-bit

The DS loads `libusb-1.0.dll` dynamically from its own folder. It does not
link against `libusb-1.0.lib`.

- Tested version: **libusb 1.0.30**, **MinGW32** build.
- Official download: <https://github.com/libusb/libusb/releases>
- MSYS2 alternative: `mingw-w64-i686-libusb` package (32-bit 1.0.x version).
- Must be **32-bit**. A 64-bit DLL cannot be loaded by the DS.
- If your MinGW32 build depends on `libwinpthread-1.dll` or other runtime DLLs,
  copy those alongside the `.ds` as well.

Place it in one of these paths:

```
libusb-1.0.dll
third_party\libusb\libusb-1.0.dll
```

`install.cmd` copies it to `%SystemRoot%\twain_32\Color USB Scanner\` if
found. If it is missing, test mode will still work, but real scanning will fail
when opening USB.

### 2. Firmware `cism216.fw`

The firmware **is not redistributed** in this repository: it is proprietary
Plustek firmware. You must extract it from an official Plustek/IRIScan installer
for your scanner.

- Name: `cism216.fw`
- Expected size: **8192 bytes**
- SHA-256 of the copy used in testing:
  `9730A258DC7152184A9C4FB7C8D12ABB3B4DE537276F4686525BEB6A72F98C15`
- Place it in the project root, in `firmware\`, or let `install.cmd` find it
  in `..\pkg\cab_extract\` if you keep that structure.

See [`firmware/README.md`](firmware/README.md) for instructions and legal notice.

### 3. Zadig 2.9 and WinUSB

The DS uses libusb, so the scanner **must not be bound to `usbscan.sys`**
(the Plustek driver). With **Zadig 2.9**:

1. Open Zadig as Administrator.
2. **Options → List All Devices**.
3. Select `07B3 0412` ("600dpi USB Scanner").
4. In the driver box choose **WinUSB** (or libusbK).
5. Click **Replace Driver** / **Install Driver**.
6. Unplug and replug the scanner.

To revert to the original driver, use **Zadig → Restore Default Driver** or
reinstall the Plustek INF. See [Uninstall and Restore](#uninstall-and-restore).

> Zadig replaces the device driver. This may break the Windows "Scanner" app/WIA
> and the original Plustek software.
> Only do this if you accept that change and have the original installer at hand.

### 4. Compiler (only if building from source)

- **MinGW-w64 i686** (`i686-w64-mingw32-gcc`), or
- **Visual Studio 2022** with **Win32** platform (not verified in this revision).

The project includes `twain.h` and `libusb.h` headers; you do not need the
TWAIN SDK or to link libusb.

---

## Building

### MinGW-w64 32-bit

```bat
cmake -S . -B build -G "MinGW Makefiles" ^
      -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake
cmake --build build
```

or, without CMake:

```bat
make -f Makefile.mingw
```

The output is `ColorUSBScanner.ds` (PE32).

> In `cmd.exe` the line continuation character is `^`. If you use PowerShell,
> use a single line or PowerShell's continuation character.

### Visual Studio 2022 (Win32)

```bat
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A Win32
cmake --build build-vs --config Release
```

The `.ds` will be at `build-vs\Release\ColorUSBScanner.ds`.

> The Visual Studio path was not verified in the original test. If you plan to
> publish a binary, check with `dumpbin /exports` that exactly `DS_Entry` appears
> and that the PE is `machine (x86)`.

If CMake reports `CMAKE_SIZEOF_VOID_P` is 8, you are building x64. Use
`-A Win32` or the i686 toolchain.

---

## Installing

1. Copy `libusb-1.0.dll` (32-bit) to the project root or to
   `third_party\libusb\`.
2. Copy `cism216.fw` to the root or to `firmware\`.
3. Run **as Administrator**:

```bat
install.cmd
```

The script copies to `%SystemRoot%\twain_32\Color USB Scanner\`:

```
ColorUSBScanner.ds
libusb-1.0.dll     (if found)
cism216.fw         (if found)
```

If the old Plustek Data Source `Color USB Scanner.ds` exists, it is backed up as
`Color USB Scanner.ds.plustek.bak` so NAPS2 does not show two entries with the
same name.

No registry entries are needed: the TWAIN DSM enumerates `.ds` files from
`twain_32` (including subfolders, as Canon's DS does). Close and reopen NAPS2
after installing.

If your TWAIN application does not list the DS, also copy
`ColorUSBScanner.ds` to `%SystemRoot%\twain_32\` (some DSMs only look at the
top level).

---

## Testing Without Hardware (Test Mode)

Before connecting the scanner, validate the DS with a synthetic image:

```bat
set COLORUSBSCANNER_TEST_PATTERN=1
test_pattern.cmd
```

Or open NAPS2 and scan: you will get a test image (black border, grid, cross
and gradient) at the selected DPI.

Test mode **does not touch USB**: you can use it without Zadig, without libusb
and without firmware.

---

## Testing With Hardware

1. Bind the scanner to **WinUSB** with Zadig 2.9 and unplug/replug.
2. Copy 32-bit `libusb-1.0.dll` and `cism216.fw` alongside the `.ds`.
3. Do **not** set `COLORUSBSCANNER_TEST_PATTERN`.
4. In NAPS2: TWAIN profile **Color USB Scanner**, **Color**, **300 dpi**, A4.
   You can also run:

```bat
scan_real.cmd
```

or manually:

```bat
set "COLORUSBSCANNER_TEST_PATTERN="
set COLORUSBSCANNER_DEBUG=1
"C:\Program Files\NAPS2\NAPS2.Console.exe" ^
  --driver twain --device "Color USB Scanner" ^
  --dpi 300 --bitdepth color -o real_scan.pdf -v
```

When scanning, the DS:

- uploads `cism216.fw` and boots the firmware (`0x69`);
- turns on the lamp and moves the paper to `abs_y0=70`;
- configures `0x20` for 2480×3508, 8-bit color;
- reads 3508 lines of 3 BGR planes via bulk IN;
- returns 24-bit RGB to NAPS2.

The expected result is an A4 PDF at 300 dpi and 2480×3508 pixels.

### Error Log

With `COLORUSBSCANNER_DEBUG=1`, the DS writes to:

```
%TEMP%\ColorUSBScanner.log
```

Most likely errors:

- `OPENDS usb_open failed`: scanner is not bound to WinUSB, or `libusb-1.0.dll`
  is 64-bit / not alongside the `.ds`.
- `Could not read firmware`: `cism216.fw` is missing or the path is wrong.
- `Firmware booted but scanner does not respond to get_id`: wrong firmware
  or USB in bad state.
- `move_paper failed` / motor timeout: paper not inserted correctly or sensor issue.
- `Scanner not ready to transfer (0x35)`: motor/scan did not start.

---

## Known Issues and Limitations

- **No shading calibration**: `gt68xx_sheetfed_scanner_calibrate()` has not been
  ported. The image may have vertical bands, dust or color cast. This is the next
  step for production quality.
- **Only 300 dpi color A4 validated** on hardware. Other resolutions and grayscale
  mode are not confirmed.
- **`07B3:045F` not supported** by current code (only searches for `0412`).
- **32-bit DS**: does not appear in 64-bit TWAIN applications.
- **Not a WIA driver**: does not appear in the Windows "Scanner" app. It is a
  TWAIN Data Source for NAPS2, IrfanView, Photoshop, etc.
- **Unsigned DS**: SmartScreen or antivirus may warn. Verify the binary SHA-256
  and do not run releases from untrusted third parties.
- **Zadig changes the device driver**: may break WIA/Plustek; use `uninstall.cmd`
  and restore the original driver if needed.

---

## Uninstall and Restore

Run **as Administrator**:

```bat
uninstall.cmd
```

The script:

- removes `ColorUSBScanner.ds` from `%SystemRoot%\twain_32\Color USB Scanner\`;
- restores `Color USB Scanner.ds.plustek.bak` as `Color USB Scanner.ds` if
  it exists;
- leaves `libusb-1.0.dll` and `cism216.fw` in the folder (delete them manually).

Then, to return the scanner to its original driver:

1. Open **Zadig 2.9** as Administrator.
2. **Options → List All Devices**.
3. Select `07B3 0412`.
4. Click **Restore Default Driver** (or reinstall the Plustek INF).
5. Unplug/replug the scanner.
6. If you were using the Windows "Scanner" app, reinstall the Plustek software/WIA.

`uninstall.cmd` cannot safely automate kernel driver restoration; Zadig or the
original INF is the recommended path.

---

## Repository Structure

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
│  └─ README.md          (does not include cism216.fw)
├─ src/
│  ├─ twain_ds.c          DS_Entry, identity, capabilities, transfer
│  ├─ libusb_dyn.c/.h     dynamic loading of libusb-1.0.dll
│  ├─ gt68xx_usb.c/.h     open/claim/reset, endpoints, bulk/control
│  ├─ gt68xx_proto.c/.h   firmware, init, motor, setup and real scan
│  └─ ColorUSBScanner.def exports DS_Entry
├─ third_party/
│  ├─ twain.h             TWAIN header (TWAIN Working Group)
│  └─ libusb/libusb.h     libusb-1.0 header
├─ prebuilt/
│  └─ ColorUSBScanner-test.ds  convenience binary (see below)
└─ reference/             SANE gt68xx reference sources (not compiled)
```

### About `prebuilt/ColorUSBScanner-test.ds`

This is a convenience binary for testing TWAIN without compiling. It is not an
official release and may not correspond to the latest commit. For real use,
build from source or use a release with a published SHA-256.

---

## Security

- The `.ds` **is not signed**. Windows SmartScreen or antivirus may block it;
  do not disable system protection to work around it. Build from source or
  verify the release hash.
- The firmware is uploaded to the scanner when the DS opens. Incorrect or
  interrupted firmware upload may leave the device in a bad state; do not
  disconnect during upload.
- `install.cmd` and `uninstall.cmd` modify `%SystemRoot%\twain_32` and the
  device driver. Run them only as Administrator and with a backup of the
  original driver.
- Do not redistribute `cism216.fw` without Plustek's permission.

---

## License and Attribution

The code in `src/` is a derivative work of the SANE `gt68xx` backend and is
distributed under **GPL-2.0-or-later**. The full text of GPL v2 is in
[`LICENSE`](LICENSE). Attribution and details in [`NOTICE.md`](NOTICE.md).

- SANE `gt68xx`: <https://gitlab.com/sane-project/backends>
- `third_party/twain.h`: TWAIN Working Group.
- `third_party/libusb/libusb.h`: libusb, LGPL-2.1-or-later.
- `cism216.fw`: proprietary Plustek firmware; **not included**.

If you cannot accept the GPL, you must reimplement the protocol from scratch
without copying SANE code.

---

## No Warranty

This software is provided "AS IS", without warranty of any kind. Use of
firmware/motor commands on hardware is at your own risk. Keep USB accessible
and test first on a test machine/VM.
