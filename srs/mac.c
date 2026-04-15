#include "mac.h"

#include <stdio.h>
#include <string.h>

/*
 * mac.c — канальный уровень (симуляция IEEE 802.15.4 MAC).
 *
 * Реализованы:
 *   - сериализация/десериализация MHR с FCF, DSN, PAN ID, адресами;
 *   - вычисление и проверка FCS (CRC-16/ITU-T);
 *   - отправка Data-кадра с флагом ACK Request;
 *   - приём кадра и автоматическая отправка ACK;
 *   - фильтрация по PAN ID и адресу назначения.
 *
 * Не реализовано (за рамками учебного проекта):
 *   - CSMA-CA с backoff-счётчиком;
 *   - Beacon-кадры и GTS (Guaranteed Time Slots);
 *   - 64-битная (EUI-64) адресация.
 */

/* ------------------------------------------------------------------ */
/* CRC-16/ITU-T (полином 0x1021, начальное значение 0x0000)           */
/* Используется как FCS в IEEE 802.15.4                               */
/* ------------------------------------------------------------------ */
uint16_t mac_fcs(const uint8_t *data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* Сериализация MAC-кадра в байтовый буфер (PSDU для PHY)             */
/* ------------------------------------------------------------------ */
static size_t mac_serialize(const mac_frame_t *frame,
                            uint8_t *buf, size_t buf_len)
{
    size_t pos = 0;

    /* FCF — little-endian */
    buf[pos++] = (uint8_t)(frame->header.fcf & 0xFF);
    buf[pos++] = (uint8_t)(frame->header.fcf >> 8);

    /* DSN */
    buf[pos++] = frame->header.dsn;

    /* PAN ID — little-endian */
    buf[pos++] = (uint8_t)(frame->header.pan_id & 0xFF);
    buf[pos++] = (uint8_t)(frame->header.pan_id >> 8);

    /* Destination address — little-endian */
    buf[pos++] = (uint8_t)(frame->header.dst_addr & 0xFF);
    buf[pos++] = (uint8_t)(frame->header.dst_addr >> 8);

    /* Source address — little-endian */
    buf[pos++] = (uint8_t)(frame->header.src_addr & 0xFF);
    buf[pos++] = (uint8_t)(frame->header.src_addr >> 8);

    /* Payload */
    if (frame->payload_len > 0) {
        memcpy(buf + pos, frame->payload, frame->payload_len);
        pos += frame->payload_len;
    }

    /* FCS: считаем от начала кадра до конца payload */
    uint16_t fcs = mac_fcs(buf, pos);
    buf[pos++] = (uint8_t)(fcs & 0xFF);
    buf[pos++] = (uint8_t)(fcs >> 8);

    (void)buf_len; /* caller гарантирует достаточный размер */
    return pos;
}

/* ------------------------------------------------------------------ */
/* Десериализация PSDU в mac_frame_t                                  */
/* ------------------------------------------------------------------ */
static int mac_deserialize(const uint8_t *buf, size_t len,
                           mac_frame_t *frame)
{
    /* Минимальная длина: 9 байт MHR + 2 байта FCS */
    if (len < 11)
        return MAC_ERR_INVALID_PARAM;

    size_t pos = 0;

    /* FCF */
    frame->header.fcf  = (uint16_t)buf[pos];
    frame->header.fcf |= (uint16_t)buf[pos + 1] << 8;
    pos += 2;

    /* DSN */
    frame->header.dsn = buf[pos++];

    /* PAN ID */
    frame->header.pan_id  = (uint16_t)buf[pos];
    frame->header.pan_id |= (uint16_t)buf[pos + 1] << 8;
    pos += 2;

    /* Destination address */
    frame->header.dst_addr  = (uint16_t)buf[pos];
    frame->header.dst_addr |= (uint16_t)buf[pos + 1] << 8;
    pos += 2;

    /* Source address */
    frame->header.src_addr  = (uint16_t)buf[pos];
    frame->header.src_addr |= (uint16_t)buf[pos + 1] << 8;
    pos += 2;

    /* Payload (всё, кроме последних 2 байт FCS) */
    size_t payload_len = len - pos - MAC_FCS_SIZE;
    if (payload_len > MAC_MAX_PAYLOAD_SIZE)
        return MAC_ERR_INVALID_PARAM;

    frame->payload_len = payload_len;
    if (payload_len > 0)
        memcpy(frame->payload, buf + pos, payload_len);
    pos += payload_len;

    /* FCS: считываем */
    uint16_t recv_fcs  = (uint16_t)buf[pos];
    recv_fcs |= (uint16_t)buf[pos + 1] << 8;

    /* FCS: проверяем */
    uint16_t calc_fcs = mac_fcs(buf, len - MAC_FCS_SIZE);
    if (recv_fcs != calc_fcs) {
        fprintf(stderr, "[MAC] FCS error: expected 0x%04X, got 0x%04X\n",
                calc_fcs, recv_fcs);
        return MAC_ERR_IO;
    }

    frame->fcs = recv_fcs;
    return MAC_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Отправка ACK-кадра в ответ на принятый DSN                         */
/* ------------------------------------------------------------------ */
static int mac_send_ack(mac_ctx_t *ctx, uint8_t dsn)
{
    mac_frame_t ack;
    memset(&ack, 0, sizeof(ack));

    ack.header.fcf = MAC_FCF_FRAME_TYPE(MAC_FRAME_TYPE_ACK);
    ack.header.dsn = dsn;
    ack.payload_len = 0;

    /* ACK-кадр не содержит адресов согласно стандарту */
    uint8_t buf[PHY_MAX_PACKET_SIZE];
    /* FCF (2) + DSN (1) + FCS (2) = 5 байт */
    buf[0] = (uint8_t)(ack.header.fcf & 0xFF);
    buf[1] = (uint8_t)(ack.header.fcf >> 8);
    buf[2] = dsn;
    uint16_t fcs = mac_fcs(buf, 3);
    buf[3] = (uint8_t)(fcs & 0xFF);
    buf[4] = (uint8_t)(fcs >> 8);

    return phy_send(ctx->phy, buf, 5);
}

/* ------------------------------------------------------------------ */
/* Публичные функции                                                   */
/* ------------------------------------------------------------------ */

void mac_init(mac_ctx_t *ctx, phy_ctx_t *phy,
              uint16_t pan_id, uint16_t short_addr)
{
    if (!ctx || !phy)
        return;
    ctx->phy        = phy;
    ctx->pan_id     = pan_id;
    ctx->short_addr = short_addr;
    ctx->dsn        = 0;
    printf("[MAC] Initialized: PAN=0x%04X addr=0x%04X\n",
           pan_id, short_addr);
}

int mac_send(mac_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *payload, size_t len)
{
    if (!ctx || !payload || len == 0 || len > MAC_MAX_PAYLOAD_SIZE)
        return MAC_ERR_INVALID_PARAM;

    mac_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    /* FCF: Data-кадр, короткая адресация, ACK Request, PAN ID Compression */
    frame.header.fcf = MAC_FCF_FRAME_TYPE(MAC_FRAME_TYPE_DATA)
                     | MAC_FCF_ACK_REQUEST
                     | MAC_FCF_PAN_ID_COMPRESS
                     | ((uint16_t)MAC_ADDR_MODE_SHORT << 10)  /* DstAddrMode */
                     | ((uint16_t)MAC_ADDR_MODE_SHORT << 14); /* SrcAddrMode */

    frame.header.dsn      = ctx->dsn++;
    frame.header.pan_id   = ctx->pan_id;
    frame.header.dst_addr = dst_addr;
    frame.header.src_addr = ctx->short_addr;
    frame.payload_len     = len;
    memcpy(frame.payload, payload, len);

    uint8_t buf[PHY_MAX_PACKET_SIZE];
    size_t  frame_len = mac_serialize(&frame, buf, sizeof(buf));

    int ret = phy_send(ctx->phy, buf, frame_len);
    if (ret != PHY_SUCCESS)
        return MAC_ERR_IO;

    printf("[MAC] TX Data DSN=%u to 0x%04X (%zu bytes payload)\n",
           frame.header.dsn, dst_addr, len);

    /*
     * Ожидание ACK (упрощённо — один phy_recv).
     * В реальности здесь таймаут macAckWaitDuration = 54 символа.
     */
    uint8_t ack_buf[16];
    ssize_t ack_len = phy_recv(ctx->phy, ack_buf, sizeof(ack_buf));
    if (ack_len < 5) {
        fprintf(stderr, "[MAC] No ACK received for DSN=%u\n",
                frame.header.dsn);
        return MAC_ERR_NO_ACK;
    }

    uint8_t ack_frame_type = ack_buf[0] & 0x07;
    uint8_t ack_dsn        = ack_buf[2];

    if (ack_frame_type != MAC_FRAME_TYPE_ACK ||
        ack_dsn != frame.header.dsn) {
        fprintf(stderr, "[MAC] ACK mismatch: type=%u DSN=%u\n",
                ack_frame_type, ack_dsn);
        return MAC_ERR_NO_ACK;
    }

    printf("[MAC] ACK received for DSN=%u\n", frame.header.dsn);
    return MAC_SUCCESS;
}

int mac_recv(mac_ctx_t *ctx, mac_frame_t *frame)
{
    if (!ctx || !frame)
        return MAC_ERR_INVALID_PARAM;

    uint8_t buf[PHY_MAX_PACKET_SIZE];
    ssize_t n = phy_recv(ctx->phy, buf, sizeof(buf));
    if (n <= 0)
        return MAC_ERR_IO;

    int ret = mac_deserialize(buf, (size_t)n, frame);
    if (ret != MAC_SUCCESS)
        return ret;

    /* Фильтрация по PAN ID */
    if (frame->header.pan_id != ctx->pan_id &&
        frame->header.pan_id != MAC_BROADCAST_PAN) {
        printf("[MAC] RX: dropped (PAN 0x%04X != 0x%04X)\n",
               frame->header.pan_id, ctx->pan_id);
        return MAC_ERR_INVALID_PARAM;
    }

    /* Фильтрация по адресу назначения */
    if (frame->header.dst_addr != ctx->short_addr &&
        frame->header.dst_addr != MAC_BROADCAST_ADDR) {
        printf("[MAC] RX: dropped (dst 0x%04X != 0x%04X)\n",
               frame->header.dst_addr, ctx->short_addr);
        return MAC_ERR_INVALID_PARAM;
    }

    /* Отправляем ACK, если отправитель его запросил */
    if (frame->header.fcf & MAC_FCF_ACK_REQUEST) {
        mac_send_ack(ctx, frame->header.dsn);
    }

    printf("[MAC] RX Data DSN=%u from 0x%04X (%zu bytes payload)\n",
           frame->header.dsn, frame->header.src_addr, frame->payload_len);

    return MAC_SUCCESS;
}
