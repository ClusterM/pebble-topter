#pragma once

#include <pebble.h>
#include "totp.h"

#define MAX_DIGITS 8

typedef struct {
  TextLayer *name_layer;
  TextLayer *code_layer;
  TextLayer *detail_layer;
} AccountView;

// Глобальные переменные UI
extern Window *s_window;
extern ScrollLayer *s_scroll_layer;
extern Layer *s_container_layer;
extern TextLayer *s_empty_layer;
extern TotpAccount s_accounts[MAX_ACCOUNTS];
extern size_t s_account_count;
extern AccountView s_account_views[MAX_ACCOUNTS];
extern char s_code_cache[MAX_ACCOUNTS][MAX_DIGITS + 2];
extern uint64_t s_counter_cache[MAX_ACCOUNTS];

// Инициализация UI
void ui_init(void);

// Деинициализация UI
void ui_deinit(void);

// Обновление кодов (force=true для полной перестройки)
void ui_update_codes(bool force);

// Перестроение содержимого скролла
void ui_rebuild_scroll_content(void);

// Обработчик тика (обновление каждую секунду)
void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed);
