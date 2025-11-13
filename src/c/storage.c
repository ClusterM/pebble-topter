#include "storage.h"
#include "ui.h"
#include <string.h>

typedef struct {
  char name[NAME_MAX_LEN + 1];
  uint8_t secret_len;
  uint8_t secret[SECRET_BYTES_MAX];
  uint16_t period;
  uint8_t digits;
} __attribute__((__packed__)) PersistedAccount;

typedef struct {
  uint8_t count;
  PersistedAccount accounts[MAX_ACCOUNTS];
} __attribute__((__packed__)) PersistedData;

void storage_save_accounts(void) {
  PersistedData data;
  memset(&data, 0, sizeof(data));
  data.count = (uint8_t)s_account_count;
  for (size_t i = 0; i < s_account_count && i < MAX_ACCOUNTS; i++) {
    PersistedAccount *dest = &data.accounts[i];
    strncpy(dest->name, s_accounts[i].name, sizeof(dest->name) - 1);
    dest->secret_len = (uint8_t)s_accounts[i].secret_len;
    memcpy(dest->secret, s_accounts[i].secret, s_accounts[i].secret_len);
    dest->period = (uint16_t)s_accounts[i].period;
    dest->digits = s_accounts[i].digits;
  }
  size_t size_to_write = sizeof(data.count) + s_account_count * sizeof(PersistedAccount);
  persist_write_data(PERSIST_KEY_ACCOUNTS, &data, size_to_write);
}

void storage_load_accounts(void) {
  if (!persist_exists(PERSIST_KEY_ACCOUNTS)) {
    s_account_count = 0;
    return;
  }
  int stored_size = persist_get_size(PERSIST_KEY_ACCOUNTS);
  if (stored_size < (int)sizeof(uint8_t)) {
    s_account_count = 0;
    return;
  }

  uint8_t buffer[sizeof(PersistedData)];
  if (stored_size > (int)sizeof(buffer)) {
    stored_size = sizeof(buffer);
  }
  persist_read_data(PERSIST_KEY_ACCOUNTS, buffer, stored_size);
  PersistedData *data = (PersistedData *)buffer;
  size_t count = data->count;
  if (count > MAX_ACCOUNTS) {
    count = MAX_ACCOUNTS;
  }

  for (size_t i = 0; i < count; i++) {
    PersistedAccount *src = &data->accounts[i];
    TotpAccount *dst = &s_accounts[i];
    memset(dst, 0, sizeof(*dst));
    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
    dst->secret_len = src->secret_len;
    if (dst->secret_len > SECRET_BYTES_MAX) {
      dst->secret_len = SECRET_BYTES_MAX;
    }
    memcpy(dst->secret, src->secret, dst->secret_len);
    dst->period = src->period > 0 ? src->period : DEFAULT_PERIOD;
    dst->digits = src->digits >= MIN_DIGITS && src->digits <= MAX_DIGITS ? src->digits : DEFAULT_DIGITS;
  }
  s_account_count = count;
  for (size_t i = 0; i < MAX_ACCOUNTS; i++) {
    s_counter_cache[i] = UINT64_MAX;
    memset(s_code_cache[i], 0, sizeof(s_code_cache[i]));
  }
}
