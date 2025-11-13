#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_ACCOUNTS 10
#define NAME_MAX_LEN 32
#define SECRET_BASE32_MAX_LEN 64
#define SECRET_BYTES_MAX 64
#define DEFAULT_PERIOD 30
#define DEFAULT_DIGITS 6
#define MIN_DIGITS 6
#define MAX_DIGITS 8

typedef struct {
  char name[NAME_MAX_LEN + 1];
  uint8_t secret[SECRET_BYTES_MAX];
  size_t secret_len;
  uint32_t period;
  uint8_t digits;
} TotpAccount;

// Генерация TOTP кода
bool totp_generate(const TotpAccount *account, time_t now, char *output, size_t output_len, uint64_t *out_counter);

// Декодирование base32 секрета
int base32_decode(const char *input, uint8_t *output, size_t output_max);
