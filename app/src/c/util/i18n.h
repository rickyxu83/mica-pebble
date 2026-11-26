/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#pragma once

#include <pebble.h>

typedef enum {
  MICA_LANGUAGE_EN = 0,
  MICA_LANGUAGE_ZH = 1,
} MicaLanguage;

typedef enum {
  STR_ALARMS = 0,
  STR_TIMERS,
  STR_REMINDERS,
  STR_QUOTA,
  STR_ABOUT,
  STR_LEGAL,
  STR_GOOD_MORNING,
  STR_GOOD_AFTERNOON,
  STR_GOOD_EVENING,
  STR_GOOD_NIGHT,
  STR_HOLD_SELECT,
  STR_YOU,
  STR_DICTATE,
  STR_NO_TIMERS,
  STR_NO_ALARMS,
  STR_NO_REMINDERS,
  STR_ALLOW,
  STR_DENY,
  STR_DELETE,
  STR_SENT,
  STR_THANK_YOU,
  STR_ERROR_RETRY,
  STR_SNOOZED,
  STR_FAILED,
  STR_IMPORTANT,
  STR_PRIVACY,
  STR_LOCATION,
  STR_TODAY,
  STR_TOMORROW,
  STR_TIMES_UP,
  STR_ALARM,
  STR_NO_SUBSCRIPTION,
  STR_TEST_CONNECTION,
  STR_COUNT
} MicaStringId;

void i18n_init(void);
void i18n_set_language(int language);
int i18n_get_language(void);
bool i18n_is_chinese(void);
const char *S(MicaStringId id);
