#include "selection_layer.h"

typedef struct {
  int index;
  int width;
} CellInfo;

struct SelectionLayer {
  Layer *layer;
  int num_cells;
  int active_cell;
  CellInfo *cells;
  int cell_padding;
  GColor active_bg_color;
  GColor inactive_bg_color;
  SelectionLayerCallbacks callbacks;
  void *callback_context;
  Window *window;
};

// Global reference to current selection layer for update proc
static SelectionLayer *s_current_selection_layer = NULL;

static void prv_draw_cell(SelectionLayer *selection_layer, GContext *ctx, int index) {
  if (index >= selection_layer->num_cells) return;
  
  GRect bounds = layer_get_bounds(selection_layer->layer);
  
  // Calculate cell position
  int cell_x = 0;
  for (int i = 0; i < index; i++) {
    cell_x += selection_layer->cells[i].width + selection_layer->cell_padding;
  }
  
  int cell_width = selection_layer->cells[index].width;
  int cell_height = bounds.size.h;
  
  // Draw cell background
  GColor bg_color = (index == selection_layer->active_cell) ? 
                    selection_layer->active_bg_color : 
                    selection_layer->inactive_bg_color;
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, GRect(cell_x, 0, cell_width, cell_height), 4, GCornersAll);
  
  // Draw cell text
  if (selection_layer->callbacks.get_cell_text) {
    char *text = selection_layer->callbacks.get_cell_text(index, selection_layer->callback_context);
    if (text) {
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx,
                        text,
                        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                        GRect(cell_x, (cell_height - 28) / 2, cell_width, 28),
                        GTextOverflowModeTrailingEllipsis,
                        GTextAlignmentCenter,
                        NULL);
    }
  }
}

static void prv_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_current_selection_layer) return;
  
  for (int i = 0; i < s_current_selection_layer->num_cells; i++) {
    prv_draw_cell(s_current_selection_layer, ctx, i);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  SelectionLayer *selection_layer = (SelectionLayer*)context;
  
  selection_layer->active_cell++;
  if (selection_layer->active_cell >= selection_layer->num_cells) {
    selection_layer->active_cell = 0;
    if (selection_layer->callbacks.complete) {
      selection_layer->callbacks.complete(selection_layer->callback_context);
    }
    return;
  }
  
  layer_mark_dirty(selection_layer->layer);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  SelectionLayer *selection_layer = (SelectionLayer*)context;
  
  if (selection_layer->callbacks.increment) {
    selection_layer->callbacks.increment(selection_layer->active_cell, 1, selection_layer->callback_context);
  }
  
  layer_mark_dirty(selection_layer->layer);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  SelectionLayer *selection_layer = (SelectionLayer*)context;
  
  if (selection_layer->callbacks.decrement) {
    selection_layer->callbacks.decrement(selection_layer->active_cell, 1, selection_layer->callback_context);
  }
  
  layer_mark_dirty(selection_layer->layer);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

SelectionLayer* selection_layer_create(GRect frame, int num_cells) {
  SelectionLayer *selection_layer = malloc(sizeof(SelectionLayer));
  if (!selection_layer) return NULL;
  
  selection_layer->layer = layer_create(frame);
  if (!selection_layer->layer) {
    free(selection_layer);
    return NULL;
  }
  
  layer_set_update_proc(selection_layer->layer, prv_layer_update_proc);
  
  selection_layer->num_cells = num_cells;
  selection_layer->active_cell = 0;
  selection_layer->cell_padding = 0;
  selection_layer->active_bg_color = GColorBlack;
  selection_layer->inactive_bg_color = GColorDarkGray;
  selection_layer->callback_context = NULL;
  selection_layer->window = NULL;
  
  selection_layer->cells = calloc(num_cells, sizeof(CellInfo));
  if (!selection_layer->cells) {
    layer_destroy(selection_layer->layer);
    free(selection_layer);
    return NULL;
  }
  
  for (int i = 0; i < num_cells; i++) {
    selection_layer->cells[i].index = i;
    selection_layer->cells[i].width = 30;
  }
  
  memset(&selection_layer->callbacks, 0, sizeof(SelectionLayerCallbacks));
  
  // Set as current for update proc
  s_current_selection_layer = selection_layer;
  
  return selection_layer;
}

void selection_layer_destroy(SelectionLayer *selection_layer) {
  if (!selection_layer) return;
  
  if (s_current_selection_layer == selection_layer) {
    s_current_selection_layer = NULL;
  }
  
  if (selection_layer->cells) {
    free(selection_layer->cells);
  }
  
  if (selection_layer->layer) {
    layer_destroy(selection_layer->layer);
  }
  
  free(selection_layer);
}

Layer* selection_layer_get_layer(SelectionLayer *selection_layer) {
  return selection_layer ? selection_layer->layer : NULL;
}

void selection_layer_set_cell_width(SelectionLayer *selection_layer, int index, int width) {
  if (!selection_layer || index >= selection_layer->num_cells) return;
  selection_layer->cells[index].width = width;
}

void selection_layer_set_cell_padding(SelectionLayer *selection_layer, int padding) {
  if (!selection_layer) return;
  selection_layer->cell_padding = padding;
}

void selection_layer_set_active_bg_color(SelectionLayer *selection_layer, GColor color) {
  if (!selection_layer) return;
  selection_layer->active_bg_color = color;
}

void selection_layer_set_inactive_bg_color(SelectionLayer *selection_layer, GColor color) {
  if (!selection_layer) return;
  selection_layer->inactive_bg_color = color;
}

void selection_layer_set_click_config_onto_window(SelectionLayer *selection_layer, Window *window) {
  if (!selection_layer || !window) return;
  selection_layer->window = window;
  window_set_click_config_provider_with_context(window, prv_click_config_provider, selection_layer);
}

void selection_layer_set_callbacks(SelectionLayer *selection_layer, void *context, SelectionLayerCallbacks callbacks) {
  if (!selection_layer) return;
  selection_layer->callback_context = context;
  selection_layer->callbacks = callbacks;
}

