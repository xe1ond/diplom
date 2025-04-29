#include "phy.h"
#include <stdio.h>

int main() {
    const char *port = "/dev/ttyS0";  // Путь к порту UART

    // Инициализация UART
    int fd = init_uart(port);
    if (fd == -1) {
        fprintf(stderr, "Failed to initialize UART.\n");
        return 1;
    }

    // Пример отправки данных через UART
    uint8_t data_to_send[] = "Hello, UART!";
    if (send_uart(fd, data_to_send, sizeof(data_to_send)) == -1) {
        fprintf(stderr, "Failed to send data.\n");
    }

    // Пример получения данных через UART
    uint8_t received_data[128];
    ssize_t bytes_read = receive_uart(fd, received_data, sizeof(received_data));
    if (bytes_read == -1) {
        fprintf(stderr, "Failed to receive data.\n");
    } else {
        printf("Received data: %.*s\n", (int)bytes_read, received_data);
    }

    // Закрытие UART
    close_uart(fd);
    return 0;
}
