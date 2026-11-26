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

#include "settings.h"
#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "../util/persist_keys.h"

static EventHandle s_event_handle;
static char s_assistant_runtime[16];

static void prv_app_message_handler(DictionaryIterator *iter, void *context);

void settings_init() {
  s_event_handle = events_app_message_register_inbox_received(prv_app_message_handler, NULL);
}

void settings_deinit() {
  events_app_message_unsubscribe(s_event_handle);
}

QuickLaunchBehaviour settings_get_quick_launch_behaviour() {
  int result = persist_read_int(PERSIST_KEY_QUICK_LAUNCH_BEHAVIOUR);
  if (result == 0) {
    return QuickLaunchBehaviourConverseWithTimeout;
  }
  return result;
}

VibePatternSetting settings_get_alarm_vibe_pattern() {
  int result = persist_read_int(PERSIST_KEY_ALARM_VIBE_PATTERN);
  if (result == 0) {
    return VibePatternSettingStandard;
  }
  return result;
}

VibePatternSetting settings_get_timer_vibe_pattern() {
  int result = persist_read_int(PERSIST_KEY_TIMER_VIBE_PATTERN);
  if (result == 0) {
    return VibePatternSettingStandard;
  }
  return result;
}

ThemeColour settings_get_theme_colour() {
  int result = persist_read_int(PERSIST_KEY_THEME_COLOUR);
  if (result < ThemeColourLavender || result > ThemeColourMelon) {
    return ThemeColourLavender;
  }
  return result;
}

int settings_get_idle_exit_timeout() {
  // Default ON at 30 s: the firmware's own app idle timeout does not
  // fire while Mica is in the foreground on the owner's watch (it does
  // fire in system apps such as Timeline), so the app implements the
  // return-to-watchface itself.
  if (!persist_exists(PERSIST_KEY_IDLE_EXIT_TIMEOUT)) {
    return 30;
  }
  const int result = persist_read_int(PERSIST_KEY_IDLE_EXIT_TIMEOUT);
  switch (result) {
    case 0:
    case 15:
    case 20:
    case 30:
    case 60:
      return result;
    default:
      return 30;
  }
}

bool settings_get_should_confirm_transcripts() {
  // the default is false, so we don't have to check whether it exists.
  return persist_read_bool(PERSIST_KEY_CONFIRM_TRANSCRIPTS);
}

bool settings_get_use_system_cjk_font() {
  return persist_read_bool(PERSIST_KEY_USE_SYSTEM_CJK_FONT);
}

bool settings_get_quiet_listen() {
  // Default ON: dictation is Mica's main input and a single notification
  // popup aborts it, so hold notifications back while dictating unless
  // the user turned this off in settings.
  if (!persist_exists(PERSIST_KEY_QUIET_LISTEN)) {
    return true;
  }
  return persist_read_bool(PERSIST_KEY_QUIET_LISTEN);
}

int settings_get_ui_language() {
  if (!persist_exists(PERSIST_KEY_UI_LANGUAGE)) {
    return 1;
  }
  return persist_read_int(PERSIST_KEY_UI_LANGUAGE) == 0 ? 0 : 1;
}

const char* settings_get_assistant_runtime() {
  if (persist_exists(PERSIST_KEY_ASSISTANT_RUNTIME)) {
    persist_read_string(PERSIST_KEY_ASSISTANT_RUNTIME, s_assistant_runtime, sizeof(s_assistant_runtime));
    s_assistant_runtime[sizeof(s_assistant_runtime) - 1] = '\0';
    return s_assistant_runtime;
  }
  return "automatic";
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  for (Tuple *tuple = dict_read_first(iter); tuple; tuple = dict_read_next(iter)) {
    if (tuple->key == MESSAGE_KEY_QUICK_LAUNCH_BEHAVIOUR) {
      int value = atoi(tuple->value->cstring);
      persist_write_int(PERSIST_KEY_QUICK_LAUNCH_BEHAVIOUR, value);
    } else if (tuple->key == MESSAGE_KEY_ALARM_VIBE_PATTERN) {
      persist_write_int(PERSIST_KEY_ALARM_VIBE_PATTERN, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_TIMER_VIBE_PATTERN) {
      persist_write_int(PERSIST_KEY_TIMER_VIBE_PATTERN, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_CONFIRM_TRANSCRIPTS) {
      persist_write_bool(PERSIST_KEY_CONFIRM_TRANSCRIPTS, tuple->value->int8);
    } else if (tuple->key == MESSAGE_KEY_USE_SYSTEM_CJK_FONT) {
      // Applied on next launch so custom fonts reload consistently.
      persist_write_bool(PERSIST_KEY_USE_SYSTEM_CJK_FONT, tuple->value->int8);
    } else if (tuple->key == MESSAGE_KEY_THEME_COLOUR) {
      persist_write_int(PERSIST_KEY_THEME_COLOUR, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_IDLE_EXIT_TIMEOUT) {
      persist_write_int(PERSIST_KEY_IDLE_EXIT_TIMEOUT, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_QUIET_LISTEN) {
      persist_write_bool(PERSIST_KEY_QUIET_LISTEN, tuple->value->int8);
    } else if (tuple->key == MESSAGE_KEY_UI_LANGUAGE) {
      // Applied on the next app launch so custom fonts reload consistently.
      persist_write_int(PERSIST_KEY_UI_LANGUAGE, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_ASSISTANT_RUNTIME) {
      persist_write_string(PERSIST_KEY_ASSISTANT_RUNTIME, tuple->value->cstring);
    }
  }
}
