# Firmware `cism216.fw`

This repository **does not include** `cism216.fw`. It is proprietary Plustek
firmware and is not redistributed without permission.

## How to obtain it

Extract `cism216.fw` from an official Plustek OpticSlim M12 or IRIScan Express 2
installer that matches your scanner. Do not use firmware from a different model
or from unofficial sources.

- Expected name: `cism216.fw`
- Size: **8192 bytes**
- SHA-256 of the copy used in testing:
  `9730A258DC7152184A9C4FB7C8D12ABB3B4DE537276F4686525BEB6A72F98C15`

Verify the size before using it:

```bat
for %F in (cism216.fw) do @echo %~zF
```

It should print `8192`.

## Where to place it

You can place it in any of these paths before running `install.cmd`:

```
cism216.fw
firmware\cism216.fw
..\pkg\cab_extract\cism216.fw   (structure used by the author)
```

`install.cmd` copies it to `%SystemRoot%\twain_32\Color USB Scanner\`.

## Warning

If the firmware is not the correct one, the upload may fail and the scanner may
be left in an inconsistent state. Do not disconnect the USB during the upload.
