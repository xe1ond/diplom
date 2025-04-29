#include "aps.h"
#include "aes.h"
#include "phy.h"
#include "streebog.h"

#include <string.h>
#include <stdint.h>

#ifdef __arm__
#define PLATFORM_ALIGNMENT 4
#else
#define PLATFORM_ALIGNMENT 1
#endif

// Собираем сообщение вручную
static size_t serialize_aps_message(const aps_message_t *msg, uint8_t *buffer) {
    size_t offset = 0;

    memcpy(buffer + offset, &msg->ciphertext_len, sizeof(msg->ciphertext_len));
    offset += sizeof(msg->ciphertext_len);

    memcpy(buffer + offset, msg->ciphertext, msg->ciphertext_len);
    offset += msg->ciphertext_len;

    memcpy(buffer + offset, msg->auth_tag, STREEBOG_HASH_SIZE);
    offset += STREEBOG_HASH_SIZE;

    return offset;
}

// Разбираем сообщение вручную
static int deserialize_aps_message(aps_message_t *msg, const uint8_t *buffer, size_t total_len) {
    size_t offset = 0;

    if (total_len < sizeof(msg->ciphertext_len))
        return 0;

    memcpy(&msg->ciphertext_len, buffer + offset, sizeof(msg->ciphertext_len));
    offset += sizeof(msg->ciphertext_len);

    if (msg->ciphertext_len > sizeof(msg->ciphertext))
        return 0; // защита от переполнения

    if (offset + msg->ciphertext_len + STREEBOG_HASH_SIZE > total_len)
        return 0;

    memcpy(msg->ciphertext, buffer + offset, msg->ciphertext_len);
    offset += msg->ciphertext_len;

    memcpy(msg->auth_tag, buffer + offset, STREEBOG_HASH_SIZE);
    offset += STREEBOG_HASH_SIZE;

    return 1;
}

// Отправка
void send_aps_message(const char *port, const uint8_t *plaintext, size_t len,
                      const uint8_t *aes_key, const uint8_t *aes_iv) {
    aps_message_t msg;
    uint8_t buffer[512];
    memset(&msg, 0, sizeof(msg));

    // Шифруем данные AES
    aes_cbc_encrypt(plaintext, len, aes_key, aes_iv, msg.ciphertext);
    msg.ciphertext_len = len;

    // Вычисляем Стрибог хеш от зашифрованных данных
    streebog_hash(msg.ciphertext, len, msg.auth_tag);

    // Сериализация
    size_t packet_len = serialize_aps_message(&msg, buffer);

    // Отправляем через PHY
    phy_send(port, buffer, packet_len);
}

// Приём
int receive_aps_message(const char *port, uint8_t *plaintext, size_t *len,
                        const uint8_t *aes_key, const uint8_t *aes_iv) {
    uint8_t buffer[512];
    aps_message_t msg;
    memset(&msg, 0, sizeof(msg));

    ssize_t recv_len = phy_recv(port, buffer, sizeof(buffer));
    if (recv_len <= 0)
        return 0;

    if (!deserialize_aps_message(&msg, buffer, recv_len))
        return 0;

    // Проверка аутентичности: пересчитать хеш
    uint8_t computed_tag[STREEBOG_HASH_SIZE];
    streebog_hash(msg.ciphertext, msg.ciphertext_len, computed_tag);

    if (memcmp(computed_tag, msg.auth_tag, STREEBOG_HASH_SIZE) != 0)
        return 0;  // Хеши не совпадают, ошибка целостности

    // Дешифруем
    aes_cbc_decrypt(msg.ciphertext, msg.ciphertext_len, aes_key, aes_iv, plaintext);
    *len = msg.ciphertext_len;

    return 1;
}
