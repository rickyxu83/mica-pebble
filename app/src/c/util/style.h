/*
 * Copyright 2025 Google LLC
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

#ifndef COLOURS_H
#define COLOURS_H

#include <pebble.h>

// These resolve through the theme table (mica_theme_page_color) so the
// settings theme picker retints every surface that goes through the
// central colour definitions. They are function calls, not constants:
// only use them where a runtime value is acceptable.
#define ACCENT_COLOUR mica_theme_page_color()
#define BRANDED_BACKGROUND_COLOUR COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite)
#define SELECTION_HIGHLIGHT_COLOUR COLOR_FALLBACK(ACCENT_COLOUR, GColorBlack)

void bobby_status_bar_config(StatusBarLayer *status_bar);
void bobby_status_bar_result_pane_config(StatusBarLayer *status_bar);

// Central theme colours. The page colour is the light tint used for page
// backgrounds and branded surfaces; the deep accent is a saturated shade
// from the same family, used where a small element must stay legible on a
// light background (e.g. the thinking wave bars).
GColor mica_theme_page_color(void);
GColor mica_theme_deep_accent(void);

#endif //COLOURS_H
