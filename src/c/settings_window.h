#pragma once

#include <pebble.h>

typedef struct SettingsWindow SettingsWindow;

// Create settings window
SettingsWindow* settings_window_create(void);

// Destroy settings window
void settings_window_destroy(SettingsWindow *settings_window);

// Push settings window
void settings_window_push(SettingsWindow *settings_window, bool animated);

// Pop settings window
void settings_window_pop(SettingsWindow *settings_window, bool animated);

