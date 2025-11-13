#include "ui.h"
#include "totp.h"
#include <string.h>

// Глобальные переменные UI
Window *s_window;
ScrollLayer *s_scroll_layer;
Layer *s_container_layer;
TextLayer *s_empty_layer;
TotpAccount s_accounts[MAX_ACCOUNTS];
size_t s_account_count;
AccountView s_account_views[MAX_ACCOUNTS];
char s_code_cache[MAX_ACCOUNTS][MAX_DIGITS + 2];
uint64_t s_counter_cache[MAX_ACCOUNTS];

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

void ui_rebuild_scroll_content(void) {
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

void ui_update_codes(bool force) {
  if (s_account_count == 0) {
    return;
  }
  time_t now = time(NULL);
  for (size_t i = 0; i < s_account_count; i++) {
    uint64_t counter = 0;
    char code_buffer[MAX_DIGITS + 2];
    if (!totp_generate(&s_accounts[i], now, code_buffer, sizeof(code_buffer), &counter)) {
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

void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  ui_update_codes(false);
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
  text_layer_set_background_color(s_empty_layer, GColorClear);
  text_layer_set_text_color(s_empty_layer, GColorWhite);
  text_layer_set_text(s_empty_layer, "Add accounts via\nthe phone settings.");
  layer_add_child(window_layer, text_layer_get_layer(s_empty_layer));

  ui_rebuild_scroll_content();
  ui_update_codes(true);
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
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
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
