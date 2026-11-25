/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */

#include <pebble.h>
#include "fonts.h"
#include "i18n.h"

typedef struct {
  const char *title_font;
  uint32_t title_font_custom;
  uint16_t title_font_cap; // https://en.wikipedia.org/wiki/Cap_height
  const char *text_font;
  uint32_t text_font_custom;
  uint16_t text_font_cap;
  const char *small_font;
  uint16_t small_font_cap;
  const char *content_font;
  uint16_t content_font_cap;
  const char *small_content_font;
  uint16_t small_content_font_cap;
} FontsSpec;

FontsSpec s_specs[] = {
  [PreferredContentSizeSmall] = {
    .title_font = FONT_KEY_GOTHIC_18_BOLD,
    .title_font_cap = 11,
    .text_font  = FONT_KEY_GOTHIC_18,
    .text_font_cap = 11,
    .small_font = FONT_KEY_GOTHIC_14_BOLD,
    .small_font_cap = 9,
    .content_font = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    .content_font_cap = 18,
    .small_content_font = FONT_KEY_LECO_20_BOLD_NUMBERS,
    .small_content_font_cap = 14,
  },
  [PreferredContentSizeMedium] = {
    .title_font = FONT_KEY_GOTHIC_24_BOLD,
    .title_font_cap = 14,
    .text_font  = FONT_KEY_GOTHIC_24_BOLD,
    .text_font_cap = 14,
    .small_font = FONT_KEY_GOTHIC_18_BOLD,
    .small_font_cap = 11,
    .content_font = FONT_KEY_LECO_32_BOLD_NUMBERS,
    .content_font_cap = 22,
    .small_content_font = FONT_KEY_LECO_20_BOLD_NUMBERS,
    .small_content_font_cap = 14,
  },
  [PreferredContentSizeLarge] = {
    .title_font = FONT_KEY_GOTHIC_28_BOLD,
    .title_font_cap = 18,
    .text_font  = FONT_KEY_GOTHIC_28,
    .text_font_cap = 18,
    .small_font = FONT_KEY_GOTHIC_24_BOLD,
    .small_font_cap = 14,
    .content_font = FONT_KEY_LECO_36_BOLD_NUMBERS,
    .content_font_cap = 25,
    .small_content_font = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    .small_content_font_cap = 18,
  },
#if defined(RESOURCE_ID_FONT_GOTHIC_36)
  [PreferredContentSizeExtraLarge] = {
#if defined(PBL_ROUND)
    // Gabbro needs a narrower text column, so use 28px body text rather than
    // Emery's 36px face. The home-screen mascot can still use the large font.
    .title_font = FONT_KEY_GOTHIC_28_BOLD,
    .title_font_cap = 18,
    .text_font = FONT_KEY_GOTHIC_28,
    .text_font_cap = 18,
    .small_font = FONT_KEY_GOTHIC_24_BOLD,
    .small_font_cap = 14,
#else
    .title_font = NULL,
    .title_font_custom = RESOURCE_ID_FONT_GOTHIC_36_BOLD,
    .title_font_cap = 22,
    .text_font = NULL,
    .text_font_custom  = RESOURCE_ID_FONT_GOTHIC_36,
    .text_font_cap = 22,
    .small_font = FONT_KEY_GOTHIC_28_BOLD,
    .small_font_cap = 18,
#endif
    .content_font = FONT_KEY_LECO_42_NUMBERS,
    .content_font_cap = 29,
    .small_content_font = FONT_KEY_LECO_32_BOLD_NUMBERS,
    .small_content_font_cap = 22,
  },
#endif
};

static FontsConfig s_config;
static bool s_chinese_font_loaded;

static GFont prv_load_font(const char *system_key, uint32_t resource_id) {
  if (system_key) {
    return fonts_get_system_font(system_key);
  }
  return fonts_load_custom_font(resource_get_handle(resource_id));
}

static void prv_unload_font(const char *system_key, GFont font) {
  if (!system_key) {
    fonts_unload_custom_font(font);
  }
}

void fonts_load(void) {
  s_chinese_font_loaded = false;
  const FontsSpec *spec = &s_specs[preferred_content_size()];
  s_config = (FontsConfig) {
    .title_font = prv_load_font(spec->title_font, spec->title_font_custom),
    .title_font_cap = spec->title_font_cap,
    .text_font  = prv_load_font(spec->text_font, spec->text_font_custom),
    .text_font_cap = spec->text_font_cap,
    .small_font = fonts_get_system_font(spec->small_font),
    .small_font_cap = spec->small_font_cap,
    .content_font = fonts_get_system_font(spec->content_font),
    .content_font_cap = spec->content_font_cap,
    .small_content_font = fonts_get_system_font(spec->small_content_font),
    .small_content_font_cap = spec->small_content_font_cap,
  };
  if (i18n_is_chinese()) {
    // Embedded subset font; community language packs do not patch the app
    // system fonts on Core Devices, so system-font rendering is not offered.
    if (!spec->title_font) {
      fonts_unload_custom_font(s_config.title_font);
    }
    if (!spec->text_font) {
      fonts_unload_custom_font(s_config.text_font);
    }
    GFont chinese = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MICA_ZH_18));
    s_config.title_font = chinese;
    s_config.title_font_cap = 18;
    s_config.text_font = chinese;
    s_config.text_font_cap = 18;
    s_config.small_font = chinese;
    s_config.small_font_cap = 18;
    s_chinese_font_loaded = true;
  }
}

void fonts_unload(void) {
  if (s_chinese_font_loaded) {
    fonts_unload_custom_font(s_config.title_font);
    s_chinese_font_loaded = false;
    return;
  }
  const FontsSpec *spec = &s_specs[preferred_content_size()];
  prv_unload_font(spec->title_font, s_config.title_font);
  prv_unload_font(spec->text_font, s_config.text_font);
}

const FontsConfig *fonts_get_config(void) {
  return &s_config;
}
