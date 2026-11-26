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

#include "alarm_menu.h"
#include "../alarms/manager.h"
#include "../util/face_layer.h"
#include "../util/fonts.h"
#include "../util/style.h"
#include "../util/round.h"
#include "../util/transition.h"
#include "../util/i18n.h"
#include "../util/time.h"
#include "../util/vector_layer.h"
#include "../util/memory/malloc.h"
#include "../util/memory/sdk.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

// Custom page header replacing the firmware status bar (whose time
// text is tiny and not resizable): a 36 px time line in
// GOTHIC_28_BOLD over a 1 px separator; content starts below both.
#define MENU_HEADER_HEIGHT 36
#define MENU_CONTENT_TOP (MENU_HEADER_HEIGHT + 1)

typedef struct {
  MenuLayer *menu_layer;
  TextLayer *header_layer;
  Layer *header_separator;
  TextLayer *empty_text_layer;
  bool for_timers;
  EventHandle tick_handle;
  EventHandle header_tick_handle;
  char header_time[8];
  FaceLayer *empty_face_layer;
} AlarmMenuWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_disappear(Window* window);
static uint16_t prv_get_num_rows(struct MenuLayer* menu_layer, uint16_t section_index, void* context);
static void prv_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* context);
static void prv_select_click(struct MenuLayer* menu_layer, MenuIndex* cell_index, void* context);
static void prv_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context);
static void prv_header_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context);
static void prv_header_create(Window *window);
static void prv_header_update_time(AlarmMenuWindowData *data);
static void prv_header_apply_colors(AlarmMenuWindowData *data, GColor background, GColor foreground);
static void prv_cancel_alarm(ActionMenu* layer, const ActionMenuItem* item, void* context);
static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context);
static void prv_show_empty(Window *window);

void alarm_menu_window_push(bool for_timers) {
  AlarmMenuWindowData *data = bmalloc(sizeof(AlarmMenuWindowData));
  memset(data, 0, sizeof(*data));
  data->for_timers = for_timers;

  Window *window = bwindow_create();
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
    .appear = prv_window_appear,
    .disappear = prv_window_disappear,
  });
  mica_animate_window_push(window);
}

static void prv_window_load(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  const FontsConfig *fonts = fonts_get_config();
  data->menu_layer = bmenu_layer_create(GRect(0, MENU_CONTENT_TOP, window_bounds.size.w, window_bounds.size.h - MENU_CONTENT_TOP));
  menu_layer_set_highlight_colors(data->menu_layer, SELECTION_HIGHLIGHT_COLOUR, gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));
  mica_round_menu(data->menu_layer);
  menu_layer_set_callbacks(data->menu_layer, window, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  data->empty_face_layer = NULL;
  prv_header_create(window);
  data->header_tick_handle = events_tick_timer_service_subscribe_context(MINUTE_UNIT, prv_header_tick_handler, window);
  data->empty_text_layer = btext_layer_create(GRect(10, MENU_CONTENT_TOP + 4, window_bounds.size.w - 20, window_bounds.size.h - MENU_CONTENT_TOP - 44));
  text_layer_set_text_color(data->empty_text_layer, gcolor_legible_over(ACCENT_COLOUR));
  text_layer_set_background_color(data->empty_text_layer, GColorClear);
  text_layer_set_font(data->empty_text_layer, fonts->title_font);
  text_layer_set_text_alignment(data->empty_text_layer, GTextAlignmentCenter);
  text_layer_set_text(data->empty_text_layer, data->for_timers ? S(STR_NO_TIMERS) : S(STR_NO_ALARMS));
  if (prv_get_num_rows(data->menu_layer, 0, window) == 0) {
    prv_show_empty(window);
  } else {
    window_set_background_color(window, GColorWhite);
    prv_header_apply_colors(data, GColorWhite, GColorBlack);
    layer_add_child(root_layer, menu_layer_get_layer(data->menu_layer));
    menu_layer_set_click_config_onto_window(data->menu_layer, window);
  }
}

static void prv_header_separator_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorDarkGray, GColorBlack));
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void prv_header_create(Window *window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  data->header_layer = btext_layer_create(GRect(0, 0, window_bounds.size.w, MENU_HEADER_HEIGHT));
  text_layer_set_font(data->header_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(data->header_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(data->header_layer));
  data->header_separator = blayer_create(GRect(0, MENU_HEADER_HEIGHT, window_bounds.size.w, 1));
  layer_set_update_proc(data->header_separator, prv_header_separator_update);
  layer_add_child(root_layer, data->header_separator);
  prv_header_update_time(data);
}

static void prv_header_update_time(AlarmMenuWindowData *data) {
  time_t now = time(NULL);
  format_time(data->header_time, sizeof(data->header_time), localtime(&now));
  text_layer_set_text(data->header_layer, data->header_time);
}

static void prv_header_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  format_time(data->header_time, sizeof(data->header_time), tick_time);
  text_layer_set_text(data->header_layer, data->header_time);
}

static void prv_header_apply_colors(AlarmMenuWindowData *data, GColor background, GColor foreground) {
  text_layer_set_background_color(data->header_layer, background);
  text_layer_set_text_color(data->header_layer, foreground);
}

static void prv_show_empty(Window *window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);

  // Remove the menu, if it's present.
  layer_remove_from_parent(menu_layer_get_layer(data->menu_layer));
  window_set_click_config_provider(window, NULL);

  // The empty state's mascot is a live face running its idle
  // expression rotation (created once; this function can be re-entered
  // from appear / cancel, and idle_start is a no-op while running).
  if (!data->empty_face_layer) {
    data->empty_face_layer = face_layer_create(GRect(window_bounds.size.w / 2 - 32, window_bounds.size.h - 88, 64, 64));
    face_layer_set_expression(data->empty_face_layer, FaceEyeDot, FaceEyeDot,
        FaceMouthOmega, 0, 0, 0, FacePropNone);
  }
  window_set_background_color(window, BRANDED_BACKGROUND_COLOUR);
  prv_header_apply_colors(data, BRANDED_BACKGROUND_COLOUR, gcolor_legible_over(BRANDED_BACKGROUND_COLOUR));
  layer_add_child(root_layer, text_layer_get_layer(data->empty_text_layer));
  layer_add_child(root_layer, face_layer_get_layer(data->empty_face_layer));
  face_layer_idle_start(data->empty_face_layer);
}

static void prv_window_unload(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (data->header_tick_handle) {
    events_tick_timer_service_unsubscribe(data->header_tick_handle);
    data->header_tick_handle = NULL;
  }
  menu_layer_destroy(data->menu_layer);
  text_layer_destroy(data->header_layer);
  layer_destroy(data->header_separator);
  text_layer_destroy(data->empty_text_layer);
  if (data->empty_face_layer != NULL) {
    face_layer_idle_stop(data->empty_face_layer);
    face_layer_destroy(data->empty_face_layer);
  }
  window_destroy(window);
  free(data);
}

static void prv_window_appear(Window* window) {
  mica_page_apply_background(window);
  AlarmMenuWindowData* data = window_get_user_data(window);
  prv_header_apply_colors(data, mica_page_color(), GColorBlack);
  prv_header_update_time(data);
  if (!data->for_timers) {
    return;
  }
  data->tick_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_tick_handler, window);
  // A potential reason for us disappearing and reappearing is an alarm going off, in which case our old data will no
  // longer make any sense.
  if (alarm_manager_get_alarm_count() == 0) {
    prv_show_empty(window);
  } else {
    menu_layer_reload_data(data->menu_layer);
  }
}

static void prv_window_disappear(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (!data->tick_handle) {
    return;
  }
  events_tick_timer_service_unsubscribe(data->tick_handle);
}

static uint16_t prv_get_num_rows(struct MenuLayer* menu_layer, uint16_t section_index, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      ++relevant_count;
    }
  }
  return relevant_count;
}


static void prv_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == cell_index->row) {
        Alarm* alarm = alarm_manager_get_alarm(i);
        time_t t = alarm_get_time(alarm);
        time_t now = time(NULL);
        if (data->for_timers) {
          char name[10];
          time_t remaining = t - now;
          int hours = remaining / 3600;
          int minutes = (remaining / 60) % 60;
          int seconds = remaining % 60;
          snprintf(name, sizeof(name), "%d:%02d:%02d", hours, minutes, seconds);
          menu_cell_basic_draw(ctx, cell_layer, name, alarm_get_name(alarm), NULL);
        } else {
          char* alarm_name = alarm_get_name(alarm);
          if (!alarm_name) {
            char title[10];
            char subtitle[20];
            struct tm time_struct = *localtime(&t);
            format_time_ampm(title, sizeof(title), &time_struct);
            time_t midnight = time_start_of_today();
            if (t < midnight + 86400) {
              snprintf(subtitle, sizeof(subtitle), "%s", S(STR_TODAY));
            } else if (t < midnight + 86400 * 2) {
              snprintf(subtitle, sizeof(subtitle), "%s", S(STR_TOMORROW));
            } else {
              strftime(subtitle, sizeof(subtitle), "%a, %b %d", &time_struct);
            }
            menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
          } else {
            char subtitle[40];
            format_datetime(subtitle, sizeof(subtitle), alarm_get_time(alarm));
            menu_cell_basic_draw(ctx, cell_layer, alarm_name, subtitle, NULL);
          }
        }
        return;
      }
      ++relevant_count;
    }
  }
}

static void prv_select_click(struct MenuLayer* menu_layer, MenuIndex* cell_index, void* context) {
  ActionMenuLevel *root_level = baction_menu_level_create(1);
  action_menu_level_add_action(root_level, "Delete", prv_cancel_alarm, NULL);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite),
      .foreground = COLOR_FALLBACK(gcolor_legible_over(ACCENT_COLOUR), GColorBlack),
    },
    .align = ActionMenuAlignCenter,
    .did_close = prv_action_menu_close,
    .context = context,
  };
  action_menu_open(&config);
}

static void prv_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  if (!data->for_timers) {
    return;
  }
  menu_layer_reload_data(data->menu_layer);
}

static void prv_cancel_alarm(ActionMenu* layer, const ActionMenuItem* item, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == menu_layer_get_selected_index(data->menu_layer).row) {
        Alarm* alarm = alarm_manager_get_alarm(i);
        alarm_manager_cancel_alarm(alarm_get_time(alarm), alarm_is_timer(alarm));
        if (alarm_manager_get_alarm_count() == 0) {
          prv_show_empty(context);
        } else {
          menu_layer_reload_data(data->menu_layer);
        }
        action_menu_close(layer, true);
        return;
      }
      ++relevant_count;
    }
  }
}

static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
}
