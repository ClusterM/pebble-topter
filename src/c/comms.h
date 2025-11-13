#pragma once

#include <pebble.h>
#include "totp.h"

// Initialize phone communication
void comms_init(void);

// Deinitialize communication
void comms_deinit(void);

// Send sync request
void comms_request_sync(void);

// Data sending functions from watch to phone are not used (unidirectional sync)

// Parse account count
bool comms_parse_count(size_t count);

// Parse individual account
bool comms_parse_account(size_t id, const char *data);
