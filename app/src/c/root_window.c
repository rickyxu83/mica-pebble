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

#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "root_window.h"
#include "converse/session_window.h"
#include "menus/root_menu.h"
#include "menus/prompt_menu.h"
#include "util/face_layer.h"
#include "util/logging.h"
#include "util/style.h"
#include "util/transition.h"
#include "util/time.h"
#include "util/i18n.h"
#include "util/fonts.h"
#include "util/memory/malloc.h"
#include "util/memory/sdk.h"
#include "version/version.h"
#include "vibes/haptic_feedback.h"

#define BILLY_MESSAGE_KEY_WATCH_READY 10123

struct RootWindow {
  Window* window;
  ActionBarLayer* action_bar;
  SessionWindow* session_window;
  GBitmap* question_icon;
  GBitmap* dictation_icon;
  GBitmap* more_icon;
  TextLayer* time_layer;
  TextLayer* brand_layer;
  TextLayer* greeting_layer;
  TextLayer* version_layer;
  FaceLayer* mascot_layer;
  AppTimer* mascot_timer;
  int16_t mascot_base_y;
  uint32_t mascot_phase_ms;
  AppTimer* expression_timer;
  uint8_t expression_cycles;
  uint16_t expression_shown_ms;
  EventHandle event_handle;
  EventHandle app_message_handle;
  bool touch_subscribed;
  bool touch_tracking;
  bool touch_moved_too_far;
  uint64_t touch_down_time_ms;
  int16_t touch_down_x;
  int16_t touch_down_y;
  char time_string[6];
  char version_string[9];
  bool greeting_overridden;
};

static void prv_window_load(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_disappear(Window* window);
static void prv_click_config_provider(void *context);
static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_more_clicked(ClickRecognizerRef recognizer, void* context);
static void prv_up_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context);
static int prv_load_suggestions(char*** suggestions);
static void prv_app_message_handler(DictionaryIterator *iter, void *context);
static void prv_send_watch_ready(void);
static void prv_mascot_breath_tick(void *context);
static void prv_expression_tick(void *context);
static void prv_expression_restore(void *context);
static void prv_touch_event_handler(const TouchEvent *event, void *context);

RootWindow* root_window_create() {
  RootWindow* rw = bmalloc(sizeof(RootWindow));
  memset(rw, 0, sizeof(RootWindow));
  rw->window = bwindow_create();
  window_set_window_handlers(rw->window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .disappear = prv_window_disappear,
  });
  window_set_user_data(rw->window, rw);
  return rw;
}

void root_window_push(RootWindow* window) {
  mica_animate_window_push(window->window);
}

void root_window_destroy(RootWindow* window) {
  window_destroy(window->window);
  free(window);
}

Window* root_window_get_window(RootWindow* window) {
  return window->window;
}

#define ROOT_TOUCH_TAP_MAX_MS 300
#define ROOT_TOUCH_TAP_MAX_DISTANCE_PX 10

#define EXPRESSION_PERIOD_MS 3000
#define EXPRESSION_BLINK_MS 150
#define EXPRESSION_HAPPY_MS 700
#define EXPRESSION_HAPPY_EVERY 4

static uint64_t prv_now_ms(void) {
  time_t seconds;
  uint16_t milliseconds;
  time_ms(&seconds, &milliseconds);
  return ((uint64_t)(uint32_t)seconds * 1000) + milliseconds;
}

static bool prv_touch_is_tap_distance(int16_t dx, int16_t dy) {
  const int32_t distance_sq = (int32_t)dx * dx + (int32_t)dy * dy;
  return distance_sq < ROOT_TOUCH_TAP_MAX_DISTANCE_PX *
      ROOT_TOUCH_TAP_MAX_DISTANCE_PX;
}

static void prv_touch_event_handler(const TouchEvent *event, void *context) {
  RootWindow *rw = context;
  if (!event || !rw || !rw->touch_subscribed) {
    return;
  }
  if (event->non_navigational) {
    // Contacts that began as an unarmed touch on the idle watchface are
    // not app input, even if later events in the contact look like a tap.
    rw->touch_tracking = false;
    return;
  }
  if (window_stack_get_top_window() != rw->window) {
    rw->touch_tracking = false;
    return;
  }

  switch (event->type) {
    case TouchEvent_Touchdown:
      rw->touch_tracking = true;
      rw->touch_moved_too_far = false;
      rw->touch_down_time_ms = prv_now_ms();
      rw->touch_down_x = event->x;
      rw->touch_down_y = event->y;
      break;
    case TouchEvent_PositionUpdate:
      if (rw->touch_tracking &&
          !prv_touch_is_tap_distance(event->x - rw->touch_down_x,
              event->y - rw->touch_down_y)) {
        rw->touch_moved_too_far = true;
      }
      break;
    case TouchEvent_Liftoff: {
      const bool is_tap = rw->touch_tracking && !rw->touch_moved_too_far &&
          prv_now_ms() - rw->touch_down_time_ms <= ROOT_TOUCH_TAP_MAX_MS &&
          prv_touch_is_tap_distance(event->x - rw->touch_down_x,
              event->y - rw->touch_down_y);
      rw->touch_tracking = false;
      if (is_tap) {
        // The home screen has no MenuLayer/ScrollLayer for the firmware
        // gesture bridge to map this tap onto, so invoke Select's exact
        // existing handler.
        prv_prompt_clicked(NULL, rw);
      }
      break;
    }
  }
}

static void prv_window_load(Window *window) {
  // RootWindow* root_window = (RootWindow*)window_get_user_data(window);
}

static void prv_window_appear(Window* window) {
  size_t heap_size = heap_bytes_free();
  RootWindow* rw = window_get_user_data(window);
  GRect bounds = layer_get_bounds(window_get_root_layer(rw->window));
  window_set_background_color(rw->window, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite));
  rw->question_icon = bgbitmap_create_with_resource(RESOURCE_ID_QUESTION_ICON);
  rw->dictation_icon = bgbitmap_create_with_resource(RESOURCE_ID_DICTATION_ICON);
  rw->more_icon = bgbitmap_create_with_resource(RESOURCE_ID_MORE_ICON);
  rw->action_bar = baction_bar_layer_create();
  action_bar_layer_set_context(rw->action_bar, rw);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_UP, rw->question_icon);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_SELECT, rw->dictation_icon);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_DOWN, rw->more_icon);
  action_bar_layer_add_to_window(rw->action_bar, window);
  action_bar_layer_set_click_config_provider(rw->action_bar, prv_click_config_provider);
#if PBL_DISPLAY_WIDTH >= 200
  uint16_t time_height = 48;
#else
  uint16_t time_height = 40;
#endif
  int16_t content_width = bounds.size.w - ACTION_BAR_WIDTH;
  int16_t time_y = PBL_IF_ROUND_ELSE(10, 5);
  rw->time_layer = btext_layer_create(GRect(0, time_y, content_width, time_height));
  text_layer_set_text_alignment(rw->time_layer, GTextAlignmentCenter);
#if PBL_DISPLAY_WIDTH >= 200
  text_layer_set_font(rw->time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
#else
  text_layer_set_font(rw->time_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
#endif
  text_layer_set_text(rw->time_layer, "12:34");
  text_layer_set_background_color(rw->time_layer, GColorClear);
  layer_add_child(window_get_root_layer(rw->window), (Layer *)rw->time_layer);
  rw->brand_layer = btext_layer_create(GRect(0, time_y + time_height, content_width, 20));
  text_layer_set_font(rw->brand_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(rw->brand_layer, GTextAlignmentCenter);
  text_layer_set_background_color(rw->brand_layer, GColorClear);
  text_layer_set_text(rw->brand_layer, "MICA");
  layer_add_child(window_get_root_layer(rw->window), text_layer_get_layer(rw->brand_layer));

  rw->mascot_base_y = time_y + time_height + 24;
  // The mascot is the NOMI-style face (util/face_layer.c), drawn live so
  // it can blink and make expressions; its resting expression is
  // DOT + OMEGA (style 2). The idle expression state machine below
  // drives the rest.
  rw->mascot_layer = face_layer_create(GRect((content_width - 64) / 2, rw->mascot_base_y, 64, 64));
  face_layer_set_expression(rw->mascot_layer, FaceEyeDot,
      FaceMouthOmega, 0, 0, 0);
  layer_add_child(window_get_root_layer(rw->window), face_layer_get_layer(rw->mascot_layer));
  // On the entrance push the mascot leads the page slide (see
  // util/transition.c); registrations outside a push are ignored.
  mica_transition_register_lead_layer(face_layer_get_layer(rw->mascot_layer));

  int16_t greeting_inset = PBL_IF_ROUND_ELSE(24, 8);
  rw->greeting_layer = btext_layer_create(GRect(greeting_inset, rw->mascot_base_y + 72,
      content_width - greeting_inset * 2, PBL_IF_ROUND_ELSE(62, 52)));
  text_layer_set_font(rw->greeting_layer, fonts_get_config()->title_font);
  text_layer_set_text_alignment(rw->greeting_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(rw->greeting_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(rw->greeting_layer, GColorClear);
  layer_add_child(window_get_root_layer(rw->window), text_layer_get_layer(rw->greeting_layer));
  rw->greeting_overridden = false;
  if (version_is_updated() || rand() < RAND_MAX / 10) {
    rw->greeting_overridden = true;
    text_layer_set_text(rw->greeting_layer, S(STR_HOLD_SELECT));
  }

  VersionInfo version_info = version_get_current();
  snprintf(rw->version_string, sizeof(rw->version_string), "v%d.%d", version_info.major, version_info.minor);
  rw->version_string[sizeof(rw->version_string) - 1] = '\0';
  rw->version_layer = btext_layer_create(GRect(0, bounds.size.h - PBL_IF_ROUND_ELSE(22, 18), content_width - 4, 18));
  text_layer_set_font(rw->version_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(rw->version_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentRight));
  text_layer_set_background_color(rw->version_layer, GColorClear);
  text_layer_set_text(rw->version_layer, rw->version_string);
  layer_add_child(window_get_root_layer(rw->window), (Layer *)rw->version_layer);


  if (!rw->event_handle) {
    rw->event_handle = events_tick_timer_service_subscribe_context(MINUTE_UNIT, prv_time_changed, rw);
    time_t now = time(NULL);
    prv_time_changed(localtime(&now), MINUTE_UNIT, rw);
  }
  if (!rw->app_message_handle) {
    rw->app_message_handle = events_app_message_register_inbox_received(prv_app_message_handler, rw);
  }
  prv_send_watch_ready();
  if (!rw->touch_subscribed) {
    touch_service_subscribe(prv_touch_event_handler, rw);
    rw->touch_subscribed = true;
  }
  // Idle breathing starts only once the entrance choreography has
  // settled (280 ms slide + 50 ms logo lead), so the two never fight
  // over the mascot's frame. prv_window_disappear cancels the timer, so
  // any navigation stops the breathing instantly.
  rw->mascot_phase_ms = 0;
  rw->mascot_timer = app_timer_register(350, prv_mascot_breath_tick, rw);
  rw->expression_cycles = 0;
  rw->expression_timer = app_timer_register(EXPRESSION_PERIOD_MS,
      prv_expression_tick, rw);
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Window appeared. Heap usage increased %d bytes", heap_size - heap_bytes_free());
}

// Idle expressions for the mascot face: a ~3 s cycle that is a 150 ms
// blink, except every fourth cycle, which is a 700 ms happy flash
// (^^ eyes) instead. One timer alternates between the tick
// (idle -> expression) and restore (expression -> idle) callbacks, so
// exactly one expression timer is ever pending; prv_window_disappear
// cancels whichever is outstanding.
static void prv_expression_tick(void *context) {
  RootWindow *rw = context;
  rw->expression_cycles++;
  if (rw->expression_cycles % EXPRESSION_HAPPY_EVERY == 0) {
    face_layer_set_expression(rw->mascot_layer, FaceEyeHappy,
        FaceMouthOmega, 0, 0, 0);
    rw->expression_shown_ms = EXPRESSION_HAPPY_MS;
  } else {
    face_layer_set_expression(rw->mascot_layer, FaceEyeBlink,
        FaceMouthOmega, 0, 0, 0);
    rw->expression_shown_ms = EXPRESSION_BLINK_MS;
  }
  rw->expression_timer = app_timer_register(rw->expression_shown_ms,
      prv_expression_restore, rw);
}

static void prv_expression_restore(void *context) {
  RootWindow *rw = context;
  face_layer_set_expression(rw->mascot_layer, FaceEyeDot,
      FaceMouthOmega, 0, 0, 0);
  rw->expression_timer = app_timer_register(
      EXPRESSION_PERIOD_MS - rw->expression_shown_ms,
      prv_expression_tick, rw);
}

// The mascot gently bobs ±2 px on a ~2400 ms sine loop while the home
// window is visible.
#define MASCOT_BREATH_PERIOD_MS 2400
#define MASCOT_BREATH_STEP_MS 100
#define MASCOT_BREATH_AMPLITUDE_PX 2

static void prv_mascot_breath_tick(void *context) {
  RootWindow *rw = context;
  rw->mascot_phase_ms = (rw->mascot_phase_ms + MASCOT_BREATH_STEP_MS) %
      MASCOT_BREATH_PERIOD_MS;
  const uint32_t angle = rw->mascot_phase_ms * TRIG_MAX_ANGLE /
      MASCOT_BREATH_PERIOD_MS;
  const int16_t offset = (int16_t)(MASCOT_BREATH_AMPLITUDE_PX *
      sin_lookup(angle) / TRIG_MAX_RATIO);
  GRect frame = layer_get_frame(face_layer_get_layer(rw->mascot_layer));
  frame.origin.y = rw->mascot_base_y + offset;
  layer_set_frame(face_layer_get_layer(rw->mascot_layer), frame);
  rw->mascot_timer = app_timer_register(MASCOT_BREATH_STEP_MS,
      prv_mascot_breath_tick, rw);
}

static void prv_window_disappear(Window* window) {
  size_t heap_size = heap_bytes_free();
  RootWindow* rw = window_get_user_data(window);
  if (rw->touch_subscribed) {
    touch_service_unsubscribe();
    rw->touch_subscribed = false;
  }
  rw->touch_tracking = false;
  if (rw->mascot_timer) {
    app_timer_cancel(rw->mascot_timer);
    rw->mascot_timer = NULL;
  }
  if (rw->expression_timer) {
    app_timer_cancel(rw->expression_timer);
    rw->expression_timer = NULL;
  }
  if (rw->event_handle) {
    events_tick_timer_service_unsubscribe(rw->event_handle);
    rw->event_handle = NULL;
  }
  if (rw->app_message_handle) {
    events_app_message_unsubscribe(rw->app_message_handle);
  }
  action_bar_layer_destroy(rw->action_bar);
  gbitmap_destroy(rw->question_icon);
  gbitmap_destroy(rw->dictation_icon);
  gbitmap_destroy(rw->more_icon);
  text_layer_destroy(rw->time_layer);
  text_layer_destroy(rw->brand_layer);
  text_layer_destroy(rw->greeting_layer);
  text_layer_destroy(rw->version_layer);
  face_layer_destroy(rw->mascot_layer);
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Window disappeared. Heap usage decreased %d bytes", heap_bytes_free() - heap_size);
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  RootWindow* rw = context;
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_COBBLE_WARNING);
  if (!tuple) {
    return;
  }
  if (tuple->value->int32 == 1) {
    rw->greeting_overridden = true;
    text_layer_set_text(rw->greeting_layer, "Cobble has many Mica bugs.");
    window_set_background_color(rw->window, COLOR_FALLBACK(GColorRed, GColorDarkGray));
    vibe_haptic_feedback();
  }
}

static void prv_send_watch_ready(void) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    return;
  }
  dict_write_uint8(iter, BILLY_MESSAGE_KEY_WATCH_READY, 1);
  app_message_outbox_send();
}

static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context) {
  RootWindow* rw = context;
  format_time(rw->time_string, sizeof(rw->time_string), tick_time);
  text_layer_set_text(rw->time_layer, rw->time_string);
  if (rw->greeting_overridden) {
    return;
  }
  if (tick_time->tm_hour >= 6 && tick_time->tm_hour < 12) {
    text_layer_set_text(rw->greeting_layer, S(STR_GOOD_MORNING));
  } else if (tick_time->tm_hour >= 12 && tick_time->tm_hour < 18) {
    text_layer_set_text(rw->greeting_layer, S(STR_GOOD_AFTERNOON));
  } else if (tick_time->tm_hour >= 18 && tick_time->tm_hour < 22) {
    text_layer_set_text(rw->greeting_layer, S(STR_GOOD_EVENING));
  } else {
    text_layer_set_text(rw->greeting_layer, S(STR_GOOD_NIGHT));
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_clicked);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_prompt_clicked);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_more_clicked);
}

static void prv_up_clicked(ClickRecognizerRef recognizer, void *context) {
  char **suggestions;
  int count = prv_load_suggestions(&suggestions);
  prompt_menu_push(suggestions, count);
}

static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context) {
  session_window_push(0, NULL);
}

static void prv_more_clicked(ClickRecognizerRef recognizer, void* context) {
  root_menu_window_push();
}

static int prv_load_suggestions(char*** suggestions) {
  ResHandle handle = resource_get_handle(i18n_is_chinese() ? RESOURCE_ID_SAMPLE_PROMPTS_ZH : RESOURCE_ID_SAMPLE_PROMPTS);
  size_t size = resource_size(handle);
  char* buffer = bmalloc(size);
  resource_load(handle, (uint8_t*)buffer, size);
  int count = 1;
  for (size_t i = 0; i < size; ++i) {
    if (buffer[i] == '\n') {
      ++count;
    }
  }
  *suggestions = bmalloc(sizeof(char*) * count);
  for (int i = 0; i < count; ++i) {
    (*suggestions)[i] = buffer;
    buffer = strchr(buffer, '\n');
    if (buffer) {
      *buffer = '\0';
      ++buffer;
    }
  }
  return count;
}
