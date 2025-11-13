#include <pebble.h>
#include "ui.h"
#include "storage.h"
#include "comms.h"
#include "config.h"

// === App lifecycle ==========================================================

static void prv_init(void) {
#ifdef DEBUG
  APP_LOG(APP_LOG_LEVEL_WARNING, "========================================");
  APP_LOG(APP_LOG_LEVEL_WARNING, "DEBUG MODE IS ENABLED!");
  APP_LOG(APP_LOG_LEVEL_WARNING, "Using fake TOTP accounts for testing");
  APP_LOG(APP_LOG_LEVEL_WARNING, "========================================");
#endif
  
  // Load account count
  storage_load_accounts();
  ui_set_total_count(s_total_account_count);

  ui_init();
  comms_init();
}

static void prv_deinit(void) {
  comms_deinit();
  ui_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}