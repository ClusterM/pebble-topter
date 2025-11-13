#pragma once

#include "totp.h"

#define PERSIST_KEY_COUNT 0
#define PERSIST_KEY_ACCOUNTS_START 8

// Получить количество аккаунтов
size_t storage_get_count(void);

// Установить количество аккаунтов
void storage_set_count(size_t count);

// Загрузить аккаунт по ID
bool storage_load_account(size_t id, TotpAccount *account);

// Сохранить аккаунт по ID
bool storage_save_account(size_t id, const TotpAccount *account);

// Удалить аккаунт по ID
void storage_delete_account(size_t id);

// Загрузка всех аккаунтов (для обратной совместимости)
void storage_load_accounts(void);

// Сохранение всех аккаунтов (для обратной совместимости)
void storage_save_accounts(void);
