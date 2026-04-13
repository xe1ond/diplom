#ifndef APS_H
#define APS_H

#include <stdint.h>
#include <stddef.h>
#include "nwk.h"
#include "streebog.h"

/*
 * APS (Application Support Sublayer) — верхний уровень ZigBee-стека.
 *
 * В данной реализации APS выполняет:
 *   - шифрование полезной нагрузки алгоритмом AES-128-CBC;
 *   - вычисление MAC (Message Authentication Code) на основе
 *     хеша Стрибог-256 от зашифрованных данных;
 *   - сериализацию/десериализацию защищённого APS-кадра;
 *   - передачу и приём через NWK-уровень.
 *
 * Примечание: в стандарте ZigBee используется AES-128-CCM*.
 * Схема AES-CBC + Стрибог применяется в данном проекте
 * как учебная реализация с российской криптографией.
 */

#define APS_MAX_PAYLOAD_SIZE   100  /* макс. открытый текст, байт          */
#define APS_KEY_SIZE           16   /* AES-128: ключ 16 байт               */
#define APS_IV_SIZE            16   /* AES-CBC: вектор инициализации 16 байт */

/*
 * Защищённый APS-кадр.
 *
 *  +------------------+-------------+---------------------+
 *  | ciphertext_len   | ciphertext  |      auth_tag       |
 *  |    (2 байта)     | (до 116 б)  | (STREEBOG_HASH_SIZE)|
 *  +------------------+-------------+---------------------+
 */
typedef struct {
    uint16_t ciphertext_len;
    uint8_t  ciphertext[APS_MAX_PAYLOAD_SIZE + APS_IV_SIZE]; /* с выравниванием */
    uint8_t  auth_tag[STREEBOG_HASH_SIZE];
} aps_frame_t;

/*
 * Контекст APS-уровня.
 */
typedef struct {
    nwk_ctx_t *nwk;
    uint8_t    aes_key[APS_KEY_SIZE];
    uint8_t    aes_iv[APS_IV_SIZE];
} aps_ctx_t;

/* Инициализация APS. */
void aps_init(aps_ctx_t *ctx, nwk_ctx_t *nwk,
              const uint8_t *aes_key, const uint8_t *aes_iv);

/*
 * Отправка защищённого APS-сообщения.
 * plaintext шифруется AES-CBC, затем вычисляется Стрибог-256 от шифротекста.
 */
int aps_send(aps_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *plaintext, size_t len);

/*
 * Приём и проверка APS-сообщения.
 * Проверяет auth_tag, расшифровывает, записывает открытый текст в plaintext.
 * Возвращает число байт или отрицательный код ошибки.
 */
int aps_recv(aps_ctx_t *ctx, uint8_t *plaintext, size_t *len,
             uint16_t *src_addr);

#endif /* APS_H */
