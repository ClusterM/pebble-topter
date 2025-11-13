#include "storage.h"
#include "ui.h"
#include <string.h>

typedef struct {
  char label[NAME_MAX_LEN + 1];
  char account_name[NAME_MAX_LEN + 1];
  uint8_t secret_len;
  uint8_t secret[SECRET_BYTES_MAX];
  uint16_t period;
  uint8_t digits;
} __attribute__((__packed__)) PersistedAccount;

// Get account count
size_t storage_get_count(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_get_count: checking PERSIST_KEY_COUNT=%d", PERSIST_KEY_COUNT);
  bool exists = persist_exists(PERSIST_KEY_COUNT);
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_get_count: persist_exists=%d", exists ? 1 : 0);
  if (!exists) {
    APP_LOG(APP_LOG_LEVEL_INFO, "storage_get_count: key does not exist, returning 0");
    return 0;
  }
  int32_t count = persist_read_int(PERSIST_KEY_COUNT);
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_get_count: read count=%ld", (long)count);
  return (size_t)count;
}

// Set account count
void storage_set_count(size_t count) {
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_set_count: setting count=%d to key %d", (int)count, PERSIST_KEY_COUNT);
  persist_write_int(PERSIST_KEY_COUNT, count);
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_set_count: write completed");
}

// Load account by ID
bool storage_load_account(size_t id, TotpAccount *account) {
  if (!account) return false;

  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  APP_LOG(APP_LOG_LEVEL_INFO, "Checking persistence key %d for account %d", (int)key, (int)id);
  if (!persist_exists(key)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Persistence key %d does not exist", (int)key);
    return false;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "Reading account %d from storage", (int)id);
  PersistedAccount data;
  persist_read_data(key, &data, sizeof(data));

  memset(account, 0, sizeof(*account));
  strncpy(account->label, data.label, sizeof(account->label) - 1);
  strncpy(account->account_name, data.account_name, sizeof(account->account_name) - 1);
  account->secret_len = data.secret_len;
  if (account->secret_len > SECRET_BYTES_MAX) {
    account->secret_len = SECRET_BYTES_MAX;
  }
  memcpy(account->secret, data.secret, account->secret_len);
  account->period = data.period > 0 ? data.period : DEFAULT_PERIOD;
  account->digits = data.digits >= MIN_DIGITS && data.digits <= MAX_DIGITS ? data.digits : DEFAULT_DIGITS;

  APP_LOG(APP_LOG_LEVEL_INFO, "Account %d loaded: label='%s', account_name='%s', secret_len=%d, digits=%d, period=%d",
           (int)id, account->label, account->account_name, (int)account->secret_len, (int)account->digits, (int)account->period);
  return true;
}

// Save account by ID
bool storage_save_account(size_t id, const TotpAccount *account) {
  if (!account) return false;

  PersistedAccount data;
  memset(&data, 0, sizeof(data));
  strncpy(data.label, account->label, sizeof(data.label) - 1);
  strncpy(data.account_name, account->account_name, sizeof(data.account_name) - 1);
  data.secret_len = account->secret_len;
  memcpy(data.secret, account->secret, account->secret_len);
  data.period = account->period;
  data.digits = account->digits;

  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  return persist_write_data(key, &data, sizeof(data)) == sizeof(data);
}

// Delete account by ID
void storage_delete_account(size_t id) {
  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  persist_delete(key);
}

// Load account count from storage
void storage_load_accounts(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_load_accounts: starting");
  size_t count = storage_get_count();
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_load_accounts: got count=%d", (int)count);
  s_total_account_count = count;
  APP_LOG(APP_LOG_LEVEL_INFO, "storage_load_accounts: set s_total_account_count=%d", (int)s_total_account_count);
}

