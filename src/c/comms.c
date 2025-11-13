#include "comms.h"
#include "ui.h"
#include "totp.h"
#include "storage.h"
#include "message_keys.auto.h"
#include <string.h>

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

  char buffer[SECRET_BASE32_MAX_LEN + NAME_MAX_LEN + 24];
  strncpy(buffer, line, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  char *name = buffer;
  char *secret = strchr(buffer, '|');
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

  // Убираем пробелы
  char *trim_ptr = name;
  while (*trim_ptr == ' ' || *trim_ptr == '\t') trim_ptr++;
  memmove(name, trim_ptr, strlen(trim_ptr) + 1);
  trim_ptr = name + strlen(name) - 1;
  while (trim_ptr >= name && (*trim_ptr == ' ' || *trim_ptr == '\t')) {
    *trim_ptr = '\0';
    trim_ptr--;
  }

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

  if (name[0] == '\0' || secret[0] == '\0') {
    return false;
  }

  TotpAccount account;
  memset(&account, 0, sizeof(account));
  strncpy(account.name, name, sizeof(account.name) - 1);

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

bool comms_parse_payload(const char *payload) {
  if (!payload) {
    return false;
  }

  TotpAccount new_accounts[MAX_ACCOUNTS];
  size_t count = 0;

  const char *pos = payload;
  while (*pos && count < MAX_ACCOUNTS) {
    const char *end = strchr(pos, ';');
    size_t len = end ? (size_t)(end - pos) : strlen(pos);
    if (len > 0) {
      char line[SECRET_BASE32_MAX_LEN + NAME_MAX_LEN + 24];
      size_t copy_len = len;
      if (copy_len >= sizeof(line)) {
        copy_len = sizeof(line) - 1;
      }
      memcpy(line, pos, copy_len);
      line[copy_len] = '\0';
      if (prv_parse_line(line, &new_accounts[count])) {
        count++;
      }
    }
    if (!end) {
      break;
    }
    pos = end + 1;
  }

  if (count == 0) {
    return false;
  }

  s_account_count = count;
  for (size_t i = 0; i < count; i++) {
    s_accounts[i] = new_accounts[i];
    s_counter_cache[i] = UINT64_MAX;
    memset(s_code_cache[i], 0, sizeof(s_code_cache[i]));
  }
  return true;
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *payload = dict_find(iter, MESSAGE_KEY_AppKeyPayload);
  if (!payload || payload->type != TUPLE_CSTRING) {
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Received configuration payload");
  const char *payload_str = payload->value->cstring;
  if (payload_str[0] == '\0') {
    s_account_count = 0;
    ui_rebuild_scroll_content();
    ui_update_codes(true);
    storage_save_accounts();
    prv_send_status(1);
    return;
  }

  bool parse_ok = comms_parse_payload(payload_str);
  if (!parse_ok) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to parse payload");
    prv_send_status(0);
    return;
  }

  ui_rebuild_scroll_content();
  ui_update_codes(true);
  storage_save_accounts();
  prv_send_status(1);
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Outbox send failed: %d", reason);
}

static void prv_outbox_sent(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox message sent");
}

void comms_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_register_outbox_sent(prv_outbox_sent);
  const int inbox_size = 512;
  const int outbox_size = 128;
  AppMessageResult result = app_message_open(inbox_size, outbox_size);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage open failed: %d", result);
  } else {
    prv_request_sync();
  }
}

void comms_deinit(void) {
  app_message_deregister_callbacks();
}

void comms_request_sync(void) {
  prv_request_sync();
}
