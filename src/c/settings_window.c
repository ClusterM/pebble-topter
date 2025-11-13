#include "settings_window.h"
#include "pin_window.h"
#include "storage.h"
#include "ui.h"

#define MENU_SECTION_MAIN 0
#define MENU_ROW_PIN_ACTION 0
#define MENU_ROW_STATUSBAR_TOGGLE 1

typedef enum {
  PIN_MODE_NONE,
  PIN_MODE_SET_FIRST,      // Setting PIN - first entry
  PIN_MODE_SET_CONFIRM,    // Setting PIN - confirmation
  PIN_MODE_DISABLE         // Disabling PIN - verification
} PinMode;

struct SettingsWindow {
  Window *window;
  MenuLayer *menu_layer;
  PinWindow *pin_window;
  PinMode current_mode;
  Pin first_pin;  // Store first PIN entry for confirmation
};

static SettingsWindow *s_settings_window = NULL;

// ============================================================================
// PIN window callbacks
// ============================================================================

static void prv_pin_setup_complete(Pin pin, void *context) {
  SettingsWindow *settings = (SettingsWindow*)context;
  
  switch (settings->current_mode) {
    case PIN_MODE_SET_FIRST:
      // First PIN entry - save and ask for confirmation
      settings->first_pin = pin;
      settings->current_mode = PIN_MODE_SET_CONFIRM;
      
      pin_window_reset(settings->pin_window);
      pin_window_set_main_text(settings->pin_window, "Confirm PIN");
      pin_window_set_sub_text(settings->pin_window, "Enter PIN again");
      break;
      
    case PIN_MODE_SET_CONFIRM:
      // Second PIN entry - verify match
      if (pin.digits[0] == settings->first_pin.digits[0] &&
          pin.digits[1] == settings->first_pin.digits[1] &&
          pin.digits[2] == settings->first_pin.digits[2]) {
        // PINs match - save it
        storage_set_pin(pin.digits[0], pin.digits[1], pin.digits[2]);
        pin_window_pop(settings->pin_window, true);
        
        vibes_short_pulse();
        APP_LOG(APP_LOG_LEVEL_INFO, "PIN set successfully");
        
        settings->current_mode = PIN_MODE_NONE;
        if (settings->menu_layer) {
          menu_layer_reload_data(settings->menu_layer);
        }
      } else {
        // PINs don't match - start over
        vibes_long_pulse();
        settings->current_mode = PIN_MODE_SET_FIRST;
        
        pin_window_reset(settings->pin_window);
        pin_window_set_main_text(settings->pin_window, "PIN Mismatch");
        pin_window_set_sub_text(settings->pin_window, "Try again");
        
        APP_LOG(APP_LOG_LEVEL_WARNING, "PIN confirmation failed");
      }
      break;
      
    case PIN_MODE_DISABLE:
      // Verify current PIN to disable
      if (storage_verify_pin(pin.digits[0], pin.digits[1], pin.digits[2])) {
        // Correct PIN - disable it
        storage_clear_pin();
        pin_window_pop(settings->pin_window, true);
        
        vibes_double_pulse();
        APP_LOG(APP_LOG_LEVEL_INFO, "PIN disabled");
        
        settings->current_mode = PIN_MODE_NONE;
        if (settings->menu_layer) {
          menu_layer_reload_data(settings->menu_layer);
        }
      } else {
        // Wrong PIN
        vibes_long_pulse();
        
        pin_window_reset(settings->pin_window);
        pin_window_set_main_text(settings->pin_window, "Wrong PIN");
        pin_window_set_sub_text(settings->pin_window, "Try again");
        
        APP_LOG(APP_LOG_LEVEL_WARNING, "PIN verification failed");
      }
      break;
      
    default:
      break;
  }
}

// ============================================================================
// Menu layer callbacks
// ============================================================================

static uint16_t prv_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return 2;  // PIN action and Status Bar toggle
}

static int16_t prv_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  menu_cell_basic_header_draw(ctx, cell_layer, "Settings");
}

static void prv_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  bool has_pin = storage_is_pin_enabled();
  bool statusbar_enabled = storage_is_statusbar_enabled();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_ACTION:
      if (has_pin) {
        menu_cell_basic_draw(ctx, cell_layer, "Disable PIN", "Enter PIN to remove", NULL);
      } else {
        menu_cell_basic_draw(ctx, cell_layer, "Set PIN", "Enter PIN twice", NULL);
      }
      break;
      
    case MENU_ROW_STATUSBAR_TOGGLE:
      menu_cell_basic_draw(ctx, cell_layer, "Status Bar", 
                          statusbar_enabled ? "Enabled" : "Disabled", NULL);
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  SettingsWindow *settings = (SettingsWindow*)data;
  bool has_pin = storage_has_pin();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_ACTION:
      // Create PIN window if it doesn't exist
      if (!settings->pin_window) {
        settings->pin_window = pin_window_create((PinWindowCallbacks){
          .pin_complete = prv_pin_setup_complete
        }, settings);
      }
      
      if (settings->pin_window) {
        pin_window_reset(settings->pin_window);
        
        if (has_pin) {
          // Disable PIN mode
          settings->current_mode = PIN_MODE_DISABLE;
          pin_window_set_main_text(settings->pin_window, "Disable PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter current PIN");
        } else {
          // Set PIN mode - first entry
          settings->current_mode = PIN_MODE_SET_FIRST;
          pin_window_set_main_text(settings->pin_window, "Set PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter new PIN");
        }
        
        pin_window_push(settings->pin_window, true);
      }
      break;
      
    case MENU_ROW_STATUSBAR_TOGGLE:
      // Toggle status bar setting
      {
        bool current = storage_is_statusbar_enabled();
        storage_set_statusbar_enabled(!current);
        
        // Reload menu to show new status
        menu_layer_reload_data(menu_layer);
        
        // Reload main window to apply changes
        ui_reload_window();
        
        vibes_short_pulse();
      }
      break;
  }
}

// ============================================================================
// Window lifecycle
// ============================================================================

static void prv_window_load(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create menu layer
  settings->menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(settings->menu_layer, settings, (MenuLayerCallbacks){
    .get_num_sections = prv_menu_get_num_sections_callback,
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_header_height = prv_menu_get_header_height_callback,
    .draw_header = prv_menu_draw_header_callback,
    .draw_row = prv_menu_draw_row_callback,
    .select_click = prv_menu_select_callback,
  });
  
  menu_layer_set_click_config_onto_window(settings->menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(settings->menu_layer));
}

static void prv_window_unload(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  
  if (settings->menu_layer) {
    menu_layer_destroy(settings->menu_layer);
    settings->menu_layer = NULL;
  }
  
  if (settings->pin_window) {
    pin_window_destroy(settings->pin_window);
    settings->pin_window = NULL;
  }
}

// ============================================================================
// Public API
// ============================================================================

SettingsWindow* settings_window_create(void) {
  if (s_settings_window) {
    return s_settings_window;
  }
  
  SettingsWindow *settings = malloc(sizeof(SettingsWindow));
  if (!settings) return NULL;
  
  memset(settings, 0, sizeof(SettingsWindow));
  
  settings->window = window_create();
  if (!settings->window) {
    free(settings);
    return NULL;
  }
  
  window_set_user_data(settings->window, settings);
  window_set_window_handlers(settings->window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  
  s_settings_window = settings;
  return settings;
}

void settings_window_destroy(SettingsWindow *settings_window) {
  if (!settings_window) return;
  
  if (settings_window->window) {
    window_destroy(settings_window->window);
  }
  
  if (s_settings_window == settings_window) {
    s_settings_window = NULL;
  }
  
  free(settings_window);
}

void settings_window_push(SettingsWindow *settings_window, bool animated) {
  if (settings_window && settings_window->window) {
    window_stack_push(settings_window->window, animated);
  }
}

void settings_window_pop(SettingsWindow *settings_window, bool animated) {
  if (settings_window && settings_window->window) {
    window_stack_remove(settings_window->window, animated);
  }
}

