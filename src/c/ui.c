#include "ui.h"
#include "totp.h"
#include "storage.h"
#include "settings_window.h"
#include <string.h>

// UI global variables
Window *s_window;
MenuLayer *s_menu_layer;
TextLayer *s_empty_layer;
size_t s_total_account_count;
AccountCache *s_account_cache;
static bool s_is_loading = false;
static SettingsWindow *s_settings_window = NULL;

// ============================================================================
// Account loading and caching
// ============================================================================

static void prv_load_account(size_t index) {
  if (index >= s_total_account_count) return;
  
  AccountCache *cache = &s_account_cache[index];
  if (cache->account) return;
  
  cache->account = malloc(sizeof(TotpAccount));
  if (!cache->account) {
    return;
  }
  
  if (!storage_load_account(index, cache->account)) {
    free(cache->account);
    cache->account = NULL;
    return;
  }
  
  cache->code_valid = false;
  memset(cache->code, 0, sizeof(cache->code));
}

static void prv_free_account_cache(void) {
  if (!s_account_cache) return;
  
  for (size_t i = 0; i < s_total_account_count; i++) {
    if (s_account_cache[i].account) {
      free(s_account_cache[i].account);
      s_account_cache[i].account = NULL;
    }
  }
  
  free(s_account_cache);
  s_account_cache = NULL;
}

static void prv_init_account_cache(void) {
  prv_free_account_cache();
  
  if (s_total_account_count == 0) return;
  
  s_account_cache = calloc(s_total_account_count, sizeof(AccountCache));
  if (!s_account_cache) {
    return;
  }
  
  for (size_t i = 0; i < s_total_account_count; i++) {
    prv_load_account(i);
  }
}

// ============================================================================
// MenuLayer callbacks
// ============================================================================

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  (void)menu_layer;
  (void)section_index;
  (void)data;
  return s_total_account_count;
}

static int16_t prv_menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  (void)menu_layer;
  (void)data;
  
  AccountCache *cache = &s_account_cache[cell_index->row];
  
  // Calculate height based on content
  int16_t height = 0 + 15; // top padding + label
  
  if (cache->account &&cache->account->account_name[0] != '\0') {
    height += 10; // account name
  }
  
  height += 35; // code
  height += 1; // time remaining
  height += 0; // bottom padding
  
  return height;
}

static void prv_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  (void)data;
  
  if (cell_index->row >= s_total_account_count) return;
  
  AccountCache *cache = &s_account_cache[cell_index->row];
  if (!cache->account) {
    menu_cell_basic_draw(ctx, cell_layer, "Error", "Failed to load", NULL);
    return;
  }
  
  GRect bounds = layer_get_bounds(cell_layer);
  int16_t y = 0;
  
  // Always use black text (no highlight visual feedback needed)
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx,
                    cache->account->label,
                    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                    GRect(4, y, bounds.size.w - 8, 20),
                    GTextOverflowModeTrailingEllipsis,
                    GTextAlignmentLeft,
                    NULL);
  y += 15;
  
  // Draw account name (if present)
  if (cache->account->account_name[0] != '\0') {
    graphics_draw_text(ctx,
                      cache->account->account_name,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14),
                      GRect(4, y, bounds.size.w - 8, 16),
                      GTextOverflowModeTrailingEllipsis,
                      GTextAlignmentLeft,
                      NULL);
    y += 10;
  }
  
  // Draw TOTP code (large, centered)
  const char *code_text = cache->code_valid ? cache->code : "------";
  graphics_draw_text(ctx,
                    code_text,
                    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                    GRect(0, y, bounds.size.w, 34),
                    GTextOverflowModeTrailingEllipsis,
                    GTextAlignmentCenter,
                    NULL);
  y += 35;

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, GPoint(0, y), GPoint(cache->remaining * bounds.size.w / cache->account->period, y));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(0, y), GPoint(bounds.size.w, y));
}

// ============================================================================
// Code generation and updates
// ============================================================================

void ui_update_codes(void) {
  if (!s_account_cache || s_total_account_count == 0) return;
  
  time_t now = time(NULL);
  bool needs_redraw = false;
  bool needs_vibe = false;
  
  for (size_t i = 0; i < s_total_account_count; i++) {
    AccountCache *cache = &s_account_cache[i];
    if (!cache->account) continue;
    
    if (!totp_generate(cache->account, now, cache->code, sizeof(cache->code), NULL)) {
      snprintf(cache->code, sizeof(cache->code), "ERROR");
      cache->code_valid = false;
    } else {
      cache->code_valid = true;
    }
    
    // Calculate time remaining
    uint32_t period = cache->account->period > 0 ? cache->account->period : DEFAULT_PERIOD;
    uint32_t elapsed = (uint32_t)(now % period);
    cache->remaining = period - elapsed;
    if (cache->remaining == 0) {
      cache->remaining = period;
      needs_vibe = true;
    }    
    if (cache->remaining <= 2 || cache->remaining == period) {
      needs_vibe = true;
    }

    needs_redraw = true;
  }
  
  if (needs_redraw && s_menu_layer) {
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }

  if (needs_vibe) {
    uint32_t const segments[] = { 50 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };    
    vibes_enqueue_custom_pattern(pat);
  }
}

void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  ui_update_codes();
}

// ============================================================================
// Empty state management
// ============================================================================

static void prv_update_empty_state(void) {
  if (!s_empty_layer || !s_menu_layer) return;
  
  bool has_accounts = s_total_account_count > 0;
  
  // Update empty layer text based on loading state
  char const *text = "";
  if (!has_accounts) {
    if (s_is_loading) {
      text = "Loading...";
    } else {
      text = "No accounts.\nAdd on the phone.";
    }
  }

  Layer *layer = text_layer_get_layer(s_empty_layer);

  text_layer_set_text_alignment(s_empty_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(s_empty_layer, GColorWhite);
  text_layer_set_text_color(s_empty_layer, GColorBlack);
  text_layer_set_text(s_empty_layer, text);

  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  GSize size = graphics_text_layout_get_content_size(text, 
    fonts_get_system_font(FONT_KEY_GOTHIC_18), 
    GRect(0, 0, bounds.size.w, bounds.size.h), 
    GTextOverflowModeTrailingEllipsis, 
    GTextAlignmentCenter);
  layer_set_frame(layer, GRect(0, bounds.size.h / 2 - size.h / 2, bounds.size.w, size.h));  
  
  layer_set_hidden(layer, has_accounts);
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), !has_accounts);
}

// ============================================================================
// Public API
// ============================================================================

void ui_set_total_count(size_t count) {
  s_total_account_count = count;
  s_is_loading = false;
  prv_init_account_cache();
  
  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
  
  prv_update_empty_state();
  ui_update_codes();
}

void ui_set_loading(bool loading) {
  s_is_loading = loading;
  prv_update_empty_state();
}

void ui_reload_data(void) {
  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
}

// ============================================================================
// Click handlers
// ============================================================================

static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Open settings window
  if (!s_settings_window) {
    s_settings_window = settings_window_create();
  }
  
  if (s_settings_window) {
    settings_window_push(s_settings_window, true);
  }
}

static void prv_click_config_provider(void *context) {
  // Set up menu layer click config
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_cell_height = prv_menu_get_cell_height_callback,
    .draw_row = prv_menu_draw_row_callback,
  });
  
  // Add long press for settings
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click_handler, NULL);
}

// ============================================================================
// Window lifecycle
// ============================================================================

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create menu layer
  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_cell_height = prv_menu_get_cell_height_callback,
    .draw_row = prv_menu_draw_row_callback,
  });
  
  // Disable highlight by making it the same color as background
  menu_layer_set_highlight_colors(s_menu_layer, GColorWhite, GColorBlack);
  
  // Set up custom click config provider instead of using menu layer's default
  window_set_click_config_provider_with_context(window, prv_click_config_provider, NULL);
  
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
  
  // Create empty state label
  s_empty_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(window_layer, text_layer_get_layer(s_empty_layer));
  
  prv_update_empty_state();
  ui_update_codes();
}

static void prv_window_unload(Window *window) {
  if (s_menu_layer) {
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  
  if (s_empty_layer) {
    text_layer_destroy(s_empty_layer);
    s_empty_layer = NULL;
  }
  
  prv_free_account_cache();
}

// ============================================================================
// UI initialization and cleanup
// ============================================================================

void ui_init(void) {
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
  
  if (s_settings_window) {
    settings_window_destroy(s_settings_window);
    s_settings_window = NULL;
  }
  
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}



