/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * Small round-screen layout helpers shared by Mica windows.
 */
#pragma once

#include <pebble.h>

#define MICA_ROUND_TEXT_INSET 8

static inline void mica_round_scroll_text(TextLayer *text_layer, ScrollLayer *scroll_layer) {
#if defined(PBL_ROUND)
  // Call after the text layer is attached to the window hierarchy. The flow
  // follows the circle in screen coordinates and paging avoids clipped lines.
  text_layer_enable_screen_text_flow_and_paging(text_layer, MICA_ROUND_TEXT_INSET);
  scroll_layer_set_paging(scroll_layer, true);
#endif
}

static inline void mica_round_scroll(ScrollLayer *scroll_layer) {
#if defined(PBL_ROUND)
  scroll_layer_set_paging(scroll_layer, true);
#endif
}

static inline void mica_round_menu(MenuLayer *menu_layer) {
#if defined(PBL_ROUND)
  menu_layer_set_center_focused(menu_layer, true);
#endif
}

static inline void mica_round_action_indicator(GRect *frame) {
#if defined(PBL_ROUND)
  // The stock right-aligned select circle is half outside the round display.
  frame->origin.x -= 32;
#endif
}
