#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_ACCOUNTS 64
#define NAME_MAX_LEN 32
#define SECRET_BASE32_MAX_LEN 64
#define SECRET_BYTES_MAX 64
#define DEFAULT_PERIOD 30
#define DEFAULT_DIGITS 6
#define MIN_DIGITS 6
#define MAX_DIGITS 8

typedef struct {
  char label[NAME_MAX_LEN + 1];
  char account_name[NAME_MAX_LEN + 1];
  uint8_t secret[SECRET_BYTES_MAX];
  size_t secret_len;
  uint32_t period;
  uint8_t digits;
} TotpAccount;

// Generate TOTP code
bool totp_generate(const TotpAccount *account, time_t now, char *output, size_t output_len, uint64_t *out_counter);

// Decode base32 secret
int base32_decode(const char *input, uint8_t *output, size_t output_max);

// Encode to base32
int base32_encode(const uint8_t *input, size_t input_len, char *output, size_t output_max);
