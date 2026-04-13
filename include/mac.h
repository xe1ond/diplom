#ifndef MAC_H
#define MAC_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "phy.h"

/* IEEE 802.15.4 MAC constants */
#define MAC_MAX_PAYLOAD_SIZE    118   /* 127 - 2(FCS) - 7(min MHR)        */
#define MAC_FCS_SIZE            2     /* CRC-16/ITU-T                      */
#define MAC_BROADCAST_ADDR      0xFFFF
#define MAC_BROADCAST_PAN       0xFFFF

/* Типы кадров (FCF bits [2:0]) */
#define MAC_FRAME_TYPE_BEACON   0x00
#define MAC_FRAME_TYPE_DATA     0x01
#define MAC_FRAME_TYPE_ACK      0x02
#define MAC_FRAME_TYPE_CMD      0x03

/* Биты FCF */
#define MAC_FCF_FRAME_TYPE(t)   ((t) & 0x07)
#define MAC_FCF_SEC_ENABLED     (1 << 3)
#define MAC_FCF_FRAME_PENDING   (1 << 4)
#define MAC_FCF_ACK_REQUEST     (1 << 5)
#define MAC_FCF_PAN_ID_COMPRESS (1 << 6)
/* Режим адресации: 0x02 = 16-бит, 0x03 = 64-бит */
#define MAC_ADDR_MODE_SHORT     0x02
#define MAC_ADDR_MODE_NONE      0x00

/* Коды возврата */
#define MAC_SUCCESS             0
#define MAC_ERR_INVALID_PARAM  -1
#define MAC_ERR_CHANNEL_BUSY   -2
#define MAC_ERR_NO_ACK         -3
#define MAC_ERR_IO             -4

/*
 * Frame Control Field (2 байта).
 * Хранится как uint16_t little-endian согласно стандарту.
 */
typedef uint16_t mac_fcf_t;

/*
 * Заголовок MAC-кадра (MHR — MAC Header).
 *
 * Упрощённая версия для диплома: короткая адресация (16 бит),
 * один PAN для источника и получателя (PAN ID Compression = 1).
 *
 *  +-------+-----+--------+---------+---------+
 *  |  FCF  | DSN | PAN ID | DstAddr | SrcAddr |
 *  | 2 байт| 1 б | 2 байта| 2 байта | 2 байта |
 *  +-------+-----+--------+---------+---------+
 */
typedef struct {
    mac_fcf_t  fcf;
    uint8_t    dsn;        /* Data Sequence Number         */
    uint16_t   pan_id;     /* PAN Identifier               */
    uint16_t   dst_addr;   /* короткий адрес получателя    */
    uint16_t   src_addr;   /* короткий адрес отправителя   */
} mac_header_t;

/*
 * Полный MAC-кадр: заголовок + полезная нагрузка + FCS.
 */
typedef struct {
    mac_header_t header;
    uint8_t      payload[MAC_MAX_PAYLOAD_SIZE];
    size_t       payload_len;
    uint16_t     fcs;      /* вычисляется при сериализации */
} mac_frame_t;

/*
 * Контекст MAC-уровня.
 */
typedef struct {
    phy_ctx_t  *phy;
    uint16_t    short_addr;  /* адрес данного узла           */
    uint16_t    pan_id;
    uint8_t     dsn;         /* счётчик последовательности   */
} mac_ctx_t;

/* Инициализация MAC. */
void mac_init(mac_ctx_t *ctx, phy_ctx_t *phy,
              uint16_t pan_id, uint16_t short_addr);

/*
 * Отправка MAC Data-кадра с запросом ACK.
 * Возвращает MAC_SUCCESS или код ошибки.
 */
int mac_send(mac_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *payload, size_t len);

/*
 * Приём MAC-кадра.
 * Заполняет frame и возвращает MAC_SUCCESS или код ошибки.
 * ACK отправляется автоматически, если его запросил отправитель.
 */
int mac_recv(mac_ctx_t *ctx, mac_frame_t *frame);

/* Вычисление CRC-16/ITU-T (FCS). */
uint16_t mac_fcs(const uint8_t *data, size_t len);

#endif /* MAC_H */
