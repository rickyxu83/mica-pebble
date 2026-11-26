/* Copyright 2026 Mica contributors. Licensed under Apache 2.0. */

#include "face_layer.h"

#include "memory/sdk.h"

// Face geometry, as fractions of the layer bounds: the eyes sit at
// 0.24/0.76 of the width and 0.42 of the height, the mouth is centred
// at 0.64 of the height, and the resting capsule eye is 0.19 of the
// width by 0.32 of the height. Everything derives from the bounds, so
// one implementation reads from the 64 px mascot down to the
// thinking face.
#define FACE_EYE_LEFT_X_NUM 24
#define FACE_EYE_RIGHT_X_NUM 76
#define FACE_EYE_Y_NUM 42
#define FACE_MOUTH_Y_NUM 64
#define FACE_PERCENT 100

typedef struct {
  FaceEyeKind left_eye;
  FaceEyeKind right_eye;
  FaceMouthKind mouth;
  FaceProp prop;
  int16_t look_x;
  int16_t look_y;
  uint32_t wave_phase;
  uint16_t intro_progress;
} FaceLayerData;

static void prv_layer_render(Layer *layer, GContext *ctx);

FaceLayer *face_layer_create(GRect frame) {
  FaceLayer *layer = blayer_create_with_data(frame, sizeof(FaceLayerData));
  FaceLayerData *data = layer_get_data(layer);
  data->left_eye = FaceEyeDot;
  data->right_eye = FaceEyeDot;
  data->mouth = FaceMouthSmile;
  data->prop = FacePropNone;
  data->intro_progress = 1000;
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

void face_layer_set_expression(FaceLayer *layer, FaceEyeKind left_eye,
    FaceEyeKind right_eye, FaceMouthKind mouth, int16_t look_x,
    int16_t look_y, uint32_t wave_phase, FaceProp prop) {
  FaceLayerData *data = layer_get_data(layer);
  data->left_eye = left_eye;
  data->right_eye = right_eye;
  data->mouth = mouth;
  data->prop = prop;
  data->look_x = look_x;
  data->look_y = look_y;
  data->wave_phase = wave_phase;
  layer_mark_dirty(layer);
}

void face_layer_set_intro_progress(FaceLayer *layer, uint16_t permille) {
  FaceLayerData *data = layer_get_data(layer);
  data->intro_progress = permille > 1000 ? 1000 : permille;
  layer_mark_dirty(layer);
}

static int16_t prv_stroke_width(const GRect *bounds) {
  int16_t width = bounds->size.w * 8 / FACE_PERCENT;
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
      int16_t eye_w = w * 19 / FACE_PERCENT;
      int16_t eye_h = h * 32 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, center_y,
          eye_w < 3 ? 3 : eye_w, eye_h < 3 ? 3 : eye_h);
      break;
    }
    case FaceEyeBlink: {
      int16_t eye_w = w * 22 / FACE_PERCENT;
      int16_t eye_h = h * 7 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, center_y,
          eye_w < 4 ? 4 : eye_w, eye_h < 2 ? 2 : eye_h);
      break;
    }
    case FaceEyeHappy: {
      // An upward arc (^^): the top half of a circle, drawn from the
      // left point (270 deg) over the top to the right point (90 deg);
      // graphics_draw_arc normalises the wrapped end angle.
      int16_t diameter = w * 26 / FACE_PERCENT;
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
      int16_t radius = w * 7 / FACE_PERCENT;
      graphics_fill_circle(ctx, GPoint(center_x, center_y),
          radius < 2 ? 2 : radius);
      break;
    }
    case FaceEyeHeart: {
      // A filled heart: two circular lobes plus a downward triangle
      // whose base spans the lobes' outer edges and whose apex is the
      // bottom point. The GPath is created, drawn and destroyed within
      // this call, so the stack points stay valid throughout.
      int16_t heart_w = w * 26 / FACE_PERCENT;
      int16_t heart_h = h * 30 / FACE_PERCENT;
      if (heart_w < 8) {
        heart_w = 8;
      }
      if (heart_h < 8) {
        heart_h = 8;
      }
      int16_t lobe_r = heart_w * 27 / 100;
      int16_t lobe_dx = heart_w * 23 / 100;
      int16_t lobe_y = center_y - heart_h * 18 / 100;
      graphics_fill_circle(ctx, GPoint(center_x - lobe_dx, lobe_y),
          lobe_r < 2 ? 2 : lobe_r);
      graphics_fill_circle(ctx, GPoint(center_x + lobe_dx, lobe_y),
          lobe_r < 2 ? 2 : lobe_r);
      GPoint points[3] = {
          GPoint(center_x - heart_w / 2, lobe_y),
          GPoint(center_x + heart_w / 2, lobe_y),
          GPoint(center_x, center_y + heart_h * 55 / 100),
      };
      GPathInfo info = {.num_points = 3, .points = points};
      GPath *path = gpath_create(&info);
      if (path) {
        gpath_draw_filled(ctx, path);
        gpath_destroy(path);
      }
      break;
    }
    case FaceEyeSleepy: {
      // A gentle downward arc (‿): the bottom half of a circle, the
      // same 90 -> 270 sweep the omega mouth uses.
      int16_t diameter = w * 24 / FACE_PERCENT;
      if (diameter < 6) {
        diameter = 6;
      }
      graphics_draw_arc(ctx,
          GRect(center_x - diameter / 2, center_y - diameter / 2,
              diameter, diameter),
          GOvalScaleModeFitCircle,
          DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
      break;
    }
  }
}

// The omega mouth at a given scale (permille of full size; the intro
// morph grows it in from small). Two bottom-half arcs (∪∪) side by
// side, their circles touching at the centre so the pair reads as a
// tiny "ω". A bottom arc sweeps 90 -> 270 in this codebase's angle
// convention (the smile arc's 120 -> 240 passes through 180, the
// bottom point).
static void prv_draw_mouth_omega(GContext *ctx, const GRect *bounds,
    uint16_t scale_permille) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  const int16_t center_x = bounds->origin.x + w / 2;
  const int16_t mouth_y = bounds->origin.y +
      h * FACE_MOUTH_Y_NUM / FACE_PERCENT;
  int16_t radius = (int16_t)(w * 85 / 1000 * scale_permille / 1000);
  if (radius < 2) {
    radius = 2;
  }
  int16_t stroke = (int16_t)(h * 65 / 1000 * scale_permille / 1000);
  graphics_context_set_stroke_width(ctx, stroke < 2 ? 2 : stroke);
  for (int side = -1; side <= 1; side += 2) {
    int16_t cx = center_x + side * radius;
    graphics_draw_arc(ctx,
        GRect(cx - radius, mouth_y - radius, radius * 2, radius * 2),
        GOvalScaleModeFitCircle,
        DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
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
      int16_t radius = w * (big ? 27 : 21) / FACE_PERCENT;
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
      int16_t mouth_w = w * 24 / FACE_PERCENT;
      int16_t mouth_h = h * 7 / FACE_PERCENT;
      prv_fill_capsule(ctx, center_x, mouth_y,
          mouth_w < 4 ? 4 : mouth_w, mouth_h < 2 ? 2 : mouth_h);
      break;
    }
    case FaceMouthOmega: {
      prv_draw_mouth_omega(ctx, bounds, 1000);
      break;
    }
    case FaceMouthWave: {
      // A sine polyline spanning two full cycles, its amplitude tapered
      // to zero at both ends so it reads as a voice waveform.
      const int16_t half_w = w * 24 / FACE_PERCENT;
      const int16_t amplitude = h * 8 / FACE_PERCENT;
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

// Sunglasses replace the eyes: two filled lenses centred on the eye
// positions, joined by a bridge, with temple lines running out to the
// edges of the box.
static void prv_draw_sunglasses(GContext *ctx, const GRect *bounds,
    int16_t left_x, int16_t right_x, int16_t eye_y) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  int16_t lens_w = w * 30 / FACE_PERCENT;
  int16_t lens_h = h * 20 / FACE_PERCENT;
  if (lens_w < 6) {
    lens_w = 6;
  }
  if (lens_h < 4) {
    lens_h = 4;
  }
  const int16_t bridge_y = eye_y - lens_h / 4;
  const int16_t radius = lens_h * 2 / 5;
  graphics_fill_rect(ctx,
      GRect(left_x - lens_w / 2, eye_y - lens_h / 2, lens_w, lens_h),
      radius, GCornersAll);
  graphics_fill_rect(ctx,
      GRect(right_x - lens_w / 2, eye_y - lens_h / 2, lens_w, lens_h),
      radius, GCornersAll);
  graphics_draw_line(ctx, GPoint(left_x + lens_w / 2, bridge_y),
      GPoint(right_x - lens_w / 2, bridge_y));
  graphics_draw_line(ctx, GPoint(bounds->origin.x + 1, bridge_y),
      GPoint(left_x - lens_w / 2, bridge_y));
  graphics_draw_line(ctx, GPoint(right_x + lens_w / 2, bridge_y),
      GPoint(bounds->origin.x + w - 1, bridge_y));
}

// A small flower held at the lower right: a stem with a leaf and five
// petals around a centre dot, all in the face ink so the blossom reads
// as a simple silhouette.
static void prv_draw_flower(GContext *ctx, const GRect *bounds) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  const int16_t cx = bounds->origin.x + w * 82 / FACE_PERCENT;
  const int16_t cy = bounds->origin.y + h * 64 / FACE_PERCENT;
  const int16_t stem_bottom = bounds->origin.y + h * 86 / FACE_PERCENT;
  graphics_draw_line(ctx, GPoint(cx, cy),
      GPoint(cx - w * 2 / FACE_PERCENT, stem_bottom));
  int16_t leaf_r = w * 4 / FACE_PERCENT;
  graphics_fill_circle(ctx,
      GPoint(cx - w * 6 / FACE_PERCENT, (cy + stem_bottom) / 2),
      leaf_r < 2 ? 2 : leaf_r);
  const int16_t petal_r = w * 5 / FACE_PERCENT;
  const int16_t petal_d = w * 6 / FACE_PERCENT;
  for (int i = 0; i < 5; ++i) {
    uint32_t angle = (uint32_t)i * TRIG_MAX_ANGLE / 5 + DEG_TO_TRIGANGLE(18);
    int16_t px = cx + (int16_t)(petal_d * sin_lookup(angle) / TRIG_MAX_RATIO);
    // cos(a) via the shifted sine keeps to the one trig primitive the
    // rest of the face uses.
    int16_t py = cy - (int16_t)(petal_d *
        sin_lookup(angle + TRIG_MAX_ANGLE / 4) / TRIG_MAX_RATIO);
    graphics_fill_circle(ctx, GPoint(px, py), petal_r < 2 ? 2 : petal_r);
  }
  int16_t centre_r = w * 3 / FACE_PERCENT;
  graphics_fill_circle(ctx, GPoint(cx, cy), centre_r < 2 ? 2 : centre_r);
}

// A music note at the upper right: a round note head, a stem rising
// from its right side, and a short flag flicking off the stem's top.
static void prv_draw_music_note(GContext *ctx, const GRect *bounds) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  int16_t head_r = w * 6 / FACE_PERCENT;
  if (head_r < 3) {
    head_r = 3;
  }
  const int16_t head_x = bounds->origin.x + w * 76 / FACE_PERCENT;
  const int16_t head_y = bounds->origin.y + h * 32 / FACE_PERCENT;
  const int16_t stem_x = head_x + head_r;
  const int16_t stem_top = bounds->origin.y + h * 12 / FACE_PERCENT;
  graphics_fill_circle(ctx, GPoint(head_x, head_y), head_r);
  graphics_draw_line(ctx, GPoint(stem_x, head_y), GPoint(stem_x, stem_top));
  graphics_draw_line(ctx, GPoint(stem_x, stem_top),
      GPoint(stem_x + w * 9 / FACE_PERCENT,
          stem_top + h * 8 / FACE_PERCENT));
}

static void prv_draw_one_z(GContext *ctx, int16_t x, int16_t y,
    int16_t zw, int16_t zh) {
  graphics_draw_line(ctx, GPoint(x, y), GPoint(x + zw, y));
  graphics_draw_line(ctx, GPoint(x + zw, y), GPoint(x, y + zh));
  graphics_draw_line(ctx, GPoint(x, y + zh), GPoint(x + zw, y + zh));
}

// Two "Z"s floating at the upper right, the second smaller and higher.
// wave_phase carries the act's elapsed milliseconds here: the Zs
// drift upward a few pixels and loop over the course of the act.
static void prv_draw_zzz(GContext *ctx, const GRect *bounds,
    uint32_t elapsed_ms) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  const int16_t drift = (int16_t)((elapsed_ms % 1200) * 6 / 1200);
  int16_t big_w = w * 12 / FACE_PERCENT;
  int16_t big_h = h * 12 / FACE_PERCENT;
  if (big_w < 4) {
    big_w = 4;
  }
  if (big_h < 4) {
    big_h = 4;
  }
  const int16_t big_x = bounds->origin.x + w * 68 / FACE_PERCENT;
  const int16_t big_y = bounds->origin.y + h * 24 / FACE_PERCENT - drift;
  prv_draw_one_z(ctx, big_x, big_y, big_w, big_h);
  int16_t small_w = big_w * 2 / 3;
  int16_t small_h = big_h * 2 / 3;
  prv_draw_one_z(ctx, big_x + w * 16 / FACE_PERCENT,
      big_y - h * 12 / FACE_PERCENT,
      small_w < 3 ? 3 : small_w, small_h < 3 ? 3 : small_h);
}

static int16_t prv_lerp(int16_t from, int16_t to, uint16_t permille) {
  return from + (int16_t)((int32_t)(to - from) * permille / 1000);
}

// The bars -> face intro morph. At progress 0 the layer shows the
// launcher icon's three sound bars: two shorter side bars evenly
// spaced around a tall middle bar, all centred on the eye line. Over
// the first 60% the side bars glide outward into the eye positions
// and grow into the capsule eyes; the middle bar shrinks toward the
// mouth line over the first 70% and disappears; the omega mouth
// grows in over the last 40%. Single ink throughout, no alpha.
static void prv_draw_intro(GContext *ctx, const GRect *bounds,
    const FaceLayerData *data) {
  const int16_t w = bounds->size.w;
  const int16_t h = bounds->size.h;
  const int16_t ox = bounds->origin.x;
  const int16_t oy = bounds->origin.y;
  const uint16_t p = data->intro_progress;
  const uint16_t glide = p < 600 ? (uint16_t)(p * 1000 / 600) : 1000;
  const int16_t eye_y = oy + h * FACE_EYE_Y_NUM / FACE_PERCENT;
  const int16_t bar_w = prv_lerp(w * 13 / FACE_PERCENT,
      w * 19 / FACE_PERCENT, glide);
  const int16_t bar_h = prv_lerp(h * 22 / FACE_PERCENT,
      h * 32 / FACE_PERCENT, glide);
  const int16_t left_x = ox + prv_lerp(w * 32 / FACE_PERCENT,
      w * FACE_EYE_LEFT_X_NUM / FACE_PERCENT, glide);
  const int16_t right_x = ox + prv_lerp(w * 68 / FACE_PERCENT,
      w * FACE_EYE_RIGHT_X_NUM / FACE_PERCENT, glide);
  prv_fill_capsule(ctx, left_x, eye_y,
      bar_w < 3 ? 3 : bar_w, bar_h < 3 ? 3 : bar_h);
  prv_fill_capsule(ctx, right_x, eye_y,
      bar_w < 3 ? 3 : bar_w, bar_h < 3 ? 3 : bar_h);
  if (p < 700) {
    const uint16_t shrink = (uint16_t)(p * 1000 / 700);
    const int16_t mid_w = prv_lerp(w * 14 / FACE_PERCENT,
        w * 5 / FACE_PERCENT, shrink);
    const int16_t mid_h = prv_lerp(h * 40 / FACE_PERCENT, 0, shrink);
    const int16_t mid_y = prv_lerp(eye_y,
        oy + h * FACE_MOUTH_Y_NUM / FACE_PERCENT, shrink);
    if (mid_h >= 2) {
      prv_fill_capsule(ctx, ox + w / 2, mid_y,
          mid_w < 2 ? 2 : mid_w, mid_h);
    }
  }
  if (p >= 600) {
    if (data->mouth == FaceMouthOmega) {
      prv_draw_mouth_omega(ctx, bounds,
          (uint16_t)((p - 600) * 1000 / 400));
    } else {
      prv_draw_mouth(ctx, bounds, data);
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

  if (data->intro_progress < 1000) {
    prv_draw_intro(ctx, &bounds, data);
    return;
  }

  const int16_t eye_y = bounds.origin.y +
      bounds.size.h * FACE_EYE_Y_NUM / FACE_PERCENT + data->look_y;
  const int16_t left_x = bounds.origin.x +
      bounds.size.w * FACE_EYE_LEFT_X_NUM / FACE_PERCENT + data->look_x;
  const int16_t right_x = bounds.origin.x +
      bounds.size.w * FACE_EYE_RIGHT_X_NUM / FACE_PERCENT + data->look_x;
  if (data->prop == FacePropSunglasses) {
    prv_draw_sunglasses(ctx, &bounds, left_x, right_x, eye_y);
  } else {
    prv_draw_eye(ctx, &bounds, data->left_eye, left_x, eye_y);
    prv_draw_eye(ctx, &bounds, data->right_eye, right_x, eye_y);
  }
  prv_draw_mouth(ctx, &bounds, data);
  switch (data->prop) {
    case FacePropFlower:
      prv_draw_flower(ctx, &bounds);
      break;
    case FacePropMusicNote:
      prv_draw_music_note(ctx, &bounds);
      break;
    case FacePropZzz:
      prv_draw_zzz(ctx, &bounds, data->wave_phase);
      break;
    default:
      break;
  }
}
