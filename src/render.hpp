#pragma once

#include "32blit.hpp"
#include "snake.hpp"


// Snek's own drawing: board, snake, apples, HUD and logo. Text, panels,
// particles and popups come from the chili game SDK (chili/fx.hpp).
namespace snek::gfx {

constexpr int CELL = 8;
constexpr int HUD_H = 16;
constexpr int BOARD_X = 0;
constexpr int BOARD_Y = HUD_H;

namespace colors {
  const blit::Pen bg(10, 18, 14);
  const blit::Pen text(236, 245, 220);
  const blit::Pen dim(120, 150, 120);
  const blit::Pen accent(140, 230, 90);
  const blit::Pen gold(255, 210, 60);
  const blit::Pen red(235, 70, 70);
  const blit::Pen shadow(0, 0, 0);
}

enum class SnakeLook : uint8_t { NORMAL, DEAD_LIGHT, DEAD_DARK };

// Offset applied to everything on the board, for screen shake.
void set_shake(blit::Point offset);
blit::Point shake_offset();

// Sets chili::theme to snek's colours.
void apply_theme();

blit::Point cell_to_px(blit::Point cell);

void draw_board(bool walls, uint32_t time);
void draw_snake(const Board &board, uint32_t time, SnakeLook look = SnakeLook::NORMAL);
void draw_apple(blit::Point cell, uint32_t time);
void draw_golden_apple(blit::Point cell, uint32_t time, int steps_left);
void draw_hud(uint32_t score, uint32_t best, int length, uint32_t time);

// Animated "SNEK" title made of snake scales.
void draw_logo(int y, uint32_t time);

}
