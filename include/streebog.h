#ifndef STREEBOG_H
#define STREEBOG_H

#include <stdint.h>
#include <stddef.h>

#define STREEBOG_HASH_SIZE 32  /* Стрибог-256: выход 256 бит = 32 байта */
void streebog_hash(const uint8_t *data, size_t len, uint8_t *out_hash);

#endif /* STREEBOG_H */
