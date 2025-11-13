#include "ui.h"
#include "totp.h"
#include "storage.h"
#include <string.h>

// UI global variables
Window *s_window;
ScrollLayer *s_scroll_layer;
Layer *s_container_layer;
TextLayer *s_empty_layer;
size_t s_total_account_count;
AccountView *s_account_views;
size_t s_visible_start;
size_t s_visible_count;

// Free account memory
static void prv_free_account(AccountView *view) {
  if (view->account) {
    free(view->account);
    view->account = NULL;
  }
}

// Load account into memory
static bool prv_load_account(AccountView *view) {
  if (view->account) return true; // already loaded

  APP_LOG(APP_LOG_LEVEL_INFO, "Allocating memory for account %d", (int)view->id);
  view->account = malloc(sizeof(TotpAccount));
  if (!view->account) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory for account %d", (int)view->id);
    return false;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "Loading account %d from storage", (int)view->id);
  if (!storage_load_account(view->id, view->account)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Storage load failed for account %d", (int)view->id);
    free(view->account);
    view->account = NULL;
    return false;
  }

  return true;
}

// Unload account from memory
static void prv_unload_account(AccountView *view) {
  prv_free_account(view);
}

// Free all UI elements
static void prv_destroy_account_views(void) {
  if (!s_account_views) return;

  for (size_t i = 0; i < s_visible_count; i++) {
    AccountView *view = &s_account_views[i];
    prv_free_account(view);

    if (view->label_layer) {
      text_layer_destroy(view->label_layer);
      view->label_layer = NULL;
    }
    if (view->account_name_layer) {
      text_layer_destroy(view->account_name_layer);
      view->account_name_layer = NULL;
    }
    if (view->code_layer) {
      text_layer_destroy(view->code_layer);
      view->code_layer = NULL;
    }
    if (view->detail_layer) {
      text_layer_destroy(view->detail_layer);
      view->detail_layer = NULL;
    }
  }

  free(s_account_views);
  s_account_views = NULL;
  s_visible_count = 0;

  if (s_container_layer) {
    layer_destroy(s_container_layer);
    s_container_layer = NULL;
  }
}

static void prv_update_empty_state(void) {
  if (!s_scroll_layer || !s_empty_layer) {
    return;
  }

  bool has_accounts = s_total_account_count > 0;
  layer_set_hidden(text_layer_get_layer(s_empty_layer), has_accounts);
  if (!has_accounts) {
    scroll_layer_set_content_size(s_scroll_layer, GSize(layer_get_bounds(scroll_layer_get_layer(s_scroll_layer)).size.w, 0));
  }
}

void ui_rebuild_scroll_content(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "ui_rebuild_scroll_content called, total_count: %d", (int)s_total_account_count);
  if (!s_scroll_layer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "No scroll layer");
    return;
  }

  prv_destroy_account_views();

  Layer *scroll_layer = scroll_layer_get_layer(s_scroll_layer);
  GRect bounds = layer_get_bounds(scroll_layer);

  if (s_total_account_count == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "No accounts, showing empty state");
    prv_update_empty_state();
    return;
  }

  // Create array of visible views (for simplicity show all, but can be optimized in future)
  s_visible_count = s_total_account_count;
  s_visible_start = 0;

  s_account_views = calloc(s_visible_count, sizeof(AccountView));
  if (!s_account_views) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate account views");
    return;
  }

  int16_t y = 4;
  const int16_t width = bounds.size.w;

  s_container_layer = layer_create(GRect(0, 0, width, 4));
  scroll_layer_add_child(s_scroll_layer, s_container_layer);

  for (size_t i = 0; i < s_visible_count; i++) {
    size_t global_index = s_visible_start + i;
    AccountView *view = &s_account_views[i];
    view->id = global_index;
    memset(view->code, 0, sizeof(view->code));

    APP_LOG(APP_LOG_LEVEL_INFO, "Loading account %d", (int)global_index);

    // Try to load account
    if (!prv_load_account(view)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load account %d", (int)global_index);
      // If failed to load, skip
      continue;
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "Account %d loaded successfully: %s", (int)global_index, view->account->label);

    // Label (main label)
    GRect label_frame = GRect(4, y, width - 8, 20);
    view->label_layer = text_layer_create(label_frame);
    text_layer_set_font(view->label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_background_color(view->label_layer, GColorWhite);
    text_layer_set_text_color(view->label_layer, GColorBlack);
    text_layer_set_text(view->label_layer, view->account->label);
    text_layer_set_text_alignment(view->label_layer, GTextAlignmentLeft);
    layer_add_child(s_container_layer, text_layer_get_layer(view->label_layer));

    y += 20;

    // Account name (if present)
    if (view->account->account_name[0] != '\0') {
      GRect account_name_frame = GRect(4, y, width - 8, 16);
      view->account_name_layer = text_layer_create(account_name_frame);
      text_layer_set_font(view->account_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
      text_layer_set_background_color(view->account_name_layer, GColorWhite);
      text_layer_set_text_color(view->account_name_layer, GColorBlack);
      text_layer_set_text(view->account_name_layer, view->account->account_name);
      text_layer_set_text_alignment(view->account_name_layer, GTextAlignmentLeft);
      layer_add_child(s_container_layer, text_layer_get_layer(view->account_name_layer));
      y += 16;
    }

    // Code
    GRect code_frame = GRect(0, y, width, 34);
    view->code_layer = text_layer_create(code_frame);
    text_layer_set_font(view->code_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_background_color(view->code_layer, GColorWhite);
    text_layer_set_text_color(view->code_layer, GColorBlack);
    text_layer_set_text_alignment(view->code_layer, GTextAlignmentCenter);
    text_layer_set_text(view->code_layer, "INIT");
    APP_LOG(APP_LOG_LEVEL_INFO, "Initialized code_layer for account %d with 'INIT', layer=%p", (int)global_index, view->code_layer);
    layer_add_child(s_container_layer, text_layer_get_layer(view->code_layer));
    APP_LOG(APP_LOG_LEVEL_INFO, "Added code_layer to container for account %d", (int)global_index);

    y += 34;

    // Timer
    GRect detail_frame = GRect(4, y - 6, width - 8, 18);
    view->detail_layer = text_layer_create(detail_frame);
    text_layer_set_font(view->detail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_background_color(view->detail_layer, GColorWhite);
    text_layer_set_text_color(view->detail_layer, GColorBlack);
    text_layer_set_text_alignment(view->detail_layer, GTextAlignmentRight);
    text_layer_set_text(view->detail_layer, "");
    layer_add_child(s_container_layer, text_layer_get_layer(view->detail_layer));

    y += 24;
  }

  layer_set_frame(s_container_layer, GRect(0, 0, width, y));
  scroll_layer_set_content_size(s_scroll_layer, GSize(width, y));
  prv_update_empty_state();
}

void ui_update_codes(void) {
  if (!s_account_views || s_visible_count == 0) {
    return;
  }

  time_t now = time(NULL);
  for (size_t i = 0; i < s_visible_count; i++) {
    AccountView *view = &s_account_views[i];
    if (!view->account || !view->code_layer) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Account %d: account=%p, code_layer=%p", (int)i, view->account, view->code_layer);
      continue;
    }

    // APP_LOG(APP_LOG_LEVEL_INFO, "Generating TOTP for account %d: label='%s', secret_len=%d, digits=%d, period=%d, now=%ld",
    //          (int)i, view->account->label, (int)view->account->secret_len, (int)view->account->digits, (int)view->account->period, (long)now);

    uint32_t period_val = view->account->period > 0 ? view->account->period : DEFAULT_PERIOD;
    uint64_t counter = (uint64_t)(now / period_val);
    // APP_LOG(APP_LOG_LEVEL_INFO, "Calculated counter: %llu for period %d", counter, (int)period_val);

    if (!totp_generate(view->account, now, view->code, sizeof(view->code), NULL)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "TOTP generation failed for account %d", (int)i);
      text_layer_set_text(view->code_layer, "ERROR");
      continue;
    }

    //APP_LOG(APP_LOG_LEVEL_INFO, "Generated code: '%s' (len=%d)", view->code, (int)strlen(view->code));
    // Log each character in hex for debugging
    // for (size_t j = 0; j < strlen(view->code) && j < 10; j++) {
    //   APP_LOG(APP_LOG_LEVEL_INFO, "view->code[%d] = '%c' (0x%02x)", j, view->code[j], (unsigned char)view->code[j]);
    // }
    text_layer_set_text(view->code_layer, view->code);
    //APP_LOG(APP_LOG_LEVEL_INFO, "Text set to '%s' for account %d", view->code, (int)i);

    uint32_t period = view->account->period > 0 ? view->account->period : DEFAULT_PERIOD;
    uint32_t elapsed = (uint32_t)(now % period);
    uint32_t remaining = period - elapsed;
    if (remaining == 0) {
      remaining = period;
    }

    if (view->detail_layer) {
      static char detail_buffer[16];
      snprintf(detail_buffer, sizeof(detail_buffer), "%lus", (unsigned long)remaining);
      text_layer_set_text(view->detail_layer, detail_buffer);
    }
  }
}

void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  ui_update_codes();
}

// Set total account count
void ui_set_total_count(size_t count) {
  APP_LOG(APP_LOG_LEVEL_INFO, "ui_set_total_count called with %d", (int)count);
  s_total_account_count = count;
  APP_LOG(APP_LOG_LEVEL_INFO, "s_total_account_count set to %d", (int)s_total_account_count);
}

// Load account at index (for display)
void ui_load_account_at_index(size_t index) {
  if (!s_account_views || index >= s_visible_count) return;
  prv_load_account(&s_account_views[index]);
}

// Unload account at index (free memory)
void ui_unload_account_at_index(size_t index) {
  if (!s_account_views || index >= s_visible_count) return;
  prv_unload_account(&s_account_views[index]);
}

// Scroll handler (placeholder for now)
void ui_scroll_handler(ClickRecognizerRef recognizer, void *context) {
  // TODO: implement lazy loading on scroll
}

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
  text_layer_set_background_color(s_empty_layer, GColorWhite);
  text_layer_set_text_color(s_empty_layer, GColorBlack);
  text_layer_set_text(s_empty_layer, "Add accounts via\nthe phone settings.");
  layer_add_child(window_layer, text_layer_get_layer(s_empty_layer));

  ui_rebuild_scroll_content();
  ui_update_codes();
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

void ui_init(void) {
  // Initialize counters (don't reset s_total_account_count, it's already set)
  s_visible_start = 0;
  s_visible_count = 0;
  s_account_views = NULL;

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, ui_tick_handler);
}

void ui_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_window) {
    window_destroy(s_window);
  }
}
