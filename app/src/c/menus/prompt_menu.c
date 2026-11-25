/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "prompt_menu.h"
#include "../converse/session_window.h"
#include "../util/fonts.h"
#include "../util/round.h"
#include "../util/transition.h"
#include "../util/style.h"
#include "../util/memory/malloc.h"
#include "../util/memory/sdk.h"
#include <pebble.h>

typedef struct {
  MenuLayer *menu;
  char **items;
  int count;
} PromptMenuData;

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  PromptMenuData *data = context;
  return data->count;
}

static int16_t prv_cell_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return PBL_IF_ROUND_ELSE(58, 48);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  PromptMenuData *data = context;
  GRect bounds = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, data->items[index->row], fonts_get_config()->small_font,
      GRect(PBL_IF_ROUND_ELSE(12, 8), 3, bounds.size.w - PBL_IF_ROUND_ELSE(24, 16), bounds.size.h - 6),
      GTextOverflowModeWordWrap, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft), NULL);
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *context) {
  PromptMenuData *data = context;
  session_window_push(0, data->items[index->row]);
}

static void prv_appear(Window *window) {
  PromptMenuData *data = window_get_user_data(window);
  mica_animate_window_push(window);
  menu_layer_set_normal_colors(data->menu, mica_page_color(), GColorBlack);
}

static void prv_load(Window *window) {
  PromptMenuData *data = window_get_user_data(window);
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  data->menu = bmenu_layer_create(bounds);
  menu_layer_set_callbacks(data->menu, data, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_cell_height = prv_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  menu_layer_set_highlight_colors(data->menu, SELECTION_HIGHLIGHT_COLOUR,
      gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));
  mica_round_menu(data->menu);
  menu_layer_set_click_config_onto_window(data->menu, window);
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(data->menu));
}

static void prv_unload(Window *window) {
  PromptMenuData *data = window_get_user_data(window);
  menu_layer_destroy(data->menu);
  free(data->items[0]);
  free(data->items);
  free(data);
  window_destroy(window);
}

void prompt_menu_push(char **items, int count) {
  Window *window = bwindow_create();
  PromptMenuData *data = bmalloc(sizeof(PromptMenuData));
  data->items = items;
  data->count = count;
  data->menu = NULL;
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_load,
    .appear = prv_appear,
    .unload = prv_unload,
  });
  window_stack_push(window, false);
}
