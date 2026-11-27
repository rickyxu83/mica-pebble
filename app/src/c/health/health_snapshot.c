/*
 * Copyright 2026 for Mica.
 * Licensed under the Apache License, Version 2.0.
 */

#include "health_snapshot.h"
#include "../util/logging.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

#define UNAVAILABLE -1
#define SEND_MAX_TRIES 8
#define SEND_RETRY_MS 250
#define WORKOUT_MIN_SECONDS 600

static EventHandle s_inbox_handle;
static AppTimer *s_retry_timer;
static int s_send_tries;
static bool s_reply_pending;

static bool prv_metric_available(HealthMetric metric, time_t start, time_t end) {
  return (health_service_metric_accessible(metric, start, end) &
          HealthServiceAccessibilityMaskAvailable) != 0;
}

static int32_t prv_sum_or_unavailable(HealthMetric metric, time_t start, time_t end) {
  if (!prv_metric_available(metric, start, end)) {
    return UNAVAILABLE;
  }
  return (int32_t)health_service_sum(metric, start, end);
}

typedef struct {
  time_t now;
  int count;
  int32_t last_ago_minutes;
  int32_t last_duration_minutes;
} WorkoutScan;

static bool prv_workout_callback(HealthActivity activity, time_t start, time_t end, void *context) {
  WorkoutScan *scan = (WorkoutScan *)context;
  time_t duration = end - start;
  if (duration >= WORKOUT_MIN_SECONDS) {
    if (scan->count == 0) {
      // Iterating into the past, so the first qualifying session is the
      // most recent one.
      int32_t ago = (int32_t)((scan->now - end) / 60);
      scan->last_ago_minutes = ago < 0 ? 0 : ago;
      scan->last_duration_minutes = (int32_t)(duration / 60);
    }
    scan->count++;
  }
  return true;
}

static void prv_send_reply(void *context);

static void prv_schedule_retry() {
  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
  }
  s_retry_timer = app_timer_register(SEND_RETRY_MS, prv_send_reply, NULL);
}

static void prv_send_reply(void *context) {
  s_retry_timer = NULL;
  if (!s_reply_pending) {
    return;
  }
  time_t now = time(NULL);
  time_t today_start = time_start_of_today();
  time_t sleep_start = today_start - 12 * 3600;
  time_t sleep_end = today_start + 12 * 3600;

  int32_t steps = prv_sum_or_unavailable(HealthMetricStepCount, today_start, now);
  int32_t active_seconds = prv_sum_or_unavailable(HealthMetricActiveSeconds, today_start, now);
  int32_t distance = prv_sum_or_unavailable(HealthMetricWalkedDistanceMeters, today_start, now);
  int32_t sleep_seconds = prv_sum_or_unavailable(HealthMetricSleepSeconds, sleep_start, sleep_end);
  int32_t restful_seconds = prv_sum_or_unavailable(HealthMetricSleepRestfulSeconds, sleep_start, sleep_end);

  int32_t resting_hr = UNAVAILABLE;
  if (sleep_seconds > 0 &&
      (health_service_metric_aggregate_averaged_accessible(HealthMetricHeartRateBPM,
          sleep_start, sleep_end, HealthAggregationMin, HealthServiceTimeScopeOnce) &
       HealthServiceAccessibilityMaskAvailable)) {
    // No dedicated resting-HR metric exists on the watch; the minimum
    // heart rate over last night's sleep window is the honest proxy, and
    // the phone labels it exactly that way.
    HealthValue min_hr = health_service_aggregate_averaged(HealthMetricHeartRateBPM,
        sleep_start, sleep_end, HealthAggregationMin, HealthServiceTimeScopeOnce);
    resting_hr = min_hr > 0 ? (int32_t)min_hr : UNAVAILABLE;
  }

  HealthValue current_hr = health_service_peek_current_value(HealthMetricHeartRateBPM);

  WorkoutScan scan = {
    .now = now,
    .count = 0,
    .last_ago_minutes = UNAVAILABLE,
    .last_duration_minutes = UNAVAILABLE,
  };
  HealthActivityMask workout_mask = HealthActivityRun | HealthActivityOpenWorkout;
  if (health_service_any_activity_accessible(workout_mask, now - 7 * 86400, now) &
      HealthServiceAccessibilityMaskAvailable) {
    health_service_activities_iterate(workout_mask, now - 7 * 86400, now,
        HealthIterationDirectionPast, prv_workout_callback, &scan);
  }

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    if (++s_send_tries < SEND_MAX_TRIES) {
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Health reply outbox busy (%d); retrying.", result);
      prv_schedule_retry();
    } else {
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Health reply dropped after %d tries.", s_send_tries);
      s_reply_pending = false;
    }
    return;
  }
  s_reply_pending = false;
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_STEPS_TODAY, steps);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_SLEEP_MINUTES,
      sleep_seconds == UNAVAILABLE ? UNAVAILABLE : sleep_seconds / 60);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_SLEEP_RESTFUL_MINUTES,
      restful_seconds == UNAVAILABLE ? UNAVAILABLE : restful_seconds / 60);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_RESTING_HR, resting_hr);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_CURRENT_HR,
      current_hr > 0 ? (int32_t)current_hr : UNAVAILABLE);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_ACTIVE_MINUTES_TODAY,
      active_seconds == UNAVAILABLE ? UNAVAILABLE : active_seconds / 60);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_DISTANCE_TODAY, distance);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_WORKOUT_COUNT_7D, scan.count);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_LAST_WORKOUT_AGO_MINUTES, scan.last_ago_minutes);
  dict_write_int32(iter, MESSAGE_KEY_HEALTH_LAST_WORKOUT_DURATION_MINUTES, scan.last_duration_minutes);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Sending health reply failed: %d.", result);
  }
}

static void prv_inbox_received(DictionaryIterator *iterator, void *context) {
  if (dict_find(iterator, MESSAGE_KEY_HEALTH_REQUEST) == NULL) {
    return;
  }
  if (s_reply_pending) {
    return; // a reply is already on its way
  }
  s_reply_pending = true;
  s_send_tries = 0;
  prv_send_reply(NULL);
}

void health_snapshot_init(void) {
  s_inbox_handle = events_app_message_register_inbox_received(prv_inbox_received, NULL);
}
