#include <pebble.h>
#include "ui.h"
#include "storage.h"
#include "comms.h"

// === App lifecycle ==========================================================

static void prv_init(void) {
  storage_load_accounts();
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