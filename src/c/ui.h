#pragma once

#include <pebble.h>
#include "totp.h"

#define MAX_DIGITS 8

typedef struct {
  size_t id;  // Account ID in storage
  TotpAccount *account;  // Pointer to loaded account (NULL if not loaded)
  char code[9];  // Buffer for TOTP code (max 8 digits + null)
  TextLayer *label_layer;
  TextLayer *account_name_layer;
  TextLayer *code_layer;
  TextLayer *detail_layer;
} AccountView;

// UI global variables
extern Window *s_window;
extern ScrollLayer *s_scroll_layer;
extern Layer *s_container_layer;
extern TextLayer *s_empty_layer;
extern size_t s_total_account_count;  // total account count
extern AccountView *s_account_views;  // dynamic array
extern size_t s_visible_start;  // index of first visible account
extern size_t s_visible_count;  // number of visible accounts

// UI initialization
void ui_init(void);

// UI deinitialization
void ui_deinit(void);

// Set total account count
void ui_set_total_count(size_t count);

// Load account at index (for display)
void ui_load_account_at_index(size_t index);

// Unload account at index (free memory)
void ui_unload_account_at_index(size_t index);

// Update codes for visible accounts
void ui_update_codes(void);

// Rebuild scroll content
void ui_rebuild_scroll_content(void);

// Tick handler (update every second)
void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed);

// Scroll handler
void ui_scroll_handler(ClickRecognizerRef recognizer, void *context);
