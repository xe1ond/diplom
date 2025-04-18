#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "include/aes.h"           // твоя реализация AES-128
//#include "crypt.h"      // обёртка над libgost
#include "srs/phy.h"           // эмуляция PHY (UART)
#include "srs/mac.h"           // MAC уровень
#include "srs/aps.h"           // APS + криптография

#define TEST_DATA_LEN 16

int main() {
    const char *port = "/dev/tty0";  // виртуальный порт (QEMU, QNX)
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

    // Инициализация PHY
    if (init_uart(port) != 0) {
        fprintf(stderr, "Failed to initialize UART.\n");
        return 1;
    }

    printf("[*] Sending secured APS message...\n");

    send_aps_message_with_encryption(
        port,
        test_data,
        TEST_DATA_LEN,
        key,
        iv
    );

    printf("[*] Waiting to receive secured APS message...\n");

    uint8_t recv_buffer[256];
    size_t recv_len = sizeof(recv_buffer);
    if (receive_aps_message_with_decryption(
            port, recv_buffer, &recv_len, key, iv) > 0) {
        
        printf("[+] Received and verified message:\n");
        for (size_t i = 0; i < recv_len; i++) {
            printf("%02X ", recv_buffer[i]);
        }
        printf("\n");
    } else {
        fprintf(stderr, "[-] Failed to receive or verify data.\n");
    }

    return 0;
}
