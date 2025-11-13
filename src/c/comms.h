#pragma once

#include <pebble.h>

// Инициализация связи с телефоном
void comms_init(void);

// Деинициализация связи
void comms_deinit(void);

// Отправка запроса на синхронизацию
void comms_request_sync(void);

// Парсинг и применение конфигурации аккаунтов
bool comms_parse_payload(const char *payload);
