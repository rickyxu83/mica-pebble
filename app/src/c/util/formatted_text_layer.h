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

#pragma once

#include <pebble.h>

typedef Layer FormattedTextLayer;

FormattedTextLayer* formatted_text_layer_create(GRect frame);
Layer* formatted_text_layer_get_layer(FormattedTextLayer* layer);
void formatted_text_layer_destroy(FormattedTextLayer* layer);
void formatted_text_layer_set_text(FormattedTextLayer* layer, const char* text);
// Constrain this custom renderer to the round display's safe text width.
void formatted_text_layer_enable_round_layout(FormattedTextLayer* layer);
void formatted_text_layer_set_title_font(FormattedTextLayer* layer, GFont font);
void formatted_text_layer_set_subtitle_font(FormattedTextLayer* layer, GFont font);
void formatted_text_layer_set_body_font(FormattedTextLayer* layer, GFont font);
void formatted_text_layer_set_text_alignment(FormattedTextLayer* layer, GTextAlignment alignment);
GSize formatted_text_layer_get_content_size(FormattedTextLayer* layer);
