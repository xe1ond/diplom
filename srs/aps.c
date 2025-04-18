#include "aps.h"
#include "aes.h"
#include "streebog.h"
#include "phy.h"

#include <string.h>

void send_aps_message_with_encryption(
    const char *port,
    const uint8_t *data,
    size_t len,
    const uint8_t *key,
    const uint8_t *iv
) {
    aps_message_t msg;
    memset(&msg, 0, sizeof(msg));

    // Шифруем данные (простая CBC, блоками)
    aes_cbc_encrypt(data, len, key, iv, msg.encrypted_data);
    msg.data_len = len;

    // Хешируем зашифрованные данные
    stribog_hash(msg.encrypted_data, msg.data_len, msg.streebog_hash);

    // Отправляем по PHY
    phy_send(port, (uint8_t *)&msg, sizeof(aps_message_t));
}

int receive_aps_message_with_decryption(
    const char *port,
    uint8_t *out_buf,
    size_t *out_len,
    const uint8_t *key,
    const uint8_t *iv
) {
    aps_message_t msg;
    memset(&msg, 0, sizeof(msg));

    if (phy_recv(port, (uint8_t *)&msg, sizeof(msg)) <= 0)
        return 0;

    // Проверяем хеш
    uint8_t computed_hash[STREEBOG_HASH_SIZE];
    stribog_hash(msg.encrypted_data, msg.data_len, computed_hash);

    if (memcmp(msg.streebog_hash, computed_hash, STREEBOG_HASH_SIZE) != 0)
        return 0;

    // Расшифровываем
    aes_cbc_decrypt(msg.encrypted_data, msg.data_len, key, iv, out_buf);
    *out_len = msg.data_len;

    return 1;
}
