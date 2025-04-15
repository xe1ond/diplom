#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "aes.h"         // Библиотека для AES
#include "streebog.h"    // Библиотека для Стрибога
// Функция для шифрования данных с использованием AES-128 в режиме CBC
void aes_encrypt(uint8_t *input, uint8_t *key, uint8_t *iv, uint8_t *output, size_t len) {
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    for (size_t i = 0; i < len; i += 16) {
        AES_CBC_encrypt_buffer(&ctx, output + i, input + i, 16);  // Шифрование 16 байтов за раз
    }
}
// Функция для хеширования данных с использованием Стрибог (ГОСТ)
void stribog_hash(uint8_t *data, size_t data_len, uint8_t *hash) {
    generate_streebog_hash(data, data_len, hash);  // Хеширование с использованием ГОСТ (Стрибог)
}
// Структура для хранения пакета с шифрованными данными и хешем
typedef struct {
    uint8_t encrypted_data[MAX_PACKET_SIZE];
    uint8_t hash[32];  // Хеш длиной 256 бит
    size_t data_len;
} aps_packet_t;

// Функция для отправки зашифрованных данных с хешем
void send_aps_message_with_encryption(const char *port, uint8_t *data, size_t data_len, uint8_t *key, uint8_t *iv) {
    uint8_t encrypted_data[MAX_PACKET_SIZE];
    uint8_t hash[32];  // Хеш Стрибога (256 бит)

    // Шифрование данных с использованием AES-128
    aes_encrypt(data, key, iv, encrypted_data, data_len);

    // Генерация хеша с использованием Стрибога
    stribog_hash(encrypted_data, data_len, hash);

    // Формируем пакет с зашифрованными данными и хешем
    aps_packet_t packet;
    memcpy(packet.encrypted_data, encrypted_data, data_len);
    memcpy(packet.hash, hash, 32);  // Хеш данных

    // Отправляем зашифрованный пакет с хешем через MAC слой
    send_mac_packet(port, (mac_packet_t *)&packet);
}

// Функция для получения зашифрованных данных и проверки хеша
int receive_aps_message_with_decryption(const char *port, uint8_t *data, size_t *data_len, uint8_t *key, uint8_t *iv) {
    mac_packet_t packet;
    int bytesRead = receive_mac_packet(port, &packet);
    if (bytesRead > 0) {
        aps_packet_t *aps_packet = (aps_packet_t *)packet.data;
        
        // Проверка хеша
        uint8_t computed_hash[32];
        stribog_hash(aps_packet->encrypted_data, *data_len, computed_hash);

        if (memcmp(aps_packet->hash, computed_hash, 32) != 0) {
            printf("Data integrity check failed.\n");
            return -1;  // Ошибка проверки целостности
        }

        // Дешифровка данных с использованием AES-128
        aes_encrypt(aps_packet->encrypted_data, key, iv, data, *data_len);  // Дешифрование с использованием AES

        return bytesRead;
    }
    return -1;
}
