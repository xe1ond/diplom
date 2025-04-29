#include <stdint.h>
#include <string.h>
#include <stdlib.h>  // Для rand_r(), в случае если нужно не криптографическое случайное число
#include <sysctl.h>  // Для вызова sysctl для криптографически безопасных случайных чисел

#define MAX_PACKET_SIZE 128
#define SEQUENCE_NUMBER_MAX 255

// Структура для хранения заголовка MAC
typedef struct {
    uint8_t frame_type;    // Тип кадра (например, Data Frame)
    uint8_t seq_num;       // Номер последовательности
    uint8_t dst_addr[8];   // Адрес получателя
    uint8_t src_addr[8];   // Адрес отправителя
    uint8_t data[MAX_PACKET_SIZE];  // Данные
} mac_packet_t;

// Функция для генерации криптографически безопасного случайного числа
int generate_secure_random(uint8_t *output, size_t length) {
    if (output == NULL || length == 0) {
        return -1;  // Ошибка: неверные параметры
    }

    // Используем системный вызов QNX для получения случайных данных
    int result = sysctl(CTL_RANDOM, 0, NULL, output, length);
    return result == -1 ? -1 : 0;
}

// Функция для формирования пакета
void create_mac_packet(mac_packet_t *packet, uint8_t *data, size_t data_len) {
    if (packet == NULL || data == NULL || data_len > MAX_PACKET_SIZE) {
        return;  // Невалидные параметры
    }

    packet->frame_type = 0x01;  // Тип кадра (Data Frame)
    
    // Генерация случайного номера последовательности с использованием криптографически безопасного генератора
    if (generate_secure_random(&packet->seq_num, 1) != 0) {
        packet->seq_num = rand() % (SEQUENCE_NUMBER_MAX + 1);  // Если ошибка в генерации, используем обычный rand
    }

    // Копирование данных в пакет
    memcpy(packet->data, data, data_len);
}

// Функция для отправки пакета через UART
int send_mac_packet(const char *port, mac_packet_t *packet) {
    if (port == NULL || packet == NULL) {
        return -1;  // Ошибка: неверные параметры
    }

    // Отправка данных
    return send_uart(port, (uint8_t *)packet, sizeof(mac_packet_t));
}

// Функция для получения пакета через UART
int receive_mac_packet(const char *port, mac_packet_t *packet) {
    if (port == NULL || packet == NULL) {
        return -1;  // Ошибка: неверные параметры
    }

    // Получение данных
    return receive_uart(port, (uint8_t *)packet, sizeof(mac_packet_t));
}
