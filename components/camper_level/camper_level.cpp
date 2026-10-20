#include "camper_level.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome::camper_level {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr float MAX_TILT_DEG = 25.0f;  // the vans never tilt further, however large the angle
static constexpr float DEG = 3.14159265f / 180.0f;

// Shows or hides a part, only when that changes.
static void show(lv_obj_t *obj, bool shown) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == !shown) return;
  if (shown) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// ---------- Data ----------

// A number of an attribute, sent as a number or as a text.
static float number(JsonVariantConst v, float fallback) {
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  const char *s = v | "";
  if (!*s) return fallback;
  char *end = nullptr;
  const float f = strtof(s, &end);
  return end == s ? fallback : f;
}

void Level::read(JsonObjectConst data) {
  const char *state = data["state"] | "";
  if (!data["state"].is<const char *>()) {
    mode = CHOOSE;
    return;
  }
  if (!strcmp(state, "unavailable") || !strcmp(state, "unknown") || !*state) {
    mode = OFFLINE;
    return;
  }
  JsonObjectConst a = data["attributes"];
  pitch = number(a["pitch"], 0);
  roll = number(a["roll"], 0);
  static const char *const KEYS[4] = {"wedge_fl", "wedge_fr", "wedge_rl", "wedge_rr"};
  for (int i = 0; i < 4; ++i) wedge[i] = number(a[KEYS[i]], -1);
  mode = LIVE;
}

// The tone of an angle: green while level, amber while nearly, red above.
static Tone tone_of(float deg, float good, float warn) {
  const float a = std::fabs(deg);
  return a <= good + 0.001f ? Tone::GOOD : a <= warn + 0.001f ? Tone::BUSY : Tone::ALERT;
}

// "1.2°" (or "1,2°") with the screen's decimal mark; the degree sign only where the font has it.
static std::string degrees(const tessera::Plugin *p, float v, Font font) {
  char buf[16];
  if (std::fabs(v) < 0.05f) v = 0;  // never "-0.0"
  snprintf(buf, sizeof buf, "%.1f", v);
  std::string s = buf;
  const std::string mark = p->text("decimal");
  const size_t dot = s.find('.');
  if (dot != std::string::npos && !mark.empty()) s.replace(dot, 1, mark);
  const std::string with = s + "°";
  return ui::covers(with, font) ? with : s;
}

// The largest font whose line fits `h` and in which `sample` fits `w`.
static Font fitting_font(const std::string &sample, int w, int h) {
  for (Font f : {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
    if (f == Font::DIGITS && !ui::covers(sample, f)) continue;
    if (ui::line_height(f) <= h && ui::text_width(sample, f) <= w) return f;
  }
  return Font::BODY;
}

// ---------- A van that tilts ----------
// Side view (pitch, the front on the right) or rear view (roll), drawn as thick lines and two round wheels in a box.
// In units u: the side view is 52 x 18 with its wheels under it, the rear view 32 x 24.
class Van {
 public:
  // The width and height a box needs for a unit u, room for the tilt included.
  static int box_w(float u) { return (int) std::ceil(60 * u); }
  static int box_h(float u) { return (int) std::ceil(52 * u); }
  static float unit_for(int w, int h) { return std::max(0.25f, std::min(w / 60.0f, h / 52.0f)); }

  void make(lv_obj_t *parent, bool side, int x, int y, float u) {
    side_ = side;
    u_ = u;
    cx_ = box_w(u) / 2.0f;
    cy_ = box_h(u) / 2.0f;
    for (int i = 0; i < 2; ++i) {
      line_[i] = lv_line_create(parent);
      lv_obj_set_pos(line_[i], x, y);
      lv_obj_remove_flag(line_[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_line_rounded(line_[i], false, 0);
    }
    lv_obj_set_style_line_width(line_[0], std::max(2, (int) std::lround((side ? 18 : 24) * u)), 0);
    lv_obj_set_style_line_width(line_[1], std::max(1, (int) std::lround((side ? 5 : 7) * u)), 0);
    if (side) {
      // A second window: the cab's on the right, the living room's on the left.
      win2_ = lv_line_create(parent);
      lv_obj_set_pos(win2_, x, y);
      lv_obj_remove_flag(win2_, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_line_rounded(win2_, false, 0);
      lv_obj_set_style_line_width(win2_, std::max(1, (int) std::lround(5 * u)), 0);
    }
    for (int i = 0; i < 2; ++i) {
      wheel_[i] = ui::block(parent, theme::INK);
      const int d = std::max(3, (int) std::lround(8 * u));
      lv_obj_set_size(wheel_[i], d, d);
      lv_obj_set_style_radius(wheel_[i], LV_RADIUS_CIRCLE, 0);
    }
    x_ = x;
    y_ = y;
    angle_ = 1000;
    set(0, Tone::NORMAL);
  }

  // Tilts the van to `deg` (screen degrees, clockwise) in the colour of `tone`.
  void set(float deg, Tone tone) {
    deg = std::max(-MAX_TILT_DEG, std::min(MAX_TILT_DEG, deg));
    const int key = (int) std::lround(deg * 10);
    if (key != angle_) {
      angle_ = key;
      const float a = deg * DEG, ca = std::cos(a), sa = std::sin(a);
      auto put = [&](lv_point_precise_t &p, float x, float y) {
        p.x = (lv_value_precise_t) std::lround(cx_ + (x * ca - y * sa) * u_);
        p.y = (lv_value_precise_t) std::lround(cy_ + (x * sa + y * ca) * u_);
      };
      if (side_) {
        put(pts_[0][0], -26, 0), put(pts_[0][1], 26, 0);       // the body
        put(pts_[1][0], 10, -4), put(pts_[1][1], 22, -4);      // the cab's window
        put(pts_[2][0], -20, -4), put(pts_[2][1], -8, -4);     // a window of the living room
        lv_line_set_points(win2_, pts_[2], 2);
      } else {
        put(pts_[0][0], -16, 0), put(pts_[0][1], 16, 0);
        put(pts_[1][0], -12, -6), put(pts_[1][1], 12, -6);     // the rear window
      }
      lv_line_set_points(line_[0], pts_[0], 2);
      lv_line_set_points(line_[1], pts_[1], 2);
      const float wx = side_ ? 15 : 12, wy = side_ ? 10 : 13;
      for (int i = 0; i < 2; ++i) {
        const float x = i ? wx : -wx;
        const int d = lv_obj_get_width(wheel_[i]);
        const int px = (int) std::lround(cx_ + (x * ca - wy * sa) * u_) - d / 2;
        const int py = (int) std::lround(cy_ + (x * sa + wy * ca) * u_) - d / 2;
        lv_obj_set_pos(wheel_[i], x_ + px, y_ + py);
      }
    }
    if (tone != tone_) {
      tone_ = tone;
      paint();
    }
  }

  void visible(bool shown) {
    for (auto *o : {line_[0], line_[1], win2_, wheel_[0], wheel_[1]}) show(o, shown);
  }

  // Light and dark: the colours again.
  void paint() {
    if (!line_[0]) return;
    lv_obj_set_style_line_color(line_[0], ui::tone_color(tone_, theme::MUTED), 0);
    lv_obj_set_style_line_color(line_[1], ui::color(theme::CARD), 0);
    if (win2_) lv_obj_set_style_line_color(win2_, ui::color(theme::CARD), 0);
    for (auto *w : wheel_) lv_obj_set_style_bg_color(w, ui::color(theme::INK_SOFT), 0);
  }

 private:
  bool side_ = true;
  float u_ = 1, cx_ = 0, cy_ = 0;
  int x_ = 0, y_ = 0, angle_ = 1000;
  Tone tone_ = Tone::ACCENT;
  lv_obj_t *line_[2]{}, *win2_{}, *wheel_[2]{};
  lv_point_precise_t pts_[3][2]{};
};

// ---------- The bubble ----------
class Bubble {
 public:
  void make(lv_obj_t *parent, int x, int y, int d) {
    x_ = x;
    y_ = y;
    d_ = d;
    ring_ = ui::block(parent, theme::TRACK);
    lv_obj_set_pos(ring_, x, y);
    lv_obj_set_size(ring_, d, d);
    lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
    // The half-way ring and the cross.
    inner_ = ui::block(parent, theme::TRACK);
    const int di = d / 2;
    lv_obj_set_pos(inner_, x + (d - di) / 2, y + (d - di) / 2);
    lv_obj_set_size(inner_, di, di);
    lv_obj_set_style_radius(inner_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_, std::max(1, ui::px(1)), 0);
    const int m = d / 10;
    cross_pts_[0][0] = {(lv_value_precise_t) (x + m), (lv_value_precise_t) (y + d / 2)};
    cross_pts_[0][1] = {(lv_value_precise_t) (x + d - m), (lv_value_precise_t) (y + d / 2)};
    cross_pts_[1][0] = {(lv_value_precise_t) (x + d / 2), (lv_value_precise_t) (y + m)};
    cross_pts_[1][1] = {(lv_value_precise_t) (x + d / 2), (lv_value_precise_t) (y + d - m)};
    for (int i = 0; i < 2; ++i) {
      cross_[i] = lv_line_create(parent);
      lv_obj_remove_flag(cross_[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_line_width(cross_[i], std::max(1, ui::px(1)), 0);
      lv_line_set_points(cross_[i], cross_pts_[i], 2);
    }
    bubble_d_ = std::max(ui::px(10), d / 6);
    bubble_ = ui::block(parent, theme::MUTED);
    lv_obj_set_size(bubble_, bubble_d_, bubble_d_);
    lv_obj_set_style_radius(bubble_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(bubble_, std::max(1, ui::px(2)), 0);
    paint();
    set(0, 0, false, Tone::NORMAL);
  }

  // The bubble at dx, dy of the ring's radius (-1 to 1 each, cut at the ring), or hidden.
  void set(float dx, float dy, bool shown, Tone tone) {
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1) dx /= len, dy /= len;
    const float r = (d_ - bubble_d_) / 2.0f - ui::px(2);
    lv_obj_set_pos(bubble_, x_ + (int) std::lround(d_ / 2.0f + dx * r - bubble_d_ / 2.0f),
                   y_ + (int) std::lround(d_ / 2.0f + dy * r - bubble_d_ / 2.0f));
    for (auto *o : {ring_, inner_, cross_[0], cross_[1], bubble_}) show(o, shown);
    ui::set_fill(bubble_, tone, theme::MUTED);
  }

  void paint() {
    if (!ring_) return;
    lv_obj_set_style_bg_color(ring_, ui::color(theme::TRACK), 0);
    lv_obj_set_style_border_color(inner_, ui::color(theme::SUBTLE), 0);
    for (auto *c : cross_) lv_obj_set_style_line_color(c, ui::color(theme::SUBTLE), 0);
    lv_obj_set_style_border_color(bubble_, ui::color(theme::CARD), 0);
  }

 private:
  int x_ = 0, y_ = 0, d_ = 0, bubble_d_ = 0;
  lv_obj_t *ring_{}, *inner_{}, *cross_[2]{}, *bubble_{};
  lv_point_precise_t cross_pts_[2][2]{};
};

// ---------- One gauge: a van and its angle ----------
struct Gauge {
  Van van;
  lv_obj_t *value{};
  Font font = Font::BODY;
  bool shown = false;

  // Lays the van and its number out in a room: side by side when the room is wide, the number under the van else.
  void make(lv_obj_t *parent, bool side, int x, int y, int w, int h) {
    const std::string sample = "-10.0°";
    if (w >= h * 1.6f) {
      const float u = Van::unit_for(w * 45 / 100, h);
      const int bw = Van::box_w(u), bh = Van::box_h(u);
      van.make(parent, side, x, y + (h - bh) / 2, u);
      const int lw = w - bw - ui::px(4);
      font = fitting_font(sample, lw, h);
      value = ui::label(parent, font, theme::INK);
      lv_obj_set_width(value, lw);
      lv_obj_set_pos(value, x + bw + ui::px(4), y + (h - ui::line_height(font)) / 2);
    } else {
      font = fitting_font(sample, w, h * 45 / 100);
      const int lh = ui::line_height(font);
      const float u = Van::unit_for(w, h - lh);
      const int bw = Van::box_w(u), bh = Van::box_h(u);
      const int top = y + (h - lh - bh) / 2;
      van.make(parent, side, x + (w - bw) / 2, top, u);
      value = ui::label(parent, font, theme::INK);
      lv_obj_set_width(value, w);
      lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(value, x, top + bh);
    }
    shown = true;
  }

  void visible(bool on) {
    if (!shown) return;
    van.visible(on);
    show(value, on);
  }
};

// ---------- The tile ----------
class LevelTile : public tessera::Tile {
 public:
  explicit LevelTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    entity_ = c.entity;
    tile_ = c.tile;
    good_ = c.options["good"] | 1.0f;
    warn_ = std::max(good_, (float) (c.options["warn"] | 3.0f));
    scale_ = std::max(1.0f, (float) (c.options["scale"] | 10.0f));
    gain_ = c.options["gain"] | 3.0f;
    sign_pitch_ = (c.options["invert_pitch"] | false) ? -1.0f : 1.0f;
    sign_roll_ = (c.options["invert_roll"] | false) ? -1.0f : 1.0f;

    const int W = c.width, H = c.height, gap = ui::px(6);
    lv_obj_t *p = c.parent;
    if (c.columns <= 1 && c.rows <= 1 && W < H * 1.6f) {
      // One cell: the bubble, and both numbers in one line under it.
      const int lh = ui::line_height(Font::BODY);
      const int d = std::max(ui::px(20), std::min(W, H - lh - gap / 2));
      bubble_.make(p, (W - d) / 2, 0, d);
      line_ = ui::label(p, Font::BODY, theme::INK);
      lv_obj_set_width(line_, W);
      lv_obj_set_style_text_align(line_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(line_, 0, H - lh);
    } else if (W >= H * 1.3f) {
      // Wide: the bubble on the left, the two vans stacked on the right.
      const int d = std::min(H, W * 45 / 100);
      bubble_.make(p, 0, (H - d) / 2, d);
      const int x = d + gap, w = W - x, h = (H - gap) / 2;
      pitch_.make(p, true, x, 0, w, h);
      roll_.make(p, false, x, h + gap, w, H - h - gap);
    } else {
      // Square or tall: the bubble on top, the vans under it, side by side or stacked.
      const int d = std::min(W, H * 55 / 100);
      bubble_.make(p, (W - d) / 2, 0, d);
      const int y = d + gap, h = H - y;
      if (h >= W * 6 / 10) {
        const int h1 = (h - gap) / 2;
        pitch_.make(p, true, 0, y, W, h1);
        roll_.make(p, false, 0, y + h1 + gap, W, h - h1 - gap);
      } else {
        const int w1 = (W - gap) / 2;
        pitch_.make(p, true, 0, y, w1, h);
        roll_.make(p, false, w1 + gap, y, W - w1 - gap, h);
      }
    }
    // What to do or what is wrong, over the middle of the bubble.
    note_ = ui::label(p, Font::BODY, theme::MUTED);
    lv_obj_set_width(note_, W);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(note_, 0, (H - ui::line_height(Font::BODY)) / 2);
  }

  void on_state(JsonObjectConst data) override { level_.read(data); }

  void on_tick(uint32_t) override {
    const bool live = level_.mode == Level::LIVE;
    const float p = sign_pitch_ * level_.pitch, r = sign_roll_ * level_.roll;
    const float worst = std::max(std::fabs(p), std::fabs(r));
    // Without values only the note shows: what to do, or that the sensor is offline.
    bubble_.set(r / scale_, p / scale_, live, live ? tone_of(worst, good_, warn_) : Tone::NORMAL);
    paint_gauge(pitch_, -p, p, live);
    paint_gauge(roll_, r, r, live);
    if (line_) {
      ui::set_text(line_, live ? degrees(plugin_, p, Font::BODY) + "  " + degrees(plugin_, r, Font::BODY) : "");
      show(line_, live);
    }
    ui::set_text(note_, level_.mode == Level::CHOOSE ? plugin_->text("choose")
                        : level_.mode == Level::OFFLINE ? plugin_->text("offline") : "");
    show(note_, !live);
  }

  void on_theme() override {
    bubble_.paint();
    if (pitch_.shown) pitch_.van.paint();
    if (roll_.shown) roll_.van.paint();
    ui::set_color(note_, theme::MUTED);
    if (pitch_.value) ui::set_color(pitch_.value, theme::INK);
    if (roll_.value) ui::set_color(roll_.value, theme::INK);
    on_tick(0);
  }

  void on_tap() override {
    if (!entity_.empty()) tessera::open_card(plugin_->plugin_id(), "wedges", roomy::plain(entity_), tile_);
  }

 private:
  // The van tilts by `tilt` times the gain; its number says `value`.
  void paint_gauge(Gauge &g, float tilt, float value, bool live) {
    if (!g.shown) return;
    g.visible(live);
    if (!live) return;
    g.van.set(tilt * gain_, tone_of(value, good_, warn_));
    ui::set_text(g.value, degrees(plugin_, value, g.font));
  }

  const tessera::Plugin *plugin_;
  tessera::Text entity_;
  int tile_ = -1;
  float good_ = 1, warn_ = 3, scale_ = 10, gain_ = 3, sign_pitch_ = 1, sign_roll_ = 1;
  Level level_;
  Bubble bubble_;
  Gauge pitch_, roll_;
  lv_obj_t *line_{}, *note_{};
};

// ---------- The card: the wedges per wheel ----------
// Pitch and roll at the top; under them the van from above with a wedge height beside each wheel.
class WedgesCard : public tessera::Card {
 public:
  explicit WedgesCard(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    lv_obj_t *p = c.parent;
    const int W = c.width, H = c.height, gap = ui::px(8);
    // Two gauges in a row: the van from the side and from behind.
    const int gh = H * 34 / 100, gw = (W - gap) / 2;
    pitch_.make(p, true, 0, 0, gw, gh);
    roll_.make(p, false, gw + gap, 0, W - gw - gap, gh);

    // The van from above, the front on top.
    const int lh = ui::line_height(Font::BODY);
    const int top = gh + gap + lh, bottom = H - lh;
    const int vh = std::max(ui::px(30), bottom - top - gap), vw = std::min(W * 30 / 100, vh * 55 / 100);
    const int vx = (W - vw) / 2, vy = top + gap / 2;
    body_ = ui::block(p, theme::TRACK);
    lv_obj_set_pos(body_, vx, vy);
    lv_obj_set_size(body_, vw, vh);
    front_ = ui::label(p, Font::BODY, theme::MUTED);
    lv_obj_set_width(front_, W);
    lv_obj_set_style_text_align(front_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(front_, 0, vy - lh);
    ui::set_text(front_, plugin_->text("front"));
    rear_ = ui::label(p, Font::BODY, theme::MUTED);
    lv_obj_set_width(rear_, W);
    lv_obj_set_style_text_align(rear_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(rear_, 0, vy + vh);
    ui::set_text(rear_, plugin_->text("rear"));

    // A wheel at each corner and its wedge beside it, outside the body.
    const int ww = std::max(ui::px(6), vw / 5), wl = std::max(ui::px(12), vh / 5);
    const Font font = fitting_font("10.0 cm", vx - ww - gap, vh / 3);
    for (int i = 0; i < 4; ++i) {
      const bool right = i & 1, rear = i & 2;
      const int wy = rear ? vy + vh - wl - vh / 10 : vy + vh / 10;
      wheel_[i] = ui::block(p, theme::INK_SOFT);
      lv_obj_set_size(wheel_[i], ww, wl);
      lv_obj_set_pos(wheel_[i], right ? vx + vw - ww / 2 : vx - ww / 2, wy);
      wedge_[i] = ui::label(p, font, theme::INK);
      const int lw = vx - ww / 2 - gap;
      lv_obj_set_width(wedge_[i], lw);
      lv_obj_set_style_text_align(wedge_[i], right ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(wedge_[i], right ? vx + vw + ww / 2 + gap : 0, wy + (wl - ui::line_height(font)) / 2);
    }
    none_ = ui::label(p, Font::BODY, theme::MUTED);
    lv_obj_set_width(none_, W);
    lv_obj_set_style_text_align(none_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(none_, 0, (H - lh) / 2);
    ui::set_text(none_, plugin_->text("no_wedges"));
  }

  void on_state(JsonObjectConst data) override { level_.read(data); }

  void on_tick(uint32_t) override {
    const bool live = level_.mode == Level::LIVE;
    // The vans and their angles, with the default limits (the card does not know the tile's options).
    pitch_.visible(live);
    roll_.visible(live);
    if (live) {
      pitch_.van.set(-level_.pitch * 3, tone_of(level_.pitch, 1, 3));
      roll_.van.set(level_.roll * 3, tone_of(level_.roll, 1, 3));
      ui::set_text(pitch_.value, degrees(plugin_, level_.pitch, pitch_.font));
      ui::set_text(roll_.value, degrees(plugin_, level_.roll, roll_.font));
    }

    // The van from above with its wedges, or the note why there are none.
    const bool wedges = live && level_.has_wedges();
    for (auto *o : {body_, front_, rear_}) show(o, wedges);
    for (int i = 0; i < 4; ++i) {
      show(wheel_[i], wedges);
      show(wedge_[i], wedges);
      if (!wedges) continue;
      const float v = level_.wedge[i];
      char buf[12];
      snprintf(buf, sizeof buf, "%.1f", v);
      std::string n = buf;
      const std::string mark = plugin_->text("decimal");
      if (!mark.empty() && n.find('.') != std::string::npos) n.replace(n.find('.'), 1, mark);
      ui::set_text(wedge_[i], tessera::fill(plugin_->text("cm"), "n", n));
      // A wheel that needs a wedge stands out; its number is ink, one that needs none is muted.
      ui::set_color(wedge_[i], v >= 0.05f ? theme::INK : theme::MUTED);
      ui::set_fill(wheel_[i], v >= 0.05f ? Tone::ACCENT : Tone::NORMAL, theme::INK_SOFT);
    }
    ui::set_text(none_, !live ? plugin_->text(level_.mode == Level::CHOOSE ? "choose" : "offline")
                              : plugin_->text("no_wedges"));
    show(none_, !wedges);
  }

  void on_theme() override {
    pitch_.van.paint();
    roll_.van.paint();
    lv_obj_set_style_bg_color(body_, ui::color(theme::TRACK), 0);
    ui::set_color(front_, theme::MUTED);
    ui::set_color(rear_, theme::MUTED);
    ui::set_color(none_, theme::MUTED);
    on_tick(0);
  }

 private:
  const tessera::Plugin *plugin_;
  Level level_;
  Gauge pitch_, roll_;
  lv_obj_t *body_{}, *front_{}, *rear_{}, *none_{}, *wheel_[4]{}, *wedge_[4]{};
};

void CamperLevel::setup() {
  add_tile("level", [this]() { return new LevelTile(this); });
  add_card("wedges", [this]() { return new WedgesCard(this); }, false);
}

}  // namespace esphome::camper_level
