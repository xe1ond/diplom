#ifndef PHY_H
#define PHY_H

#include <stdint.h>
#include <stddef.h>

int init_uart(const char *device);
int phy_send(const char *device, const uint8_t *data, size_t len);
int phy_recv(const char *device, uint8_t *buffer, size_t len);

#endif
