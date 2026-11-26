//
// Created by Katharine Berry on 3/5/25.
//

#include <pebble.h>
#include "style.h"
#include "../settings/settings.h"

// Per-theme colours: a light page tint and a deep accent from the same
// family. Indexed by ThemeColour - 1; Lavender is the default theme.
typedef struct {
  GColor page;
  GColor deep_accent;
} MicaThemeColours;

static const MicaThemeColours s_theme_colours[] = {
  { GColorRichBrilliantLavender, GColorVividViolet },  // Lavender
  { GColorCeleste, GColorVividCerulean },              // Celeste
  { GColorMintGreen, GColorJaegerGreen },              // Mint
  { GColorMelon, GColorFolly },                        // Melon
};

static const MicaThemeColours* prv_theme_colours(void) {
  return &s_theme_colours[settings_get_theme_colour() - 1];
}

GColor mica_theme_page_color(void) {
  return prv_theme_colours()->page;
}

GColor mica_theme_deep_accent(void) {
  return prv_theme_colours()->deep_accent;
}

void bobby_status_bar_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(status_bar, StatusBarLayerSeparatorModeDotted);
}

void bobby_status_bar_result_pane_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite), gcolor_legible_over(ACCENT_COLOUR));
  status_bar_layer_set_separator_mode(status_bar, PBL_IF_COLOR_ELSE(StatusBarLayerSeparatorModeNone, StatusBarLayerSeparatorModeDotted));
}