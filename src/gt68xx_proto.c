/*
 * gt68xx_proto.c - protocolo GT-6816 para el Plustek OpticSlim M12.
 *
 * Portado de SANE:
 *   - gt68xx_gt6816.c: firmware, stop_scan, lamp, is_moving.
 *   - gt68xx_generic.c: move_paper, setup_scan, set_afe, set_exposure,
 *     start_scan, read_scanned_data.
 *   - gt68xx_mid.c: line mode CIS BGR (3 planos de scan_bpl bytes).
 *
 * La ruta implementada es color 8-bit, 300 dpi, A4 (210x297 mm), sin
 * calibración de sombreado; aplica gamma 2.0. El modo de prueba no toca USB.
 */
#include "gt68xx_proto.h"

#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_last_error[512];

static void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error) - 1, fmt, ap);
    g_last_error[sizeof(g_last_error) - 1] = 0;
    va_end(ap);
}

const char *gt68xx_proto_last_error(void)
{
    return g_last_error;
}

static int get_ds_dir(char *dir, size_t dir_size)
{
    HMODULE mod = NULL;
    DWORD len;

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)(const void *)&gt68xx_proto_init,
                            &mod)) {
        return -1;
    }

    len = GetModuleFileNameA(mod, dir, (DWORD)dir_size);
    if (len == 0 || len >= dir_size) {
        return -1;
    }
    while (len > 0 && dir[len - 1] != '\\' && dir[len - 1] != '/') {
        len--;
    }
    if (len > 0) {
        dir[len - 1] = 0;
    }
    return 0;
}

static int read_file(const char *path, unsigned char **data, size_t *length)
{
    FILE *f;
    long size;
    unsigned char *buf;
    size_t got;

    *data = NULL;
    *length = 0;

    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    size = ftell(f);
    if (size <= 0 || size > (16 * 1024 * 1024)) {
        fclose(f);
        return -1;
    }
    rewind(f);

    buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return -1;
    }
    got = fread(buf, 1, (size_t)size, f);
    fclose(f);

    if (got != (size_t)size) {
        free(buf);
        return -1;
    }

    *data = buf;
    *length = (size_t)size;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Protocolo de bajo nivel del GT-6816 (portado de SANE gt68xx)              */
/* ------------------------------------------------------------------------- */

/* gt68xx_low.h: los comandos "normales" son paquetes de 64 bytes; los
 * comandos "small" (p. ej. stop_scan 0x41) son de 8 bytes. */
#define GT68XX_PACKET_SIZE        64
#define GT68XX_SMALL_PACKET_SIZE  8
#define GT68XX_FW_BLOCK_SIZE      64
#define GT68XX_CMD_TIMEOUT_MS     5000
#define GT68XX_BULK_TIMEOUT_MS    10000

#define GT68XX_REQ_TYPE_OUT       0x40
#define GT68XX_REQ_TYPE_IN        0xC0
#define GT68XX_REQUEST            0x01

#define GT68XX_MEM_WRITE_VALUE    0x200B
#define GT68XX_MEM_READ_VALUE     0x200C
#define GT68XX_SEND_CMD_VALUE     0x2010
#define GT68XX_SEND_CMD_INDEX     0x3F40
#define GT68XX_RECV_RES_VALUE     0x2011
#define GT68XX_RECV_RES_INDEX     0x3F00
#define GT68XX_SEND_SMALL_VALUE   0x2012
#define GT68XX_RECV_SMALL_VALUE   0x2013

static int control_out(gt68xx_device *dev, uint16_t value, uint16_t index,
                       const uint8_t *data, uint16_t length)
{
    int r = gt68xx_usb_control(dev, GT68XX_REQ_TYPE_OUT, GT68XX_REQUEST,
                               value, index, (uint8_t *)data, length,
                               GT68XX_CMD_TIMEOUT_MS);
    if (r < 0) {
        set_error("control OUT fallo: %s", gt68xx_usb_last_error());
        return -1;
    }
    if (r != (int)length) {
        set_error("control OUT corto: %d/%u", r, (unsigned)length);
        return -1;
    }
    return 0;
}

static int control_in(gt68xx_device *dev, uint16_t value, uint16_t index,
                      uint8_t *data, uint16_t length)
{
    int r;
    memset(data, 0, length);
    r = gt68xx_usb_control(dev, GT68XX_REQ_TYPE_IN, GT68XX_REQUEST,
                           value, index, data, length, GT68XX_CMD_TIMEOUT_MS);
    if (r < 0) {
        set_error("control IN fallo: %s", gt68xx_usb_last_error());
        return -1;
    }
    if (r != (int)length) {
        set_error("control IN corto: %d/%u", r, (unsigned)length);
        return -1;
    }
    return 0;
}

/* Equivale a gt68xx_device_req(): comando normal de 64 bytes y respuesta de
 * 64 bytes (gt68xx_low.c, gt68xx_device_generic_req). */
static int device_req(gt68xx_device *dev, const uint8_t *cmd, uint8_t *res)
{
    uint8_t out[GT68XX_PACKET_SIZE];
    uint8_t in[GT68XX_PACKET_SIZE];

    memset(out, 0, sizeof(out));
    memcpy(out, cmd, GT68XX_PACKET_SIZE);

    if (control_out(dev, GT68XX_SEND_CMD_VALUE, GT68XX_SEND_CMD_INDEX,
                    out, GT68XX_PACKET_SIZE) != 0) {
        return -1;
    }
    if (control_in(dev, GT68XX_RECV_RES_VALUE, GT68XX_RECV_RES_INDEX,
                   in, GT68XX_PACKET_SIZE) != 0) {
        return -1;
    }
    if (res) {
        memcpy(res, in, GT68XX_PACKET_SIZE);
    }
    return 0;
}

/* Equivale a gt68xx_device_small_req(): comando/respuesta de 8 bytes. */
static int small_req(gt68xx_device *dev, const uint8_t *cmd, uint8_t *res)
{
    uint8_t out[GT68XX_SMALL_PACKET_SIZE];
    uint8_t in[GT68XX_SMALL_PACKET_SIZE];

    memset(out, 0, sizeof(out));
    memcpy(out, cmd, GT68XX_SMALL_PACKET_SIZE);

    if (control_out(dev, GT68XX_SEND_SMALL_VALUE, GT68XX_SEND_CMD_INDEX,
                    out, GT68XX_SMALL_PACKET_SIZE) != 0) {
        return -1;
    }
    if (control_in(dev, GT68XX_RECV_SMALL_VALUE, GT68XX_RECV_RES_INDEX,
                   in, GT68XX_SMALL_PACKET_SIZE) != 0) {
        return -1;
    }
    if (res) {
        memcpy(res, in, GT68XX_SMALL_PACKET_SIZE);
    }
    return 0;
}

/* gt68xx_device_check_result(): respuesta 00 <cmd> ... */
static int check_result(const uint8_t *res, uint8_t command)
{
    if (!res || res[0] != 0x00 || res[1] != command) {
        set_error("Respuesta inesperada del escaner: %02X %02X (esperado 00 %02X)",
                  res ? res[0] : 0, res ? res[1] : 0, command);
        return -1;
    }
    return 0;
}

/* Equivale a gt68xx_device_memory_write(). */
static int memory_write(gt68xx_device *dev, uint16_t addr,
                        const uint8_t *data, uint16_t length)
{
    return control_out(dev, GT68XX_MEM_WRITE_VALUE, addr, data, length);
}

/* Equivale a gt68xx_device_memory_read(). */
static int memory_read(gt68xx_device *dev, uint16_t addr,
                       uint8_t *data, uint16_t length)
{
    return control_in(dev, GT68XX_MEM_READ_VALUE, addr, data, length);
}

/*
 * Portado de gt6816_download_firmware() (backend/gt68xx_gt6816.c):
 *   1. Escribe el firmware en bloques de 64 bytes en la memoria del chip.
 *   2. Relee cada bloque y compara.
 *   3. Envía el comando de arranque 0x69 0x01 <addr_lo> <addr_hi>.
 */
static int gt68xx_upload_firmware(gt68xx_device *dev,
                                  const unsigned char *firmware,
                                  size_t firmware_length)
{
    uint8_t block[GT68XX_FW_BLOCK_SIZE];
    uint8_t check[GT68XX_FW_BLOCK_SIZE];
    uint32_t addr;
    uint8_t boot[GT68XX_PACKET_SIZE];

    if (!dev || !firmware || firmware_length == 0) {
        set_error("Firmware vacio o dispositivo nulo");
        return GT68XX_ERR_IO;
    }
    if (firmware_length > 0xFFFF) {
        set_error("Firmware demasiado grande para el GT-6816: %lu bytes",
                  (unsigned long)firmware_length);
        return GT68XX_ERR_IO;
    }

    for (addr = 0; addr < firmware_length; addr += GT68XX_FW_BLOCK_SIZE) {
        size_t left = firmware_length - addr;
        size_t n = (left > GT68XX_FW_BLOCK_SIZE) ? GT68XX_FW_BLOCK_SIZE : left;

        memset(block, 0, sizeof(block));
        memcpy(block, firmware + addr, n);

        if (memory_write(dev, (uint16_t)addr, block, GT68XX_FW_BLOCK_SIZE) != 0) {
            set_error("Firmware: fallo escribiendo bloque 0x%04lX",
                      (unsigned long)addr);
            return GT68XX_ERR_IO;
        }
        if (memory_read(dev, (uint16_t)addr, check, GT68XX_FW_BLOCK_SIZE) != 0) {
            set_error("Firmware: fallo leyendo bloque 0x%04lX",
                      (unsigned long)addr);
            return GT68XX_ERR_IO;
        }
        if (memcmp(block, check, GT68XX_FW_BLOCK_SIZE) != 0) {
            set_error("Firmware: verificacion fallida en bloque 0x%04lX",
                      (unsigned long)addr);
            return GT68XX_ERR_IO;
        }
    }

    memset(boot, 0, sizeof(boot));
    boot[0] = 0x69;
    boot[1] = 0x01;
    boot[2] = (uint8_t)(addr & 0xFF);
    boot[3] = (uint8_t)((addr >> 8) & 0xFF);

    if (device_req(dev, boot, NULL) != 0) {
        set_error("Firmware: fallo el comando de arranque 0x69");
        return GT68XX_ERR_IO;
    }

    OutputDebugStringA("ColorUSBScanner: firmware cism216.fw cargado y arrancado\n");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Comandos del command set mustek-gt6816 / modelo OpticSlim M12             */
/* ------------------------------------------------------------------------- */

#define M12_BASE_XDPI          600
#define M12_BASE_YDPI          600
#define M12_Y_OFFSET_MM        1.5
#define M12_Y_OFFSET_CALIB_MM  18.0
#define M12_CALIB_HEIGHT_MM    2.5

typedef struct gt68xx_scan_geometry {
    int xdpi;
    int ydpi;
    int pixel_xs;
    int pixel_ys;
    int scan_xs;
    int scan_ys;
    int scan_bpl;
    int abs_x0;
    int abs_y0;
    int abs_xs;
    int abs_ys;
} gt68xx_scan_geometry;

static void put16(uint8_t *p, unsigned int v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static uint8_t fix_offset(int v)
{
    if (v < 0) return 0;
    if (v > 63) return 63;
    return (uint8_t)v;
}

static uint8_t fix_gain(int v)
{
    if (v < 0) return 0;
    if (v > 31) {
        v += 12;
        if (v > 63) v = 63;
    }
    return (uint8_t)v;
}

static int gt68xx_get_id(gt68xx_device *dev)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x2e;
    req[1] = 0x01;
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    return check_result(res, 0x2e);
}

static int gt68xx_lamp_control(gt68xx_device *dev, int on)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x25;
    req[1] = 0x01;
    req[2] = on ? 0x01 : 0x00;
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    return 0;
}

static int gt68xx_is_moving(gt68xx_device *dev, int *moving)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x17;
    req[1] = 0x01;
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    if (res[0] != 0x00 || res[1] != 0x17) {
        set_error("is_moving: respuesta %02X %02X", res[0], res[1]);
        return -1;
    }
    *moving = (res[2] != 0 || (res[3] != 0 && res[3] != 2));
    return 0;
}

static int gt68xx_wait_positioning(gt68xx_device *dev)
{
    int i;
    int errors = 0;

    Sleep(100);
    for (i = 0; i < 100; i++) {
        int moving = 0;
        if (gt68xx_is_moving(dev, &moving) == 0) {
            if (!moving) {
                return 0;
            }
        } else if (++errors > 10) {
            return -1;
        }
        Sleep(100);
    }
    set_error("Tiempo de espera agotado: el motor no queda quieto");
    return -1;
}

/* 0x82: mover el papel hasta abs_y0 (gt68xx_generic_move_paper). */
static int gt68xx_move_paper(gt68xx_device *dev, int abs_y0)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x82;
    req[1] = 0x01;
    put16(req + 2, (unsigned int)abs_y0);
    return device_req(dev, req, res);
}

/* 0x22: AFE del M12 {0x24,0x0a, 0x23,0x0f, 0x23,0x0b}. */
static int gt68xx_set_afe(gt68xx_device *dev)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x22;
    req[1] = 0x01;
    req[2] = fix_offset(0x24);
    req[3] = fix_gain(0x0a);
    req[4] = fix_offset(0x23);
    req[5] = fix_gain(0x0f);
    req[6] = fix_offset(0x23);
    req[7] = fix_gain(0x0b);
    return device_req(dev, req, res);
}

/* 0x76: tiempos de exposicion por defecto {0x157,0x157,0x157}. */
static int gt68xx_set_exposure(gt68xx_device *dev)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];
    unsigned int t = 0x157;

    memset(req, 0, sizeof(req));
    req[0] = 0x76;
    req[1] = 0x01;
    req[2] = req[6] = req[10] = 0x04;
    put16(req + 4, t);
    put16(req + 8, t);
    put16(req + 12, t);
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    return check_result(res, 0x76);
}

static int gt68xx_start_scan(gt68xx_device *dev)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x43;
    req[1] = 0x01;
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    return check_result(res, 0x43);
}

static int gt68xx_read_ready(gt68xx_device *dev, int *ready)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x35;
    req[1] = 0x01;
    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    *ready = (res[0] == 0x00);
    return 0;
}

/* 0x83: paperfeed; SANE lo usa al cancelar/terminar un sheet-fed para
 * expulsar la hoja. */
static int gt68xx_paperfeed(gt68xx_device *dev)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x83;
    req[1] = 0x01;
    return device_req(dev, req, res);
}

/* 0x41: stop_scan usa el comando "small" de 8 bytes. */
static int gt68xx_stop_scan(gt68xx_device *dev)
{
    uint8_t req[GT68XX_SMALL_PACKET_SIZE];
    uint8_t res[GT68XX_SMALL_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0] = 0x41;
    req[1] = 0x01;
    return small_req(dev, req, res);
}

static int gt68xx_bulk_read_exact(gt68xx_device *dev, uint8_t *buf, int length)
{
    int off = 0;
    int retries = 0;

    while (off < length) {
        int r = gt68xx_usb_bulk_read(dev, buf + off, length - off,
                                     GT68XX_BULK_TIMEOUT_MS);
        if (r < 0) {
            set_error("Lectura bulk fallo a %d/%d: %s", off, length,
                      gt68xx_usb_last_error());
            return -1;
        }
        if (r == 0) {
            if (++retries > 20) {
                set_error("Lectura bulk sin datos a %d/%d", off, length);
                return -1;
            }
            Sleep(1);
            continue;
        }
        retries = 0;
        off += r;
    }
    return 0;
}

/* Calcula la geometria del M12 para A4 (210x297mm), color 8-bit, line mode. */
static int gt68xx_compute_geometry(int dpi, gt68xx_scan_geometry *g)
{
    int pixel_y0;

    if (dpi < 50 || dpi > 600) {
        set_error("DPI no soportado: %d", dpi);
        return -1;
    }
    memset(g, 0, sizeof(*g));
    g->xdpi = dpi;
    g->ydpi = dpi;
    g->pixel_xs = (int)(210.0 * (double)dpi / 25.4 + 0.5);
    g->pixel_ys = (int)(297.0 * (double)dpi / 25.4 + 0.5);
    if (g->pixel_xs < 1 || g->pixel_ys < 1) {
        set_error("Geometria A4 invalida para %d dpi", dpi);
        return -1;
    }

    /* SANE alinea el ancho a un multiplo de 64 bytes; sobra ancho fisico
     * que se descarta al componer la imagen. */
    g->scan_xs = (g->pixel_xs + 63) & ~63;
    g->scan_bpl = g->scan_xs;          /* 8 bits por pixel y plano */
    g->scan_ys = g->pixel_ys * 3;      /* 3 lineas CIS por linea RGB */

    /* y0 de setup_scan: request->y0 (1.5mm) + model->y_offset (1.5mm). */
    pixel_y0 = (int)((M12_Y_OFFSET_MM + M12_Y_OFFSET_MM) *
                     (double)dpi / 25.4 + 0.5);
    g->abs_y0 = pixel_y0 * M12_BASE_YDPI / dpi;
    g->abs_x0 = 0;
    g->abs_xs = g->scan_xs * M12_BASE_XDPI / dpi;
    /* SANE calcula abs_ys ANTES de multiplicar scan_ys por 3 (line mode). */
    g->abs_ys = g->pixel_ys * M12_BASE_YDPI / dpi;
    return 0;
}

/* 0x20: setup_scan para SA_SCAN, color, line mode, mbs=1, mds=1, mas=0. */
static int gt68xx_setup_scan(gt68xx_device *dev, const gt68xx_scan_geometry *g)
{
    uint8_t req[GT68XX_PACKET_SIZE];
    uint8_t res[GT68XX_PACKET_SIZE];

    memset(req, 0, sizeof(req));
    req[0x00] = 0x20;
    req[0x01] = 0x01;
    put16(req + 0x02, (unsigned int)g->abs_y0);
    put16(req + 0x04, (unsigned int)g->abs_ys);
    put16(req + 0x06, (unsigned int)g->abs_x0);
    put16(req + 0x08, (unsigned int)g->abs_xs);
    req[0x0a] = 0x84;          /* 0x80 | color (1<<2), 8 bits */
    req[0x0b] = 0x60;          /* CIS, line mode */
    put16(req + 0x0c, (unsigned int)g->xdpi);
    req[0x0e] = 0x12;
    req[0x0f] = 0x00;
    put16(req + 0x10, (unsigned int)g->scan_bpl);
    put16(req + 0x12, (unsigned int)g->scan_ys);
    req[0x14] = 0x01;          /* mbs=1, mds=1, mas=0, sin backtrack */
    req[0x15] = 0x00;          /* lamp on, line mode */
    put16(req + 0x16, (unsigned int)g->ydpi);
    req[0x18] = 0x00;

    if (device_req(dev, req, res) != 0) {
        return -1;
    }
    return check_result(res, 0x20);
}

int gt68xx_proto_init(gt68xx_device *dev)
{
    char path[MAX_PATH];
    unsigned char *firmware = NULL;
    size_t firmware_length = 0;
    int rc;

    if (!dev) {
        set_error("gt68xx_proto_init: dispositivo nulo");
        return -1;
    }

    if (get_ds_dir(path, sizeof(path)) != 0) {
        set_error("No se pudo determinar la carpeta del Data Source");
        return GT68XX_ERR_IO;
    }
    if (strlen(path) + strlen("\\cism216.fw") + 1 >= sizeof(path)) {
        set_error("Ruta de firmware demasiado larga");
        return GT68XX_ERR_IO;
    }
    strcat(path, "\\cism216.fw");

    if (read_file(path, &firmware, &firmware_length) != 0) {
        set_error("No se pudo leer el firmware: %s", path);
        return GT68XX_ERR_IO;
    }

    rc = gt68xx_upload_firmware(dev, firmware, firmware_length);
    free(firmware);

    if (rc != 0) {
        return rc;
    }

    /* Tras arrancar el firmware, el escaner necesita un instante antes de
     * aceptar comandos normales. gt68xx.c hace get_id + stop_scan + lamp on. */
    Sleep(200);
    for (rc = 0; rc < 10; rc++) {
        if (gt68xx_get_id(dev) == 0) {
            break;
        }
        Sleep(200);
    }
    if (rc >= 10) {
        set_error("El firmware arranco pero el escaner no responde a get_id");
        return GT68XX_ERR_IO;
    }

    (void)gt68xx_stop_scan(dev);
    if (gt68xx_lamp_control(dev, 1) != 0) {
        set_error("No se pudo encender la lampara del escaner");
        return GT68XX_ERR_IO;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Escaneo real: A4, color 8-bit, line mode CIS                              */
/* ------------------------------------------------------------------------- */

static int gt68xx_scan_real(gt68xx_device *dev, int dpi, int pixel_type,
                            gt68xx_image *img)
{
    gt68xx_scan_geometry g;
    int bpp;
    int width, height;
    size_t size;
    uint8_t *out = NULL;
    uint8_t *line = NULL;
    int line_bytes;
    int ready = 0;
    int i, x, y;
    int ok = 0;
    uint8_t gamma_lut[256];

    if (!dev || !img) {
        set_error("Escaner o imagen nulos");
        return GT68XX_ERR_IO;
    }
    if (gt68xx_compute_geometry(dpi, &g) != 0) {
        return GT68XX_ERR_IO;
    }

    bpp = (pixel_type == GT68XX_PIXEL_RGB) ? 3 : 1;
    width = g.pixel_xs;
    height = g.pixel_ys;
    size = (size_t)width * (size_t)height * (size_t)bpp;
    if (size / (size_t)width / (size_t)bpp != (size_t)height) {
        set_error("Desbordamiento de imagen real: %dx%d", width, height);
        return GT68XX_ERR_IO;
    }
    line_bytes = g.scan_bpl * 3;

    /* Misma curva que SANE para 8 bits y gamma 2.0 (gt68xx.c). */
    for (i = 0; i < 256; i++) {
        int v = (int)(255.0 * sqrt(((double)i + 1.0) / 256.0) + 0.5);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        gamma_lut[i] = (uint8_t)v;
    }

    out = (uint8_t *)malloc(size);
    line = (uint8_t *)malloc((size_t)line_bytes);
    if (!out || !line) {
        set_error("Sin memoria para imagen real (%lu bytes) y linea (%d)",
                  (unsigned long)size, line_bytes);
        free(out);
        free(line);
        return GT68XX_ERR_NOMEM;
    }

    /* gt68xx.c: stop_scan antes de posicionar, lampara encendida. */
    (void)gt68xx_stop_scan(dev);
    if (gt68xx_lamp_control(dev, 1) != 0) {
        goto done;
    }
    Sleep(200);

    /* Mover el papel al inicio del area de escaneo (A4, y0=1.5mm). */
    if (gt68xx_move_paper(dev, g.abs_y0) != 0) {
        set_error("Fallo move_paper (0x82)");
        goto done;
    }
    if (gt68xx_wait_positioning(dev) != 0) {
        goto done;
    }

    if (gt68xx_setup_scan(dev, &g) != 0) {
        set_error("Fallo setup_scan (0x20)");
        goto done;
    }
    /* SANE omite la exposicion en escaneres sheet-fed sin calibrar; el
     * firmware ya trae 0x157. No abortamos si el comando falla. */
    (void)gt68xx_set_exposure(dev);
    if (gt68xx_set_afe(dev) != 0) {
        set_error("Fallo set_afe (0x22)");
        goto done;
    }
    if (gt68xx_start_scan(dev) != 0) {
        set_error("Fallo start_scan (0x43)");
        goto done;
    }

    /* gt68xx_scanner_internal_start_scan: 30s maximo esperando 0x35. */
    for (i = 0; i < 3000; i++) {
        if (gt68xx_read_ready(dev, &ready) != 0) {
            goto done;
        }
        if (ready) {
            break;
        }
        Sleep(10);
    }
    if (!ready) {
        set_error("El escaner no esta listo para transferir (0x35)");
        goto done;
    }

    for (y = 0; y < height; y++) {
        uint8_t *b;
        uint8_t *gg;
        uint8_t *r;

        if (gt68xx_bulk_read_exact(dev, line, line_bytes) != 0) {
            set_error("Fallo leyendo la linea %d/%d", y, height);
            goto done;
        }
        b = line;
        gg = line + g.scan_bpl;
        r = line + g.scan_bpl * 2;
        if (bpp == 3) {
            for (x = 0; x < width; x++) {
                size_t p = ((size_t)y * (size_t)width + (size_t)x) * 3;
                out[p + 0] = gamma_lut[r[x]];
                out[p + 1] = gamma_lut[gg[x]];
                out[p + 2] = gamma_lut[b[x]];
            }
        } else {
            for (x = 0; x < width; x++) {
                size_t p = (size_t)y * (size_t)width + (size_t)x;
                /* Luma aproximada; el sensor ya entrega 8 bits por canal. */
                out[p] = (uint8_t)((gamma_lut[r[x]] * 77 +
                                   gamma_lut[gg[x]] * 150 +
                                   gamma_lut[b[x]] * 29) >> 8);
            }
        }
    }
    ok = 1;

done:
    (void)gt68xx_stop_scan(dev);
    /* gt68xx.c (sane_cancel) expulsa la hoja en scanners sheet-fed. */
    (void)gt68xx_paperfeed(dev);
    (void)gt68xx_lamp_control(dev, 0);
    free(line);
    if (!ok) {
        free(out);
        return GT68XX_ERR_IO;
    }

    memset(img, 0, sizeof(*img));
    img->width = width;
    img->height = height;
    img->bytes_per_pixel = bpp;
    img->dpi = dpi;
    img->size = size;
    img->data = out;
    return 0;
}

int gt68xx_test_pattern(int dpi, int pixel_type, gt68xx_image *img)
{
    int width;
    int height;
    int bpp;
    size_t size;
    unsigned char *data;
    int x, y;

    if (!img) {
        return GT68XX_ERR_NOMEM;
    }
    memset(img, 0, sizeof(*img));

    if (dpi < 50) {
        dpi = 50;
    }
    if (dpi > 600) {
        dpi = 600;
    }

    /* Área A4 real: 210 x 297 mm. */
    width = (int)((210.0 / 25.4) * dpi + 0.5);
    height = (int)((297.0 / 25.4) * dpi + 0.5);
    if (width < 1 || height < 1 || width > 20000 || height > 30000) {
        set_error("Dimensiones de prueba invalidas: %dx%d", width, height);
        return GT68XX_ERR_IO;
    }

    bpp = (pixel_type == GT68XX_PIXEL_RGB) ? 3 : 1;
    size = (size_t)width * (size_t)height * (size_t)bpp;
    if (size / (size_t)width / (size_t)bpp != (size_t)height) {
        set_error("Desbordamiento de tamano de imagen");
        return GT68XX_ERR_IO;
    }

    data = (unsigned char *)malloc(size);
    if (!data) {
        set_error("Sin memoria para la imagen de prueba (%lu bytes)",
                  (unsigned long)size);
        return GT68XX_ERR_NOMEM;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            size_t pos = ((size_t)y * (size_t)width + (size_t)x) * (size_t)bpp;
            int v = 255;
            int border = (width > 40 ? 4 : 1);

            if (x < border || y < border || x >= width - border || y >= height - border) {
                v = 0;
            } else if (((x / 16) + (y / 16)) & 1) {
                v = 230;
            }

            /* Cruz central y algunas líneas de referencia. */
            if (abs(x * 2 - width) < 2 || abs(y * 2 - height) < 2) {
                v = 0;
            }
            if (y == height / 4 || y == (3 * height) / 4) {
                v = 80;
            }

            /* Gradiente horizontal suave. */
            v = (v * 3 + (x * 255) / width) / 4;

            if (bpp == 1) {
                data[pos] = (unsigned char)v;
            } else {
                data[pos + 0] = (unsigned char)v;
                data[pos + 1] = (unsigned char)((y * 255) / height);
                data[pos + 2] = (unsigned char)((x * 255) / width);
            }
        }
    }

    img->width = width;
    img->height = height;
    img->bytes_per_pixel = bpp;
    img->dpi = dpi;
    img->size = size;
    img->data = data;
    return 0;
}

int gt68xx_proto_scan(gt68xx_device *dev, int dpi, int pixel_type, gt68xx_image *img)
{
    if (!dev) {
        return gt68xx_test_pattern(dpi, pixel_type, img);
    }
    return gt68xx_scan_real(dev, dpi, pixel_type, img);
}

void gt68xx_image_free(gt68xx_image *img)
{
    if (!img) {
        return;
    }
    free(img->data);
    memset(img, 0, sizeof(*img));
}
