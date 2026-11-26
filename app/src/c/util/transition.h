/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */
#pragma once
#include <pebble.h>

// Window transitions are the firmware's native animated push/pop
// (window_stack_push/pop with animated=true), which is what gives system
// apps and Timeline their feel: the same motion in both directions,
// handled by the window stack. This module only owns the page background
// colour that pages are tinted with before they slide in.
void mica_page_apply_background(Window *window);
void mica_page_apply_background_colored(Window *window, GColor background);
// The current page background colour; pages with self-painting widgets
// (MenuLayer) use it to tint their own backgrounds.
GColor mica_page_color(void);
// Tint a page's status bar to match the current page color.
void mica_page_status_bar_colors(StatusBarLayer *status_bar);
