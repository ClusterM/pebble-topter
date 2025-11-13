#pragma once

#include <pebble.h>

typedef struct SelectionLayer SelectionLayer;

typedef struct {
  char* (*get_cell_text)(int index, void *context);
  void (*complete)(void *context);
  void (*increment)(int index, uint8_t clicks, void *context);
  void (*decrement)(int index, uint8_t clicks, void *context);
} SelectionLayerCallbacks;

SelectionLayer* selection_layer_create(GRect frame, int num_cells);
void selection_layer_destroy(SelectionLayer *selection_layer);
Layer* selection_layer_get_layer(SelectionLayer *selection_layer);
void selection_layer_set_cell_width(SelectionLayer *selection_layer, int index, int width);
void selection_layer_set_cell_padding(SelectionLayer *selection_layer, int padding);
void selection_layer_set_active_bg_color(SelectionLayer *selection_layer, GColor color);
void selection_layer_set_inactive_bg_color(SelectionLayer *selection_layer, GColor color);
void selection_layer_set_click_config_onto_window(SelectionLayer *selection_layer, Window *window);
void selection_layer_set_callbacks(SelectionLayer *selection_layer, void *context, SelectionLayerCallbacks callbacks);

