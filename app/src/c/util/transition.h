/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */
#pragma once
#include <pebble.h>

// Elastic window transitions: windows are pushed/popped without the
// firmware animation and their root layer frame is slid in/out with a
// custom overshoot ("settle") interpolation, so pages arrive fast, pass
// their resting place by a few percent and settle back — the lively feel
// of the firmware's internal moook curves, built only on public APIs.
// The page background colour is applied before the push so any strip of
// screen not yet covered by the sliding page reads as page, not black.
void mica_animate_window_push(Window *window);
void mica_animate_window_push_colored(Window *window, GColor background);
// Slide the window out to the right, then pop it (animated=false). Safe
// to call while an entrance is still running: the entrance is finished
// instantly first. Windows without a clean code hook for Back keep the
// firmware's native animated pop.
void mica_animate_window_pop(Window *window);

// Logo lead: a window whose load/appear creates a Mica mark layer calls
// this during the push; the mark then starts its entrance ~50 ms before
// the page slide, from a few extra pixels to the right and with a
// stronger overshoot, so the logo visibly leads and the page follows.
// Registrations outside a mica_animate_window_push are ignored.
void mica_transition_register_lead_layer(Layer *layer);

// The settle curve itself, for reuse by other animations.
// progress is ANIMATION_NORMALIZED_MIN..MAX; the eased result may exceed
// ANIMATION_NORMALIZED_MAX mid-flight (that is the overshoot).
int32_t mica_curve_settle(int32_t progress);
// InterpolateInt64Function implementations for
// animation_set_custom_interpolation: standard settle (~4% overshoot)
// and strong settle (~9% overshoot, used for the leading logo).
int64_t mica_interpolate_settle(int32_t progress, int64_t from, int64_t to);
int64_t mica_interpolate_settle_strong(int32_t progress, int64_t from, int64_t to);

void mica_page_apply_background(Window *window);
void mica_page_apply_background_colored(Window *window, GColor background);
// The current page background colour; pages with self-painting widgets
// (MenuLayer) use it to tint their own backgrounds.
GColor mica_page_color(void);
// Tint a page's status bar to match the current page color.
void mica_page_status_bar_colors(StatusBarLayer *status_bar);
