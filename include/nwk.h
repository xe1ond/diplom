#ifndef NWK_H
#define NWK_H

#include <stdint.h>
#include <stddef.h>
#include "mac.h"

/* ZigBee NWK constants */
#define NWK_MAX_ROUTE_TABLE     16   /* размер таблицы маршрутизации     */
#define NWK_MAX_RREQ_TABLE      8    /* одновременных RREQ               */
#define NWK_BROADCAST_ADDR      0xFFFF
#define NWK_COORDINATOR_ADDR    0x0000

/* Типы NWK-кадров (FCF bits [1:0]) */
#define NWK_FRAME_TYPE_DATA     0x00
#define NWK_FRAME_TYPE_CMD      0x01

/* NWK Command identifiers */
#define NWK_CMD_RREQ            0x01  /* Route Request  */
#define NWK_CMD_RREP            0x02  /* Route Reply    */

/* Коды возврата */
#define NWK_SUCCESS             0
#define NWK_ERR_INVALID_PARAM  -1
#define NWK_ERR_NO_ROUTE       -2
#define NWK_ERR_TABLE_FULL     -3
#define NWK_ERR_IO             -4

/*
 * Статус записи в таблице маршрутизации.
 */
typedef enum {
    NWK_ROUTE_ACTIVE   = 1,
    NWK_ROUTE_INACTIVE = 0
} nwk_route_status_t;

/*
 * Запись таблицы маршрутизации.
 *
 * destination — конечный адрес узла;
 * next_hop    — следующий хоп на пути к destination;
 * cost        — стоимость маршрута (число хопов).
 */
typedef struct {
    uint16_t           destination;
    uint16_t           next_hop;
    uint8_t            cost;
    nwk_route_status_t status;
} nwk_route_entry_t;

/*
 * Запись таблицы ожидающих RREQ (для защиты от дубликатов).
 */
typedef struct {
    uint16_t src_addr;
    uint8_t  rreq_id;
    uint8_t  valid;
} nwk_rreq_entry_t;

/*
 * Заголовок NWK-кадра (упрощённый ZigBee NWK Frame Header).
 *
 *  +-----+----------+---------+---------+-------+
 *  | FCF | Dst Addr | Src Addr| Radius  | SeqNum|
 *  | 2 б | 2 байта  | 2 байта | 1 байт  | 1 байт|
 *  +-----+----------+---------+---------+-------+
 */
typedef struct {
    uint16_t fcf;
    uint16_t dst_addr;
    uint16_t src_addr;
    uint8_t  radius;    /* TTL: декрементируется на каждом хопе */
    uint8_t  seq_num;
} nwk_header_t;

/*
 * Контекст NWK-уровня.
 */
typedef struct {
    mac_ctx_t        *mac;
    uint16_t          addr;
    nwk_route_entry_t route_table[NWK_MAX_ROUTE_TABLE];
    nwk_rreq_entry_t  rreq_table[NWK_MAX_RREQ_TABLE];
    uint8_t           seq_num;
    uint8_t           rreq_id;
} nwk_ctx_t;

/* Инициализация NWK. */
void nwk_init(nwk_ctx_t *ctx, mac_ctx_t *mac, uint16_t addr);

/* Добавление маршрута вручную (статическая маршрутизация). */
int nwk_add_route(nwk_ctx_t *ctx, uint16_t destination,
                  uint16_t next_hop, uint8_t cost);

/* Поиск маршрута в таблице. Возвращает указатель на запись или NULL. */
nwk_route_entry_t *nwk_find_route(nwk_ctx_t *ctx, uint16_t destination);

/*
 * Отправка RREQ (Route Request) для обнаружения маршрута.
 * Если маршрут уже есть в таблице — RREQ не отправляется.
 */
int nwk_route_discovery(nwk_ctx_t *ctx, uint16_t destination);

/*
 * Отправка NWK Data-кадра.
 * Если маршрут не найден, автоматически запускает route discovery.
 */
int nwk_send(nwk_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *data, size_t len);

/* Приём NWK-кадра. Если dst != наш адрес — пересылает дальше. */
int nwk_recv(nwk_ctx_t *ctx, uint8_t *data, size_t *len,
             uint16_t *src_addr);

#endif /* NWK_H */
