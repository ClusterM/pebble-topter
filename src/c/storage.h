#pragma once

#include "totp.h"

#define PERSIST_KEY_COUNT 0
#define PERSIST_KEY_ACCOUNTS_START 8

// Get account count
size_t storage_get_count(void);

// Set account count
void storage_set_count(size_t count);

// Load account by ID
bool storage_load_account(size_t id, TotpAccount *account);

// Save account by ID
bool storage_save_account(size_t id, const TotpAccount *account);

// Delete account by ID
void storage_delete_account(size_t id);

// Load account count from storage
void storage_load_accounts(void);

