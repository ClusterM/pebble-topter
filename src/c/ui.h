#pragma once

#include <pebble.h>
#include "totp.h"

#define MAX_DIGITS 8

typedef struct {
  TotpAccount *account;  // Pointer to loaded account (NULL if not loaded)
  char code[9];  // Buffer for TOTP code (max 8 digits + null)
  uint32_t remaining;
  bool code_valid;  // Whether code has been generated
} AccountCache;

// UI global variables
extern Window *s_window;
extern MenuLayer *s_menu_layer;
extern TextLayer *s_empty_layer;
extern size_t s_total_account_count;
extern AccountCache *s_account_cache;  // dynamic array of cached account data

// UI initialization
void ui_init(void);

// UI deinitialization
void ui_deinit(void);

// Set total account count and reload menu
void ui_set_total_count(size_t count);

// Set loading state
void ui_set_loading(bool loading);

// Update codes for all accounts
void ui_update_codes(void);

// Reload menu data
void ui_reload_data(void);

// Reload window (to apply settings changes like status bar)
void ui_reload_window(void);

// Tick handler (update every second)
void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed);
