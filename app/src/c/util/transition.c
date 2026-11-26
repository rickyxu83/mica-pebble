/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "transition.h"
#include "style.h"

#include <stdlib.h>

// ---------------------------------------------------------------------------
// Page background colour (unchanged from the native-transition era).

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

// ---------------------------------------------------------------------------
// The settle curve: easeOutBack in Q16 fixed point, no floating point.
//
//   f(t) = 1 + (s+1)*(t-1)^3 + s*(t-1)^2,  t in 0..1
//
// Fast start, a slight overshoot past 1, then a settle back onto 1 — the
// same shape as the firmware's internal moook interpolation. With s = 1.0
// the peak overshoot is ~3.7% of the travel; s = 1.6 peaks at ~9%.

#define SETTLE_Q 65536
#define SETTLE_STRENGTH_STANDARD 65536   // s = 1.0
#define SETTLE_STRENGTH_STRONG 104858    // s = 1.6

static int64_t prv_settle_apply(int32_t progress, int64_t from, int64_t to,
    int32_t strength) {
  if (from == to) {
    return to;
  }
  int64_t t = (int64_t)progress * SETTLE_Q / ANIMATION_NORMALIZED_MAX;
  if (t < 0) {
    t = 0;
  } else if (t > SETTLE_Q) {
    t = SETTLE_Q;
  }
  const int64_t u = t - SETTLE_Q;              // -65536..0
  const int64_t u2 = u * u / SETTLE_Q;         // Q16, 0..65536
  const int64_t u3 = u2 * u / SETTLE_Q;        // Q16, -65536..0
  const int64_t f = SETTLE_Q + (strength + SETTLE_Q) * u3 / SETTLE_Q +
      strength * u2 / SETTLE_Q;                // Q16 eased progress
  return from + (to - from) * f / SETTLE_Q;
}

int32_t mica_curve_settle(int32_t progress) {
  return (int32_t)prv_settle_apply(progress, 0, ANIMATION_NORMALIZED_MAX,
      SETTLE_STRENGTH_STANDARD);
}

int32_t mica_curve_settle_strong(int32_t progress) {
  return (int32_t)prv_settle_apply(progress, 0, ANIMATION_NORMALIZED_MAX,
      SETTLE_STRENGTH_STRONG);
}

// ---------------------------------------------------------------------------
// Window slide choreography.

#define MICA_ENTER_MS 280
#define MICA_EXIT_MS 200
#define MICA_LEAD_MS 50
#define MICA_LEAD_LEAD_PX 28

typedef struct {
  Window *window;
  PropertyAnimation *root_pa;
  PropertyAnimation *lead_pa;
  Layer *lead_layer;
  GRect root_target;
  GRect lead_target;
  bool exiting;
  bool cleaned;
} MicaTransition;

// Only the top window is ever visible, so one transition at a time is
// enough; starting a new one finishes the current one instantly.
static MicaTransition *s_active;
// Set by mica_transition_register_lead_layer during a window's load or
// appear (which run synchronously inside window_stack_push); consumed
// and cleared by mica_animate_window_push_colored right after the push.
static Layer *s_pending_lead;

void mica_transition_register_lead_layer(Layer *layer) {
  s_pending_lead = layer;
}

static void prv_transition_stopped(Animation *animation, bool finished,
    void *context) {
  MicaTransition *transition = context;
  if (!transition || transition->cleaned) {
    return;
  }
  transition->cleaned = true;
  // Stop the sibling animation (the one whose handler is not running);
  // its stopped handler sees `cleaned` and returns immediately.
  if (transition->root_pa &&
      animation != property_animation_get_animation(transition->root_pa)) {
    animation_unschedule(property_animation_get_animation(transition->root_pa));
  }
  if (transition->lead_pa &&
      animation != property_animation_get_animation(transition->lead_pa)) {
    animation_unschedule(property_animation_get_animation(transition->lead_pa));
  }
  // Whatever happens, layers end at their resting frames: for an exit
  // that is followed by an instant pop this is invisible, and for an
  // exit that turns out not to pop (window no longer on top) it leaves
  // the window exactly where it belongs. If the window already left the
  // stack (e.g. a native Back pop during the entrance), its layers are
  // gone or going — do not touch them.
  if (transition->window && window_stack_contains_window(transition->window)) {
    layer_set_frame(window_get_root_layer(transition->window),
        transition->root_target);
    if (transition->lead_layer) {
      layer_set_frame(transition->lead_layer, transition->lead_target);
    }
  }
  const bool exiting = transition->exiting;
  Window *window = transition->window;
  if (transition->root_pa) {
    property_animation_destroy(transition->root_pa);
  }
  if (transition->lead_pa) {
    property_animation_destroy(transition->lead_pa);
  }
  if (s_active == transition) {
    s_active = NULL;
  }
  free(transition);
  if (exiting && window && window_stack_get_top_window() == window) {
    window_stack_pop(false);
  }
}

// Finish the running transition instantly: unscheduling fires the
// stopped handler synchronously, which snaps frames (and pops, for an
// exit) and frees the context.
static void prv_finish_active(void) {
  if (s_active) {
    animation_unschedule(property_animation_get_animation(s_active->root_pa));
  }
}

void mica_animate_window_push(Window *window) {
  mica_animate_window_push_colored(window, mica_page_color());
}

void mica_animate_window_push_colored(Window *window, GColor background) {
  if (!window) {
    return;
  }
  prv_finish_active();
  mica_page_apply_background_colored(window, background);
  s_pending_lead = NULL;
  window_stack_push(window, false);

  Layer *root = window_get_root_layer(window);
  GRect target = layer_get_frame(root);
  GRect from = target;
  from.origin.x += target.size.w;
  layer_set_frame(root, from);

  MicaTransition *transition = malloc(sizeof(MicaTransition));
  memset(transition, 0, sizeof(MicaTransition));
  transition->window = window;
  transition->root_target = target;
  transition->root_pa = property_animation_create_layer_frame(root, &from,
      &target);
  Animation *animation = property_animation_get_animation(transition->root_pa);
  animation_set_duration(animation, MICA_ENTER_MS);
  animation_set_curve(animation, AnimationCurveCustomFunction);
  animation_set_custom_curve(animation, mica_curve_settle);
  animation_set_handlers(animation, (AnimationHandlers) {
    .stopped = prv_transition_stopped,
  }, transition);

  Layer *lead = s_pending_lead;
  s_pending_lead = NULL;
  if (lead) {
    // The mark leads: it starts now, from a few extra pixels to the
    // right, with the stronger overshoot; the page slide is delayed by
    // MICA_LEAD_MS so the page visibly follows the logo.
    transition->lead_layer = lead;
    transition->lead_target = layer_get_frame(lead);
    GRect lead_from = transition->lead_target;
    lead_from.origin.x += MICA_LEAD_LEAD_PX;
    layer_set_frame(lead, lead_from);
    transition->lead_pa = property_animation_create_layer_frame(lead,
        &lead_from, &transition->lead_target);
    Animation *lead_animation =
        property_animation_get_animation(transition->lead_pa);
    animation_set_duration(lead_animation, MICA_ENTER_MS);
    animation_set_curve(lead_animation, AnimationCurveCustomFunction);
    animation_set_custom_curve(lead_animation, mica_curve_settle_strong);
    animation_set_handlers(lead_animation, (AnimationHandlers) {
      .stopped = prv_transition_stopped,
    }, transition);
    animation_set_delay(animation, MICA_LEAD_MS);
    animation_schedule(lead_animation);
  }
  s_active = transition;
  animation_schedule(animation);
}

void mica_animate_window_pop(Window *window) {
  if (!window) {
    window_stack_pop(false);
    return;
  }
  if (s_active && s_active->window == window && s_active->exiting) {
    // A second pop request while this exit is already sliding: just
    // finish it (and therefore the pop) instantly.
    prv_finish_active();
    return;
  }
  prv_finish_active();
  Layer *root = window_get_root_layer(window);
  GRect target = layer_get_frame(root);
  GRect to = target;
  to.origin.x += target.size.w;

  MicaTransition *transition = malloc(sizeof(MicaTransition));
  memset(transition, 0, sizeof(MicaTransition));
  transition->window = window;
  transition->root_target = target;
  transition->exiting = true;
  transition->root_pa = property_animation_create_layer_frame(root, &target,
      &to);
  Animation *animation = property_animation_get_animation(transition->root_pa);
  animation_set_duration(animation, MICA_EXIT_MS);
  animation_set_curve(animation, AnimationCurveEaseIn);
  animation_set_handlers(animation, (AnimationHandlers) {
    .stopped = prv_transition_stopped,
  }, transition);
  s_active = transition;
  animation_schedule(animation);
}
