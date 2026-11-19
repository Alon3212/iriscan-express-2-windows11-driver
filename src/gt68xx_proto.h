/*
 * gt68xx_proto.h - interfaz del protocolo GT-6816.
 *
 * Estado actual: subida de firmware, inicialización, motor y lectura real
 * portados de SANE para el OpticSlim M12 (GT-6816). La ruta principal es
 * color 8-bit, 300 dpi, A4, line mode CIS. No incluye calibración de sombreado
 * (se aplica solo gamma 2.0); para producción hay que portar el calibrador.
 * El modo de prueba sigue generando una imagen sintética.
 */
#ifndef GT68XX_PROTO_H
#define GT68XX_PROTO_H

#include <stddef.h>
#include "gt68xx_usb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GT68XX_ERR_NOT_IMPLEMENTED (-100)
#define GT68XX_ERR_IO              (-101)
#define GT68XX_ERR_NOMEM           (-102)

#define GT68XX_PIXEL_GRAY 0
#define GT68XX_PIXEL_RGB  1

typedef struct gt68xx_image {
    int            width;
    int            height;
    int            bytes_per_pixel;
    int            dpi;
    size_t         size;
    unsigned char *data;
} gt68xx_image;

int  gt68xx_proto_init(gt68xx_device *dev);
int  gt68xx_proto_scan(gt68xx_device *dev, int dpi, int pixel_type, gt68xx_image *img);
void gt68xx_image_free(gt68xx_image *img);

int  gt68xx_test_pattern(int dpi, int pixel_type, gt68xx_image *img);

const char *gt68xx_proto_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GT68XX_PROTO_H */
