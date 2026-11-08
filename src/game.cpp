#include "32blit.hpp"
#include "game.hpp"

#include "chili/audio.hpp"
#include "chili/fx.hpp"
#include "chili/menu.hpp"
#include "chili/scene.hpp"
#include "chili/screens.hpp"
#include "chili/ui.hpp"

#include "render.hpp"
#include "save.hpp"
#include "snake.hpp"
#include "sounds.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

using namespace blit;
using namespace snek;

namespace audio = chili::audio;
namespace fx = chili::fx;

namespace {

const char *const speed_tags[] = {"S", "N", "F"};

// Step length in ms at the start of a round and the fastest it ramps up to.
const uint16_t start_step_ms[] = {200, 150, 110};
const uint16_t min_step_ms[] = {115, 80, 60};
const uint16_t apple_points[] = {50, 100, 150};

const uint32_t COUNTDOWN_MS = 1350;
const uint32_t DYING_MS = 1300;

chili::Scenes scenes;
fx::Particles particles(48);
fx::FloatingText popups(6);
fx::Shake shake;

Board board;
Board demo;
uint32_t step_timer = 0;
uint32_t demo_timer = 0;

uint32_t score = 0;
bool won = false;
int new_rank = -1;
uint32_t best_at_start = 0;

SaveData &settings() {
  return save.data;
}

uint32_t best_score() {
  return settings().scores.best().score;
}

uint16_t step_ms() {
  int s = settings().speed;
  int v = int(start_step_ms[s]) - board.apples_eaten() * 4;
  return uint16_t(std::max(v, int(min_step_ms[s])));
}

uint32_t points_per_apple() {
  return apple_points[settings().speed] + (settings().walls ? 50 : 0);
}

void apply_audio_settings() {
  audio::set_music(settings().music);
  audio::set_sound(settings().sound);
}

bool entering_name = false;

void ui_sound(chili::UiSound s) {
  switch(s) {
    case chili::UiSound::move: audio::play_sfx(sounds::menu_move); break;
    // A setting changing plays the select blip; a letter changing, the move one.
    case chili::UiSound::change: audio::play_sfx(entering_name ? sounds::menu_move : sounds::menu_select); break;
    case chili::UiSound::select: audio::play_sfx(sounds::menu_select); break;
    case chili::UiSound::back: audio::play_sfx(sounds::menu_back); break;
  }
}

bool read_dir(Dir &d) {
  if(buttons.pressed & Button::DPAD_UP) { d = UP; return true; }
  if(buttons.pressed & Button::DPAD_DOWN) { d = DOWN; return true; }
  if(buttons.pressed & Button::DPAD_LEFT) { d = LEFT; return true; }
  if(buttons.pressed & Button::DPAD_RIGHT) { d = RIGHT; return true; }
  return false;
}

Point cell_centre(Point cell) {
  return gfx::cell_to_px(cell) + Point(gfx::CELL / 2, gfx::CELL / 2);
}

// Scene changes, defined once every scene exists.
void start_round();
void go_to_title();
void finish_round();
void open_scores();
void open_credits();
void pause_game();
void resume_game();

// ---------------------------------------------------------------------------
// Shared drawing

void update_demo(uint32_t dt) {
  demo_timer += dt;
  if(demo_timer < 130) return;
  demo_timer = 0;
  demo.autopilot();
  Event e = demo.step();
  if(e == Event::DIED || e == Event::WON || demo.length() > 60) demo.reset(false);
}

void render_demo_background(uint32_t time) {
  gfx::set_shake(Point(0, 0));
  gfx::draw_board(false, time);
  gfx::draw_apple(demo.food(), time);
  if(demo.golden_active()) gfx::draw_golden_apple(demo.golden(), time, demo.golden_steps_left());
  gfx::draw_snake(demo, time);
  fx::dim(150);
}

void render_game_world(uint32_t time, gfx::SnakeLook look) {
  gfx::draw_hud(score, best_at_start, board.length(), time);
  gfx::draw_board(board.walls(), time);
  gfx::draw_apple(board.food(), time);
  if(board.golden_active()) gfx::draw_golden_apple(board.golden(), time, board.golden_steps_left());
  gfx::draw_snake(board, time, look);
  particles.render(gfx::shake_offset());
  popups.render();
}

// The high score table with snek's speed/walls column and gold blink.
class SnekScoreTable : public chili::ScoreTable {
protected:
  void draw_row(int place, const char *name, uint32_t score, Rect row, bool highlight, uint32_t time) override {
    if(highlight) {
      screen.pen = ((time / 250) & 1) ? Pen(200, 160, 40, 120) : Pen(200, 160, 40, 70);
      screen.rectangle(row);
    }
    // The score ends at x 88; the tag column takes the rest.
    chili::ScoreTable::draw_row(place, name, score, Rect(row.x, row.y, row.w - 22, row.h), false, time);
    if(!name) return;
    const ScoreEntry &e = settings().scores[place];
    std::string tag = speed_tags[e.speed < 3 ? e.speed : 1];
    if(e.walls) tag += "W";
    fx::text(tag, Point(row.x + row.w - 4, row.y + 1), gfx::colors::dim, TextAlign::top_right);
  }
};

SnekScoreTable score_table;

void render_score_table(int top, int highlight, uint32_t time) {
  score_table.render(settings().scores, Rect(14, top - 1, 100, SCORE_SLOTS * 9), highlight, time);
}

// ---------------------------------------------------------------------------
// Title

// Title menu look: a green bar and a little snake head pointing at the
// selected row.
class TitleMenu : public chili::Menu {
public:
  using chili::Menu::Menu;
  uint32_t time = 0;

protected:
  void draw_item(const Item &item, Rect row, bool selected) override {
    int y = row.y + 1;
    if(selected) {
      screen.pen = Pen(70, 150, 80, 150);
      screen.rectangle(row);
      int wiggle = (time / 200) & 1;
      screen.pen = Pen(160, 240, 96);
      screen.rectangle(Rect(6 + wiggle, y + 1, 5, 5));
      screen.pen = Pen(15, 20, 15);
      screen.pixel(Point(9 + wiggle, y + 2));
    }
    fx::text(item.label, Point(row.x + 4, y), selected ? gfx::colors::text : Pen(170, 200, 160));
    std::string value = value_text(item);
    if(value.empty()) return;
    if(selected) value = "< " + value + " >";
    fx::text(value, Point(row.x + row.w - 4, y), selected ? gfx::colors::accent : gfx::colors::dim, TextAlign::top_right);
  }
};

struct Title : chili::Scene {
  TitleMenu menu{{
    chili::Menu::action("PLAY", [] { start_round(); }),
    chili::Menu::choice("SPEED", {"SLOW", "NORMAL", "FAST"}, save.data.speed, [] { save.store(); }),
    chili::Menu::toggle("WALLS", save.data.walls, [] { save.store(); }),
    chili::Menu::toggle("MUSIC", save.data.music, [] {
      apply_audio_settings();
      if(save.data.music) audio::play(sounds::title);
      save.store();
    }),
    chili::Menu::toggle("SOUND", save.data.sound, [] {
      apply_audio_settings();
      save.store();
    }),
    chili::Menu::action("HIGH SCORES", [] { open_scores(); }),
    chili::Menu::action("CREDITS", [] { open_credits(); }),
  }};

  Title() { menu.row_height = 9; }

  void update(uint32_t dt) override {
    update_demo(dt);
    menu.update(scenes.pressed());
  }

  void render(uint32_t time) override {
    render_demo_background(time);
    gfx::draw_logo(6, time);
    fx::text("Eat them apples!", Point(64, 31), gfx::colors::dim, TextAlign::top_center);
    menu.time = time;
    menu.render(Rect(14, 42, 100, menu.count() * 9));
    if(best_score() > 0) {
      std::string best = "BEST " + std::to_string(best_score()) + " " + settings().scores.best().name;
      fx::text(best, Point(64, 118), gfx::colors::gold, TextAlign::top_center);
    }
  }
} title;

// ---------------------------------------------------------------------------
// High scores and credits

struct Scores : chili::Scene {
  void update(uint32_t dt) override {
    update_demo(dt);
    if(scenes.pressed() & (Button::A | Button::B | Button::MENU)) {
      chili::play_ui_sound(chili::UiSound::back);
      scenes.go(title);
    }
  }

  void render(uint32_t time) override {
    render_demo_background(time);
    fx::big_text("HIGH SCORES", Point(64, 16), 1, gfx::colors::gold);
    fx::panel(Rect(8, 28, 112, 56));
    render_score_table(34, -1, time);
    fx::text("S/N/F = speed  W = walls", Point(64, 92), gfx::colors::dim, TextAlign::top_center);
    fx::text("B: BACK", Point(64, 114), gfx::colors::text, TextAlign::top_center);
  }
} scores_scene;

// Credits page with the snek logo on top.
class CreditsPage : public chili::TextScreen {
public:
  using chili::TextScreen::TextScreen;

  void render(uint32_t time) override {
    gfx::draw_logo(6, time);
    fx::panel(Rect(6, 34, 116, 76));
    int y = 39;
    for(size_t i = 0; i < lines.size(); i++) {
      fx::text(lines[i], Point(64, y), i == 0 ? gfx::colors::accent : gfx::colors::text, TextAlign::top_center);
      y += line_height;
    }
    fx::text("B: BACK", Point(64, 114), gfx::colors::text, TextAlign::top_center);
  }
};

struct Credits : chili::Scene {
  CreditsPage page{"CREDITS", {
    "snek v1.0.0",
    "",
    "Game: TeriyakiGod",
    "Original chiptune",
    "music made for snek",
    "Made with 32blit",
    "for the VGC Zero",
    "chilichip.eu",
  }};

  void update(uint32_t dt) override {
    update_demo(dt);
    if(page.update(scenes.pressed())) scenes.go(title);
  }

  void render(uint32_t time) override {
    render_demo_background(time);
    page.render(time);
  }
} credits;

// ---------------------------------------------------------------------------
// A round: ready, playing, paused, dying

struct Ready : chili::Scene {
  void update(uint32_t dt) override {
    Dir d;
    if(read_dir(d)) board.queue_turn(d);

    uint32_t t = scenes.time_in_scene();
    uint32_t prev = t > 10 ? t - 10 : 0;
    // Beep on each count: 3 (on entry), 2, 1, then GO.
    for(uint32_t mark : {450u, 900u}) {
      if(prev < mark && t >= mark) audio::play_sfx(sounds::countdown);
    }
    if(t >= COUNTDOWN_MS) {
      audio::play_sfx(sounds::go);
      audio::play(sounds::game);
      scenes.go(playing_scene());
    }
  }

  void render(uint32_t time) override {
    render_game_world(time, gfx::SnakeLook::NORMAL);
    uint32_t t = scenes.time_in_scene();
    const char *label = t < 450 ? "3" : t < 900 ? "2" : "1";
    int pulse = 3 + ((t % 450) < 120 ? 1 : 0);
    fx::big_text(label, Point(64, 60), pulse, gfx::colors::text);
    if(board.walls()) {
      fx::text("WALLS ON", Point(64, 88), Pen(214, 120, 48), TextAlign::top_center);
    }
  }

  static chili::Scene &playing_scene();
} ready;

void add_popup(Point at, uint32_t points, Pen color) {
  char buf[8];
  snprintf(buf, sizeof(buf), "+%lu", (unsigned long)points);
  // Keep the text on screen when eating at the board edges.
  at.x = std::max<int32_t>(14, std::min<int32_t>(114, at.x));
  at.y = std::max<int32_t>(gfx::BOARD_Y + 4, at.y);
  popups.add(at, buf, color);
}

struct Dying : chili::Scene {
  void update(uint32_t dt) override {
    if(scenes.time_in_scene() >= DYING_MS) finish_round();
  }

  void render(uint32_t time) override {
    uint32_t t = scenes.time_in_scene();
    gfx::SnakeLook look = gfx::SnakeLook::DEAD_DARK;
    if(t < 700) look = ((t / 100) & 1) ? gfx::SnakeLook::DEAD_LIGHT : gfx::SnakeLook::DEAD_DARK;
    render_game_world(time, look);
    // Red flash right at the moment of impact.
    if(t < 150) {
      screen.pen = Pen(255, 40, 40, uint8_t(120 - t * 120 / 150));
      screen.rectangle(Rect(0, 0, 128, 128));
    }
  }
} dying;

struct Playing : chili::Scene {
  void on_event(Event e) {
    switch(e) {
      case Event::MOVED:
        break;

      case Event::ATE: {
        score += points_per_apple();
        Point at = cell_centre(board.head());
        particles.burst(at, Pen(235, 60, 70), 10);
        add_popup(at - Point(0, 6), points_per_apple(), gfx::colors::text);
        if(board.golden_active() && board.golden_steps_left() == GOLDEN_LIFETIME) {
          audio::play_sfx(sounds::golden_spawn);
        } else {
          audio::play_sfx(sounds::eat);
        }
        break;
      }

      case Event::ATE_GOLDEN: {
        uint32_t pts = points_per_apple() * 5;
        score += pts;
        Point at = cell_centre(board.head());
        particles.burst(at, gfx::colors::gold, 20);
        add_popup(at - Point(0, 6), pts, gfx::colors::gold);
        audio::play_sfx(sounds::golden);
        break;
      }

      case Event::DIED:
        audio::play_sfx(sounds::die);
        audio::play(sounds::game_over);
        shake.start(450);
        scenes.go(dying);
        break;

      case Event::WON:
        won = true;
        score += points_per_apple() * 10;
        particles.burst(cell_centre(board.head()), gfx::colors::gold, 40);
        finish_round();
        break;
    }
  }

  void update(uint32_t dt) override {
    if(buttons.pressed & (Button::MENU | Button::Y)) {
      pause_game();
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
    int s = settings().speed;
    float progress = float(start_step_ms[s] - step_ms()) / float(start_step_ms[s] - min_step_ms[s]);
    audio::set_tempo(1.0f - 0.2f * progress);
  }

  void render(uint32_t time) override {
    render_game_world(time, gfx::SnakeLook::NORMAL);
    if(scenes.time_in_scene() < 400) {
      fx::big_text("GO!", Point(64, 60), 3, gfx::colors::accent);
    }
  }
} playing;

chili::Scene &Ready::playing_scene() {
  return playing;
}

// Pause menu look: centred labels.
class PauseMenu : public chili::Menu {
public:
  using chili::Menu::Menu;

protected:
  void draw_item(const Item &item, Rect row, bool selected) override {
    if(selected) {
      screen.pen = Pen(70, 150, 80, 150);
      screen.rectangle(Rect(row.x, row.y, row.w, 9));
    }
    fx::text(item.label, Point(64, row.y + 1), selected ? gfx::colors::text : gfx::colors::dim, TextAlign::top_center);
  }
};

struct Paused : chili::Scene {
  PauseMenu menu{{
    chili::Menu::action("RESUME", [] { resume_game(); }),
    chili::Menu::action("RESTART", [] { start_round(); }),
    chili::Menu::action("MENU", [] { go_to_title(); }),
  }};

  void enter() override { menu.select(0); }

  void update(uint32_t dt) override {
    if(buttons.pressed & Button::MENU) {
      audio::pause(false);
      scenes.go(playing);
      return;
    }
    menu.update(scenes.pressed());
  }

  void render(uint32_t time) override {
    render_game_world(time, gfx::SnakeLook::NORMAL);
    fx::dim(140);
    fx::panel(Rect(24, 36, 80, 58));
    fx::big_text("PAUSED", Point(64, 47), 2, gfx::colors::text);
    menu.render(Rect(30, 60, 68, 30));
  }
} paused;

// ---------------------------------------------------------------------------
// After a round: name entry and results

struct NameEntryScene : chili::Scene {
  chili::NameEntry name;

  void enter() override {
    name.start(settings().last_name);
    entering_name = true;
  }

  void update(uint32_t dt) override {
    if(!name.update(scenes.pressed())) return;
    ScoreEntry e{};
    memcpy(e.name, name.name(), 3);
    e.score = score;
    e.length = board.length();
    e.speed = settings().speed;
    e.walls = settings().walls ? 1 : 0;
    settings().scores.insert(new_rank, e);
    memcpy(settings().last_name, name.name(), sizeof(settings().last_name));
    save.store();
    entering_name = false;
    scenes.go(results_scene());
  }

  void render(uint32_t time) override {
    render_game_world(time, gfx::SnakeLook::DEAD_DARK);
    fx::dim(150);
    fx::panel(Rect(10, 22, 108, 86));

    Pen heading = ((time / 250) & 1) ? gfx::colors::gold : gfx::colors::text;
    fx::big_text(new_rank == 0 ? "NEW BEST!" : "HIGH SCORE!", Point(64, 33), new_rank == 0 ? 2 : 1, heading);

    char line[40];
    snprintf(line, sizeof(line), "#%d   %lu", new_rank + 1, (unsigned long)score);
    fx::text(line, Point(64, 45), gfx::colors::text, TextAlign::top_center);
    fx::text("ENTER YOUR NAME", Point(64, 56), gfx::colors::dim, TextAlign::top_center);
    name.render(Point(64, 78), time);
    fx::text("A: OK   B: BACK", Point(64, 96), gfx::colors::dim, TextAlign::top_center);
  }

  static chili::Scene &results_scene();
} name_entry;

struct Results : chili::Scene {
  void update(uint32_t dt) override {
    if(scenes.time_in_scene() < 400) return;  // don't skip the screen with a held button
    uint32_t pressed = scenes.pressed();
    if(pressed & Button::A) {
      start_round();
    } else if(pressed & (Button::B | Button::MENU)) {
      chili::play_ui_sound(chili::UiSound::back);
      go_to_title();
    }
  }

  void render(uint32_t time) override {
    render_game_world(time, won ? gfx::SnakeLook::NORMAL : gfx::SnakeLook::DEAD_DARK);
    fx::dim(150);
    fx::panel(Rect(6, 8, 116, 112));

    fx::big_text(won ? "YOU WIN!" : "GAME OVER", Point(64, 19), 2, won ? gfx::colors::gold : gfx::colors::red);

    char line[40];
    snprintf(line, sizeof(line), "SCORE %lu   LENGTH %d", (unsigned long)score, board.length());
    fx::text(line, Point(64, 32), gfx::colors::text, TextAlign::top_center);

    if(new_rank == 0) {
      Pen c = ((time / 250) & 1) ? gfx::colors::gold : gfx::colors::text;
      fx::text("NEW BEST SCORE!", Point(64, 42), c, TextAlign::top_center);
    } else if(new_rank > 0) {
      snprintf(line, sizeof(line), "RANK #%d", new_rank + 1);
      fx::text(line, Point(64, 42), gfx::colors::gold, TextAlign::top_center);
    } else {
      fx::text("So close. Try again!", Point(64, 42), gfx::colors::dim, TextAlign::top_center);
    }

    screen.pen = Pen(90, 170, 90, 120);
    screen.rectangle(Rect(14, 52, 100, 1));
    render_score_table(56, new_rank, time);

    if(scenes.time_in_scene() >= 400) {
      fx::text("A: AGAIN    B: MENU", Point(64, 108), gfx::colors::text, TextAlign::top_center);
    }
  }
} results;

chili::Scene &NameEntryScene::results_scene() {
  return results;
}

// ---------------------------------------------------------------------------
// Scene changes

void start_round() {
  board.reset(settings().walls);
  score = 0;
  won = false;
  new_rank = -1;
  best_at_start = best_score();
  step_timer = 0;
  particles.clear();
  popups.clear();
  audio::stop();
  audio::play_sfx(sounds::countdown);
  scenes.go(ready);
}

void go_to_title() {
  demo.reset(false);
  audio::play(sounds::title);
  scenes.go(title);
}

void finish_round() {
  new_rank = settings().scores.rank(score);
  if(new_rank >= 0) {
    audio::play(sounds::fanfare);
    scenes.go(name_entry);
  } else {
    scenes.go(results);
  }
}

void open_scores() {
  scenes.go(scores_scene);
}

void open_credits() {
  scenes.go(credits);
}

void pause_game() {
  audio::play_sfx(sounds::pause);
  audio::pause(true);
  scenes.go(paused);
}

void resume_game() {
  audio::pause(false);
  scenes.go(playing);
}

}

// ---------------------------------------------------------------------------

void init() {
  set_screen_mode(ScreenMode::hires);
  load_save();
  gfx::apply_theme();
  chili::ui_sound = ui_sound;
  audio::init();
  apply_audio_settings();
  go_to_title();
}

void update(uint32_t time) {
  static uint32_t last_update = 0;
  uint32_t dt = last_update ? time - last_update : 0;
  last_update = time;

  audio::update(time);
  particles.update(dt);
  popups.update(dt);
  shake.update(dt);
  scenes.update(time);
}

void render(uint32_t time) {
  screen.alpha = 255;
  screen.pen = gfx::colors::bg;
  screen.clear();
  gfx::set_shake(shake.offset());
  scenes.render(time);
}
