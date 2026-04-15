#include "aps.h"

#include <openssl/aes.h>
#include <string.h>
#include <stdio.h>

/*
 * aps.c — уровень поддержки приложений (Application Support Sublayer).
 *
 * Криптографическая схема:
 *   Шифрование:  AES-128-CBC (OpenSSL AES_cbc_encrypt)
 *   Целостность: Стрибог-256 от шифротекста (streebog_hash)
 *
 * Порядок операций при отправке:
 *   1. Выравниваем plaintext до кратного AES_BLOCK_SIZE (PKCS#7-подобно).
 *   2. Шифруем AES-CBC.
 *   3. Считаем Стрибог-256 от шифротекста → auth_tag.
 *   4. Сериализуем APS-кадр и передаём в NWK.
 *
 * Порядок операций при приёме:
 *   1. Принимаем из NWK, десериализуем APS-кадр.
 *   2. Пересчитываем Стрибог-256 от шифротекста, сравниваем с auth_tag.
 *   3. Расшифровываем AES-CBC.
 */

/* ------------------------------------------------------------------ */
/* PKCS#7-подобное выравнивание до блока AES (16 байт)                */
/* ------------------------------------------------------------------ */
static size_t aps_pad(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t out_size)
{
    size_t padded = ((in_len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE)
                    * AES_BLOCK_SIZE;
    if (padded == 0)
        padded = AES_BLOCK_SIZE;

    if (padded > out_size)
        return 0;

    memcpy(out, in, in_len);
    uint8_t pad_byte = (uint8_t)(padded - in_len);
    memset(out + in_len, pad_byte, padded - in_len);
    return padded;
}

/* ------------------------------------------------------------------ */
/* Сериализация APS-кадра                                              */
/* ------------------------------------------------------------------ */
static size_t aps_serialize(const aps_frame_t *frame,
                             uint8_t *buf, size_t buf_len)
{
    size_t needed = sizeof(frame->ciphertext_len)
                  + frame->ciphertext_len
                  + STREEBOG_HASH_SIZE;
    if (needed > buf_len)
        return 0;

    size_t pos = 0;

    buf[pos++] = (uint8_t)(frame->ciphertext_len & 0xFF);
    buf[pos++] = (uint8_t)(frame->ciphertext_len >> 8);

    memcpy(buf + pos, frame->ciphertext, frame->ciphertext_len);
    pos += frame->ciphertext_len;

    memcpy(buf + pos, frame->auth_tag, STREEBOG_HASH_SIZE);
    pos += STREEBOG_HASH_SIZE;

    return pos;
}

/* ------------------------------------------------------------------ */
/* Десериализация APS-кадра                                            */
/* ------------------------------------------------------------------ */
static int aps_deserialize(const uint8_t *buf, size_t total,
                            aps_frame_t *frame)
{
    if (total < sizeof(frame->ciphertext_len) + STREEBOG_HASH_SIZE)
        return -1;

    size_t pos = 0;

    frame->ciphertext_len  = (uint16_t)buf[pos];
    frame->ciphertext_len |= (uint16_t)buf[pos + 1] << 8;
    pos += 2;

    if (frame->ciphertext_len > sizeof(frame->ciphertext))
        return -1;

    if (pos + frame->ciphertext_len + STREEBOG_HASH_SIZE > total)
        return -1;

    memcpy(frame->ciphertext, buf + pos, frame->ciphertext_len);
    pos += frame->ciphertext_len;

    memcpy(frame->auth_tag, buf + pos, STREEBOG_HASH_SIZE);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Публичные функции                                                   */
/* ------------------------------------------------------------------ */

void aps_init(aps_ctx_t *ctx, nwk_ctx_t *nwk,
              const uint8_t *aes_key, const uint8_t *aes_iv)
{
    if (!ctx || !nwk || !aes_key || !aes_iv)
        return;
    ctx->nwk = nwk;
    memcpy(ctx->aes_key, aes_key, APS_KEY_SIZE);
    memcpy(ctx->aes_iv,  aes_iv,  APS_IV_SIZE);
    printf("[APS] Initialized (AES-128-CBC + Стрибог-256)\n");
}

int aps_send(aps_ctx_t *ctx, uint16_t dst_addr,
             const uint8_t *plaintext, size_t len)
{
    if (!ctx || !plaintext || len == 0 || len > APS_MAX_PAYLOAD_SIZE)
        return -1;

    aps_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    /* 1. Выравниваем plaintext */
    uint8_t padded[APS_MAX_PAYLOAD_SIZE + APS_IV_SIZE];
    size_t  padded_len = aps_pad(plaintext, len, padded, sizeof(padded));
    if (padded_len == 0) {
        fprintf(stderr, "[APS] Padding error\n");
        return -1;
    }

    /* 2. Шифруем AES-128-CBC.
     *    AES_cbc_encrypt изменяет IV, поэтому используем его копию.
     */
    AES_KEY aes_enc_key;
    if (AES_set_encrypt_key(ctx->aes_key, 128, &aes_enc_key) != 0) {
        fprintf(stderr, "[APS] AES key setup error\n");
        return -1;
    }

    uint8_t iv_copy[APS_IV_SIZE];
    memcpy(iv_copy, ctx->aes_iv, APS_IV_SIZE);

    AES_cbc_encrypt(padded, frame.ciphertext, padded_len,
                    &aes_enc_key, iv_copy, AES_ENCRYPT);
    frame.ciphertext_len = (uint16_t)padded_len;

    /* 3. Стрибог-256 от шифротекста */
    streebog_hash(frame.ciphertext, frame.ciphertext_len, frame.auth_tag);

    /* 4. Сериализуем и отправляем через NWK */
    uint8_t buf[256];
    size_t  buf_len = aps_serialize(&frame, buf, sizeof(buf));
    if (buf_len == 0) {
        fprintf(stderr, "[APS] Serialization error\n");
        return -1;
    }

    printf("[APS] TX to 0x%04X: %zu bytes plaintext → %u bytes ciphertext\n",
           dst_addr, len, frame.ciphertext_len);

    return nwk_send(ctx->nwk, dst_addr, buf, buf_len);
}

int aps_recv(aps_ctx_t *ctx, uint8_t *plaintext, size_t *len,
             uint16_t *src_addr)
{
    if (!ctx || !plaintext || !len || !src_addr)
        return -1;

    uint8_t nwk_buf[256];
    size_t  nwk_len = sizeof(nwk_buf);

    int ret = nwk_recv(ctx->nwk, nwk_buf, &nwk_len, src_addr);
    if (ret != NWK_SUCCESS)
        return -1;

    aps_frame_t frame;
    if (aps_deserialize(nwk_buf, nwk_len, &frame) != 0) {
        fprintf(stderr, "[APS] Deserialization error\n");
        return -1;
    }

    /* 2. Проверяем целостность: пересчитываем Стрибог-256 */
    uint8_t computed_tag[STREEBOG_HASH_SIZE];
    streebog_hash(frame.ciphertext, frame.ciphertext_len, computed_tag);

    if (memcmp(computed_tag, frame.auth_tag, STREEBOG_HASH_SIZE) != 0) {
        fprintf(stderr, "[APS] Auth tag mismatch — message rejected\n");
        return -1;
    }

    /* 3. Расшифровываем AES-128-CBC */
    AES_KEY aes_dec_key;
    if (AES_set_decrypt_key(ctx->aes_key, 128, &aes_dec_key) != 0) {
        fprintf(stderr, "[APS] AES key setup error\n");
        return -1;
    }

    uint8_t iv_copy[APS_IV_SIZE];
    memcpy(iv_copy, ctx->aes_iv, APS_IV_SIZE);

    uint8_t decrypted[APS_MAX_PAYLOAD_SIZE + APS_IV_SIZE];
    AES_cbc_encrypt(frame.ciphertext, decrypted, frame.ciphertext_len,
                    &aes_dec_key, iv_copy, AES_DECRYPT);

    /* Снимаем PKCS#7-подобный паддинг */
    if (frame.ciphertext_len == 0) {
        fprintf(stderr, "[APS] Empty ciphertext\n");
        return -1;
    }
    uint8_t pad_byte   = decrypted[frame.ciphertext_len - 1];
    size_t  actual_len = frame.ciphertext_len - pad_byte;

    if (pad_byte == 0 || pad_byte > AES_BLOCK_SIZE ||
        actual_len > *len) {
        fprintf(stderr, "[APS] Invalid padding\n");
        return -1;
    }

    memcpy(plaintext, decrypted, actual_len);
    *len = actual_len;

    printf("[APS] RX from 0x%04X: %u bytes ciphertext → %zu bytes plaintext\n",
           *src_addr, frame.ciphertext_len, actual_len);

    return (int)actual_len;
}
