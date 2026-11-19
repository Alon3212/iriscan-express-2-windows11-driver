/*
 * gt68xx_proto.h - GT-6816 protocol interface.
 *
 * Current status: firmware upload, initialization, motor control and real
 * image read ported from SANE for the OpticSlim M12 (GT-6816). The main path
 * is 8-bit color, 300 dpi, A4, CIS line mode. It does not include shading
 * calibration (only gamma 2.0 is applied); production use requires porting
 * the calibrator. Test mode still generates a synthetic image.
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
