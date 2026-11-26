/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */
#pragma once

#include <pebble.h>

// A minimal NOMI-style face drawn with vector primitives in a single ink
// colour: two eyes and a mouth whose shapes change to form expressions,
// plus optional props (sunglasses, a flower, a music note, floating Zs).
// The face scales to whatever frame it is given; the same layer serves
// as the large home-screen mascot and as the small "thinking" face.
//
// Expressions are set explicitly with face_layer_set_expression();
// callers drive blinking, idle acts and the thinking face's wandering
// gaze themselves. The one exception is the self-contained idle driver
// (face_layer_idle_start/stop below), which steps the same expression
// rotation as the home-screen mascot for callers that just want a
// living face (e.g. menu empty states).

typedef enum {
  FaceEyeDot = 0,  // Vertical capsules (the resting eyes).
  FaceEyeBlink,    // Short horizontal rounded lines.
  FaceEyeHappy,    // ^^ carets (upward arcs).
  FaceEyeSmall,    // Small filled circles (the thinking face's eyes).
  FaceEyeHeart,    // A filled heart.
  FaceEyeSleepy,   // A gentle downward arc (‿).
} FaceEyeKind;

typedef enum {
  FaceMouthSmile = 0,  // Gentle upward arc.
  FaceMouthBigSmile,   // Wider, deeper arc.
  FaceMouthFlat,       // Short horizontal line.
  FaceMouthWave,       // Animated sine polyline (speaking/thinking).
  FaceMouthOmega,      // Two small ∪ arcs side by side (a tiny "ω").
} FaceMouthKind;

// Props are drawn in the same ink as the face. Sunglasses replace the
// eyes outright; the others sit beside the face.
typedef enum {
  FacePropNone = 0,
  FacePropSunglasses,  // Two dark lenses + bridge + temples, over the eyes.
  FacePropFlower,      // A small flower at the lower right.
  FacePropMusicNote,   // A music note at the upper right.
  FacePropZzz,         // Two "Z"s floating at the upper right.
} FaceProp;

typedef Layer FaceLayer;

FaceLayer *face_layer_create(GRect frame);
void face_layer_destroy(FaceLayer *layer);
Layer *face_layer_get_layer(FaceLayer *layer);

// Sets the current expression and marks the layer dirty. The two eyes
// are controlled independently (a wink is Blink + Dot). look_x/look_y
// offset the eyes in pixels (a wandering gaze for the thinking face).
// wave_phase is a trig angle (0..TRIG_MAX_ANGLE) animating the wave
// mouth; for FacePropZzz it instead carries the act's elapsed
// milliseconds and drifts the Zs upward. Both are ignored otherwise.
void face_layer_set_expression(FaceLayer *layer, FaceEyeKind left_eye,
    FaceEyeKind right_eye, FaceMouthKind mouth, int16_t look_x,
    int16_t look_y, uint32_t wave_phase, FaceProp prop);

// The bars -> face intro morph: while progress is below 1000 the layer
// ignores the current expression and draws three sound bars (like the
// launcher icon) interpolating into the face — the side bars glide out
// into the capsule eyes, the middle bar shrinks down toward the mouth
// and vanishes, and the omega mouth grows in over the last 40%.
// Progress is in permille (0..1000); 1000 (the default) is the
// finished face. Callers drive the progress with their own timer.
void face_layer_set_intro_progress(FaceLayer *layer, uint16_t permille);

// Self-contained idle performance: steps the home mascot's expression
// rotation (2 s slots: happy, look-around, hearts, sunglasses, flower,
// sleepy + Zzz, music, wink, then a rest slot with one mid-slot blink)
// on a timer owned by the layer. No intro morph — the intro progress
// stays at 1000. Exactly one timer is ever outstanding; starting an
// already-running driver is a no-op. face_layer_idle_stop() cancels
// the timer and leaves the current expression on screen; destroy also
// stops the driver, but callers should stop it explicitly first.
void face_layer_idle_start(FaceLayer *layer);
void face_layer_idle_stop(FaceLayer *layer);
