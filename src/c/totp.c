#include "totp.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
  uint32_t state[5];
  uint64_t count;
  uint8_t buffer[64];
} Sha1Context;

typedef struct {
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];
} Sha256Context;

typedef struct {
  uint64_t state[8];
  uint64_t count[2];
  uint8_t buffer[128];
} Sha512Context;

static bool prv_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int prv_base32_value(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= '2' && c <= '7') {
    return 26 + (c - '2');
  }
  return -1;
}

static char prv_base32_char(int val) {
  if (val >= 0 && val <= 25) {
    return (char)('A' + val);
  }
  if (val >= 26 && val <= 31) {
    return (char)('2' + (val - 26));
  }
  return '=';
}

int base32_encode(const uint8_t *input, size_t input_len, char *output, size_t output_max) {
  if (!input || !output || output_max == 0) {
    return -1;
  }

  size_t output_len = 0;
  size_t i = 0;

  while (i < input_len) {
    // Get 5 bytes (40 bits) or less
    uint64_t buffer = 0;
    int bits = 0;

    for (int j = 0; j < 5 && i < input_len; j++) {
      buffer = (buffer << 8) | input[i++];
      bits += 8;
    }

    // Encode into 8 base32 characters
    for (int j = 35; j >= 0 && output_len < output_max; j -= 5) {
      if (bits >= 5) {
        int val = (buffer >> j) & 0x1F;
        output[output_len++] = prv_base32_char(val);
        bits -= 5;
      } else if (output_len < output_max) {
        output[output_len++] = '=';
      }
    }
  }

  if (output_len < output_max) {
    output[output_len] = '\0';
  }

  return (int)output_len;
}

int base32_decode(const char *input, uint8_t *output, size_t output_max) {
  int buffer = 0;
  int bits_left = 0;
  size_t count = 0;

  for (const char *p = input; *p; p++) {
    char c = *p;
    if (prv_is_space(c) || c == '-') {
      continue;
    }
    if (c >= 'a' && c <= 'z') {
      c = (char)(c - 'a' + 'A');
    }
    if (c == '=') {
      break;
    }
    int val = prv_base32_value(c);
    if (val < 0) {
      return -1;
    }
    buffer = (buffer << 5) | val;
    bits_left += 5;

    if (bits_left >= 8) {
      bits_left -= 8;
      if (count >= output_max) {
        return -1;
      }
      output[count++] = (uint8_t)((buffer >> bits_left) & 0xFF);
    }
  }
  return (int)count;
}

static uint32_t prv_rol32(uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

static void prv_sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)buffer[i * 4] << 24)
        | ((uint32_t)buffer[i * 4 + 1] << 16)
        | ((uint32_t)buffer[i * 4 + 2] << 8)
        | ((uint32_t)buffer[i * 4 + 3]);
  }
  for (int i = 16; i < 80; i++) {
    w[i] = prv_rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];

  for (int i = 0; i < 80; i++) {
    uint32_t f;
    uint32_t k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t temp = prv_rol32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = prv_rol32(b, 30);
    b = a;
    a = temp;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

static void prv_sha1_init(Sha1Context *ctx) {
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
  ctx->count = 0;
}

static void prv_sha1_update(Sha1Context *ctx, const uint8_t *data, size_t len) {
  size_t i = 0;
  size_t j = (size_t)((ctx->count >> 3) & 63);
  ctx->count += ((uint64_t)len) << 3;

  if ((j + len) > 63) {
    size_t part_len = 64 - j;
    memcpy(&ctx->buffer[j], &data[0], part_len);
    prv_sha1_transform(ctx->state, ctx->buffer);
    for (i = part_len; i + 63 < len; i += 64) {
      prv_sha1_transform(ctx->state, &data[i]);
    }
    j = 0;
  } else {
    i = 0;
  }
  memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void prv_sha1_final(Sha1Context *ctx, uint8_t digest[20]) {
  uint8_t finalcount[8];
  for (int i = 0; i < 8; i++) {
    finalcount[i] = (uint8_t)((ctx->count >> ((7 - i) * 8)) & 0xFF);
  }

  uint8_t c = 0x80;
  prv_sha1_update(ctx, &c, 1);
  uint8_t zero = 0x00;
  while ((ctx->count & 504) != 448) {
    prv_sha1_update(ctx, &zero, 1);
  }
  prv_sha1_update(ctx, finalcount, 8);

  for (int i = 0; i < 20; i++) {
    digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
  }
}

// ============================================================================
// HMAC-SHA1
// ============================================================================

void prv_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t out[20]) {
  uint8_t key_block[64];
  memset(key_block, 0, sizeof(key_block));

  if (key_len > sizeof(key_block)) {
    Sha1Context key_ctx;
    prv_sha1_init(&key_ctx);
    prv_sha1_update(&key_ctx, key, key_len);
    uint8_t key_hash[20];
    prv_sha1_final(&key_ctx, key_hash);
    memcpy(key_block, key_hash, sizeof(key_hash));
  } else {
    memcpy(key_block, key, key_len);
  }

  uint8_t o_key_pad[64];
  uint8_t i_key_pad[64];
  for (int i = 0; i < 64; i++) {
    o_key_pad[i] = key_block[i] ^ 0x5C;
    i_key_pad[i] = key_block[i] ^ 0x36;
  }

  Sha1Context inner_ctx;
  prv_sha1_init(&inner_ctx);
  prv_sha1_update(&inner_ctx, i_key_pad, sizeof(i_key_pad));
  prv_sha1_update(&inner_ctx, data, data_len);
  uint8_t inner_digest[20];
  prv_sha1_final(&inner_ctx, inner_digest);

  Sha1Context outer_ctx;
  prv_sha1_init(&outer_ctx);
  prv_sha1_update(&outer_ctx, o_key_pad, sizeof(o_key_pad));
  prv_sha1_update(&outer_ctx, inner_digest, sizeof(inner_digest));
  prv_sha1_final(&outer_ctx, out);
}

// ============================================================================
// SHA256
// ============================================================================

#define SHA256_ROTR(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define SHA256_CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROTR(x,2) ^ SHA256_ROTR(x,13) ^ SHA256_ROTR(x,22))
#define SHA256_EP1(x) (SHA256_ROTR(x,6) ^ SHA256_ROTR(x,11) ^ SHA256_ROTR(x,25))
#define SHA256_SIG0(x) (SHA256_ROTR(x,7) ^ SHA256_ROTR(x,18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x,17) ^ SHA256_ROTR(x,19) ^ ((x) >> 10))

static const uint32_t k256[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void prv_sha256_transform(Sha256Context *ctx, const uint8_t data[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;
  
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)data[i * 4] << 24) |
           ((uint32_t)data[i * 4 + 1] << 16) |
           ((uint32_t)data[i * 4 + 2] << 8) |
           ((uint32_t)data[i * 4 + 3]);
  }
  
  for (int i = 16; i < 64; i++) {
    w[i] = SHA256_SIG1(w[i - 2]) + w[i - 7] + SHA256_SIG0(w[i - 15]) + w[i - 16];
  }
  
  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];
  
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + k256[i] + w[i];
    uint32_t t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static void prv_sha256_init(Sha256Context *ctx) {
  ctx->count = 0;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

static void prv_sha256_update(Sha256Context *ctx, const uint8_t *data, size_t len) {
  size_t buflen = (size_t)(ctx->count & 63);
  ctx->count += len;
  
  if (buflen + len >= 64) {
    memcpy(ctx->buffer + buflen, data, 64 - buflen);
    prv_sha256_transform(ctx, ctx->buffer);
    data += 64 - buflen;
    len -= 64 - buflen;
    buflen = 0;
    
    while (len >= 64) {
      prv_sha256_transform(ctx, data);
      data += 64;
      len -= 64;
    }
  }
  
  memcpy(ctx->buffer + buflen, data, len);
}

static void prv_sha256_final(Sha256Context *ctx, uint8_t digest[32]) {
  size_t buflen = (size_t)(ctx->count & 63);
  ctx->buffer[buflen++] = 0x80;
  
  if (buflen > 56) {
    memset(ctx->buffer + buflen, 0, 64 - buflen);
    prv_sha256_transform(ctx, ctx->buffer);
    buflen = 0;
  }
  
  memset(ctx->buffer + buflen, 0, 56 - buflen);
  
  uint64_t bitcount = ctx->count * 8;
  for (int i = 0; i < 8; i++) {
    ctx->buffer[63 - i] = (uint8_t)(bitcount >> (i * 8));
  }
  
  prv_sha256_transform(ctx, ctx->buffer);
  
  for (int i = 0; i < 32; i++) {
    digest[i] = (uint8_t)(ctx->state[i >> 2] >> ((3 - (i & 3)) * 8));
  }
}

// ============================================================================
// HMAC-SHA256
// ============================================================================

void prv_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t out[32]) {
  uint8_t key_block[64];
  memset(key_block, 0, sizeof(key_block));

  if (key_len > sizeof(key_block)) {
    Sha256Context key_ctx;
    prv_sha256_init(&key_ctx);
    prv_sha256_update(&key_ctx, key, key_len);
    uint8_t key_hash[32];
    prv_sha256_final(&key_ctx, key_hash);
    memcpy(key_block, key_hash, sizeof(key_hash));
  } else {
    memcpy(key_block, key, key_len);
  }

  uint8_t o_key_pad[64];
  uint8_t i_key_pad[64];
  for (int i = 0; i < 64; i++) {
    o_key_pad[i] = key_block[i] ^ 0x5C;
    i_key_pad[i] = key_block[i] ^ 0x36;
  }

  Sha256Context inner_ctx;
  prv_sha256_init(&inner_ctx);
  prv_sha256_update(&inner_ctx, i_key_pad, sizeof(i_key_pad));
  prv_sha256_update(&inner_ctx, data, data_len);
  uint8_t inner_digest[32];
  prv_sha256_final(&inner_ctx, inner_digest);

  Sha256Context outer_ctx;
  prv_sha256_init(&outer_ctx);
  prv_sha256_update(&outer_ctx, o_key_pad, sizeof(o_key_pad));
  prv_sha256_update(&outer_ctx, inner_digest, sizeof(inner_digest));
  prv_sha256_final(&outer_ctx, out);
}

// ============================================================================
// SHA512
// ============================================================================

#define SHA512_ROTR(x,n) (((x) >> (n)) | ((x) << (64-(n))))
#define SHA512_CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA512_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA512_EP0(x) (SHA512_ROTR(x,28) ^ SHA512_ROTR(x,34) ^ SHA512_ROTR(x,39))
#define SHA512_EP1(x) (SHA512_ROTR(x,14) ^ SHA512_ROTR(x,18) ^ SHA512_ROTR(x,41))
#define SHA512_SIG0(x) (SHA512_ROTR(x,1) ^ SHA512_ROTR(x,8) ^ ((x) >> 7))
#define SHA512_SIG1(x) (SHA512_ROTR(x,19) ^ SHA512_ROTR(x,61) ^ ((x) >> 6))

static const uint64_t k512[80] = {
  0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
  0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
  0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
  0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
  0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
  0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
  0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
  0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
  0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
  0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
  0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
  0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
  0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
  0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
  0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
  0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
  0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
  0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
  0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
  0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void prv_sha512_transform(Sha512Context *ctx, const uint8_t data[128]) {
  uint64_t w[80];
  uint64_t a, b, c, d, e, f, g, h;
  
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint64_t)data[i * 8] << 56) |
           ((uint64_t)data[i * 8 + 1] << 48) |
           ((uint64_t)data[i * 8 + 2] << 40) |
           ((uint64_t)data[i * 8 + 3] << 32) |
           ((uint64_t)data[i * 8 + 4] << 24) |
           ((uint64_t)data[i * 8 + 5] << 16) |
           ((uint64_t)data[i * 8 + 6] << 8) |
           ((uint64_t)data[i * 8 + 7]);
  }
  
  for (int i = 16; i < 80; i++) {
    w[i] = SHA512_SIG1(w[i - 2]) + w[i - 7] + SHA512_SIG0(w[i - 15]) + w[i - 16];
  }
  
  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];
  
  for (int i = 0; i < 80; i++) {
    uint64_t t1 = h + SHA512_EP1(e) + SHA512_CH(e, f, g) + k512[i] + w[i];
    uint64_t t2 = SHA512_EP0(a) + SHA512_MAJ(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static void prv_sha512_init(Sha512Context *ctx) {
  ctx->count[0] = 0;
  ctx->count[1] = 0;
  ctx->state[0] = 0x6a09e667f3bcc908ULL;
  ctx->state[1] = 0xbb67ae8584caa73bULL;
  ctx->state[2] = 0x3c6ef372fe94f82bULL;
  ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
  ctx->state[4] = 0x510e527fade682d1ULL;
  ctx->state[5] = 0x9b05688c2b3e6c1fULL;
  ctx->state[6] = 0x1f83d9abfb41bd6bULL;
  ctx->state[7] = 0x5be0cd19137e2179ULL;
}

static void prv_sha512_update(Sha512Context *ctx, const uint8_t *data, size_t len) {
  size_t buflen = (size_t)(ctx->count[0] & 127);
  
  uint64_t old_count = ctx->count[0];
  ctx->count[0] += len;
  if (ctx->count[0] < old_count) {
    ctx->count[1]++;
  }
  
  if (buflen + len >= 128) {
    memcpy(ctx->buffer + buflen, data, 128 - buflen);
    prv_sha512_transform(ctx, ctx->buffer);
    data += 128 - buflen;
    len -= 128 - buflen;
    buflen = 0;
    
    while (len >= 128) {
      prv_sha512_transform(ctx, data);
      data += 128;
      len -= 128;
    }
  }
  
  memcpy(ctx->buffer + buflen, data, len);
}

static void prv_sha512_final(Sha512Context *ctx, uint8_t digest[64]) {
  size_t buflen = (size_t)(ctx->count[0] & 127);
  ctx->buffer[buflen++] = 0x80;
  
  if (buflen > 112) {
    memset(ctx->buffer + buflen, 0, 128 - buflen);
    prv_sha512_transform(ctx, ctx->buffer);
    buflen = 0;
  }
  
  memset(ctx->buffer + buflen, 0, 112 - buflen);
  
  uint64_t bitcount_high = (ctx->count[1] << 3) | (ctx->count[0] >> 61);
  uint64_t bitcount_low = ctx->count[0] << 3;
  
  for (int i = 0; i < 8; i++) {
    ctx->buffer[119 - i] = (uint8_t)(bitcount_high >> (i * 8));
    ctx->buffer[127 - i] = (uint8_t)(bitcount_low >> (i * 8));
  }
  
  prv_sha512_transform(ctx, ctx->buffer);
  
  for (int i = 0; i < 64; i++) {
    digest[i] = (uint8_t)(ctx->state[i >> 3] >> ((7 - (i & 7)) * 8));
  }
}

// ============================================================================
// HMAC-SHA512
// ============================================================================

void prv_hmac_sha512(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t out[64]) {
  uint8_t key_block[128];
  memset(key_block, 0, sizeof(key_block));

  if (key_len > sizeof(key_block)) {
    Sha512Context key_ctx;
    prv_sha512_init(&key_ctx);
    prv_sha512_update(&key_ctx, key, key_len);
    uint8_t key_hash[64];
    prv_sha512_final(&key_ctx, key_hash);
    memcpy(key_block, key_hash, sizeof(key_hash));
  } else {
    memcpy(key_block, key, key_len);
  }

  uint8_t o_key_pad[128];
  uint8_t i_key_pad[128];
  for (int i = 0; i < 128; i++) {
    o_key_pad[i] = key_block[i] ^ 0x5C;
    i_key_pad[i] = key_block[i] ^ 0x36;
  }

  Sha512Context inner_ctx;
  prv_sha512_init(&inner_ctx);
  prv_sha512_update(&inner_ctx, i_key_pad, sizeof(i_key_pad));
  prv_sha512_update(&inner_ctx, data, data_len);
  uint8_t inner_digest[64];
  prv_sha512_final(&inner_ctx, inner_digest);

  Sha512Context outer_ctx;
  prv_sha512_init(&outer_ctx);
  prv_sha512_update(&outer_ctx, o_key_pad, sizeof(o_key_pad));
  prv_sha512_update(&outer_ctx, inner_digest, sizeof(inner_digest));
  prv_sha512_final(&outer_ctx, out);
}

// ============================================================================
// TOTP Generation
// ============================================================================

bool totp_generate(const TotpAccount *account, time_t now, char *output, size_t output_len, uint64_t *out_counter) {
  if (!account || account->secret_len == 0 || !output || output_len == 0) {
    return false;
  }
  uint32_t period = account->period > 0 ? account->period : DEFAULT_PERIOD;
  uint8_t digits = account->digits >= MIN_DIGITS && account->digits <= MAX_DIGITS ? account->digits : DEFAULT_DIGITS;

  uint64_t counter = (uint64_t)(now / period);
  uint8_t message[8];
  for (int i = 7; i >= 0; i--) {
    message[i] = (uint8_t)(counter & 0xFF);
    counter >>= 8;
  }
  counter = (uint64_t)(now / period);

  // Select hash algorithm
  uint8_t hash[64];  // Max size for SHA512
  size_t hash_len;
  
  switch (account->algorithm) {
    case TOTP_ALGO_SHA256:
      prv_hmac_sha256(account->secret, account->secret_len, message, sizeof(message), hash);
      hash_len = 32;
      break;
      
    case TOTP_ALGO_SHA512:
      prv_hmac_sha512(account->secret, account->secret_len, message, sizeof(message), hash);
      hash_len = 64;
      break;
      
    case TOTP_ALGO_SHA1:
    default:
      prv_hmac_sha1(account->secret, account->secret_len, message, sizeof(message), hash);
      hash_len = 20;
      break;
  }

  uint8_t offset = hash[hash_len - 1] & 0x0F;
  uint32_t binary =
      ((uint32_t)(hash[offset] & 0x7F) << 24) |
      ((uint32_t)(hash[offset + 1] & 0xFF) << 16) |
      ((uint32_t)(hash[offset + 2] & 0xFF) << 8) |
      ((uint32_t)(hash[offset + 3] & 0xFF));

  uint32_t divisor = 1;
  for (uint8_t i = 0; i < digits; i++) {
    divisor *= 10;
  }
  uint32_t otp = binary % divisor;
  if (output_len < (size_t)digits + 1) {
    return false;
  }
  snprintf(output, output_len, "%0*u", digits, (unsigned int)otp);

  if (out_counter) {
    *out_counter = counter;
  }
  return true;
}
