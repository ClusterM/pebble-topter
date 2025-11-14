#include "comms.h"
#include "ui.h"
#include "totp.h"
#include "storage.h"
#include "message_keys.auto.h"
#include <string.h>

// Sync state
static size_t s_sync_expected_count = 0;
static size_t s_sync_received_count = 0;

static void prv_request_sync(void) {
  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to begin outbox: %d", res);
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_AppKeyRequest, 1);
  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to send request: %d", res);
  }
}

static void prv_send_status(uint8_t status_code) {
  DictionaryIterator *iter = NULL;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_AppKeyStatus, status_code);
  app_message_outbox_send();
}

static bool prv_parse_line(const char *line, TotpAccount *out_account) {
  if (!line || !out_account) {
    return false;
  }

  char buffer[SECRET_BASE32_MAX_LEN + LABEL_MAX_LEN + ACCOUNT_NAME_MAX_LEN + 32];
  strncpy(buffer, line, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  char *label = buffer;
  char *account_name = strchr(buffer, '|');
  if (!account_name) {
    return false;
  }
  *account_name = '\0';
  account_name++;

  char *secret = strchr(account_name, '|');
  if (!secret) {
    return false;
  }
  *secret = '\0';
  secret++;

  char *period_str = strchr(secret, '|');
  if (period_str) {
    *period_str = '\0';
    period_str++;
  }
  char *digits_str = NULL;
  if (period_str) {
    digits_str = strchr(period_str, '|');
    if (digits_str) {
      *digits_str = '\0';
      digits_str++;
    }
  }

  // Trim spaces for label
  char *trim_ptr = label;
  while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
  memmove(label, trim_ptr, strlen(trim_ptr) + 1);
  trim_ptr = label + strlen(label) - 1;
  while (trim_ptr >= label && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
    *trim_ptr = '\0';
    trim_ptr--;
  }

  // Trim spaces for account_name
  trim_ptr = account_name;
  while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
  memmove(account_name, trim_ptr, strlen(trim_ptr) + 1);
  trim_ptr = account_name + strlen(account_name) - 1;
  while (trim_ptr >= account_name && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
    *trim_ptr = '\0';
    trim_ptr--;
  }

  // Trim spaces for secret
  trim_ptr = secret;
  while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
  memmove(secret, trim_ptr, strlen(trim_ptr) + 1);
  trim_ptr = secret + strlen(secret) - 1;
  while (trim_ptr >= secret && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
    *trim_ptr = '\0';
    trim_ptr--;
  }

  if (period_str) {
    trim_ptr = period_str;
    while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
    memmove(period_str, trim_ptr, strlen(trim_ptr) + 1);
    trim_ptr = period_str + strlen(period_str) - 1;
    while (trim_ptr >= period_str && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
      *trim_ptr = '\0';
      trim_ptr--;
    }
  }

  if (digits_str) {
    trim_ptr = digits_str;
    while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
    memmove(digits_str, trim_ptr, strlen(trim_ptr) + 1);
    trim_ptr = digits_str + strlen(digits_str) - 1;
    while (trim_ptr >= digits_str && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
      *trim_ptr = '\0';
      trim_ptr--;
    }
  }

  if (label[0] == '\0' || secret[0] == '\0') {
    return false;
  }

  TotpAccount account;
  memset(&account, 0, sizeof(account));
  strncpy(account.label, label, sizeof(account.label) - 1);
  if (account_name[0] != '\0') {
    strncpy(account.account_name, account_name, sizeof(account.account_name) - 1);
  }

  uint8_t secret_bytes[SECRET_BYTES_MAX];
  int decoded_len = base32_decode(secret, secret_bytes, sizeof(secret_bytes));
  if (decoded_len <= 0) {
    return false;
  }
  account.secret_len = (size_t)decoded_len;
  memcpy(account.secret, secret_bytes, account.secret_len);

  account.period = period_str && period_str[0] ? (uint32_t)atoi(period_str) : DEFAULT_PERIOD;
  if (account.period == 0) {
    account.period = DEFAULT_PERIOD;
  }
  account.digits = digits_str && digits_str[0] ? (uint8_t)atoi(digits_str) : DEFAULT_DIGITS;
  if (account.digits < MIN_DIGITS || account.digits > MAX_DIGITS) {
    account.digits = DEFAULT_DIGITS;
  }

  *out_account = account;
  return true;
}

bool comms_parse_count(size_t count) {
  s_sync_expected_count = count;
  s_sync_received_count = 0;

  storage_set_count(0);
  ui_set_total_count(0);
  ui_set_loading(true);

  return true;
}

bool comms_parse_account(size_t id, const char *data) {
  if (!data) return false;

  TotpAccount account;
  if (!prv_parse_line(data, &account)) {
    return false;
  }

  if (!storage_save_account(id, &account)) {
    return false;
  }

  s_sync_received_count++;

  if (s_sync_received_count >= s_sync_expected_count) {
    storage_set_count(s_sync_expected_count);
    ui_set_total_count(s_sync_expected_count);
    prv_send_status(1);
  }

  return true;
}


static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  
  Tuple *count_tuple = dict_find(iter, MESSAGE_KEY_AppKeyCount);
  if (count_tuple) {
    if (count_tuple->type == TUPLE_INT) {
      size_t count = (size_t)count_tuple->value->int32;
      comms_parse_count(count);
      return;
    } else if (count_tuple->type == TUPLE_UINT) {
      size_t count = (size_t)count_tuple->value->uint16;
      comms_parse_count(count);
      return;
    }
  }

  Tuple *entry_tuple = dict_find(iter, MESSAGE_KEY_AppKeyEntry);
  Tuple *id_tuple = dict_find(iter, MESSAGE_KEY_AppKeyEntryId);
  if (entry_tuple && id_tuple) {
    if (entry_tuple->type == TUPLE_CSTRING && id_tuple->type == TUPLE_INT) {
      size_t id = (size_t)id_tuple->value->int32;
      const char *data = entry_tuple->value->cstring;
      comms_parse_account(id, data);
      return;
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  (void)reason;
  (void)context;
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  (void)iter;
  (void)reason;
  (void)context;
}

static void prv_outbox_sent(DictionaryIterator *iter, void *context) {
  (void)iter;
  (void)context;
}

void comms_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_register_outbox_sent(prv_outbox_sent);
  
  const int inbox_size = 512;
  const int outbox_size = 128;
  if (app_message_open(inbox_size, outbox_size) == APP_MSG_OK) {
    prv_request_sync();
  }
}

void comms_deinit(void) {
  app_message_deregister_callbacks();
}

void comms_request_sync(void) {
  prv_request_sync();
}
