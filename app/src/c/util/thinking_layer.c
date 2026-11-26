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

#include "face_layer.h"
#include "memory/sdk.h"

// The thinking indicator is the Mica face in miniature: small eyes whose
// gaze wanders on a sine while the mouth runs a voice-wave animation.
// One looping animation drives both, phase-shifted so the motion feels
// organic rather than mechanical. The face lives in a child FaceLayer
// whose frame is scaled by the reveal animation, preserving the
// grow-in / shrink-out behaviour.
#define WAVE_CYCLE_MS 1600
#define REVEAL_IN_MS 200
#define REVEAL_OUT_MS 150

static void prv_animation_update(Animation* animation, const AnimationProgress progress);
static void prv_reveal_update(Animation* animation, const AnimationProgress progress);
static void prv_reveal_stopped(Animation* animation, bool finished, void* context);
static void prv_apply_reveal(ThinkingLayer *layer);
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
  FaceLayer *face;
  GRect frame;
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
  data->frame = rect;
  data->dismissing = false;
  // The face starts at zero size in the centre; the reveal animation
  // grows it to the full frame (see prv_apply_reveal).
  data->face = face_layer_create(GRect(rect.size.w / 2, rect.size.h / 2, 0, 0));
  face_layer_set_expression(data->face, FaceEyeSmall, FaceEyeSmall,
      FaceMouthWave, 0, 0, 0, FacePropNone);
  layer_add_child(layer, face_layer_get_layer(data->face));
  data->animation = animation_create();
  animation_set_curve(data->animation, AnimationCurveLinear);
  animation_set_duration(data->animation, WAVE_CYCLE_MS);
  animation_set_play_count(data->animation, ANIMATION_PLAY_COUNT_INFINITE);
  animation_set_handlers(data->animation, (AnimationHandlers){
                                              .started = NULL,
                                              .stopped = NULL,
}, layer);
  animation_set_implementation(data->animation, &s_animation_implementation);
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
  // layer_destroy does not destroy children, so the face goes first.
  if (data->face) {
    face_layer_destroy(data->face);
    data->face = NULL;
  }
  layer_destroy(layer);
}

void thinking_layer_destroy(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  if (data->dismissing) {
    return;
  }
  prv_destroy_now(layer);
}

void thinking_layer_dismiss(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  if (data->dismissing) {
    return;
  }
  // The caller has already detached the layer (the session flow removes
  // it, then dismisses it): shrink out over REVEAL_OUT_MS, then destroy.
  // The layer is self-contained, so finishing the teardown a beat later
  // is safe.
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

// Scales the child face's frame by the current reveal fraction,
// centred in the layer's frame.
static void prv_apply_reveal(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  if (!data->face) {
    return;
  }
  int16_t face_w = (int16_t)((int32_t)data->frame.size.w *
      (int32_t)data->reveal / (int32_t)ANIMATION_NORMALIZED_MAX);
  int16_t face_h = (int16_t)((int32_t)data->frame.size.h *
      (int32_t)data->reveal / (int32_t)ANIMATION_NORMALIZED_MAX);
  layer_set_frame(face_layer_get_layer(data->face),
      GRect(data->frame.size.w / 2 - face_w / 2,
          data->frame.size.h / 2 - face_h / 2, face_w, face_h));
}

static void prv_animation_update(Animation* animation, const AnimationProgress progress) {
  ThinkingLayer* layer = animation_get_context(animation);
  ThinkingLayerData* data = layer_get_data(layer);
  data->progress = progress;
  if (!data->face) {
    return;
  }
  const uint32_t angle = (uint32_t)((uint64_t)progress * TRIG_MAX_ANGLE /
      ANIMATION_NORMALIZED_MAX);
  const int16_t look_x = (int16_t)(3 * sin_lookup(angle) / TRIG_MAX_RATIO);
  const int16_t look_y = (int16_t)(2 *
      sin_lookup((angle * 2) % TRIG_MAX_ANGLE) / TRIG_MAX_RATIO);
  const uint32_t wave_phase = (angle * 2) % TRIG_MAX_ANGLE;
  face_layer_set_expression(data->face, FaceEyeSmall, FaceEyeSmall,
      FaceMouthWave, look_x, look_y, wave_phase, FacePropNone);
}

static void prv_reveal_update(Animation* animation, const AnimationProgress progress) {
  ThinkingLayer* layer = animation_get_context(animation);
  ThinkingLayerData* data = layer_get_data(layer);
  data->reveal = data->reveal_from +
      (AnimationProgress)(((int32_t)data->reveal_to - (int32_t)data->reveal_from) *
          (int32_t)progress / (int32_t)ANIMATION_NORMALIZED_MAX);
  prv_apply_reveal(layer);
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
  prv_apply_reveal(layer);
  animation_destroy(animation);
  if (data->dismissing) {
    prv_destroy_now(layer);
  }
}
