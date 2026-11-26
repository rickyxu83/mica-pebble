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

#include "thinking_layer.h"

#include <pebble.h>

#include "memory/sdk.h"
#include "style.h"

// Three vertical wave bars - the same motif as the Mica logo - breathing
// on a phase-shifted sine so the motion is smooth rather than mechanical.
#define WAVE_CYCLE_MS 1600
#define REVEAL_IN_MS 200
#define REVEAL_OUT_MS 150
#define BAR_COUNT 3

static void prv_animation_update(Animation* animation, const AnimationProgress progress);
static void prv_reveal_update(Animation* animation, const AnimationProgress progress);
static void prv_reveal_stopped(Animation* animation, bool finished, void* context);
static void prv_layer_render(Layer* layer, GContext* ctx);
static void prv_destroy_now(ThinkingLayer *layer);

static AnimationImplementation s_animation_implementation;
static AnimationImplementation s_reveal_implementation;

typedef struct {
  Animation *animation;
  Animation *reveal_animation;
  AnimationProgress progress;
  AnimationProgress reveal;
  AnimationProgress reveal_from;
  AnimationProgress reveal_to;
  bool dismissing;
} ThinkingLayerData;

ThinkingLayer* thinking_layer_create(GRect rect) {
  if (s_animation_implementation.update == NULL) {
    s_animation_implementation = (AnimationImplementation){
        .update = prv_animation_update
    };
    s_reveal_implementation = (AnimationImplementation){
        .update = prv_reveal_update
    };
  }

  ThinkingLayer* layer = blayer_create_with_data(rect, sizeof(ThinkingLayerData));
  ThinkingLayerData* data = layer_get_data(layer);
  data->progress = 0;
  data->reveal = 0;
  data->reveal_from = 0;
  data->reveal_to = ANIMATION_NORMALIZED_MAX;
  data->dismissing = false;
  data->animation = animation_create();
  animation_set_curve(data->animation, AnimationCurveLinear);
  animation_set_duration(data->animation, WAVE_CYCLE_MS);
  animation_set_play_count(data->animation, ANIMATION_PLAY_COUNT_INFINITE);
  animation_set_handlers(data->animation, (AnimationHandlers){
                                              .started = NULL,
                                              .stopped = NULL,
}, layer);
  animation_set_implementation(data->animation, &s_animation_implementation);
  layer_set_update_proc(layer, prv_layer_render);
  animation_schedule(data->animation);

  // Grow in instead of popping into existence.
  data->reveal_animation = animation_create();
  animation_set_curve(data->reveal_animation, AnimationCurveEaseOut);
  animation_set_duration(data->reveal_animation, REVEAL_IN_MS);
  animation_set_handlers(data->reveal_animation, (AnimationHandlers){
                                              .started = NULL,
                                              .stopped = prv_reveal_stopped,
}, layer);
  animation_set_implementation(data->reveal_animation, &s_reveal_implementation);
  animation_schedule(data->reveal_animation);
  return layer;
}

static void prv_destroy_now(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  Animation *reveal = data->reveal_animation;
  data->reveal_animation = NULL;
  if (reveal) {
    animation_destroy(reveal);
  }
  if (data->animation) {
    animation_destroy(data->animation);
    data->animation = NULL;
  }
  layer_destroy(layer);
}

void thinking_layer_destroy(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  if (data->dismissing) {
    return;
  }
  if (layer_get_parent(layer) != NULL) {
    // Still on screen (window teardown): destroy immediately, as before.
    prv_destroy_now(layer);
    return;
  }
  // Already detached (the session flow removes the layer, then destroys
  // it): shrink out over REVEAL_OUT_MS, then destroy. The layer is
  // self-contained, so finishing the teardown a beat later is safe.
  if (data->reveal_animation) {
    Animation *old = data->reveal_animation;
    data->reveal_animation = NULL;
    animation_unschedule(old);
    animation_destroy(old);
  }
  data->dismissing = true;
  if (data->animation) {
    animation_unschedule(data->animation);
  }
  data->reveal_from = data->reveal;
  data->reveal_to = 0;
  data->reveal_animation = animation_create();
  animation_set_curve(data->reveal_animation, AnimationCurveEaseIn);
  animation_set_duration(data->reveal_animation, REVEAL_OUT_MS);
  animation_set_handlers(data->reveal_animation, (AnimationHandlers){
                                              .started = NULL,
                                              .stopped = prv_reveal_stopped,
}, layer);
  animation_set_implementation(data->reveal_animation, &s_reveal_implementation);
  animation_schedule(data->reveal_animation);
}

static void prv_animation_update(Animation* animation, const AnimationProgress progress) {
  ThinkingLayer* layer = animation_get_context(animation);
  ThinkingLayerData* data = layer_get_data(layer);
  data->progress = progress;
  layer_mark_dirty(layer);
}

static void prv_reveal_update(Animation* animation, const AnimationProgress progress) {
  ThinkingLayer* layer = animation_get_context(animation);
  ThinkingLayerData* data = layer_get_data(layer);
  data->reveal = data->reveal_from +
      (AnimationProgress)(((int32_t)data->reveal_to - (int32_t)data->reveal_from) *
          (int32_t)progress / (int32_t)ANIMATION_NORMALIZED_MAX);
  layer_mark_dirty(layer);
}

static void prv_reveal_stopped(Animation* animation, bool finished, void* context) {
  ThinkingLayer* layer = context;
  ThinkingLayerData* data = layer_get_data(layer);
  if (data->reveal_animation != animation) {
    // Teardown already owns this layer (see prv_destroy_now).
    return;
  }
  data->reveal_animation = NULL;
  data->reveal = data->reveal_to;
  animation_destroy(animation);
  if (data->dismissing) {
    prv_destroy_now(layer);
  }
}

static void prv_draw_bar(GContext* ctx, int center_x, int center_y, int width, int height) {
  if (height < 2) {
    return;
  }
  int radius = width / 2;
  if (height <= width) {
    graphics_fill_circle(ctx, GPoint(center_x, center_y), height / 2);
    return;
  }
  int top = center_y - height / 2;
  graphics_fill_rect(ctx, GRect(center_x - radius, top + radius, width, height - width), 0, GCornerNone);
  graphics_fill_circle(ctx, GPoint(center_x, top + radius), radius);
  graphics_fill_circle(ctx, GPoint(center_x, top + height - radius), radius);
}

static void prv_layer_render(Layer* layer, GContext* ctx) {
  ThinkingLayerData* data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, COLOR_FALLBACK(mica_theme_deep_accent(), GColorBlack));
  const int slot_w = bounds.size.w / BAR_COUNT;
  const int bar_w = slot_w / 2;
  const int min_h = bounds.size.h / 3;
  const int max_h = bounds.size.h;
  const int center_y = bounds.origin.y + bounds.size.h / 2;
  for (int i = 0; i < BAR_COUNT; ++i) {
    uint32_t angle = (uint32_t)(((uint64_t)data->progress * TRIG_MAX_ANGLE /
        ANIMATION_NORMALIZED_MAX) + (uint64_t)i * (TRIG_MAX_ANGLE / BAR_COUNT)) % TRIG_MAX_ANGLE;
    int32_t sine = sin_lookup(angle);
    int height = min_h + (int)(((int32_t)(max_h - min_h) * (sine + TRIG_MAX_RATIO)) /
        (2 * TRIG_MAX_RATIO));
    height = (int)((int32_t)height * (int32_t)data->reveal / (int32_t)ANIMATION_NORMALIZED_MAX);
    int center_x = bounds.origin.x + i * slot_w + slot_w / 2;
    prv_draw_bar(ctx, center_x, center_y, bar_w, height);
  }
}
