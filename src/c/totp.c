#include "totp.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
  uint32_t state[5];
  uint64_t count;
  uint8_t buffer[64];
} Sha1Context;

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
    // Получаем 5 байт (40 бит) или меньше
    uint64_t buffer = 0;
    int bits = 0;

    for (int j = 0; j < 5 && i < input_len; j++) {
      buffer = (buffer << 8) | input[i++];
      bits += 8;
    }

    // Кодируем в 8 символов base32
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

bool totp_generate(const TotpAccount *account, time_t now, char *output, size_t output_len, uint64_t *out_counter) {
  if (!account || account->secret_len == 0 || !output || output_len == 0) {
    return false;
  }
  uint32_t period = account->period > 0 ? account->period : DEFAULT_PERIOD;
  uint8_t digits = account->digits >= MIN_DIGITS && account->digits <= MAX_DIGITS ? account->digits : DEFAULT_DIGITS;

  APP_LOG(APP_LOG_LEVEL_INFO, "totp_generate: now=%ld, period=%d", (long)now, (int)period);
  uint64_t counter = (uint64_t)(now / period);
  APP_LOG(APP_LOG_LEVEL_INFO, "totp_generate: counter=%llu", counter);
  uint8_t message[8];
  for (int i = 7; i >= 0; i--) {
    message[i] = (uint8_t)(counter & 0xFF);
    counter >>= 8;
  }
  counter = (uint64_t)(now / period);

  uint8_t hash[20];
  prv_hmac_sha1(account->secret, account->secret_len, message, sizeof(message), hash);

  uint8_t offset = hash[19] & 0x0F;
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
