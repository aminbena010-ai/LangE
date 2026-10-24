/* =========================================================
 * SHA-256 (FIPS 180-4)
 * ========================================================= */

#include "hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint32_t HASH_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void hash_block(
    uint32_t state[8],
    const unsigned char block[64]
)
{
    uint32_t w[64];

    for (int i = 0; i < 16; i++) {
        w[i] =
            ((uint32_t)block[i * 4] << 24) |
            ((uint32_t)block[i * 4 + 1] << 16) |
            ((uint32_t)block[i * 4 + 2] << 8) |
            ((uint32_t)block[i * 4 + 3]);
    }

    for (int i = 16; i < 64; i++) {
        uint32_t s0 =
            ROTR32(w[i - 15], 7) ^
            ROTR32(w[i - 15], 18) ^
            (w[i - 15] >> 3);

        uint32_t s1 =
            ROTR32(w[i - 2], 17) ^
            ROTR32(w[i - 2], 19) ^
            (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 =
            ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);

        uint32_t ch = (e & f) ^ ((~e) & g);

        uint32_t temp1 =
            h + s1 + ch + HASH_K[i] + w[i];

        uint32_t s0 =
            ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);

        uint32_t maj =
            (a & b) ^ (a & c) ^ (b & c);

        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

typedef struct HashContext {
    uint32_t state[8];
    unsigned char buffer[64];
    size_t buffer_length;
    uint64_t total_length;
} HashContext;

static void hash_init(HashContext *ctx)
{
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;

    ctx->buffer_length = 0;
    ctx->total_length = 0;
}

static void hash_update(
    HashContext *ctx,
    const unsigned char *data,
    size_t length
)
{
    ctx->total_length += (uint64_t)length;

    while (length > 0) {
        size_t space = 64 - ctx->buffer_length;
        size_t take = length < space ? length : space;

        memcpy(ctx->buffer + ctx->buffer_length, data, take);

        ctx->buffer_length += take;
        data += take;
        length -= take;

        if (ctx->buffer_length == 64) {
            hash_block(ctx->state, ctx->buffer);
            ctx->buffer_length = 0;
        }
    }
}

static void hash_final(
    HashContext *ctx,
    unsigned char out[32]
)
{
    uint64_t bits = ctx->total_length * 8;

    unsigned char pad = 0x80;
    hash_update(ctx, &pad, 1);

    unsigned char zero = 0;

    while (ctx->buffer_length != 56)
        hash_update(ctx, &zero, 1);

    unsigned char length_bytes[8];

    for (int i = 0; i < 8; i++)
        length_bytes[i] =
            (unsigned char)(bits >> (56 - i * 8));

    hash_update(ctx, length_bytes, 8);

    for (int i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

void hash_sha256(
    const void *data,
    size_t length,
    unsigned char out[32]
)
{
    HashContext ctx;

    hash_init(&ctx);
    hash_update(&ctx, data, length);
    hash_final(&ctx, out);
}

int hash_file_sha256(
    const char *path,
    unsigned char out[32]
)
{
    if (!path)
        return 0;

    FILE *file = fopen(path, "rb");

    if (!file)
        return 0;

    HashContext ctx;
    hash_init(&ctx);

    unsigned char chunk[65536];
    size_t read_count;

    while ((read_count = fread(chunk, 1, sizeof(chunk), file)) > 0)
        hash_update(&ctx, chunk, read_count);

    int ok = !ferror(file);

    fclose(file);

    if (!ok)
        return 0;

    hash_final(&ctx, out);

    return 1;
}

void hash_hex(
    const unsigned char digest[32],
    char out[65]
)
{
    static const char digits[] = "0123456789abcdef";

    for (int i = 0; i < 32; i++) {
        out[i * 2] = digits[digest[i] >> 4];
        out[i * 2 + 1] = digits[digest[i] & 0x0f];
    }

    out[64] = '\0';
}
