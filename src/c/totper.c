#include <pebble.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "message_keys.auto.h"

#define MAX_ACCOUNTS 10
#define NAME_MAX_LEN 32
#define SECRET_BASE32_MAX_LEN 64
#define SECRET_BYTES_MAX 64
#define DEFAULT_PERIOD 30
#define DEFAULT_DIGITS 6
#define MIN_DIGITS 6
#define MAX_DIGITS 8
#define PERSIST_KEY_ACCOUNTS 1

typedef struct {
  char name[NAME_MAX_LEN + 1];
  uint8_t secret[SECRET_BYTES_MAX];
  size_t secret_len;
  uint32_t period;
  uint8_t digits;
} TotpAccount;

typedef struct {
  TextLayer *name_layer;
  TextLayer *code_layer;
  TextLayer *detail_layer;
} AccountView;

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

static Window *s_window;
static ScrollLayer *s_scroll_layer;
static Layer *s_container_layer;
static TextLayer *s_empty_layer;

static TotpAccount s_accounts[MAX_ACCOUNTS];
static size_t s_account_count;
static AccountView s_account_views[MAX_ACCOUNTS];
static char s_code_cache[MAX_ACCOUNTS][MAX_DIGITS + 2];
static uint64_t s_counter_cache[MAX_ACCOUNTS];

// === Utility helpers =======================================================

static bool prv_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void prv_trim(char *str) {
  if (!str) {
    return;
  }
  size_t len = strlen(str);
  size_t start = 0;
  while (start < len && prv_is_space(str[start])) {
    start++;
  }
  size_t end = len;
  while (end > start && prv_is_space(str[end - 1])) {
    end--;
  }
  if (start > 0) {
    memmove(str, str + start, end - start);
  }
  str[end - start] = '\0';
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

static int prv_base32_decode(const char *input, uint8_t *output, size_t output_max) {
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

// === SHA1 / HMAC implementation ============================================

typedef struct {
  uint32_t state[5];
  uint64_t count;
  uint8_t buffer[64];
} Sha1Context;

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

static void prv_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t out[20]) {
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

static bool prv_totp_generate(const TotpAccount *account, time_t now, char *output, size_t output_len, uint64_t *out_counter) {
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

// === Persistence ===========================================================

static void prv_save_accounts(void) {
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

static void prv_load_accounts(void) {
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

// === Parsing ===============================================================

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

  prv_trim(name);
  prv_trim(secret);
  if (period_str) {
    prv_trim(period_str);
  }
  if (digits_str) {
    prv_trim(digits_str);
  }

  if (name[0] == '\0' || secret[0] == '\0') {
    return false;
  }

  TotpAccount account;
  memset(&account, 0, sizeof(account));
  strncpy(account.name, name, sizeof(account.name) - 1);

  uint8_t secret_bytes[SECRET_BYTES_MAX];
  int decoded_len = prv_base32_decode(secret, secret_bytes, sizeof(secret_bytes));
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

static bool prv_parse_payload(const char *payload) {
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

// === UI ====================================================================

static void prv_destroy_account_views(void) {
  for (size_t i = 0; i < MAX_ACCOUNTS; i++) {
    if (s_account_views[i].name_layer) {
      text_layer_destroy(s_account_views[i].name_layer);
      s_account_views[i].name_layer = NULL;
    }
    if (s_account_views[i].code_layer) {
      text_layer_destroy(s_account_views[i].code_layer);
      s_account_views[i].code_layer = NULL;
    }
    if (s_account_views[i].detail_layer) {
      text_layer_destroy(s_account_views[i].detail_layer);
      s_account_views[i].detail_layer = NULL;
    }
  }
  if (s_container_layer) {
    layer_destroy(s_container_layer);
    s_container_layer = NULL;
  }
}

static void prv_update_empty_state(void) {
  if (!s_scroll_layer || !s_empty_layer) {
    return;
  }

  bool has_accounts = s_account_count > 0;
  layer_set_hidden(text_layer_get_layer(s_empty_layer), has_accounts);
  if (!has_accounts) {
    scroll_layer_set_content_size(s_scroll_layer, GSize(layer_get_bounds(scroll_layer_get_layer(s_scroll_layer)).size.w, 0));
  }
}

static void prv_rebuild_scroll_content(void) {
  if (!s_scroll_layer) {
    return;
  }

  prv_destroy_account_views();

  Layer *scroll_layer = scroll_layer_get_layer(s_scroll_layer);
  GRect bounds = layer_get_bounds(scroll_layer);

  if (s_account_count == 0) {
    prv_update_empty_state();
    return;
  }

  int16_t y = 4;
  const int16_t width = bounds.size.w;

  s_container_layer = layer_create(GRect(0, 0, width, 4));
  scroll_layer_add_child(s_scroll_layer, s_container_layer);

  for (size_t i = 0; i < s_account_count && i < MAX_ACCOUNTS; i++) {
    AccountView *view = &s_account_views[i];
    GRect name_frame = GRect(4, y, width - 8, 22);
    view->name_layer = text_layer_create(name_frame);
    text_layer_set_font(view->name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_background_color(view->name_layer, GColorClear);
    text_layer_set_text_color(view->name_layer, GColorWhite);
    text_layer_set_text(view->name_layer, s_accounts[i].name);
    text_layer_set_text_alignment(view->name_layer, GTextAlignmentLeft);
    layer_add_child(s_container_layer, text_layer_get_layer(view->name_layer));

    y += 24;
    GRect code_frame = GRect(0, y, width, 34);
    view->code_layer = text_layer_create(code_frame);
    text_layer_set_font(view->code_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_background_color(view->code_layer, GColorClear);
    text_layer_set_text_color(view->code_layer, GColorWhite);
    text_layer_set_text_alignment(view->code_layer, GTextAlignmentCenter);
    text_layer_set_text(view->code_layer, s_code_cache[i][0] ? s_code_cache[i] : "------");
    layer_add_child(s_container_layer, text_layer_get_layer(view->code_layer));

    y += 34;
    GRect detail_frame = GRect(4, y - 6, width - 8, 18);
    view->detail_layer = text_layer_create(detail_frame);
    text_layer_set_font(view->detail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_background_color(view->detail_layer, GColorClear);
    text_layer_set_text_color(view->detail_layer, GColorWhite);
    text_layer_set_text_alignment(view->detail_layer, GTextAlignmentRight);
    text_layer_set_text(view->detail_layer, "");
    layer_add_child(s_container_layer, text_layer_get_layer(view->detail_layer));

    y += 24;
  }

  layer_set_frame(s_container_layer, GRect(0, 0, width, y));
  scroll_layer_set_content_size(s_scroll_layer, GSize(width, y));
  prv_update_empty_state();
}

static void prv_update_codes(bool force) {
  if (s_account_count == 0) {
    return;
  }
  time_t now = time(NULL);
  for (size_t i = 0; i < s_account_count; i++) {
    uint64_t counter = 0;
    char code_buffer[MAX_DIGITS + 2];
    if (!prv_totp_generate(&s_accounts[i], now, code_buffer, sizeof(code_buffer), &counter)) {
      continue;
    }
    bool changed = force || (counter != s_counter_cache[i]) || (strcmp(code_buffer, s_code_cache[i]) != 0);
    if (changed) {
      strncpy(s_code_cache[i], code_buffer, sizeof(s_code_cache[i]));
      s_code_cache[i][sizeof(s_code_cache[i]) - 1] = '\0';
      s_counter_cache[i] = counter;
      if (s_account_views[i].code_layer) {
        text_layer_set_text(s_account_views[i].code_layer, s_code_cache[i]);
      }
    }

    uint32_t period = s_accounts[i].period > 0 ? s_accounts[i].period : DEFAULT_PERIOD;
    uint32_t elapsed = (uint32_t)(now % period);
    uint32_t remaining = period - elapsed;
    if (remaining == 0) {
      remaining = period;
    }
    if (s_account_views[i].detail_layer) {
      static char detail_buffer[16];
      snprintf(detail_buffer, sizeof(detail_buffer), "%lus", (unsigned long)remaining);
      text_layer_set_text(s_account_views[i].detail_layer, detail_buffer);
    }
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_codes(false);
}

// === AppMessage ============================================================

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

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *payload = dict_find(iter, MESSAGE_KEY_AppKeyPayload);
  if (!payload || payload->type != TUPLE_CSTRING) {
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Received configuration payload");
  const char *payload_str = payload->value->cstring;
  if (payload_str[0] == '\0') {
    s_account_count = 0;
    prv_rebuild_scroll_content();
    prv_update_codes(true);
    prv_update_empty_state();
    prv_save_accounts();
    prv_send_status(1);
    return;
  }

  bool parse_ok = prv_parse_payload(payload_str);
  if (!parse_ok) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to parse payload");
    prv_send_status(0);
    return;
  }

  prv_rebuild_scroll_content();
  prv_update_codes(true);
  prv_save_accounts();
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

// === Window lifecycle ======================================================

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_shadow_hidden(s_scroll_layer, true);
  scroll_layer_set_click_config_onto_window(s_scroll_layer, window);
  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));

  s_empty_layer = text_layer_create(GRect(4, 44, bounds.size.w - 8, 80));
  text_layer_set_text_alignment(s_empty_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_empty_layer, GColorClear);
  text_layer_set_text_color(s_empty_layer, GColorWhite);
  text_layer_set_text(s_empty_layer, "Add accounts via\nthe phone settings.");
  layer_add_child(window_layer, text_layer_get_layer(s_empty_layer));

  prv_rebuild_scroll_content();
  prv_update_codes(true);
  prv_update_empty_state();
}

static void prv_window_unload(Window *window) {
  prv_destroy_account_views();
  if (s_scroll_layer) {
    scroll_layer_destroy(s_scroll_layer);
    s_scroll_layer = NULL;
  }
  if (s_empty_layer) {
    text_layer_destroy(s_empty_layer);
    s_empty_layer = NULL;
  }
}

// === App lifecycle =========================================================

static void prv_init(void) {
  prv_load_accounts();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

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

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  if (s_window) {
    window_destroy(s_window);
  }
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
