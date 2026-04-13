#ifndef PHY_H
#define PHY_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* IEEE 802.15.4 PHY constants */
#define PHY_MAX_PACKET_SIZE     127   /* максимальный размер PSDU в байтах */
#define PHY_SHR_SIZE            5     /* Preamble (4) + SFD (1)            */
#define PHY_PHR_SIZE            1     /* Frame Length field                 */
#define PHY_OVERHEAD            (PHY_SHR_SIZE + PHY_PHR_SIZE)

#define PHY_SUCCESS             0
#define PHY_ERR_BUSY           -1
#define PHY_ERR_INVALID_PARAM  -2
#define PHY_ERR_IO             -3

/*
 * Статус канала (CCA — Clear Channel Assessment).
 * В симуляции всегда возвращает PHY_CHANNEL_IDLE.
 */
typedef enum {
    PHY_CHANNEL_IDLE = 0,
    PHY_CHANNEL_BUSY = 1
} phy_cca_status_t;

/*
 * Дескриптор PHY-устройства.
 * В симуляции хранит путь к виртуальному порту и файловый дескриптор.
 */
typedef struct {
    char    device[64];
    int     fd;
    uint8_t channel;   /* номер канала 802.15.4: 11–26 (2.4 ГГц) */
    int     tx_power;  /* мощность передатчика, дБм (симуляция)  */
} phy_ctx_t;

/* Инициализация PHY. Открывает устройство и сохраняет контекст. */
int  phy_init(phy_ctx_t *ctx, const char *device, uint8_t channel);

/* Закрытие PHY. */
void phy_close(phy_ctx_t *ctx);

/* Оценка занятости канала (CCA). */
phy_cca_status_t phy_cca(const phy_ctx_t *ctx);

/*
 * Передача PSDU (PHY Service Data Unit).
 * len <= PHY_MAX_PACKET_SIZE.
 * Возвращает PHY_SUCCESS или код ошибки.
 */
int phy_send(phy_ctx_t *ctx, const uint8_t *psdu, size_t len);

/*
 * Приём PSDU.
 * buf должен быть не менее PHY_MAX_PACKET_SIZE байт.
 * Возвращает число принятых байт или код ошибки.
 */
ssize_t phy_recv(phy_ctx_t *ctx, uint8_t *buf, size_t buf_len);

#endif /* PHY_H */
