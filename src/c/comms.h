#pragma once

#include <pebble.h>
#include "totp.h"

// Инициализация связи с телефоном
void comms_init(void);

// Деинициализация связи
void comms_deinit(void);

// Отправка запроса на синхронизацию
void comms_request_sync(void);

// Функции отправки данных с часов на телефон не используются (односторонняя синхронизация)

// Парсинг количества аккаунтов
bool comms_parse_count(size_t count);

// Парсинг отдельного аккаунта
bool comms_parse_account(size_t id, const char *data);
