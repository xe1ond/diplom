int main() {
    const char *port = "/dev/ttyV0";  // Эмуляция порта
    uint8_t key[16] = {0x00};         // Пример ключа для AES-128
    uint8_t iv[16] = {0x00};          // Инициализационный вектор для AES

    // Инициализация UART
    init_uart(port);

    // Пример отправки данных с шифрованием и хешированием
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    send_aps_message_with_encryption(port, data, sizeof(data), key, iv);

    // Пример приёма и проверки целостности данных
    uint8_t recv_data[MAX_PACKET_SIZE];
    size_t recv_len = sizeof(recv_data);
    if (receive_aps_message_with_decryption(port, recv_data, &recv_len, key, iv) > 0) {
        printf("Received and decrypted data: ");
        for (size_t i = 0; i < recv_len; i++) {
            printf("%02x ", recv_data[i]);
        }
        printf("\n");
    }

    return 0;
}
