# Licencias y atribución

## Código de ColorUSBScanner

El código de `src/` es una obra derivada del backend SANE `gt68xx`
(GPL-2.0-or-later), en particular de la secuencia de comandos USB y de la
subida de firmware documentada en:

- `backend/gt68xx_low.c`
- `backend/gt68xx_gt6816.c`
- `backend/gt68xx_high.c`
- `backend/gt68xx_devices.c`

Por eso se distribuye bajo **GPL-2.0-or-later**. Si no puedes aceptar GPL,
debes reimplementar el protocolo desde cero sin copiar código de SANE.

## third_party/twain.h

Cabecera TWAIN propiedad de TWAIN Working Group. Se usa bajo la licencia
TWAIN; consulta el encabezado del archivo. Es la cabecera oficial usada por
`libksane` (KDE).

## third_party/libusb/libusb.h

Cabecera de libusb-1.0, distribuida bajo **LGPL-2.1-or-later**. La DLL
`libusb-1.0.dll` que se copie junto al Data Source conserva su propia
licencia; no se redistribuye en este repositorio.

## reference/

Copias de solo lectura de las fuentes SANE `gt68xx` para consulta. Son
GPL-2.0-or-later y **no se compilan** en el Data Source. Se incluyen para
trazabilidad del port.

## Sin garantía

Este software se proporciona "AS IS", sin garantía de ningún tipo. El uso de
comandos de firmware/motor sobre el hardware es bajo tu responsabilidad.
