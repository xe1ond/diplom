#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "phy.h"
#include "mac.h"
#include "nwk.h"
#include "aps.h"

#define PAN_ID       0xBEEF
#define ADDR_NODE_A  0x0001
#define ADDR_NODE_B  0x0002
#define PHY_CHANNEL  15          /* канал 15: 2.425 ГГц */
#define VIRTUAL_PORT "/dev/ttyV0"

static const uint8_t AES_KEY[16] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F
};

static const uint8_t AES_IV[16] = {
    0xAA, 0xBB, 0xCC, 0xDD,
    0xEE, 0xFF, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66,
    0x77, 0x88, 0x99, 0x00
};

int main(void)
{
    /* ---- PHY ---- */
    phy_ctx_t phy_a, phy_b;

    if (phy_init(&phy_a, VIRTUAL_PORT, PHY_CHANNEL) != PHY_SUCCESS) {
        fprintf(stderr, "Failed to initialize PHY for node A\n");
        return 1;
    }
    if (phy_init(&phy_b, VIRTUAL_PORT, PHY_CHANNEL) != PHY_SUCCESS) {
        fprintf(stderr, "Failed to initialize PHY for node B\n");
        phy_close(&phy_a);
        return 1;
    }

    /* ---- MAC ---- */
    mac_ctx_t mac_a, mac_b;
    mac_init(&mac_a, &phy_a, PAN_ID, ADDR_NODE_A);
    mac_init(&mac_b, &phy_b, PAN_ID, ADDR_NODE_B);

    /* ---- NWK ---- */
    nwk_ctx_t nwk_a, nwk_b;
    nwk_init(&nwk_a, &mac_a, ADDR_NODE_A);
    nwk_init(&nwk_b, &mac_b, ADDR_NODE_B);

    /*
     * В симуляции оба узла напрямую видят друг друга —
     * добавляем статические маршруты (next_hop = сам адресат).
     */
    nwk_add_route(&nwk_a, ADDR_NODE_B, ADDR_NODE_B, 1);
    nwk_add_route(&nwk_b, ADDR_NODE_A, ADDR_NODE_A, 1);

    /* ---- APS ---- */
    aps_ctx_t aps_a, aps_b;
    aps_init(&aps_a, &nwk_a, AES_KEY, AES_IV);
    aps_init(&aps_b, &nwk_b, AES_KEY, AES_IV);

    /* ---- Отправка от A к B ---- */
    const uint8_t message[] = "Hello ZigBee!";
    size_t        msg_len   = sizeof(message) - 1;

    printf("\n=== Node A → Node B ===\n");
    printf("[APP] Sending: \"%.*s\" (%zu bytes)\n",
           (int)msg_len, message, msg_len);

    int ret = aps_send(&aps_a, ADDR_NODE_B, message, msg_len);
    if (ret != NWK_SUCCESS) {
        fprintf(stderr, "[APP] Send failed (%d)\n", ret);
        goto cleanup;
    }

    /* ---- Приём на B ---- */
    uint8_t  recv_buf[APS_MAX_PAYLOAD_SIZE];
    size_t   recv_len = sizeof(recv_buf);
    uint16_t recv_src = 0;

    ret = aps_recv(&aps_b, recv_buf, &recv_len, &recv_src);
    if (ret < 0) {
        fprintf(stderr, "[APP] Receive failed (%d)\n", ret);
        goto cleanup;
    }

    printf("[APP] Received from 0x%04X (%zu bytes): \"%.*s\"\n",
           recv_src, recv_len, (int)recv_len, recv_buf);

    /* Проверка корректности */
    if (recv_len == msg_len && memcmp(recv_buf, message, msg_len) == 0)
        printf("[APP] Integrity check PASSED\n");
    else
        fprintf(stderr, "[APP] Integrity check FAILED\n");

cleanup:
    phy_close(&phy_a);
    phy_close(&phy_b);
    return (ret >= 0) ? 0 : 1;
}
