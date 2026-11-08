#pragma once

#include "chili/save.hpp"
#include "chili/scores.hpp"

#include <cstdint>

// Settings and the high score table, kept with the chili game SDK's Save.
namespace snek {

enum Speed : uint8_t { SLOW, NORMAL, FAST };

constexpr int SCORE_SLOTS = 5;

struct ScoreEntry {
  char name[4];     // three letters + terminator
  uint32_t score;
  uint16_t length;
  uint8_t speed;
  uint8_t walls;
};

struct SaveData {
  uint8_t speed = NORMAL;
  uint8_t walls = 0;
  uint8_t music = 1;
  uint8_t sound = 1;
  char last_name[4] = "AAA";
  chili::HighScores<ScoreEntry, SCORE_SLOTS> scores{};
};

extern chili::Save<SaveData> save;

// Loads the save. Imports the v1.0.0 save and the v0.0.1 high score when
// there is no current one.
void load_save();

}
