#include "phy.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/*
 * phy.c — физический уровень (симуляция IEEE 802.15.4 PHY).
 *
 * В реальном устройстве здесь был бы драйвер радиочипа
 * (например, AT86RF233 или CC2520).  В симуляции роль радиоканала
 * играет виртуальный последовательный порт (или именованный канал).
 *
 * Симулируемые функции:
 *   - выбор канала (11–26, диапазон 2.4 ГГц);
 *   - CCA (Clear Channel Assessment) — всегда IDLE в симуляции;
 *   - добавление PHY-преамбулы (SHR + PHR) при передаче;
 *   - проверка и снятие PHY-заголовка при приёме;
 *   - ограничение длины пакета (PHY_MAX_PACKET_SIZE = 127 байт).
 */

/* SFD (Start of Frame Delimiter) согласно IEEE 802.15.4-2015, §12.1.3 */
#define PHY_SFD 0xA7

int phy_init(phy_ctx_t *ctx, const char *device, uint8_t channel)
{
    if (!ctx || !device)
        return PHY_ERR_INVALID_PARAM;

    if (channel < 11 || channel > 26) {
        fprintf(stderr, "[PHY] Invalid channel %u (must be 11–26)\n", channel);
        return PHY_ERR_INVALID_PARAM;
    }

    strncpy(ctx->device, device, sizeof(ctx->device) - 1);
    ctx->device[sizeof(ctx->device) - 1] = '\0';
    ctx->channel  = channel;
    ctx->tx_power = 0; /* 0 дБм по умолчанию */

    /*
     * Открываем устройство в режиме чтения/записи без блокировки.
     * O_NOCTTY — не делать порт управляющим терминалом процесса.
     */
    ctx->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ctx->fd < 0) {
        fprintf(stderr, "[PHY] Cannot open device '%s': %s\n",
                device, strerror(errno));
        return PHY_ERR_IO;
    }

    printf("[PHY] Initialized on '%s', channel %u\n", device, channel);
    return PHY_SUCCESS;
}

void phy_close(phy_ctx_t *ctx)
{
    if (ctx && ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
        printf("[PHY] Closed '%s'\n", ctx->device);
    }
}

phy_cca_status_t phy_cca(const phy_ctx_t *ctx)
{
    (void)ctx;
    /*
     * В симуляции канал всегда свободен.
     * На реальном железе здесь выполнялся бы замер RSSI
     * и сравнение с порогом ED (Energy Detection threshold).
     */
    return PHY_CHANNEL_IDLE;
}

/*
 * Формат PHY-кадра (согласно IEEE 802.15.4):
 *
 *  +-----------+-----+-----+------ ... ------+
 *  | Preamble  | SFD | PHR |      PSDU       |
 *  | (4 байта) |(1 б)|(1 б)| (до 127 байт)  |
 *  +-----------+-----+-----+------ ... ------+
 *
 * PHR = длина PSDU в байтах (7 бит, бит 7 зарезервирован).
 */
int phy_send(phy_ctx_t *ctx, const uint8_t *psdu, size_t len)
{
    if (!ctx || !psdu)
        return PHY_ERR_INVALID_PARAM;

    if (len == 0 || len > PHY_MAX_PACKET_SIZE) {
        fprintf(stderr, "[PHY] TX: invalid PSDU length %zu\n", len);
        return PHY_ERR_INVALID_PARAM;
    }

    /* Проверяем канал перед передачей (CSMA-CA упрощённо) */
    if (phy_cca(ctx) == PHY_CHANNEL_BUSY) {
        fprintf(stderr, "[PHY] TX: channel busy, backoff required\n");
        return PHY_ERR_BUSY;
    }

    /* Собираем PHY-кадр: SHR (4 байта преамбулы + SFD) + PHR + PSDU */
    uint8_t frame[PHY_OVERHEAD + PHY_MAX_PACKET_SIZE];
    size_t  pos = 0;

    /* Преамбула — 4 нулевых байта */
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;

    /* SFD */
    frame[pos++] = PHY_SFD;

    /* PHR: длина PSDU */
    frame[pos++] = (uint8_t)(len & 0x7F);

    /* PSDU */
    memcpy(frame + pos, psdu, len);
    pos += len;

    ssize_t written = write(ctx->fd, frame, pos);
    if (written < 0) {
        fprintf(stderr, "[PHY] TX write error: %s\n", strerror(errno));
        return PHY_ERR_IO;
    }

    printf("[PHY] TX %zu bytes on channel %u\n", len, ctx->channel);
    return PHY_SUCCESS;
}

ssize_t phy_recv(phy_ctx_t *ctx, uint8_t *buf, size_t buf_len)
{
    if (!ctx || !buf || buf_len == 0)
        return PHY_ERR_INVALID_PARAM;

    /* Читаем весь возможный PHY-кадр за раз */
    uint8_t  raw[PHY_OVERHEAD + PHY_MAX_PACKET_SIZE];
    ssize_t  n = read(ctx->fd, raw, sizeof(raw));

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; /* нет данных, не ошибка */
        fprintf(stderr, "[PHY] RX read error: %s\n", strerror(errno));
        return PHY_ERR_IO;
    }

    if (n < (ssize_t)PHY_OVERHEAD) {
        fprintf(stderr, "[PHY] RX: frame too short (%zd bytes)\n", n);
        return PHY_ERR_IO;
    }

    /* Ищем SFD — байт 0xA7 — начиная с позиции 4 */
    int sfd_pos = -1;
    for (int i = 0; i < n; i++) {
        if (raw[i] == PHY_SFD) {
            sfd_pos = i;
            break;
        }
    }

    if (sfd_pos < 0) {
        fprintf(stderr, "[PHY] RX: SFD not found\n");
        return PHY_ERR_IO;
    }

    int phr_pos  = sfd_pos + 1;
    int psdu_pos = phr_pos + 1;

    if (psdu_pos >= n) {
        fprintf(stderr, "[PHY] RX: no PHR after SFD\n");
        return PHY_ERR_IO;
    }

    size_t psdu_len = raw[phr_pos] & 0x7F;

    if (psdu_len == 0 || psdu_len > PHY_MAX_PACKET_SIZE) {
        fprintf(stderr, "[PHY] RX: invalid PSDU length %zu\n", psdu_len);
        return PHY_ERR_IO;
    }

    if ((size_t)(n - psdu_pos) < psdu_len) {
        fprintf(stderr, "[PHY] RX: truncated PSDU (got %zd, expected %zu)\n",
                n - psdu_pos, psdu_len);
        return PHY_ERR_IO;
    }

    if (psdu_len > buf_len) {
        fprintf(stderr, "[PHY] RX: buffer too small\n");
        return PHY_ERR_INVALID_PARAM;
    }

    memcpy(buf, raw + psdu_pos, psdu_len);
    printf("[PHY] RX %zu bytes on channel %u\n", psdu_len, ctx->channel);
    return (ssize_t)psdu_len;
}
