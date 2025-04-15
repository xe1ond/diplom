#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

// Функция для инициализации UART
void init_uart(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Unable to open serial port");
        return;
    }

    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = ICANON;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);
}

// Функция для отправки данных через UART
void send_uart(const char *port, const uint8_t *data, size_t len) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Unable to open serial port");
        return;
    }
    write(fd, data, len);  // Отправляем данные через UART
    close(fd);
}

// Функция для получения данных из UART
int receive_uart(const char *port, uint8_t *buffer, size_t len) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Unable to open serial port");
        return -1;
    }
    int bytesRead = read(fd, buffer, len);
    close(fd);
    return bytesRead;
}
