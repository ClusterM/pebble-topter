#pragma once

#include <pebble.h>
#include "selection_layer.h"

#define PIN_WINDOW_NUM_CELLS 3
#define PIN_WINDOW_MAX_VALUE 9

typedef struct {
  int digits[PIN_WINDOW_NUM_CELLS];
} Pin;

typedef struct {
  void (*pin_complete)(Pin pin, void *context);
} PinWindowCallbacks;

typedef struct {
  Window *window;
  TextLayer *main_text;
  TextLayer *sub_text;
  SelectionLayer *selection;
  StatusBarLayer *status;
  Pin pin;
  char field_buffs[PIN_WINDOW_NUM_CELLS][2];
  int field_selection;
  GColor highlight_color;
  PinWindowCallbacks callbacks;
  void *callback_context;
} PinWindow;

// Create PIN window
PinWindow* pin_window_create(PinWindowCallbacks callbacks, void *context);

// Destroy PIN window
void pin_window_destroy(PinWindow *pin_window);

// Push PIN window onto stack
void pin_window_push(PinWindow *pin_window, bool animated);

// Pop PIN window from stack
void pin_window_pop(PinWindow *pin_window, bool animated);

// Check if PIN window is topmost
bool pin_window_get_topmost_window(PinWindow *pin_window);

// Set highlight color
void pin_window_set_highlight_color(PinWindow *pin_window, GColor color);

// Set main text
void pin_window_set_main_text(PinWindow *pin_window, const char *text);

// Set sub text
void pin_window_set_sub_text(PinWindow *pin_window, const char *text);

// Reset PIN input
void pin_window_reset(PinWindow *pin_window);

