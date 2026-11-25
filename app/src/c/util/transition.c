/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "transition.h"
#include "style.h"

#define MICA_ZOOM_MS 320
#define MICA_POP_MS 240
#define MICA_ZOOM_START_SCALE 0.12f
#define MICA_MAX_CHILDREN 8

// Field offsets verified against PebbleOS source (pebble/applib/ui/layer.h):
// bounds(8) + frame(8) + flags(4, padded) = 24, then next_sibling, parent,
// first_child. We only READ the links; nothing writes to them.
struct MicaLayer {
  GRect bounds;
  GRect frame;
  uint8_t flags;
  struct MicaLayer *next_sibling;
  struct MicaLayer *parent;
  struct MicaLayer *first_child;
};

typedef struct {
  Window *window;
  Layer **children;
  GRect *targets;
  int count;
} MicaZoom;

// Light Pebble-palette tints that stay readable with black text. Each push
// advances the cycle so consecutive pages get different backgrounds.
static const GColor s_page_palette[] = {
  GColorIcterine,
  GColorMintGreen,
  GColorMelon,
  GColorRichBrilliantLavender,
};
static int s_page_palette_index;
static GColor s_last_page_color;

static GColor prv_next_page_color(void) {
  const GColor color = s_page_palette[s_page_palette_index % 4];
  s_page_palette_index++;
  s_last_page_color = color;
  return color;
}

GColor mica_page_color(void) {
  return s_last_page_color;
}

void mica_page_status_bar_colors(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, s_last_page_color, GColorBlack);
}

static void prv_zoom_update(Animation *animation, const AnimationProgress progress) {
  MicaZoom *zoom = animation_get_context(animation);
  if (!zoom || !zoom->window) {
    return;
  }
  Layer *root = window_get_root_layer(zoom->window);
  GRect full = layer_get_frame(root);
  GPoint center = grect_center_point(&full);
  const float p = (float)progress / (float)ANIMATION_NORMALIZED_MAX;
  const float scale = MICA_ZOOM_START_SCALE + p * (1.0f - MICA_ZOOM_START_SCALE);
  for (int i = 0; i < zoom->count; ++i) {
    GRect t = zoom->targets[i];
    GRect frame;
    frame.size.w = (int16_t)(t.size.w * scale);
    frame.size.h = (int16_t)(t.size.h * scale);
    frame.origin.x = center.x + (int16_t)((t.origin.x - center.x) * scale);
    frame.origin.y = center.y + (int16_t)((t.origin.y - center.y) * scale);
    layer_set_frame(zoom->children[i], frame);
  }
}

static void prv_zoom_stopped(Animation *animation, bool finished, void *context) {
  MicaZoom *zoom = context;
  if (zoom->window) {
    for (int i = 0; i < zoom->count; ++i) {
      layer_set_frame(zoom->children[i], zoom->targets[i]);
    }
  }
  free(zoom->children);
  free(zoom->targets);
  free(zoom);
  animation_destroy(animation);
}

static const AnimationImplementation s_zoom_implementation = {
  .update = prv_zoom_update,
};

typedef struct {
  Window *window;
  Layer **children;
  GRect *targets;
  int count;
} MicaPop;

static void prv_pop_update(Animation *animation, const AnimationProgress progress) {
  MicaPop *pop = animation_get_context(animation);
  if (!pop || !pop->window) {
    return;
  }
  Layer *root = window_get_root_layer(pop->window);
  GRect full = layer_get_frame(root);
  GPoint center = grect_center_point(&full);
  const float p = (float)progress / (float)ANIMATION_NORMALIZED_MAX;
  const float scale = 1.0f - p * (1.0f - MICA_ZOOM_START_SCALE);
  for (int i = 0; i < pop->count; ++i) {
    GRect t = pop->targets[i];
    GRect frame;
    frame.size.w = (int16_t)(t.size.w * scale);
    frame.size.h = (int16_t)(t.size.h * scale);
    frame.origin.x = center.x + (int16_t)((t.origin.x - center.x) * scale);
    frame.origin.y = center.y + (int16_t)((t.origin.y - center.y) * scale);
    layer_set_frame(pop->children[i], frame);
  }
}

static void prv_pop_stopped(Animation *animation, bool finished, void *context) {
  MicaPop *pop = context;
  if (pop->window) {
    window_stack_pop(false);
  }
  free(pop->children);
  free(pop->targets);
  free(pop);
  animation_destroy(animation);
}

static const AnimationImplementation s_pop_implementation = {
  .update = prv_pop_update,
};

void mica_animate_window_pop(Window *window) {
  if (!window) {
    return;
  }
  Layer *root = window_get_root_layer(window);
  GRect full = layer_get_frame(root);
  struct MicaLayer *node = (struct MicaLayer *)root;
  struct MicaLayer *child = node->first_child;
  Layer *children[MICA_MAX_CHILDREN];
  GRect targets[MICA_MAX_CHILDREN];
  int count = 0;
  while (child && count < MICA_MAX_CHILDREN) {
    children[count] = (Layer *)child;
    targets[count] = layer_get_frame((Layer *)child);
    ++count;
    child = child->next_sibling;
  }
  if (count == 0) {
    window_stack_pop(false);
    return;
  }
  MicaPop *pop = malloc(sizeof(MicaPop));
  pop->window = window;
  pop->count = count;
  pop->children = malloc(sizeof(Layer *) * count);
  pop->targets = malloc(sizeof(GRect) * count);
  GPoint center = grect_center_point(&full);
  for (int i = 0; i < count; ++i) {
    pop->children[i] = children[i];
    pop->targets[i] = targets[i];
  }
  Animation *animation = animation_create();
  animation_set_duration(animation, MICA_POP_MS);
  animation_set_curve(animation, AnimationCurveEaseIn);
  animation_set_implementation(animation, &s_pop_implementation);
  animation_set_handlers(animation, (AnimationHandlers) {
    .stopped = prv_pop_stopped,
  }, pop);
  animation_schedule(animation);
}

void mica_transition_set_style(int style) {
  // Centre zoom is the only approved style; kept for API compatibility.
  (void)style;
}

int mica_transition_get_style(void) {
  return 0;
}

void mica_animate_window_push(Window *window) {
  mica_animate_window_push_colored(window, prv_next_page_color());
}

void mica_animate_window_push_colored(Window *window, GColor background) {
  if (!window) {
    return;
  }
  window_set_background_color(window, background);
  Layer *root = window_get_root_layer(window);
  GRect full = layer_get_frame(root);

  // Every animated page registers its top-level layers before this call, so
  // read them from the page itself: pages push a single content layer when
  // possible, otherwise we animate the root's known children recorded here.
  Layer *children[MICA_MAX_CHILDREN];
  GRect targets[MICA_MAX_CHILDREN];
  int count = 0;
  struct MicaLayer *node = (struct MicaLayer *)root;
  struct MicaLayer *child = node->first_child;
  while (child && count < MICA_MAX_CHILDREN) {
    children[count] = (Layer *)child;
    targets[count] = layer_get_frame((Layer *)child);
    ++count;
    child = child->next_sibling;
  }
  if (count == 0) {
    return;
  }

  MicaZoom *zoom = malloc(sizeof(MicaZoom));
  zoom->window = window;
  zoom->count = count;
  zoom->children = malloc(sizeof(Layer *) * count);
  zoom->targets = malloc(sizeof(GRect) * count);
  GPoint center = grect_center_point(&full);
  for (int i = 0; i < count; ++i) {
    zoom->children[i] = children[i];
    zoom->targets[i] = targets[i];
    GRect t = targets[i];
    GRect start;
    start.size.w = (int16_t)(t.size.w * MICA_ZOOM_START_SCALE);
    start.size.h = (int16_t)(t.size.h * MICA_ZOOM_START_SCALE);
    start.origin.x = center.x + (int16_t)((t.origin.x - center.x) * MICA_ZOOM_START_SCALE);
    start.origin.y = center.y + (int16_t)((t.origin.y - center.y) * MICA_ZOOM_START_SCALE);
    layer_set_frame(children[i], start);
  }

  Animation *animation = animation_create();
  animation_set_duration(animation, MICA_ZOOM_MS);
  animation_set_curve(animation, AnimationCurveEaseInOut);
  animation_set_implementation(animation, &s_zoom_implementation);
  animation_set_handlers(animation, (AnimationHandlers) {
    .stopped = prv_zoom_stopped,
  }, zoom);
  animation_schedule(animation);
}
