/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */
#pragma once
#include <pebble.h>

typedef enum {
  MICA_TRANSITION_OFF = 0,
  MICA_TRANSITION_SLIDE = 1,
  MICA_TRANSITION_ZOOM = 2,
  MICA_TRANSITION_ORIGAMI = 3,
  MICA_TRANSITION_RISE = 4,
} MicaTransitionStyle;

void mica_transition_set_style(int style);
int mica_transition_get_style(void);
void mica_animate_window_push(Window *window);
void mica_animate_window_push_colored(Window *window, GColor background);
void mica_animate_window_pop(Window *window);
// The color assigned to the most recent push; pages with self-painting
// widgets (MenuLayer) use it to tint their own backgrounds.
GColor mica_page_color(void);
// Tint a page's status bar to match the current page color.
void mica_page_status_bar_colors(StatusBarLayer *status_bar);
