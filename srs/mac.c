#include <stdint.h>
#include <string.h>

#define MAX_PACKET_SIZE 128

// Структура для хранения заголовка MAC
typedef struct {
    uint8_t frame_type;
    uint8_t seq_num;
    uint8_t dst_addr[8];
    uint8_t src_addr[8];
    uint8_t data[MAX_PACKET_SIZE];
} mac_packet_t;

// Функция для формирования пакета
void create_mac_packet(mac_packet_t *packet, uint8_t *data, size_t data_len) {
    packet->frame_type = 0x01;  // Тип кадра (Data Frame)
    packet->seq_num = rand() % 256;  // Генерация случайного номера последовательности
    memcpy(packet->data, data, data_len);  // Копирование данных
}

// Функция для отправки пакета через UART
void send_mac_packet(const char *port, mac_packet_t *packet) {
    send_uart(port, (uint8_t *)packet, sizeof(mac_packet_t));
}

// Функция для получения пакета через UART
int receive_mac_packet(const char *port, mac_packet_t *packet) {
    return receive_uart(port, (uint8_t *)packet, sizeof(mac_packet_t));
}
