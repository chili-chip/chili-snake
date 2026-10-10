#include "32blit.hpp"
#include "game.hpp"

#include "audio.hpp"
#include "render.hpp"
#include "save.hpp"
#include "snake.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

using namespace blit;
using namespace snek;

namespace {

enum class Scene : uint8_t {
  TITLE,
  SCORES,
  CREDITS,
  READY,
  PLAYING,
  PAUSED,
  DYING,
  NAME_ENTRY,
  RESULTS,
};

enum MenuItem : uint8_t { M_PLAY, M_SPEED, M_WALLS, M_MUSIC, M_SOUND, M_SCORES, M_CREDITS, M_COUNT };
enum PauseItem : uint8_t { P_RESUME, P_RESTART, P_MENU, P_COUNT };

const char *const speed_names[] = {"SLOW", "NORMAL", "FAST"};
const char *const speed_tags[] = {"S", "N", "F"};

// Step length in ms at the start of a round and the fastest it ramps up to.
const uint16_t start_step_ms[] = {200, 150, 110};
const uint16_t min_step_ms[] = {115, 80, 60};
const uint16_t apple_points[] = {50, 100, 150};

const uint32_t COUNTDOWN_MS = 1350;
const uint32_t DYING_MS = 1300;

Scene scene = Scene::TITLE;
uint32_t scene_start = 0;
uint32_t now_ms = 0;
uint32_t last_update = 0;

int menu_index = 0;
int pause_index = 0;

Board board;
Board demo;
uint32_t step_timer = 0;
uint32_t demo_timer = 0;

uint32_t score = 0;
bool won = false;
int new_rank = -1;
uint32_t best_at_start = 0;

char entry_name[4] = "AAA";
int entry_cursor = 0;

uint32_t shake_until = 0;

void enter(Scene s) {
  scene = s;
  scene_start = now_ms;
}

uint32_t in_scene() {
  return now_ms - scene_start;
}

uint32_t best_score() {
  return save.scores[0].score;
}

uint16_t step_ms() {
  int s = save.speed;
  int v = int(start_step_ms[s]) - board.apples_eaten() * 4;
  return uint16_t(std::max(v, int(min_step_ms[s])));
}

uint32_t points_per_apple() {
  return apple_points[save.speed] + (save.walls ? 50 : 0);
}

void apply_audio_settings() {
  audio::set_music_enabled(save.music);
  audio::set_sfx_enabled(save.sound);
}

void start_round() {
  board.reset(save.walls);
  score = 0;
  won = false;
  new_rank = -1;
  best_at_start = best_score();
  step_timer = 0;
  gfx::clear_effects();
  audio::stop_song();
  audio::play_sfx(audio::Sfx::COUNTDOWN);
  enter(Scene::READY);
}

void go_to_title() {
  demo.reset(false);
  audio::play_song(audio::Song::TITLE);
  enter(Scene::TITLE);
}

void finish_round() {
  new_rank = score_rank(score);
  if(new_rank >= 0) {
    memcpy(entry_name, save.last_name, sizeof(entry_name));
    entry_cursor = 0;
    audio::play_song(audio::Song::FANFARE);
    enter(Scene::NAME_ENTRY);
  } else {
    enter(Scene::RESULTS);
  }
}

bool read_dir(Dir &d) {
  if(buttons.pressed & Button::DPAD_UP) { d = UP; return true; }
  if(buttons.pressed & Button::DPAD_DOWN) { d = DOWN; return true; }
  if(buttons.pressed & Button::DPAD_LEFT) { d = LEFT; return true; }
  if(buttons.pressed & Button::DPAD_RIGHT) { d = RIGHT; return true; }
  return false;
}

// Up/down menu navigation with wrap-around. Returns true when moved.
bool menu_nav(int &index, int count) {
  if(buttons.pressed & Button::DPAD_UP) {
    index = (index + count - 1) % count;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
    return true;
  }
  if(buttons.pressed & Button::DPAD_DOWN) {
    index = (index + 1) % count;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
    return true;
  }
  return false;
}

Point cell_centre(Point cell) {
  return gfx::cell_to_px(cell) + Point(gfx::CELL / 2, gfx::CELL / 2);
}

// ---------------------------------------------------------------------------
// Update

void update_demo(uint32_t dt) {
  demo_timer += dt;
  if(demo_timer < 130) return;
  demo_timer = 0;
  demo.autopilot();
  Event e = demo.step();
  if(e == Event::DIED || e == Event::WON || demo.length() > 60) demo.reset(false);
}

void update_title() {
  menu_nav(menu_index, M_COUNT);

  int change = 0;
  if(buttons.pressed & Button::DPAD_LEFT) change = -1;
  if(buttons.pressed & (Button::DPAD_RIGHT | Button::A)) change = 1;
  if(!change) return;

  bool a = buttons.pressed & Button::A;
  switch(menu_index) {
    case M_PLAY:
      if(a) start_round();
      return;
    case M_SPEED:
      save.speed = (save.speed + 3 + change) % 3;
      break;
    case M_WALLS:
      save.walls = !save.walls;
      break;
    case M_MUSIC:
      save.music = !save.music;
      apply_audio_settings();
      if(save.music) audio::play_song(audio::Song::TITLE);
      break;
    case M_SOUND:
      save.sound = !save.sound;
      apply_audio_settings();
      break;
    case M_SCORES:
      if(a) {
        audio::play_sfx(audio::Sfx::MENU_SELECT);
        enter(Scene::SCORES);
      }
      return;
    case M_CREDITS:
      if(a) {
        audio::play_sfx(audio::Sfx::MENU_SELECT);
        enter(Scene::CREDITS);
      }
      return;
  }
  audio::play_sfx(audio::Sfx::MENU_SELECT);
  write_save_data();
}

void update_ready() {
  Dir d;
  if(read_dir(d)) board.queue_turn(d);

  uint32_t t = in_scene();
  uint32_t prev = t > 10 ? t - 10 : 0;
  // Beep on each count: 3 (on entry), 2, 1, then GO.
  for(uint32_t mark : {450u, 900u}) {
    if(prev < mark && t >= mark) audio::play_sfx(audio::Sfx::COUNTDOWN);
  }
  if(t >= COUNTDOWN_MS) {
    audio::play_sfx(audio::Sfx::GO);
    audio::play_song(audio::Song::GAME);
    enter(Scene::PLAYING);
  }
}

void on_event(Event e) {
  switch(e) {
    case Event::MOVED:
      break;

    case Event::ATE: {
      score += points_per_apple();
      Point at = cell_centre(board.head());
      gfx::burst(at, Pen(235, 60, 70), 10);
      char buf[8];
      snprintf(buf, sizeof(buf), "+%lu", (unsigned long)points_per_apple());
      gfx::popup(at - Point(0, 6), buf, gfx::colors::text);
      if(board.golden_active() && board.golden_steps_left() == GOLDEN_LIFETIME) {
        audio::play_sfx(audio::Sfx::GOLDEN_SPAWN);
      } else {
        audio::play_sfx(audio::Sfx::EAT);
      }
      break;
    }

    case Event::ATE_GOLDEN: {
      uint32_t pts = points_per_apple() * 5;
      score += pts;
      Point at = cell_centre(board.head());
      gfx::burst(at, gfx::colors::gold, 20);
      char buf[8];
      snprintf(buf, sizeof(buf), "+%lu", (unsigned long)pts);
      gfx::popup(at - Point(0, 6), buf, gfx::colors::gold);
      audio::play_sfx(audio::Sfx::GOLDEN);
      break;
    }

    case Event::DIED:
      audio::play_sfx(audio::Sfx::DIE);
      audio::play_song(audio::Song::GAME_OVER);
      shake_until = now_ms + 450;
      enter(Scene::DYING);
      break;

    case Event::WON:
      won = true;
      score += points_per_apple() * 10;
      gfx::burst(cell_centre(board.head()), gfx::colors::gold, 40);
      finish_round();
      break;
  }
}

void update_playing(uint32_t dt) {
  if(buttons.pressed & (Button::MENU | Button::Y)) {
    audio::play_sfx(audio::Sfx::PAUSE);
    audio::pause_song(true);
    pause_index = P_RESUME;
    enter(Scene::PAUSED);
    return;
  }

  Dir d;
  if(read_dir(d)) board.queue_turn(d);

  step_timer += dt;
  uint16_t interval = step_ms();
  if(step_timer < interval) return;
  step_timer -= interval;

  on_event(board.step());

  // The music speeds up with the snake.
  int s = save.speed;
  float progress = float(start_step_ms[s] - step_ms()) / float(start_step_ms[s] - min_step_ms[s]);
  audio::set_tempo_scale(1.0f - 0.2f * progress);
}

void update_paused() {
  if(buttons.pressed & Button::MENU) {
    audio::pause_song(false);
    enter(Scene::PLAYING);
    return;
  }
  menu_nav(pause_index, P_COUNT);
  if(!(buttons.pressed & Button::A)) return;

  audio::play_sfx(audio::Sfx::MENU_SELECT);
  switch(pause_index) {
    case P_RESUME:
      audio::pause_song(false);
      enter(Scene::PLAYING);
      break;
    case P_RESTART:
      start_round();
      break;
    case P_MENU:
      go_to_title();
      break;
  }
}

void update_name_entry() {
  char &c = entry_name[entry_cursor];
  if(buttons.pressed & Button::DPAD_UP) {
    c = c >= 'Z' ? 'A' : c + 1;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
  }
  if(buttons.pressed & Button::DPAD_DOWN) {
    c = c <= 'A' ? 'Z' : c - 1;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
  }
  if((buttons.pressed & Button::DPAD_LEFT) && entry_cursor > 0) {
    entry_cursor--;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
  }
  if((buttons.pressed & Button::DPAD_RIGHT) && entry_cursor < 2) {
    entry_cursor++;
    audio::play_sfx(audio::Sfx::MENU_MOVE);
  }
  if((buttons.pressed & Button::B) && entry_cursor > 0) {
    entry_cursor--;
    audio::play_sfx(audio::Sfx::MENU_BACK);
  }
  if(buttons.pressed & Button::A) {
    audio::play_sfx(audio::Sfx::MENU_SELECT);
    if(entry_cursor < 2) {
      entry_cursor++;
      return;
    }
    insert_score(new_rank, entry_name, score, board.length(), save.speed, save.walls);
    memcpy(save.last_name, entry_name, sizeof(entry_name));
    write_save_data();
    enter(Scene::RESULTS);
  }
}

void update_results() {
  if(in_scene() < 400) return; // don't skip the screen with a held button
  if(buttons.pressed & Button::A) {
    start_round();
  } else if(buttons.pressed & (Button::B | Button::MENU)) {
    audio::play_sfx(audio::Sfx::MENU_BACK);
    go_to_title();
  }
}

void update_info_screen() {
  if(buttons.pressed & (Button::A | Button::B | Button::MENU)) {
    audio::play_sfx(audio::Sfx::MENU_BACK);
    enter(Scene::TITLE);
  }
}

// ---------------------------------------------------------------------------
// Render

void render_demo_background(uint32_t time) {
  gfx::set_shake(Point(0, 0));
  gfx::draw_board(false, time);
  gfx::draw_apple(demo.food(), time);
  if(demo.golden_active()) gfx::draw_golden_apple(demo.golden(), time, demo.golden_steps_left());
  gfx::draw_snake(demo, time);
  gfx::dim_screen(150);
}

void render_title(uint32_t time) {
  render_demo_background(time);
  gfx::draw_logo(6, time);
  gfx::text("Eat them apples!", Point(64, 31), gfx::colors::dim, TextAlign::top_center);

  const int top = 43;
  const int row_h = 9;
  for(int i = 0; i < M_COUNT; i++) {
    int y = top + i * row_h;
    bool sel = i == menu_index;
    if(sel) {
      screen.pen = Pen(70, 150, 80, 150);
      screen.rectangle(Rect(14, y - 1, 100, row_h));
      // Little snake head pointing at the selected item.
      int wiggle = (time / 200) & 1;
      screen.pen = Pen(160, 240, 96);
      screen.rectangle(Rect(6 + wiggle, y + 1, 5, 5));
      screen.pen = Pen(15, 20, 15);
      screen.pixel(Point(9 + wiggle, y + 2));
    }

    Pen label_col = sel ? gfx::colors::text : Pen(170, 200, 160);
    const char *label = "";
    std::string value;
    switch(i) {
      case M_PLAY:    label = "PLAY"; break;
      case M_SPEED:   label = "SPEED"; value = speed_names[save.speed]; break;
      case M_WALLS:   label = "WALLS"; value = save.walls ? "ON" : "OFF"; break;
      case M_MUSIC:   label = "MUSIC"; value = save.music ? "ON" : "OFF"; break;
      case M_SOUND:   label = "SOUND"; value = save.sound ? "ON" : "OFF"; break;
      case M_SCORES:  label = "HIGH SCORES"; break;
      case M_CREDITS: label = "CREDITS"; break;
    }
    gfx::text(label, Point(18, y), label_col);
    if(!value.empty()) {
      if(sel) value = "< " + value + " >";
      gfx::text(value, Point(110, y), sel ? gfx::colors::accent : gfx::colors::dim, TextAlign::top_right);
    }
  }

  if(best_score() > 0) {
    std::string best = "BEST " + std::to_string(best_score()) + " " + save.scores[0].name;
    gfx::text(best, Point(64, 118), gfx::colors::gold, TextAlign::top_center);
  }
}

void render_score_table(int top, int highlight, uint32_t time) {
  for(int i = 0; i < SCORE_SLOTS; i++) {
    const ScoreEntry &e = save.scores[i];
    int y = top + i * 9;
    bool hl = i == highlight;
    if(hl) {
      screen.pen = ((time / 250) & 1) ? Pen(200, 160, 40, 120) : Pen(200, 160, 40, 70);
      screen.rectangle(Rect(14, y - 1, 100, 9));
    }
    Pen col = i == 0 ? gfx::colors::gold : gfx::colors::text;
    char rank[4];
    snprintf(rank, sizeof(rank), "%d.", i + 1);
    gfx::text(rank, Point(18, y), gfx::colors::dim);
    if(e.score == 0) {
      gfx::text("---", Point(32, y), gfx::colors::dim);
      continue;
    }
    gfx::text(e.name, Point(32, y), col);
    gfx::text(std::to_string(e.score), Point(88, y), col, TextAlign::top_right);
    std::string tag = speed_tags[e.speed < 3 ? e.speed : 1];
    if(e.walls) tag += "W";
    gfx::text(tag, Point(110, y), gfx::colors::dim, TextAlign::top_right);
  }
}

void render_scores(uint32_t time) {
  render_demo_background(time);
  gfx::big_text("HIGH SCORES", Point(64, 16), 1, gfx::colors::gold);
  gfx::panel(Rect(8, 28, 112, 56));
  render_score_table(34, -1, time);
  gfx::text("S/N/F = speed  W = walls", Point(64, 92), gfx::colors::dim, TextAlign::top_center);
  gfx::text("B: BACK", Point(64, 114), gfx::colors::text, TextAlign::top_center);
}

void render_credits(uint32_t time) {
  render_demo_background(time);
  gfx::draw_logo(6, time);
  gfx::panel(Rect(6, 34, 116, 76));
  const char *const lines[] = {
    "snek v1.0.0",
    "",
    "Game: TeriyakiGod",
    "Original chiptune",
    "music made for snek",
    "Made with 32blit",
    "for the VGC Zero",
    "chilichip.eu",
  };
  int y = 39;
  for(const char *l : lines) {
    gfx::text(l, Point(64, y), l == lines[0] ? gfx::colors::accent : gfx::colors::text, TextAlign::top_center);
    y += 8;
  }
  gfx::text("B: BACK", Point(64, 114), gfx::colors::text, TextAlign::top_center);
}

void render_game_world(uint32_t time, gfx::SnakeLook look) {
  gfx::draw_hud(score, best_at_start, board.length(), time);
  gfx::draw_board(board.walls(), time);
  gfx::draw_apple(board.food(), time);
  if(board.golden_active()) gfx::draw_golden_apple(board.golden(), time, board.golden_steps_left());
  gfx::draw_snake(board, time, look);
  gfx::draw_effects();
}

void render_ready(uint32_t time) {
  render_game_world(time, gfx::SnakeLook::NORMAL);
  uint32_t t = in_scene();
  const char *label = t < 450 ? "3" : t < 900 ? "2" : "1";
  int pulse = 3 + ((t % 450) < 120 ? 1 : 0);
  gfx::big_text(label, Point(64, 60), pulse, gfx::colors::text);
  if(board.walls()) {
    gfx::text("WALLS ON", Point(64, 88), Pen(214, 120, 48), TextAlign::top_center);
  }
}

void render_playing(uint32_t time) {
  render_game_world(time, gfx::SnakeLook::NORMAL);
  if(in_scene() < 400) {
    gfx::big_text("GO!", Point(64, 60), 3, gfx::colors::accent);
  }
}

void render_paused(uint32_t time) {
  render_game_world(time, gfx::SnakeLook::NORMAL);
  gfx::dim_screen(140);
  gfx::panel(Rect(24, 36, 80, 58));
  gfx::big_text("PAUSED", Point(64, 47), 2, gfx::colors::text);
  const char *const items[] = {"RESUME", "RESTART", "MENU"};
  for(int i = 0; i < P_COUNT; i++) {
    int y = 61 + i * 10;
    bool sel = i == pause_index;
    if(sel) {
      screen.pen = Pen(70, 150, 80, 150);
      screen.rectangle(Rect(30, y - 1, 68, 9));
    }
    gfx::text(items[i], Point(64, y), sel ? gfx::colors::text : gfx::colors::dim, TextAlign::top_center);
  }
}

void render_dying(uint32_t time) {
  uint32_t t = in_scene();
  gfx::SnakeLook look = gfx::SnakeLook::DEAD_DARK;
  if(t < 700) look = ((t / 100) & 1) ? gfx::SnakeLook::DEAD_LIGHT : gfx::SnakeLook::DEAD_DARK;
  render_game_world(time, look);
  // Red flash right at the moment of impact.
  if(t < 150) {
    screen.pen = Pen(255, 40, 40, uint8_t(120 - t * 120 / 150));
    screen.rectangle(Rect(0, 0, 128, 128));
  }
}

void render_name_entry(uint32_t time) {
  render_game_world(time, gfx::SnakeLook::DEAD_DARK);
  gfx::dim_screen(150);
  gfx::panel(Rect(10, 22, 108, 86));

  Pen title = ((time / 250) & 1) ? gfx::colors::gold : gfx::colors::text;
  gfx::big_text(new_rank == 0 ? "NEW BEST!" : "HIGH SCORE!", Point(64, 33), new_rank == 0 ? 2 : 1, title);

  char line[40];
  snprintf(line, sizeof(line), "#%d   %lu", new_rank + 1, (unsigned long)score);
  gfx::text(line, Point(64, 45), gfx::colors::text, TextAlign::top_center);
  gfx::text("ENTER YOUR NAME", Point(64, 56), gfx::colors::dim, TextAlign::top_center);

  for(int i = 0; i < 3; i++) {
    int cx = 46 + i * 18;
    bool sel = i == entry_cursor;
    char ch[2] = {entry_name[i], 0};
    gfx::big_text(ch, Point(cx, 78), 2, sel ? gfx::colors::accent : gfx::colors::text);
    if(sel) {
      int bob = (time / 200) & 1;
      screen.pen = gfx::colors::accent;
      screen.triangle(Point(cx - 3, 69 - bob), Point(cx + 3, 69 - bob), Point(cx, 66 - bob));
      screen.triangle(Point(cx - 3, 87 + bob), Point(cx + 3, 87 + bob), Point(cx, 90 + bob));
    }
  }
  gfx::text("A: OK   B: BACK", Point(64, 96), gfx::colors::dim, TextAlign::top_center);
}

void render_results(uint32_t time) {
  render_game_world(time, won ? gfx::SnakeLook::NORMAL : gfx::SnakeLook::DEAD_DARK);
  gfx::dim_screen(150);
  gfx::panel(Rect(6, 8, 116, 112));

  gfx::big_text(won ? "YOU WIN!" : "GAME OVER", Point(64, 19), 2, won ? gfx::colors::gold : gfx::colors::red);

  char line[40];
  snprintf(line, sizeof(line), "SCORE %lu   LENGTH %d", (unsigned long)score, board.length());
  gfx::text(line, Point(64, 32), gfx::colors::text, TextAlign::top_center);

  if(new_rank == 0) {
    Pen c = ((time / 250) & 1) ? gfx::colors::gold : gfx::colors::text;
    gfx::text("NEW BEST SCORE!", Point(64, 42), c, TextAlign::top_center);
  } else if(new_rank > 0) {
    snprintf(line, sizeof(line), "RANK #%d", new_rank + 1);
    gfx::text(line, Point(64, 42), gfx::colors::gold, TextAlign::top_center);
  } else {
    gfx::text("So close. Try again!", Point(64, 42), gfx::colors::dim, TextAlign::top_center);
  }

  screen.pen = Pen(90, 170, 90, 120);
  screen.rectangle(Rect(14, 52, 100, 1));
  render_score_table(56, new_rank, time);

  if(in_scene() >= 400) {
    gfx::text("A: AGAIN    B: MENU", Point(64, 108), gfx::colors::text, TextAlign::top_center);
  }
}

}

// ---------------------------------------------------------------------------

void init() {
  set_screen_mode(ScreenMode::hires);
  load_save();
  audio::init();
  apply_audio_settings();
  go_to_title();
}

void update(uint32_t time) {
  uint32_t dt = last_update ? time - last_update : 10;
  last_update = time;
  now_ms = time;

  audio::update(time);
  gfx::update_effects(dt);

  switch(scene) {
    case Scene::TITLE:
      update_demo(dt);
      update_title();
      break;
    case Scene::SCORES:
    case Scene::CREDITS:
      update_demo(dt);
      update_info_screen();
      break;
    case Scene::READY:
      update_ready();
      break;
    case Scene::PLAYING:
      update_playing(dt);
      break;
    case Scene::PAUSED:
      update_paused();
      break;
    case Scene::DYING:
      if(in_scene() >= DYING_MS) finish_round();
      break;
    case Scene::NAME_ENTRY:
      update_name_entry();
      break;
    case Scene::RESULTS:
      update_results();
      break;
  }
}

void render(uint32_t time) {
  screen.alpha = 255;
  screen.pen = gfx::colors::bg;
  screen.clear();

  if(now_ms < shake_until) {
    int mag = (shake_until - now_ms) / 120 + 1;
    gfx::set_shake(Point(int(blit::random() % (mag * 2 + 1)) - mag, int(blit::random() % (mag * 2 + 1)) - mag));
  } else {
    gfx::set_shake(Point(0, 0));
  }

  switch(scene) {
    case Scene::TITLE:      render_title(time); break;
    case Scene::SCORES:     render_scores(time); break;
    case Scene::CREDITS:    render_credits(time); break;
    case Scene::READY:      render_ready(time); break;
    case Scene::PLAYING:    render_playing(time); break;
    case Scene::PAUSED:     render_paused(time); break;
    case Scene::DYING:      render_dying(time); break;
    case Scene::NAME_ENTRY: render_name_entry(time); break;
    case Scene::RESULTS:    render_results(time); break;
  }
}
