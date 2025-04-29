#ifndef APS_H
#define APS_H

#include <stdint.h>
#include <stddef.h>

#define STREEBOG_HASH_SIZE 32    // Размер хеша Стрибог-256 в байтах
#define APS_MAX_PAYLOAD_SIZE 256 // Максимальный размер полезной нагрузки

typedef struct {
    uint8_t ciphertext[APS_MAX_PAYLOAD_SIZE];  // Зашифрованные данные
    size_t  ciphertext_len;                    // Длина шифротекста
    uint8_t auth_tag[STREEBOG_HASH_SIZE];       // Хеш (MAC)
} aps_message_t;

// Отправка зашифрованного сообщения
void send_aps_message(const char *port, const uint8_t *plaintext, size_t len,
                      const uint8_t *aes_key, const uint8_t *aes_iv);

// Приём и расшифровка сообщения
int receive_aps_message(const char *port, uint8_t *plaintext, size_t *len,
                        const uint8_t *aes_key, const uint8_t *aes_iv);

#endif // APS_H
