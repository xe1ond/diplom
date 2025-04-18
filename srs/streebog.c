#include "streebog.h"
#include <crypt.h>

void stribog_hash(const uint8_t *data, size_t len, uint8_t *out_hash) {
    gost_hash_ctx ctx;
    gost_hash_init(&ctx, GOST_HASH_256);
    gost_hash_update(&ctx, data, len);
    gost_hash_final(&ctx, out_hash);
}
