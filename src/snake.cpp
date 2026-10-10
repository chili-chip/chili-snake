#include "snake.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace blit;

namespace snek {

Point dir_vec(Dir d) {
  switch(d) {
    case UP:    return Point(0, -1);
    case DOWN:  return Point(0, 1);
    case LEFT:  return Point(-1, 0);
    case RIGHT: return Point(1, 0);
  }
  return Point(0, 0);
}

Dir opposite(Dir d) {
  switch(d) {
    case UP:    return DOWN;
    case DOWN:  return UP;
    case LEFT:  return RIGHT;
    case RIGHT: return LEFT;
  }
  return d;
}

Dir Board::dir_between(Point a, Point b) {
  int dx = (b.x - a.x + GRID_W) % GRID_W;
  int dy = (b.y - a.y + GRID_H) % GRID_H;
  if(dx == 1) return RIGHT;
  if(dx == GRID_W - 1) return LEFT;
  if(dy == 1) return DOWN;
  return UP;
}

void Board::reset(bool walls) {
  solid_walls = walls;
  memset(occupied, 0, sizeof(occupied));

  // Start as a short snake in the middle-left of the board, heading right.
  len = 3;
  for(int i = 0; i < len; i++) {
    body[i] = Point(5 - i, GRID_H / 2);
    occupied[body[i].y][body[i].x] = true;
  }
  dir = RIGHT;
  queued = 0;
  eaten = 0;
  golden_steps = 0;

  random_free_cell(apple);
}

bool Board::queue_turn(Dir d) {
  if(queued >= 2) return false;
  Dir last = queued ? queue[queued - 1] : dir;
  if(d == last || d == opposite(last)) return false;
  queue[queued++] = d;
  return true;
}

bool Board::in_bounds(Point p) const {
  return p.x >= 0 && p.y >= 0 && p.x < GRID_W && p.y < GRID_H;
}

Point Board::wrap(Point p) const {
  return Point((p.x + GRID_W) % GRID_W, (p.y + GRID_H) % GRID_H);
}

bool Board::blocked(Point p, bool tail_moves) const {
  if(!in_bounds(p)) return true;
  if(!occupied[p.y][p.x]) return false;
  // The tail cell frees up during the same step, so moving into it is fine.
  if(tail_moves && p == body[len - 1]) return false;
  return true;
}

bool Board::random_free_cell(Point &out) const {
  int free_cells = MAX_LEN - len;
  if(golden_steps > 0) free_cells--;
  if(free_cells <= 0) return false;

  int pick = blit::random() % free_cells;
  for(int y = 0; y < GRID_H; y++) {
    for(int x = 0; x < GRID_W; x++) {
      if(occupied[y][x]) continue;
      if(golden_steps > 0 && golden_pos == Point(x, y)) continue;
      if(pick-- == 0) {
        out = Point(x, y);
        return true;
      }
    }
  }
  return false;
}

Event Board::step() {
  if(queued) {
    dir = queue[0];
    queue[0] = queue[1];
    queued--;
  }

  Point next = head() + dir_vec(dir);
  if(!in_bounds(next)) {
    if(solid_walls) return Event::DIED;
    next = wrap(next);
  }

  bool eats_apple = next == apple;
  bool eats_golden = golden_steps > 0 && next == golden_pos;
  bool grows = eats_apple || eats_golden;

  if(blocked(next, !grows)) return Event::DIED;

  Point tail = body[len - 1];
  if(!grows) occupied[tail.y][tail.x] = false;

  for(int i = len - 1; i > 0; i--) body[i] = body[i - 1];
  if(grows) {
    body[len] = tail;
    len++;
  }
  body[0] = next;
  occupied[next.y][next.x] = true;

  if(golden_steps > 0) golden_steps--;

  if(eats_golden) {
    golden_steps = 0;
    if(len == MAX_LEN) return Event::WON;
    return Event::ATE_GOLDEN;
  }

  if(eats_apple) {
    eaten++;
    if(!random_free_cell(apple)) return Event::WON;
    // Every fifth apple a golden one shows up for a short while.
    if(eaten % 5 == 0 && golden_steps == 0) {
      Point p;
      if(random_free_cell(p) && p != apple) {
        golden_pos = p;
        golden_steps = GOLDEN_LIFETIME;
      }
    }
    return Event::ATE;
  }

  return Event::MOVED;
}

int Board::reachable_cells(Point from) const {
  static bool seen[GRID_H][GRID_W];
  static Point todo[MAX_LEN];
  memset(seen, 0, sizeof(seen));

  int count = 0, read = 0, write = 0;
  todo[write++] = from;
  seen[from.y][from.x] = true;

  while(read < write) {
    Point p = todo[read++];
    count++;
    for(int d = 0; d < 4; d++) {
      Point n = p + dir_vec(Dir(d));
      if(!in_bounds(n)) {
        if(solid_walls) continue;
        n = wrap(n);
      }
      if(seen[n.y][n.x] || occupied[n.y][n.x]) continue;
      seen[n.y][n.x] = true;
      todo[write++] = n;
    }
  }
  return count;
}

void Board::autopilot() {
  Point target = golden_steps > 0 ? golden_pos : apple;

  auto wrapped_distance = [this](Point a, Point b) {
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    if(!solid_walls) {
      dx = std::min(dx, GRID_W - dx);
      dy = std::min(dy, GRID_H - dy);
    }
    return dx + dy;
  };

  int best_score = -100000;
  Dir best = dir;
  for(int d = 0; d < 4; d++) {
    if(Dir(d) == opposite(dir)) continue;
    Point next = head() + dir_vec(Dir(d));
    if(!in_bounds(next)) {
      if(solid_walls) continue;
      next = wrap(next);
    }
    if(blocked(next, true)) continue;

    int score = -wrapped_distance(next, target) * 10;
    // Avoid turning into pockets that are smaller than the snake.
    if(reachable_cells(next) < len) score -= 1000;
    // A little wobble so the demo does not look robotic.
    score += blit::random() % 4;

    if(score > best_score) {
      best_score = score;
      best = Dir(d);
    }
  }

  queued = 0;
  if(best != dir) queue_turn(best);
}

}
