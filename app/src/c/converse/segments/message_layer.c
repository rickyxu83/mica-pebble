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

#include "message_layer.h"
#include "../../util/fonts.h"
#include "../../util/formatted_text_layer.h"
#include "../../util/i18n.h"
#include "../../util/memory/sdk.h"
#include "../../util/logging.h"

#include <pebble.h>

#if defined(PBL_ROUND)
#define CONTENT_PADDING 0
#else
#define CONTENT_PADDING 5
#endif

typedef struct {
  ConversationEntry* entry;
  TextLayer* speaker_layer;
  TextLayer* content_text_layer;
  Layer* content_layer;
  uint16_t content_height;
  size_t last_newline_offset;
  bool rich_content;
#if defined(PBL_ROUND)
  FormattedTextLayer *formatted_content_layer;
  char *plain_text;
  size_t plain_text_capacity;
#endif
} MessageLayerData;

static char *prv_get_content_text(MessageLayer *layer);
static int prv_get_content_height(MessageLayer* layer);
static void prv_rich_content_update(Layer *content_layer, GContext *ctx);
static int prv_rich_layout(MessageLayer *layer, GContext *ctx);
#if defined(PBL_ROUND)
static char *prv_round_plain_text(MessageLayerData *data, const char *text);
#endif

MessageLayer* message_layer_create(GRect rect, ConversationEntry* entry) {
    Layer* layer = blayer_create_with_data(rect, sizeof(MessageLayerData));
    MessageLayerData* data = layer_get_data(layer);
    const FontsConfig *fonts = fonts_get_config();
    data->entry = entry;
    data->speaker_layer = btext_layer_create(GRect(0, 0, rect.size.w, fonts->small_font_cap * 1.75));
    text_layer_set_text_alignment(data->speaker_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
    size_t content_origin_y = -5;
    EntryType type = conversation_entry_get_type(entry);
    if (type == EntryTypePrompt) {
      text_layer_set_text(data->speaker_layer, S(STR_YOU));
      content_origin_y = fonts->small_font_cap * 1.75;
    }
#if defined(PBL_ROUND)
    data->plain_text = NULL;
    data->plain_text_capacity = 0;
#endif
    // The token-based rich renderer breaks text on spaces and estimates
    // heights the same way. CJK text has no spaces, so tokens never wrap:
    // lines get ellipsized and segment heights come out wrong. System text
    // layout handles CJK line breaking natively, so always use it.
    data->rich_content = false;
    text_layer_set_font(data->speaker_layer, fonts->small_font);
    layer_add_child(layer, (Layer *)data->speaker_layer);
    data->last_newline_offset = 0;
    data->content_height = prv_get_content_height(layer);
    if (data->rich_content) {
      data->content_layer = blayer_create_with_data(GRect(CONTENT_PADDING, content_origin_y, rect.size.w - CONTENT_PADDING * 2, data->content_height), sizeof(MessageLayer*));
      MessageLayer **parent = layer_get_data(data->content_layer);
      *parent = layer;
      layer_set_update_proc(data->content_layer, prv_rich_content_update);
      data->content_text_layer = NULL;
    } else {
#if defined(PBL_ROUND)
      data->content_text_layer = NULL;
      data->formatted_content_layer = formatted_text_layer_create(GRect(0, content_origin_y, rect.size.w, data->content_height));
      formatted_text_layer_set_body_font(data->formatted_content_layer, fonts->text_font);
      formatted_text_layer_set_text(data->formatted_content_layer, prv_get_content_text(layer));
      formatted_text_layer_enable_round_layout(data->formatted_content_layer);
      data->content_height = formatted_text_layer_get_content_size(data->formatted_content_layer).h;
      data->content_layer = formatted_text_layer_get_layer(data->formatted_content_layer);
#else
      data->content_text_layer = btext_layer_create(GRect(CONTENT_PADDING, content_origin_y, rect.size.w - CONTENT_PADDING * 2, data->content_height));
      text_layer_set_text(data->content_text_layer, prv_get_content_text(layer));
      text_layer_set_font(data->content_text_layer, fonts->text_font);
      text_layer_set_overflow_mode(data->content_text_layer, GTextOverflowModeWordWrap);
      text_layer_set_text_alignment(data->content_text_layer, GTextAlignmentLeft);
      data->content_height = text_layer_get_content_size(data->content_text_layer).h;
      data->content_layer = text_layer_get_layer(data->content_text_layer);
#endif
    }
    layer_add_child(layer, data->content_layer);
    message_layer_update(layer);
    return layer;
}

void message_layer_destroy(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
  if (data->speaker_layer) {
    text_layer_destroy(data->speaker_layer);
  }
#if defined(PBL_ROUND)
  formatted_text_layer_destroy(data->formatted_content_layer);
  free(data->plain_text);
#else
  if (data->content_text_layer) {
    text_layer_destroy(data->content_text_layer);
  } else {
    layer_destroy(data->content_layer);
  }
#endif
  layer_destroy(layer);
}

void message_layer_update(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
#if defined(PBL_ROUND)
  // Reflowing a narrow TextLayer for every streamed fragment leaves stale
  // pixels at its previous offset. Lay out once when the response is complete.
  if (conversation_entry_get_type(data->entry) == EntryTypeResponse &&
      !conversation_entry_get_response(data->entry)->complete) {
    return;
  }
#endif
  const FontsConfig *fonts = fonts_get_config();
  // The text pointer can change out underneath us.
#if defined(PBL_ROUND)
  formatted_text_layer_set_text(data->formatted_content_layer, prv_get_content_text(layer));
  formatted_text_layer_enable_round_layout(data->formatted_content_layer);
  data->content_height = formatted_text_layer_get_content_size(data->formatted_content_layer).h;
#else
  if (data->content_text_layer) {
    text_layer_set_text(data->content_text_layer, prv_get_content_text(layer));
  }
  data->content_height = prv_get_content_height(layer);
#endif
  int width = layer_get_bounds(layer).size.w;
  GRect frame = layer_get_frame(layer);
  frame.size.h = data->content_height + 5;
  if (conversation_entry_get_type(data->entry) == EntryTypePrompt) {
    frame.size.h += fonts->small_font_cap * 1.75;
  }
  layer_set_frame(data->content_layer, GRect(CONTENT_PADDING, layer_get_frame(data->content_layer).origin.y, width - CONTENT_PADDING * 2, data->content_height + 5));
  layer_mark_dirty(data->content_layer);
  layer_set_frame(layer, frame);
}

#if defined(PBL_ROUND)
static char *prv_round_plain_text(MessageLayerData *data, const char *text) {
  if (!text) {
    return "";
  }
  size_t needed = strlen(text) + 1;
  if (data->plain_text_capacity < needed) {
    free(data->plain_text);
    data->plain_text = malloc(needed);
    data->plain_text_capacity = needed;
  }
  const char *read = text;
  char *write = data->plain_text;
  while (*read) {
    if (read[0] == '*' && read[1] == '*') {
      read += 2;
      continue;
    }
    *write++ = *read++;
  }
  *write = '\0';
  return data->plain_text;
}
#endif

static char *prv_get_content_text(MessageLayer *layer) {
  MessageLayerData* data = layer_get_data(layer);
  ConversationEntry* entry = data->entry;
  switch(conversation_entry_get_type(entry)) {
    case EntryTypePrompt:
      return conversation_entry_get_prompt(entry)->prompt;
    case EntryTypeResponse:
#if defined(PBL_ROUND)
      return prv_round_plain_text(data, conversation_entry_get_response(entry)->response);
#else
      return conversation_entry_get_response(entry)->response;
#endif
    default:
      return "(Mica bug)";
  }
}

static int prv_get_content_height(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
  const FontsConfig *fonts = fonts_get_config();
  if (data->rich_content) {
    return prv_rich_layout(layer, NULL);
  }
  // Measuring the whole field every time we add text is way too expensive.
  // Instead, just try to figure out where we are line breaking. We don't get enough information
  // from the text layout engine to do this in any particularly clever way, so instead we try
  // to figure out what's on the current line, and when that spills over to the next line.
  // Then we figure out (guess) exactly what spilled over and repeat from there.
  size_t offset = data->last_newline_offset;
  char* text = prv_get_content_text(layer);
  const GFont font = fonts->text_font;
  const GRect rect = GRect(0, 0, layer_get_frame(layer).size.w - CONTENT_PADDING * 2, 10000);
  // This algorithm is somewhat buggy (it can't cope with words getting broken; it assumes only one break per
  // fragment), so for content where speed is less important just actually measure the thing.
  GTextAlignment alignment = GTextAlignmentLeft;
  if (conversation_entry_get_type(data->entry) != EntryTypeResponse || conversation_entry_get_response(data->entry)->complete) {
    return graphics_text_layout_get_content_size(text, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
  }
  int height = graphics_text_layout_get_content_size(text + offset, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
  int content_height = data->content_height;

  if (height > fonts->text_font_cap * 2) {
    int h2 = 0;
    // we broke to a new line. see if we can figure out where.
    size_t len = strlen(text + offset);
    for (int i = len; i > 0; --i) {
      // TODO: maybe we should just copy it? This could cause us trouble with literals...
      char c = (text+offset)[i];
      (text+offset)[i] = 0;
      h2 = graphics_text_layout_get_content_size(text + offset, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
      (text+offset)[i] = c;
      if (h2 < height) {
        // now try to backtrack to where in the text we caused this.
        // I can't think of any way to infer this from the sizing, so just go back to a word break.
        for (int j = i - 1; j >= 0; --j) {
          if ((text+offset)[j] == ' ' || (text+offset)[j] == '-' || (text+offset)[j] == '\n') {
            i = j+1;
//            BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "New line starts \"%s\".", text+offset+i);
            break;
          }
        }
        offset += i;
        break;
      }
    }
    data->last_newline_offset = offset;
    content_height += height - h2;
  }

  return content_height;
}

static int prv_max_int(int a, int b) {
  return a > b ? a : b;
}

static int prv_bullet_radius(const FontsConfig *fonts) {
  if (fonts->text_font_cap >= 18) {
    return 4;
  }
  return 3;
}

static int prv_bullet_center_y(int y, int line_height) {
  return y + (line_height / 2) + 2;
}

static void prv_draw_bullet(GContext *ctx, const FontsConfig *fonts, int y, int line_height) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(5, prv_bullet_center_y(y, line_height)), prv_bullet_radius(fonts));
}

static int prv_measure_token_width(const char *token, GFont font) {
  return graphics_text_layout_get_content_size(token, font, GRect(0, 0, 10000, 10000), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w;
}

static void prv_rich_newline(int *x, int *y, int line_height, bool *line_start, bool *bullet_line) {
  *x = 0;
  *y += line_height;
  *line_start = true;
  *bullet_line = false;
}

static bool prv_consume_bullet_marker(const char **p) {
  if (((*p)[0] == '-' || (*p)[0] == '*' || (*p)[0] == '+') && (*p)[1] == ' ') {
    *p += 2;
    return true;
  }
  const unsigned char *u = (const unsigned char *)(*p);
  if (u[0] != 0xE2 || u[1] == 0) {
    return false;
  }
  if (u[1] == 0x80 && u[2] != 0 && (u[2] == 0xA2 || u[2] == 0xA3)) {
    *p += 3;
    if (**p == ' ') {
      ++(*p);
    }
    return true;
  }
  if (u[1] == 0x97 && u[2] != 0 && u[2] == 0xA6) {
    *p += 3;
    if (**p == ' ') {
      ++(*p);
    }
    return true;
  }
  return false;
}

static int prv_rich_layout(MessageLayer *layer, GContext *ctx) {
  const FontsConfig *fonts = fonts_get_config();
  const char *text = prv_get_content_text(layer);
  const int width = layer_get_bounds(layer).size.w - CONTENT_PADDING * 2;
  const int line_height = prv_max_int(fonts->text_font_cap, fonts->title_font_cap) + 6;
  const int bullet_indent = 14;
  int x = 0;
  int y = 0;
  bool bold = false;
  bool line_start = true;
  bool bullet_line = false;
  char token[80];

  if (!text || text[0] == 0) {
    return line_height;
  }

  if (ctx) {
    graphics_context_set_text_color(ctx, GColorBlack);
  }

  const char *p = text;
  while (*p) {
    if (p[0] == '*' && p[1] == '*') {
      bold = !bold;
      p += 2;
      continue;
    }
    if (*p == '\n') {
      prv_rich_newline(&x, &y, line_height, &line_start, &bullet_line);
      ++p;
      continue;
    }
    if (line_start) {
      while (*p == ' ') {
        ++p;
      }
      if (prv_consume_bullet_marker(&p)) {
        bullet_line = true;
        x = bullet_indent;
        if (ctx) {
          prv_draw_bullet(ctx, fonts, y, line_height);
        }
      }
      line_start = false;
    }
    if (*p == ' ') {
      int space_width = prv_measure_token_width(" ", bold ? fonts->title_font : fonts->text_font);
      if (x + space_width < width) {
        x += space_width;
      }
      ++p;
      continue;
    }

    size_t len = 0;
    while (p[len] && p[len] != ' ' && p[len] != '\n' && !(p[len] == '*' && p[len + 1] == '*') && len < sizeof(token) - 1) {
      ++len;
    }
    if (len == 0) {
      ++p;
      continue;
    }
    strncpy(token, p, len);
    token[len] = 0;
    GFont font = bold ? fonts->title_font : fonts->text_font;
    int token_width = prv_measure_token_width(token, font);
    int indent = bullet_line ? bullet_indent : 0;
    if (x > indent && x + token_width > width) {
      x = indent;
      y += line_height;
    }
    if (ctx) {
      graphics_draw_text(ctx, token, font, GRect(x, y, width - x, line_height + 4), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    x += token_width;
    p += len;
  }

  return y + line_height;
}

static void prv_rich_content_update(Layer *content_layer, GContext *ctx) {
  MessageLayer **parent = layer_get_data(content_layer);
  if (parent && *parent) {
    prv_rich_layout(*parent, ctx);
  }
}
