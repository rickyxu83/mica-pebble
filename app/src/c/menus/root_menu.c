/*
 * Copyright 2025 Google LLC
 * Modified in 2026 for Mica.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "root_menu.h"
#include "quota_window.h"
#include "alarm_menu.h"
#include "about_window.h"
#include "legal_window.h"
#include "reminders_menu.h"
#include "feedback_window.h"
#include "../util/style.h"
#include "../util/round.h"
#include "../util/transition.h"
#include "../util/i18n.h"
#include "../util/fonts.h"
#include "../util/memory/malloc.h"
#include "../util/memory/sdk.h"
#include "../util/logging.h"
#include <pebble.h>

static void prv_window_load(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_unload(Window* window);
static void prv_push_quota_screen(int index, void* context);
static void prv_push_alarm_screen(int index, void* context);
static void prv_push_timer_screen(int index, void* context);
static void prv_push_about_screen(int index, void* context);
static void prv_push_legal_screen(int index, void* context);
static void prv_push_reminders_screen(int index, void* context);
static void prv_push_feedback_screen(int index, void* context);
static void prv_push_ai_test(int index, void* context);

typedef struct {
  const char *title;
  GBitmap *icon;
  void (*callback)(int index, void *context);
} MicaMenuItem;

static MicaMenuItem s_items[8];

typedef struct {
  MenuLayer *menu_layer;
  StatusBarLayer *status_bar;
} RootMenuWindowData;

void root_menu_window_push() {
  Window* window = bwindow_create();
  RootMenuWindowData* data = bmalloc(sizeof(RootMenuWindowData));
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}

static uint16_t prv_num_rows(MenuLayer *menu_layer, uint16_t section, void *context) {
  return 8;
}

static int16_t prv_cell_height(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  return i18n_is_chinese() ? PBL_IF_ROUND_ELSE(66, 58) : PBL_IF_ROUND_ELSE(52, 44);
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *context) {
  MicaMenuItem *item = &s_items[index->row];
  if (!i18n_is_chinese()) {
    menu_cell_basic_draw(ctx, cell_layer, item->title, NULL, item->icon);
    return;
  }

  GRect bounds = layer_get_bounds(cell_layer);
  GRect icon_bounds = gbitmap_get_bounds(item->icon);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, item->icon,
      GRect((bounds.size.w - icon_bounds.size.w) / 2, 4,
            icon_bounds.size.w, icon_bounds.size.h));
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, item->title, fonts_get_config()->small_font,
      GRect(4, icon_bounds.size.h + 7, bounds.size.w - 8,
            bounds.size.h - icon_bounds.size.h - 9),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_select(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  s_items[index->row].callback(index->row, context);
}

static void prv_window_load(Window* window) {
  if (!s_items[0].title) {
    s_items[0] = (MicaMenuItem) { S(STR_ALARMS), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_ALARMS), prv_push_alarm_screen };
    s_items[1] = (MicaMenuItem) { S(STR_TIMERS), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_TIMERS), prv_push_timer_screen };
    s_items[2] = (MicaMenuItem) { S(STR_REMINDERS), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_REMINDERS), prv_push_reminders_screen };
    s_items[3] = (MicaMenuItem) { S(STR_QUOTA), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_QUOTA), prv_push_quota_screen };
    s_items[4] = (MicaMenuItem) { S(STR_FEEDBACK), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_FEEDBACK), prv_push_feedback_screen };
    s_items[5] = (MicaMenuItem) { S(STR_ABOUT), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_ABOUT), prv_push_about_screen };
    s_items[6] = (MicaMenuItem) { S(STR_LEGAL), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_LEGAL), prv_push_legal_screen };
    s_items[7] = (MicaMenuItem) { S(STR_TEST_CONNECTION), bgbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_QUOTA), prv_push_ai_test };
  }

  RootMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  data->status_bar = bstatus_bar_layer_create();
  bobby_status_bar_config(data->status_bar);
  layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));
  data->menu_layer = bmenu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT,
      window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  menu_layer_set_callbacks(data->menu_layer, window, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_cell_height = prv_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  menu_layer_set_highlight_colors(data->menu_layer, SELECTION_HIGHLIGHT_COLOUR,
      gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));
  mica_round_menu(data->menu_layer);
  menu_layer_set_click_config_onto_window(data->menu_layer, window);
  layer_add_child(root_layer, menu_layer_get_layer(data->menu_layer));
}

static void prv_window_appear(Window* window) {
  RootMenuWindowData* data = window_get_user_data(window);
  mica_page_apply_background(window);
  mica_page_status_bar_colors(data->status_bar);
  menu_layer_set_normal_colors(data->menu_layer, mica_page_color(), GColorBlack);
}

static void prv_window_unload(Window* window) {
  RootMenuWindowData* data = window_get_user_data(window);
  menu_layer_destroy(data->menu_layer);
  status_bar_layer_destroy(data->status_bar);
  for (uint32_t i = 0; i < 8; i++) {
    if (s_items[i].icon) {
      gbitmap_destroy(s_items[i].icon);
      s_items[i].icon = NULL;
    }
    s_items[i].title = NULL;
  }
  free(data);
  window_destroy(window);
}

static void prv_push_quota_screen(int index, void* context) { push_quota_window(); }
static void prv_push_alarm_screen(int index, void* context) { alarm_menu_window_push(false); }
static void prv_push_timer_screen(int index, void* context) { alarm_menu_window_push(true); }
static void prv_push_legal_screen(int index, void* context) { legal_window_push(); }

static void prv_push_ai_test(int index, void* context) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "AI test request failed: %d", result);
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_AI_TEST_REQUEST, 1);
  app_message_outbox_send();
}
static void prv_push_reminders_screen(int index, void* context) { reminders_menu_push(); }
static void prv_push_feedback_screen(int index, void* context) { feedback_window_push(); }
static void prv_push_about_screen(int index, void* context) { about_window_push(); }
