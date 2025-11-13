#include "settings_window.h"
#include "pin_window.h"
#include "storage.h"

#define MENU_SECTION_MAIN 0
#define MENU_ROW_PIN_STATUS 0
#define MENU_ROW_CHANGE_PIN 1
#define MENU_ROW_DISABLE_PIN 2

struct SettingsWindow {
  Window *window;
  MenuLayer *menu_layer;
  PinWindow *pin_window;
  bool setting_new_pin;
};

static SettingsWindow *s_settings_window = NULL;

// ============================================================================
// PIN window callbacks
// ============================================================================

static void prv_pin_setup_complete(Pin pin, void *context) {
  SettingsWindow *settings = (SettingsWindow*)context;
  
  if (settings->setting_new_pin) {
    // Save new PIN
    storage_set_pin(pin.digits[0], pin.digits[1], pin.digits[2]);
    pin_window_pop(settings->pin_window, true);
    
    // Show success feedback
    vibes_short_pulse();
    
    APP_LOG(APP_LOG_LEVEL_INFO, "PIN set successfully");
    
    // Reload menu to update status
    if (settings->menu_layer) {
      menu_layer_reload_data(settings->menu_layer);
    }
  }
}

// ============================================================================
// Menu layer callbacks
// ============================================================================

static uint16_t prv_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  bool has_pin = storage_has_pin();
  
  if (has_pin) {
    return 3;  // Status, Change PIN, Disable PIN
  } else {
    return 2;  // Status, Set PIN
  }
}

static int16_t prv_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  menu_cell_basic_header_draw(ctx, cell_layer, "PIN Settings");
}

static void prv_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  bool has_pin = storage_has_pin();
  bool pin_enabled = storage_is_pin_enabled();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_STATUS:
      if (has_pin) {
        menu_cell_basic_draw(ctx, cell_layer, "PIN Status", 
                            pin_enabled ? "Enabled" : "Disabled", NULL);
      } else {
        menu_cell_basic_draw(ctx, cell_layer, "PIN Status", "Not Set", NULL);
      }
      break;
      
    case MENU_ROW_CHANGE_PIN:
      if (has_pin) {
        menu_cell_basic_draw(ctx, cell_layer, "Change PIN", "Set new PIN code", NULL);
      } else {
        menu_cell_basic_draw(ctx, cell_layer, "Set PIN", "Create new PIN code", NULL);
      }
      break;
      
    case MENU_ROW_DISABLE_PIN:
      if (has_pin) {
        menu_cell_basic_draw(ctx, cell_layer, "Disable PIN", "Remove PIN protection", NULL);
      }
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  SettingsWindow *settings = (SettingsWindow*)data;
  bool has_pin = storage_has_pin();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_STATUS:
      // Toggle PIN enabled/disabled (only if PIN is set)
      if (has_pin) {
        bool current = storage_is_pin_enabled();
        storage_set_pin_enabled(!current);
        menu_layer_reload_data(menu_layer);
        vibes_short_pulse();
      }
      break;
      
    case MENU_ROW_CHANGE_PIN:
      // Show PIN entry window
      settings->setting_new_pin = true;
      
      if (!settings->pin_window) {
        settings->pin_window = pin_window_create((PinWindowCallbacks){
          .pin_complete = prv_pin_setup_complete
        }, settings);
      }
      
      if (settings->pin_window) {
        pin_window_reset(settings->pin_window);
        pin_window_set_highlight_color(settings->pin_window, GColorCobaltBlue);
        
        if (has_pin) {
          pin_window_set_main_text(settings->pin_window, "Change PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter new PIN");
        } else {
          pin_window_set_main_text(settings->pin_window, "Set PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter new PIN");
        }
        
        pin_window_push(settings->pin_window, true);
      }
      break;
      
    case MENU_ROW_DISABLE_PIN:
      if (has_pin) {
        // Clear PIN
        storage_clear_pin();
        menu_layer_reload_data(menu_layer);
        vibes_double_pulse();
        
        APP_LOG(APP_LOG_LEVEL_INFO, "PIN disabled");
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

