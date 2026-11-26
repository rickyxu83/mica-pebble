/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "transition.h"
#include "style.h"

// Fixed brand page tint (formerly one stop in a four-colour rotation).
// Resolved through the theme table so the settings theme picker can
// retint every page from one place.
GColor mica_page_color(void) {
  return mica_theme_page_color();
}

void mica_page_apply_background(Window *window) {
  mica_page_apply_background_colored(window, mica_page_color());
}

void mica_page_apply_background_colored(Window *window, GColor background) {
  if (!window) {
    return;
  }
  window_set_background_color(window, background);
}

void mica_page_status_bar_colors(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, mica_page_color(), GColorBlack);
}
