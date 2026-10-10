#pragma once

#include "32blit.hpp"

#include <cstdint>

// Board model: the snake, the apples and the rules. No drawing or sound here,
// so the title screen demo and the real game share the exact same logic.
namespace snek {

constexpr int GRID_W = 16;
constexpr int GRID_H = 14;
constexpr int MAX_LEN = GRID_W * GRID_H;

// How many steps a golden apple stays on the board before it vanishes.
constexpr int GOLDEN_LIFETIME = 40;

enum Dir : uint8_t { UP, DOWN, LEFT, RIGHT };

blit::Point dir_vec(Dir d);
Dir opposite(Dir d);

enum class Event : uint8_t {
  MOVED,
  ATE,
  ATE_GOLDEN,
  DIED,
  WON,
};

class Board {
public:
  void reset(bool walls);

  // Queues a turn for the next steps. Up to two turns are buffered so a quick
  // "up, left" tap pair is not lost, and a turn straight back into the body
  // is ignored.
  bool queue_turn(Dir d);

  // Advances the snake one cell and reports what happened.
  Event step();

  // Picks a turn for the attract-mode snake on the title screen.
  void autopilot();

  int length() const { return len; }
  blit::Point segment(int i) const { return body[i]; }
  blit::Point head() const { return body[0]; }
  Dir heading() const { return dir; }
  // Direction the head will move on the next step, after any queued turn.
  Dir next_heading() const { return queued ? queue[0] : dir; }
  bool walls() const { return solid_walls; }

  blit::Point food() const { return apple; }
  bool golden_active() const { return golden_steps > 0; }
  blit::Point golden() const { return golden_pos; }
  int golden_steps_left() const { return golden_steps; }

  int apples_eaten() const { return eaten; }

  // Direction from cell a to the neighbouring cell b, taking wrap-around into
  // account. Used to join body segments when drawing.
  static Dir dir_between(blit::Point a, blit::Point b);

private:
  bool in_bounds(blit::Point p) const;
  blit::Point wrap(blit::Point p) const;
  bool blocked(blit::Point p, bool tail_moves) const;
  bool random_free_cell(blit::Point &out) const;
  int reachable_cells(blit::Point from) const;

  blit::Point body[MAX_LEN];
  bool occupied[GRID_H][GRID_W];
  int len = 0;
  Dir dir = RIGHT;
  Dir queue[2];
  int queued = 0;
  bool solid_walls = false;

  blit::Point apple;
  blit::Point golden_pos;
  int golden_steps = 0;
  int eaten = 0;
};

}
