#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "aes.h"      // твоя реализация AES-128
#include "phy.h"      // эмуляция PHY (UART)
#include "mac.h"      // MAC уровень
#include "aps.h"      // APS + криптография

#define TEST_DATA_LEN 16

int main() {
    const char *port = "virtual_port";  // виртуальный порт для PHY-эмуляции
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    };
    uint8_t iv[16] = {
        0xAA, 0xBB, 0xCC, 0xDD,
        0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66,
        0x77, 0x88, 0x99, 0x00
    };

    uint8_t test_data[TEST_DATA_LEN] = {
        0xDE, 0xAD, 0xBE, 0xEF,
        0xCA, 0xFE, 0xBA, 0xBE,
        0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77
    };

    // Инициализация виртуального PHY (UART)
    if (init_uart(port) != 0) {
        fprintf(stderr, "Failed to initialize UART/PHY.\n");
        return 1;
    }

    printf("[*] Sending secured APS message...\n");

    send_aps_message(port, test_data, TEST_DATA_LEN, key, iv);

    printf("[*] Waiting to receive secured APS message...\n");

    uint8_t recv_buffer[256] = {0};
    size_t recv_len = 0;

    if (receive_aps_message(port, recv_buffer, &recv_len, key, iv)) {
        printf("[+] Received and verified message (%zu bytes):\n", recv_len);
        for (size_t i = 0; i < recv_len; i++) {
            printf("%02X ", recv_buffer[i]);
        }
        printf("\n");
    } else {
        fprintf(stderr, "[-] Failed to receive or verify data.\n");
    }

    return 0;
}
