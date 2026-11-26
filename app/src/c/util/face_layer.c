/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */

#include "face_layer.h"

#include "memory/sdk.h"

// Face geometry, as fractions of the layer bounds: the eyes sit at
// 0.29/0.71 of the width and 0.44 of the height, the mouth is centred
// at 0.63 of the height, and the resting capsule eye is 0.12 of the
// width by 0.22 of the height (style 2: shorter, fatter eyes with the
// tiny omega mouth below). Everything derives from the bounds, so one
// implementation reads from the 64 px mascot down to the thinking face.
#define FACE_EYE_LEFT_X_NUM 29
#define FACE_EYE_RIGHT_X_NUM 71
#define FACE_EYE_Y_NUM 44
#define FACE_MOUTH_Y_NUM 63
#define FACE_PERCENT 100

typedef struct {
  FaceEyeKind eyes;
  FaceMouthKind mouth;
  int16_t look_x;
  int16_t look_y;
  uint32_t wave_phase;
} FaceLayerData;

static void prv_layer_render(Layer *layer, GContext *ctx);

FaceLayer *face_layer_create(GRect frame) {
  FaceLayer *layer = blayer_create_with_data(frame, sizeof(FaceLayerData));
  FaceLayerData *data = layer_get_data(layer);
  data->eyes = FaceEyeDot;
  data->mouth = FaceMouthSmile;
  data->look_x = 0;
  data->look_y = 0;
  data->wave_phase = 0;
  layer_set_update_proc(layer, prv_layer_render);
  return layer;
}

void face_layer_destroy(FaceLayer *layer) {
  layer_destroy(layer);
}

Layer *face_layer_get_layer(FaceLayer *layer) {
  return layer;
}

void face_layer_set_expression(FaceLayer *layer, FaceEyeKind eyes,
    FaceMouthKind mouth, int16_t look_x, int16_t look_y,
    uint32_t wave_phase) {
  FaceLayerData *data = layer_get_data(layer);
  data->eyes = eyes;
  data->mouth = mouth;
  data->look_x = look_x;
  data->look_y = look_y;
  data->wave_phase = wave_phase;
  layer_mark_dirty(layer);
}

static int16_t prv_stroke_width(const GRect *bounds) {
  int16_t width = bounds->size.w * 6 / FACE_PERCENT;
  return width < 2 ? 2 : width;
}

// A capsule (stadium) centred on a point, drawn as a rounded rect whose
// corner radius is half the short side.
static void prv_fill_capsule(GContext *ctx, int16_t center_x, int16_t center_y,
    int16_t width, int16_t height) {
  if (width < 1 || height < 1) {
    return;
  }
  int16_t radius = (width < height ? width : height) / 2;
  graphics_fill_rect(ctx,
      GRect(center_x - width / 2, center_y - height / 2, width, height),
      radius, GCornersAll);
}

static void prv_draw_eye(GContext *ctx, const GRect *bounds, FaceEyeKind kind,
    int16_t center_x, int16_t center_y) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  switch (kind) {
    case FaceEyeDot: {
      int16_t eye_w = w * 12 / FACE_PERCENT;
      int16_t eye_h = h * 22 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, center_y,
          eye_w < 3 ? 3 : eye_w, eye_h < 3 ? 3 : eye_h);
      break;
    }
    case FaceEyeBlink: {
      int16_t eye_w = w * 16 / FACE_PERCENT;
      int16_t eye_h = h * 5 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, center_y,
          eye_w < 4 ? 4 : eye_w, eye_h < 2 ? 2 : eye_h);
      break;
    }
    case FaceEyeHappy: {
      // An upward arc (^^): the top half of a circle, drawn from the
      // left point (270 deg) over the top to the right point (90 deg);
      // graphics_draw_arc normalises the wrapped end angle.
      int16_t diameter = w * 18 / FACE_PERCENT;
      if (diameter < 6) {
        diameter = 6;
      }
      graphics_draw_arc(ctx,
          GRect(center_x - diameter / 2, center_y - diameter / 2,
              diameter, diameter),
          GOvalScaleModeFitCircle,
          DEG_TO_TRIGANGLE(270), DEG_TO_TRIGANGLE(450));
      break;
    }
    case FaceEyeSmall: {
      int16_t radius = w * 5 / FACE_PERCENT;
      graphics_fill_circle(ctx, GPoint(center_x, center_y),
          radius < 2 ? 2 : radius);
      break;
    }
  }
}

static void prv_draw_mouth(GContext *ctx, const GRect *bounds,
    const FaceLayerData *data) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  const int16_t center_x = bounds->origin.x + w / 2;
  const int16_t mouth_y = bounds->origin.y + h * FACE_MOUTH_Y_NUM / FACE_PERCENT;
  switch (data->mouth) {
    case FaceMouthSmile:
    case FaceMouthBigSmile: {
      // The bottom arc of a circle whose centre sits above the mouth
      // line, so the arc's midpoint lands on the mouth centre.
      const bool big = data->mouth == FaceMouthBigSmile;
      int16_t radius = w * (big ? 20 : 15) / FACE_PERCENT;
      if (radius < 3) {
        radius = 3;
      }
      int16_t circle_y = mouth_y - radius * 3 / 4;
      graphics_draw_arc(ctx,
          GRect(center_x - radius, circle_y - radius, radius * 2, radius * 2),
          GOvalScaleModeFitCircle,
          DEG_TO_TRIGANGLE(big ? 110 : 120),
          DEG_TO_TRIGANGLE(big ? 250 : 240));
      break;
    }
    case FaceMouthFlat: {
      int16_t mouth_w = w * 18 / FACE_PERCENT;
      int16_t mouth_h = h * 5 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, mouth_y,
          mouth_w < 4 ? 4 : mouth_w, mouth_h < 2 ? 2 : mouth_h);
      break;
    }
    case FaceMouthOmega: {
      // Two bottom-half arcs (∪∪) side by side, their circles touching
      // at the centre so the pair reads as a tiny "ω". A bottom arc
      // sweeps 90 -> 270 in this codebase's angle convention (the smile
      // arc's 120 -> 240 passes through 180, the bottom point).
      int16_t radius = w * 55 / 1000;
      if (radius < 3) {
        radius = 3;
      }
      int16_t stroke = h * 45 / 1000;
      graphics_context_set_stroke_width(ctx, stroke < 2 ? 2 : stroke);
      for (int side = -1; side <= 1; side += 2) {
        int16_t cx = center_x + side * radius;
        graphics_draw_arc(ctx,
            GRect(cx - radius, mouth_y - radius, radius * 2, radius * 2),
            GOvalScaleModeFitCircle,
            DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
      }
      break;
    }
    case FaceMouthWave: {
      // A sine polyline spanning two full cycles, its amplitude tapered
      // to zero at both ends so it reads as a voice waveform.
      const int16_t half_w = w * 17 / FACE_PERCENT;
      const int16_t amplitude = h * 6 / FACE_PERCENT;
      const int segments = 12;
      GPoint prev = GPoint(center_x - half_w, mouth_y);
      for (int i = 1; i <= segments; ++i) {
        int16_t x = center_x - half_w + (int16_t)(2 * half_w * i / segments);
        uint32_t envelope_angle = (uint32_t)(DEG_TO_TRIGANGLE(180) * i / segments);
        int32_t envelope = sin_lookup(envelope_angle);
        uint32_t wave_angle = data->wave_phase +
            (uint32_t)i * (TRIG_MAX_ANGLE * 2 / segments);
        int32_t offset = (int32_t)((int64_t)sin_lookup(wave_angle) * amplitude /
            TRIG_MAX_RATIO * envelope / TRIG_MAX_RATIO);
        GPoint point = GPoint(x, mouth_y + (int16_t)offset);
        graphics_draw_line(ctx, prev, point);
        prev = point;
      }
      break;
    }
  }
}

static void prv_layer_render(Layer *layer, GContext *ctx) {
  FaceLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  if (bounds.size.w <= 0 || bounds.size.h <= 0) {
    return;
  }
  const GColor ink = COLOR_FALLBACK(GColorMidnightGreen, GColorBlack);
  graphics_context_set_fill_color(ctx, ink);
  graphics_context_set_stroke_color(ctx, ink);
  graphics_context_set_stroke_width(ctx, prv_stroke_width(&bounds));
  graphics_context_set_antialiased(ctx, true);

  const int16_t eye_y = bounds.origin.y +
      bounds.size.h * FACE_EYE_Y_NUM / FACE_PERCENT + data->look_y;
  const int16_t left_x = bounds.origin.x +
      bounds.size.w * FACE_EYE_LEFT_X_NUM / FACE_PERCENT + data->look_x;
  const int16_t right_x = bounds.origin.x +
      bounds.size.w * FACE_EYE_RIGHT_X_NUM / FACE_PERCENT + data->look_x;
  prv_draw_eye(ctx, &bounds, data->eyes, left_x, eye_y);
  prv_draw_eye(ctx, &bounds, data->eyes, right_x, eye_y);
  prv_draw_mouth(ctx, &bounds, data);
}
