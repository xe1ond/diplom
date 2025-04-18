#ifndef STREEBOG_H
#define STREEBOG_H

#include <stdint.h>
#include <stddef.h>

void stribog_hash(const uint8_t *data, size_t len, uint8_t *out_hash);

#endif
