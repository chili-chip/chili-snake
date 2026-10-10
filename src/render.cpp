#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace blit;

namespace snek::gfx {

static Point shake(0, 0);

void set_shake(Point offset) {
  shake = offset;
}

Point cell_to_px(Point cell) {
  return Point(BOARD_X + cell.x * CELL, BOARD_Y + cell.y * CELL) + shake;
}

static Pen mix(Pen a, Pen b, float t) {
  return Pen(
    uint8_t(a.r + (b.r - a.r) * t),
    uint8_t(a.g + (b.g - a.g) * t),
    uint8_t(a.b + (b.b - a.b) * t));
}

static Pen scale(Pen p, float f) {
  auto c = [f](uint8_t v) { return uint8_t(std::min(255.0f, v * f)); };
  return Pen(c(p.r), c(p.g), c(p.b));
}

// ---------------------------------------------------------------------------
// Board

void draw_board(bool walls, uint32_t time) {
  const Pen light(26, 46, 32);
  const Pen dark(20, 37, 26);

  Rect area(BOARD_X + shake.x, BOARD_Y + shake.y, GRID_W * CELL, GRID_H * CELL);
  screen.pen = dark;
  screen.rectangle(area);
  screen.pen = light;
  for(int y = 0; y < GRID_H; y++) {
    for(int x = (y & 1); x < GRID_W; x += 2) {
      Point p = cell_to_px(Point(x, y));
      screen.rectangle(Rect(p.x, p.y, CELL, CELL));
    }
  }

  if(walls) {
    // Dashed orange fence around the board.
    const Pen a(214, 120, 48);
    const Pen b(120, 60, 30);
    int x0 = area.x, y0 = area.y, x1 = area.x + area.w - 1, y1 = area.y + area.h - 1;
    for(int x = x0; x <= x1; x++) {
      screen.pen = ((x >> 2) & 1) ? a : b;
      screen.pixel(Point(x, y0));
      screen.pixel(Point(x, y1));
    }
    for(int y = y0; y <= y1; y++) {
      screen.pen = ((y >> 2) & 1) ? a : b;
      screen.pixel(Point(x0, y));
      screen.pixel(Point(x1, y));
    }
  }
}

// ---------------------------------------------------------------------------
// Snake

static Rect bridge(Point p, Dir d, int inset) {
  int w = CELL - inset * 2;
  int half = CELL / 2;
  switch(d) {
    case RIGHT: return Rect(p.x + half, p.y + inset, half, w);
    case LEFT:  return Rect(p.x, p.y + inset, half, w);
    case UP:    return Rect(p.x + inset, p.y, w, half);
    case DOWN:  return Rect(p.x + inset, p.y + half, w, half);
  }
  return Rect(0, 0, 0, 0);
}

static void rounded_cell(Point p, int inset) {
  int s = CELL - inset * 2;
  screen.rectangle(Rect(p.x + inset + 1, p.y + inset, s - 2, s));
  screen.rectangle(Rect(p.x + inset, p.y + inset + 1, s, s - 2));
}

static void draw_head(const Board &board, Point p, Pen body, uint32_t time, SnakeLook look) {
  screen.pen = body;
  rounded_cell(p, 0);
  if(board.length() > 1) {
    screen.rectangle(bridge(p, Board::dir_between(board.segment(0), board.segment(1)), 1));
  }

  Dir d = board.heading();
  Point f = dir_vec(d);
  Point s(-f.y, f.x);
  bool dead = look != SnakeLook::NORMAL;

  // Eyes: 2x2 whites with a pupil looking forward.
  for(int side = -1; side <= 1; side += 2) {
    Point eye = p + Point(3, 3) + f + Point(s.x * 2 * side, s.y * 2 * side);
    if(dead) {
      screen.pen = Pen(30, 10, 10);
      screen.line(eye, eye + Point(1, 1));
      screen.line(eye + Point(1, 0), eye + Point(0, 1));
      continue;
    }
    screen.pen = Pen(250, 250, 240);
    screen.rectangle(Rect(eye.x, eye.y, 2, 2));
    screen.pen = Pen(15, 20, 15);
    screen.pixel(eye + Point(f.x > 0 ? 1 : 0, f.y > 0 ? 1 : 0));
  }
  if(dead) return;

  // Front edge centre of the head, used for the mouth and tongue.
  Point front = p + Point(3, 3) + Point(f.x > 0 ? 4 : (f.x < 0 ? -3 : 0), f.y > 0 ? 4 : (f.y < 0 ? -3 : 0));

  Point ahead = board.head() + dir_vec(board.next_heading());
  ahead = Point((ahead.x + GRID_W) % GRID_W, (ahead.y + GRID_H) % GRID_H);
  bool food_ahead = ahead == board.food() || (board.golden_active() && ahead == board.golden());

  if(food_ahead) {
    // Mouth wide open, ready to chomp.
    screen.pen = Pen(60, 10, 20);
    screen.rectangle(Rect(front.x, front.y, f.x ? 1 : 2, f.y ? 1 : 2));
  } else if((time / 110) % 12 == 0) {
    // Tongue flick.
    screen.pen = Pen(230, 50, 70);
    Point t0 = front + f;
    screen.pixel(t0);
    screen.pixel(t0 + f);
    screen.pixel(t0 + f + f + s);
    screen.pixel(t0 + f + f - s);
  }
}

void draw_snake(const Board &board, uint32_t time, SnakeLook look) {
  const Pen head_col(160, 240, 96);
  const Pen tail_col(40, 140, 70);

  int len = board.length();
  for(int i = len - 1; i >= 0; i--) {
    Point cell = board.segment(i);
    Point p = cell_to_px(cell);

    Pen col;
    if(look == SnakeLook::DEAD_LIGHT) col = Pen(235, 235, 225);
    else if(look == SnakeLook::DEAD_DARK) col = Pen(110, 60, 50);
    else col = mix(head_col, tail_col, len > 1 ? float(i) / float(len - 1) : 0.0f);

    if(i == 0) {
      draw_head(board, p, col, time, look);
      continue;
    }

    if(look == SnakeLook::NORMAL && (i & 1)) col = scale(col, 0.85f);
    screen.pen = col;

    Dir to_prev = Board::dir_between(cell, board.segment(i - 1));
    if(i == len - 1) {
      // Tail: a thinner stub joined to the previous segment.
      rounded_cell(p, 2);
      screen.rectangle(bridge(p, to_prev, 2));
      continue;
    }

    Dir to_next = Board::dir_between(cell, board.segment(i + 1));
    rounded_cell(p, 1);
    screen.rectangle(bridge(p, to_prev, 1));
    screen.rectangle(bridge(p, to_next, 1));

    if(look == SnakeLook::NORMAL) {
      // Scale highlight on every other segment.
      if(!(i & 1)) {
        screen.pen = scale(col, 1.25f);
        screen.rectangle(Rect(p.x + 3, p.y + 3, 2, 2));
      } else {
        screen.pen = scale(col, 0.8f);
        screen.pixel(Point(p.x + 3, p.y + 4));
        screen.pixel(Point(p.x + 4, p.y + 3));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Apples

static void apple_shape(Point p, int bob, Pen body, Pen shade, Pen shine) {
  Point c = p + Point(4, 4 + bob);

  screen.pen = Pen(0, 0, 0, 90);
  screen.rectangle(Rect(p.x + 2, p.y + 7, 5, 1));

  screen.pen = body;
  screen.circle(c, 3);
  screen.pen = shade;
  screen.pixel(c + Point(2, 2));
  screen.pixel(c + Point(1, 3));
  screen.pixel(c + Point(3, 1));
  screen.pen = shine;
  screen.pixel(c + Point(-2, -1));
  screen.pixel(c + Point(-1, -2));

  // Stem and leaf.
  screen.pen = Pen(110, 70, 30);
  screen.pixel(c + Point(0, -4));
  screen.pen = Pen(100, 210, 70);
  screen.pixel(c + Point(1, -4));
  screen.pixel(c + Point(2, -5));
  screen.pixel(c + Point(1, -5));
}

void draw_apple(Point cell, uint32_t time) {
  int bob = ((time / 350) & 1) ? -1 : 0;
  apple_shape(cell_to_px(cell), bob, Pen(225, 40, 52), Pen(150, 20, 34), Pen(255, 190, 190));
}

void draw_golden_apple(Point cell, uint32_t time, int steps_left) {
  // Blink when it's about to disappear.
  if(steps_left < 10 && ((time / 90) & 1)) return;

  Point p = cell_to_px(cell);
  int bob = ((time / 250) & 1) ? -1 : 0;
  apple_shape(p, bob, Pen(255, 200, 40), Pen(190, 120, 20), Pen(255, 250, 210));

  // Orbiting sparkles.
  Point c = p + Point(4, 4 + bob);
  float a = time * 0.006f;
  screen.pen = Pen(255, 255, 200);
  for(int k = 0; k < 3; k++) {
    float ang = a + k * 2.094f;
    screen.pixel(c + Point(int(roundf(cosf(ang) * 6)), int(roundf(sinf(ang) * 6))));
  }
}

// ---------------------------------------------------------------------------
// HUD

void draw_hud(uint32_t score, uint32_t best, int length, uint32_t time) {
  screen.pen = Pen(14, 26, 18);
  screen.rectangle(Rect(0, 0, 128, HUD_H));
  screen.pen = Pen(60, 120, 64);
  screen.rectangle(Rect(0, HUD_H - 1, 128, 1));

  // Apple icon + score.
  screen.pen = Pen(225, 40, 52);
  screen.circle(Point(6, 8), 3);
  screen.pen = Pen(255, 190, 190);
  screen.pixel(Point(4, 7));
  screen.pen = Pen(100, 210, 70);
  screen.pixel(Point(7, 4));
  screen.pixel(Point(8, 3));
  text(std::to_string(score), Point(13, 4), colors::text);

  bool beaten = score > best && best > 0;
  uint32_t shown_best = std::max(score, best);
  Pen best_col = beaten && ((time / 200) & 1) ? colors::gold : colors::dim;
  text("BEST " + std::to_string(shown_best), Point(126, 4), best_col, TextAlign::top_right);
}

// ---------------------------------------------------------------------------
// Effects

struct Particle {
  float x, y, vx, vy;
  int16_t life, max_life;
  Pen color;
};

struct Popup {
  float x, y;
  int16_t life;
  char text[8];
  Pen color;
};

static Particle particles[48];
static Popup popups[6];

void burst(Point px, Pen color, int count) {
  for(auto &pt : particles) {
    if(count <= 0) break;
    if(pt.life > 0) continue;
    float ang = (blit::random() % 628) / 100.0f;
    float spd = 0.02f + (blit::random() % 100) / 2000.0f;
    pt.x = px.x;
    pt.y = px.y;
    pt.vx = cosf(ang) * spd;
    pt.vy = sinf(ang) * spd - 0.03f;
    pt.max_life = pt.life = 300 + blit::random() % 300;
    pt.color = (blit::random() & 1) ? color : Pen(255, 255, 220);
    count--;
  }
}

void popup(Point px, const char *s, Pen color) {
  Popup *slot = &popups[0];
  for(auto &p : popups) {
    if(p.life <= 0) { slot = &p; break; }
    if(p.life < slot->life) slot = &p;
  }
  // Keep the text on screen when eating at the board edges.
  slot->x = std::max(14, std::min(114, int(px.x)));
  slot->y = std::max(BOARD_Y + 4, int(px.y));
  slot->life = 700;
  strncpy(slot->text, s, sizeof(slot->text) - 1);
  slot->text[sizeof(slot->text) - 1] = 0;
  slot->color = color;
}

void update_effects(uint32_t dt) {
  for(auto &pt : particles) {
    if(pt.life <= 0) continue;
    pt.life -= dt;
    pt.x += pt.vx * dt;
    pt.y += pt.vy * dt;
    pt.vy += 0.00015f * dt;
  }
  for(auto &p : popups) {
    if(p.life <= 0) continue;
    p.life -= dt;
    p.y -= 0.02f * dt;
  }
}

void draw_effects() {
  for(auto &pt : particles) {
    if(pt.life <= 0) continue;
    Pen c = pt.color;
    c.a = uint8_t(255 * pt.life / pt.max_life);
    screen.pen = c;
    screen.pixel(Point(int(pt.x), int(pt.y)) + shake);
  }
  for(auto &p : popups) {
    if(p.life <= 0) continue;
    Pen c = p.color;
    c.a = uint8_t(std::min(255, p.life * 255 / 300));
    Pen sh(0, 0, 0, c.a);
    Point at(int(p.x), int(p.y));
    screen.pen = sh;
    screen.text(p.text, minimal_font, at + Point(1, 1), true, TextAlign::center_center);
    screen.pen = c;
    screen.text(p.text, minimal_font, at, true, TextAlign::center_center);
  }
}

void clear_effects() {
  for(auto &pt : particles) pt.life = 0;
  for(auto &p : popups) p.life = 0;
}

// ---------------------------------------------------------------------------
// Logo

static const char *const logo_letters[4][5] = {
  {".####", "#....", ".###.", "....#", "####."},
  {"#...#", "##..#", "#.#.#", "#..##", "#...#"},
  {"#####", "#....", "####.", "#....", "#####"},
  {"#...#", "#..#.", "###..", "#..#.", "#...#"},
};

void draw_logo(int y, uint32_t time) {
  const int B = 4;               // block size
  const int letter_w = 5 * B;
  const int gap = B;
  const int total = 4 * letter_w + 3 * gap;
  int x0 = (128 - total) / 2;

  for(int pass = 0; pass < 2; pass++) {
    for(int l = 0; l < 4; l++) {
      int wave = int(roundf(sinf(time * 0.004f + l * 0.9f) * 2.0f));
      int lx = x0 + l * (letter_w + gap);
      for(int row = 0; row < 5; row++) {
        for(int col = 0; col < 5; col++) {
          if(logo_letters[l][row][col] != '#') continue;
          int bx = lx + col * B;
          int by = y + row * B + wave;
          if(pass == 0) {
            screen.pen = Pen(0, 0, 0, 160);
            screen.rectangle(Rect(bx + 1, by + 2, B, B));
            continue;
          }
          float t = float(l * 5 + col) / 19.0f;
          Pen c = mix(Pen(180, 245, 110), Pen(50, 165, 80), t);
          screen.pen = c;
          screen.rectangle(Rect(bx, by, B, B));
          screen.pen = scale(c, 0.7f);
          screen.rectangle(Rect(bx, by + B - 1, B, 1));
          screen.rectangle(Rect(bx + B - 1, by, 1, B));
          screen.pen = scale(c, 1.2f);
          screen.pixel(Point(bx, by));
        }
      }
    }
  }

  // Eyes on the S, so the logo itself is a snake.
  int wave0 = int(roundf(sinf(time * 0.004f) * 2.0f));
  screen.pen = Pen(250, 250, 240);
  screen.rectangle(Rect(x0 + 9, y + 1 + wave0, 2, 2));
  screen.rectangle(Rect(x0 + 13, y + 1 + wave0, 2, 2));
  screen.pen = Pen(15, 20, 15);
  screen.pixel(Point(x0 + 10, y + 1 + wave0));
  screen.pixel(Point(x0 + 14, y + 1 + wave0));

  // An apple waiting after the K.
  int bob = ((time / 350) & 1) ? -1 : 0;
  apple_shape(Point(x0 + total + 2, y + 10), bob, Pen(225, 40, 52), Pen(150, 20, 34), Pen(255, 190, 190));
}

// ---------------------------------------------------------------------------
// UI helpers

void dim_screen(uint8_t alpha) {
  screen.pen = Pen(0, 0, 0, alpha);
  screen.rectangle(Rect(0, 0, 128, 128));
}

void panel(Rect r) {
  screen.pen = Pen(10, 20, 14, 235);
  screen.rectangle(r);
  screen.pen = Pen(90, 170, 90);
  screen.rectangle(Rect(r.x + 1, r.y, r.w - 2, 1));
  screen.rectangle(Rect(r.x + 1, r.y + r.h - 1, r.w - 2, 1));
  screen.rectangle(Rect(r.x, r.y + 1, 1, r.h - 2));
  screen.rectangle(Rect(r.x + r.w - 1, r.y + 1, 1, r.h - 2));
}

void text(std::string_view s, Point p, Pen color, TextAlign align, bool shadow) {
  if(shadow) {
    screen.pen = Pen(0, 0, 0, 200);
    screen.text(s, minimal_font, p + Point(1, 1), true, align);
  }
  screen.pen = color;
  screen.text(s, minimal_font, p, true, align);
}

void big_text(std::string_view s, Point center, int scale_by, Pen color) {
  static uint8_t buf[128 * 8 * 4];
  Surface tmp(buf, PixelFormat::RGBA, Size(128, 8));

  Size sz = screen.measure_text(s, minimal_font, true);
  int w = std::min(int(sz.w), 128);
  Rect src(0, 0, w, 8);
  Rect dst(center.x - w * scale_by / 2, center.y - 4 * scale_by, w * scale_by, 8 * scale_by);

  for(int pass = 0; pass < 2; pass++) {
    memset(buf, 0, sizeof(buf));
    tmp.pen = pass == 0 ? Pen(0, 0, 0, 220) : color;
    tmp.text(s, minimal_font, Point(0, 0), true);
    Rect d = dst;
    if(pass == 0) {
      d.x += scale_by / 2 + 1;
      d.y += scale_by / 2 + 1;
    }
    screen.stretch_blit(&tmp, src, d);
  }
}

}
