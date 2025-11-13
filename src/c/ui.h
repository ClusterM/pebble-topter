#pragma once

#include <pebble.h>
#include "totp.h"

#define MAX_DIGITS 8

typedef struct {
  size_t id;  // ID аккаунта в storage
  TotpAccount *account;  // указатель на загруженный аккаунт (NULL если не загружен)
  char code[9];  // буфер для TOTP кода (макс 8 цифр + null)
  TextLayer *label_layer;
  TextLayer *account_name_layer;
  TextLayer *code_layer;
  TextLayer *detail_layer;
} AccountView;

// Глобальные переменные UI
extern Window *s_window;
extern ScrollLayer *s_scroll_layer;
extern Layer *s_container_layer;
extern TextLayer *s_empty_layer;
extern size_t s_total_account_count;  // общее количество аккаунтов
extern AccountView *s_account_views;  // динамический массив
extern size_t s_visible_start;  // индекс первого видимого аккаунта
extern size_t s_visible_count;  // количество видимых аккаунтов

// Инициализация UI
void ui_init(void);

// Деинициализация UI
void ui_deinit(void);

// Установить общее количество аккаунтов
void ui_set_total_count(size_t count);

// Загрузить аккаунт по индексу (для отображения)
void ui_load_account_at_index(size_t index);

// Выгрузить аккаунт по индексу (освободить память)
void ui_unload_account_at_index(size_t index);

// Обновление кодов для видимых аккаунтов
void ui_update_codes(void);

// Перестроение содержимого скролла
void ui_rebuild_scroll_content(void);

// Обработчик тика (обновление каждую секунду)
void ui_tick_handler(struct tm *tick_time, TimeUnits units_changed);

// Обработчик прокрутки
void ui_scroll_handler(ClickRecognizerRef recognizer, void *context);
