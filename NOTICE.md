# Licenses and attribution

## ColorUSBScanner code

The code in `src/` is a derivative work of the SANE `gt68xx` backend
(GPL-2.0-or-later), in particular:

- `backend/gt68xx_low.c`
- `backend/gt68xx_gt6816.c`
- `backend/gt68xx_generic.c`
- `backend/gt68xx_mid.c`
- `backend/gt68xx_high.c`
- `backend/gt68xx_devices.c`

It is therefore distributed under **GPL-2.0-or-later**. The full GPL v2 text
is in [`LICENSE`](LICENSE). If you cannot accept the GPL, you must reimplement
the protocol from scratch without copying SANE code.

SANE project: <https://gitlab.com/sane-project/backends>

## third_party/twain.h

TWAIN header owned by the TWAIN Working Group. It is used under the TWAIN
license; see the header of the file. This is the official header used by
`libksane` (KDE).

## third_party/libusb/libusb.h

libusb-1.0 header, distributed under **LGPL-2.1-or-later**. The
`libusb-1.0.dll` copied next to the Data Source keeps its own license; it is
not redistributed in this repository. Tested version: libusb 1.0.30 (32-bit,
MinGW32).

## cism216.fw

Proprietary Plustek firmware. **It is not redistributed** in this repository.
Users must extract it from an official installer that matches their scanner.
See [`firmware/README.md`](firmware/README.md).

## reference/

Read-only copies of the SANE `gt68xx` sources for reference. They are
GPL-2.0-or-later and are **not compiled** into the Data Source. They are
included for porting traceability.

## No warranty

This software is provided "AS IS", without warranty of any kind. Using
firmware/motor commands on the hardware is at your own risk.
