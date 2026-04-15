#include "nwk.h"

#include <stdio.h>
#include <string.h>

/*
 * nwk.c — сетевой уровень (симуляция ZigBee NWK).
 *
 * Реализованы:
 *   - таблица маршрутизации с uint16_t адресами;
 *   - статическое добавление маршрутов;
 *   - отправка/приём RREQ и RREP (упрощённый route discovery);
 *   - пересылка (forwarding) транзитных пакетов;
 *   - поле Radius (TTL) с декрементом на каждом хопе;
 *   - защита от дубликатов RREQ по (src, rreq_id).
 */

#define NWK_DEFAULT_RADIUS  7   /* максимальное число хопов по умолчанию */

/* ------------------------------------------------------------------ */
/* Сериализация NWK-заголовка                                          */
/* ------------------------------------------------------------------ */
static size_t nwk_serialize_header(const nwk_header_t *hdr,
                                   uint8_t *buf)
{
    size_t pos = 0;
    buf[pos++] = (uint8_t)(hdr->fcf & 0xFF);
    buf[pos++] = (uint8_t)(hdr->fcf >> 8);
    buf[pos++] = (uint8_t)(hdr->dst_addr & 0xFF);
    buf[pos++] = (uint8_t)(hdr->dst_addr >> 8);
    buf[pos++] = (uint8_t)(hdr->src_addr & 0xFF);
    buf[pos++] = (uint8_t)(hdr->src_addr >> 8);
    buf[pos++] = hdr->radius;
    buf[pos++] = hdr->seq_num;
    return pos; /* 8 байт */
}

/* ------------------------------------------------------------------ */
/* Десериализация NWK-заголовка                                        */
/* ------------------------------------------------------------------ */
static int nwk_deserialize_header(const uint8_t *buf, size_t len,
                                  nwk_header_t *hdr)
{
    if (len < 8)
        return NWK_ERR_INVALID_PARAM;

    hdr->fcf       = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    hdr->dst_addr  = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    hdr->src_addr  = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    hdr->radius    = buf[6];
    hdr->seq_num   = buf[7];
    return NWK_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Проверка дубликата RREQ                                             */
/* ------------------------------------------------------------------ */
static int nwk_rreq_is_dup(nwk_ctx_t *ctx,
                            uint16_t src, uint8_t rreq_id)
{
    for (int i = 0; i < NWK_MAX_RREQ_TABLE; i++) {
        if (ctx->rreq_table[i].valid &&
            ctx->rreq_table[i].src_addr == src &&
            ctx->rreq_table[i].rreq_id  == rreq_id)
            return 1;
    }
    return 0;
}

static void nwk_rreq_record(nwk_ctx_t *ctx,
                             uint16_t src, uint8_t rreq_id)
{
    /* Ищем свободную или перезаписываем первую */
    for (int i = 0; i < NWK_MAX_RREQ_TABLE; i++) {
        if (!ctx->rreq_table[i].valid) {
            ctx->rreq_table[i].src_addr = src;
            ctx->rreq_table[i].rreq_id  = rreq_id;
            ctx->rreq_table[i].valid    = 1;
            return;
        }
    }
    /* Таблица полна — перезаписываем нулевую запись */
    ctx->rreq_table[0].src_addr = src;
    ctx->rreq_table[0].rreq_id  = rreq_id;
}

/* ------------------------------------------------------------------ */
/* Отправка RREQ-команды широковещательно                             */
/* ------------------------------------------------------------------ */
static int nwk_send_rreq(nwk_ctx_t *ctx, uint16_t target)
{
    /*
     * Формат NWK Command RREQ payload (упрощённый):
     *   [0]    Command ID = 0x01
     *   [1]    RREQ ID
     *   [2–3]  Target address (little-endian)
     *   [4]    Path cost = 0 (инициатор)
     */
    uint8_t buf[MAC_MAX_PAYLOAD_SIZE];
    nwk_header_t hdr;

    hdr.fcf      = NWK_FRAME_TYPE_CMD;
    hdr.dst_addr = NWK_BROADCAST_ADDR;
    hdr.src_addr = ctx->addr;
    hdr.radius   = NWK_DEFAULT_RADIUS;
    hdr.seq_num  = ctx->seq_num++;

    size_t pos = nwk_serialize_header(&hdr, buf);

    buf[pos++] = NWK_CMD_RREQ;
    buf[pos++] = ctx->rreq_id;
    buf[pos++] = (uint8_t)(target & 0xFF);
    buf[pos++] = (uint8_t)(target >> 8);
    buf[pos++] = 0; /* path cost */

    nwk_rreq_record(ctx, ctx->addr, ctx->rreq_id);
    ctx->rreq_id++;

    printf("[NWK] Sending RREQ for 0x%04X\n", target);
    return mac_send(ctx->mac, MAC_BROADCAST_ADDR, buf, pos);
}

/* ------------------------------------------------------------------ */
/* Отправка RREP в ответ на RREQ                                       */
/* ------------------------------------------------------------------ */
static int nwk_send_rrep(nwk_ctx_t *ctx,
                          uint16_t rreq_src, uint16_t target,
                          uint8_t path_cost)
{
    /*
     * Формат NWK Command RREP payload (упрощённый):
     *   [0]    Command ID = 0x02
     *   [1]    Reserved
     *   [2–3]  Originator address (кто послал RREQ)
     *   [4–5]  Responder address  (целевой узел)
     *   [6]    Path cost
     */
    uint8_t buf[MAC_MAX_PAYLOAD_SIZE];
    nwk_header_t hdr;

    nwk_route_entry_t *rt = nwk_find_route(ctx, rreq_src);
    uint16_t next_hop = rt ? rt->next_hop : rreq_src;

    hdr.fcf      = NWK_FRAME_TYPE_CMD;
    hdr.dst_addr = rreq_src;
    hdr.src_addr = ctx->addr;
    hdr.radius   = NWK_DEFAULT_RADIUS;
    hdr.seq_num  = ctx->seq_num++;

    size_t pos = nwk_serialize_header(&hdr, buf);

    buf[pos++] = NWK_CMD_RREP;
    buf[pos++] = 0; /* reserved */
    buf[pos++] = (uint8_t)(rreq_src & 0xFF);
    buf[pos++] = (uint8_t)(rreq_src >> 8);
    buf[pos++] = (uint8_t)(target & 0xFF);
    buf[pos++] = (uint8_t)(target >> 8);
    buf[pos++] = path_cost;

    printf("[NWK] Sending RREP to 0x%04X (originator 0x%04X)\n",
           next_hop, rreq_src);
    return mac_send(ctx->mac, next_hop, buf, pos);
}

/* ------------------------------------------------------------------ */
/* Публичные функции                                                   */
/* ------------------------------------------------------------------ */

void nwk_init(nwk_ctx_t *ctx, mac_ctx_t *mac, uint16_t addr)
{
    if (!ctx || !mac)
        return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->mac     = mac;
    ctx->addr    = addr;
    ctx->seq_num = 0;
    ctx->rreq_id = 0;
    printf("[NWK] Initialized: addr=0x%04X\n", addr);
}

int nwk_add_route(nwk_ctx_t *ctx, uint16_t destination,
                  uint16_t next_hop, uint8_t cost)
{
    if (!ctx)
        return NWK_ERR_INVALID_PARAM;

    for (int i = 0; i < NWK_MAX_ROUTE_TABLE; i++) {
        /* Обновить существующую запись */
        if (ctx->route_table[i].status == NWK_ROUTE_ACTIVE &&
            ctx->route_table[i].destination == destination) {
            ctx->route_table[i].next_hop = next_hop;
            ctx->route_table[i].cost     = cost;
            printf("[NWK] Route updated: 0x%04X via 0x%04X (cost %u)\n",
                   destination, next_hop, cost);
            return NWK_SUCCESS;
        }
        /* Добавить в пустую запись */
        if (ctx->route_table[i].status == NWK_ROUTE_INACTIVE) {
            ctx->route_table[i].destination = destination;
            ctx->route_table[i].next_hop    = next_hop;
            ctx->route_table[i].cost        = cost;
            ctx->route_table[i].status      = NWK_ROUTE_ACTIVE;
            printf("[NWK] Route added: 0x%04X via 0x%04X (cost %u)\n",
                   destination, next_hop, cost);
            return NWK_SUCCESS;
        }
    }
    fprintf(stderr, "[NWK] Route table full\n");
    return NWK_ERR_TABLE_FULL;
}

nwk_route_entry_t *nwk_find_route(nwk_ctx_t *ctx, uint16_t destination)
{
    for (int i = 0; i < NWK_MAX_ROUTE_TABLE; i++) {
        if (ctx->route_table[i].status      == NWK_ROUTE_ACTIVE &&
            ctx->route_table[i].destination == destination)
            return &ctx->route_table[i];
    }
    return NULL;
}

int nwk_route_discovery(nwk_ctx_t *ctx, uint16_t destination)
{
    if (nwk_find_route(ctx, destination))
        return NWK_SUCCESS; /* маршрут уже есть */
    return nwk_send_rreq(ctx, destination);
}

int nwk_send(nwk_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0)
        return NWK_ERR_INVALID_PARAM;

    nwk_route_entry_t *rt = nwk_find_route(ctx, dst_addr);
    if (!rt) {
        printf("[NWK] No route to 0x%04X, starting discovery\n", dst_addr);
        int ret = nwk_route_discovery(ctx, dst_addr);
        if (ret != NWK_SUCCESS)
            return NWK_ERR_NO_ROUTE;
        rt = nwk_find_route(ctx, dst_addr);
        if (!rt)
            return NWK_ERR_NO_ROUTE;
    }

    uint8_t buf[MAC_MAX_PAYLOAD_SIZE];
    nwk_header_t hdr;

    hdr.fcf      = NWK_FRAME_TYPE_DATA;
    hdr.dst_addr = dst_addr;
    hdr.src_addr = ctx->addr;
    hdr.radius   = NWK_DEFAULT_RADIUS;
    hdr.seq_num  = ctx->seq_num++;

    size_t pos = nwk_serialize_header(&hdr, buf);

    if (pos + len > sizeof(buf)) {
        fprintf(stderr, "[NWK] Payload too large\n");
        return NWK_ERR_INVALID_PARAM;
    }
    memcpy(buf + pos, data, len);
    pos += len;

    printf("[NWK] TX to 0x%04X via 0x%04X (%zu bytes)\n",
           dst_addr, rt->next_hop, len);
    return mac_send(ctx->mac, rt->next_hop, buf, pos);
}

int nwk_recv(nwk_ctx_t *ctx, uint8_t *data, size_t *len,
             uint16_t *src_addr)
{
    if (!ctx || !data || !len || !src_addr)
        return NWK_ERR_INVALID_PARAM;

    mac_frame_t mac_frame;
    int ret = mac_recv(ctx->mac, &mac_frame);
    if (ret != MAC_SUCCESS)
        return NWK_ERR_IO;

    nwk_header_t hdr;
    ret = nwk_deserialize_header(mac_frame.payload,
                                 mac_frame.payload_len, &hdr);
    if (ret != NWK_SUCCESS)
        return ret;

    uint8_t frame_type = hdr.fcf & 0x03;

    /* ---- Обработка командных кадров (RREQ / RREP) ---- */
    if (frame_type == NWK_FRAME_TYPE_CMD) {
        size_t cmd_offset = 8; /* после NWK-заголовка */
        if (mac_frame.payload_len <= cmd_offset)
            return NWK_ERR_INVALID_PARAM;

        uint8_t cmd_id = mac_frame.payload[cmd_offset];

        if (cmd_id == NWK_CMD_RREQ) {
            uint8_t  rreq_id = mac_frame.payload[cmd_offset + 1];
            uint16_t target  = (uint16_t)mac_frame.payload[cmd_offset + 2]
                             | ((uint16_t)mac_frame.payload[cmd_offset + 3] << 8);
            uint8_t  cost    = mac_frame.payload[cmd_offset + 4];

            if (nwk_rreq_is_dup(ctx, hdr.src_addr, rreq_id)) {
                printf("[NWK] Duplicate RREQ from 0x%04X, dropped\n",
                       hdr.src_addr);
                return NWK_ERR_INVALID_PARAM;
            }
            nwk_rreq_record(ctx, hdr.src_addr, rreq_id);

            /* Обратный маршрут к отправителю RREQ */
            nwk_add_route(ctx, hdr.src_addr,
                          mac_frame.header.src_addr, cost + 1);

            if (target == ctx->addr) {
                /* Мы и есть цель — отвечаем RREP */
                nwk_send_rrep(ctx, hdr.src_addr, target, cost + 1);
            } else if (hdr.radius > 1) {
                /* Ретранслируем RREQ с уменьшенным radius */
                mac_frame.payload[6] = hdr.radius - 1; /* radius в буфере */
                mac_send(ctx->mac, MAC_BROADCAST_ADDR,
                         mac_frame.payload, mac_frame.payload_len);
            }
        } else if (cmd_id == NWK_CMD_RREP) {
            uint16_t originator = (uint16_t)mac_frame.payload[cmd_offset + 2]
                                | ((uint16_t)mac_frame.payload[cmd_offset + 3] << 8);
            uint16_t responder  = (uint16_t)mac_frame.payload[cmd_offset + 4]
                                | ((uint16_t)mac_frame.payload[cmd_offset + 5] << 8);
            uint8_t  path_cost  = mac_frame.payload[cmd_offset + 6];

            /* Записываем маршрут к цели */
            nwk_add_route(ctx, responder,
                          mac_frame.header.src_addr, path_cost);

            if (originator != ctx->addr) {
                /* Пересылаем RREP дальше к инициатору */
                nwk_route_entry_t *rt = nwk_find_route(ctx, originator);
                if (rt)
                    mac_send(ctx->mac, rt->next_hop,
                             mac_frame.payload, mac_frame.payload_len);
            } else {
                printf("[NWK] Route to 0x%04X discovered (cost %u)\n",
                       responder, path_cost);
            }
        }
        return NWK_ERR_INVALID_PARAM; /* CMD-кадр — не данные для верхнего уровня */
    }

    /* ---- Data-кадр ---- */
    if (hdr.dst_addr != ctx->addr && hdr.dst_addr != NWK_BROADCAST_ADDR) {
        /* Пересылка транзитного пакета */
        if (hdr.radius == 0) {
            fprintf(stderr, "[NWK] Radius expired, dropping\n");
            return NWK_ERR_IO;
        }
        nwk_route_entry_t *rt = nwk_find_route(ctx, hdr.dst_addr);
        if (rt) {
            mac_frame.payload[6] = hdr.radius - 1;
            mac_send(ctx->mac, rt->next_hop,
                     mac_frame.payload, mac_frame.payload_len);
        }
        return NWK_ERR_INVALID_PARAM; /* не для нас */
    }

    /* Кадр предназначен нам */
    size_t payload_len = mac_frame.payload_len - 8;
    if (payload_len > *len)
        payload_len = *len;

    memcpy(data, mac_frame.payload + 8, payload_len);
    *len      = payload_len;
    *src_addr = hdr.src_addr;

    printf("[NWK] RX Data from 0x%04X (%zu bytes)\n",
           hdr.src_addr, payload_len);
    return NWK_SUCCESS;
}
