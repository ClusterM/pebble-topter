#pragma once

#include "totp.h"

#define PERSIST_KEY_ACCOUNTS 1

// Загрузка аккаунтов из постоянной памяти
void storage_load_accounts(void);

// Сохранение аккаунтов в постоянную память
void storage_save_accounts(void);
