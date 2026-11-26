/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */
#pragma once

#include <pebble.h>

// A minimal NOMI-style face drawn with vector primitives in a single ink
// colour: two eyes and a mouth whose shapes change to form expressions.
// The face scales to whatever frame it is given; the same layer serves
// as the large home-screen mascot and as the small "thinking" face.
//
// Expressions are set explicitly with face_layer_set_expression(); the
// layer owns no timers or animations, so callers drive blinking, happy
// flashes and the thinking face's wandering gaze themselves.

typedef enum {
  FaceEyeDot = 0,  // Vertical capsules (the resting eyes).
  FaceEyeBlink,    // Short horizontal rounded lines.
  FaceEyeHappy,    // ^^ carets (upward arcs).
  FaceEyeSmall,    // Small filled circles (the thinking face's eyes).
} FaceEyeKind;

typedef enum {
  FaceMouthSmile = 0,  // Gentle upward arc.
  FaceMouthBigSmile,   // Wider, deeper arc.
  FaceMouthFlat,       // Short horizontal line.
  FaceMouthWave,       // Animated sine polyline (speaking/thinking).
  FaceMouthOmega,      // Two small ∪ arcs side by side (a tiny "ω").
} FaceMouthKind;

typedef Layer FaceLayer;

FaceLayer *face_layer_create(GRect frame);
void face_layer_destroy(FaceLayer *layer);
Layer *face_layer_get_layer(FaceLayer *layer);

// Sets the current expression and marks the layer dirty. look_x/look_y
// offset the eyes in pixels (a wandering gaze for the thinking face);
// wave_phase is a trig angle (0..TRIG_MAX_ANGLE) animating the wave
// mouth and is ignored by the other mouth kinds.
void face_layer_set_expression(FaceLayer *layer, FaceEyeKind eyes,
    FaceMouthKind mouth, int16_t look_x, int16_t look_y,
    uint32_t wave_phase);
