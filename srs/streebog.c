#include "streebog.h"
#include <crypt/gost_r_34_11-2012.h>

void streebog_hash(const uint8_t *data, size_t len, uint8_t *out_hash)
{
    gost_2012_ctx ctx;

    gost_2012_init(&ctx);
    gost_2012_update(&ctx, (const unsigned char *)data, len);
    gost_2012_final(&ctx);

    memcpy(out_hash, &ctx.hash, STREEBOG_HASH_SIZE);

    gost_2012_cleanup(&ctx);
}