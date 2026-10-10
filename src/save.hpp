#pragma once

#include <cstdint>

// Settings and the high score table, persisted with the 32blit save API.
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
  uint32_t magic;
  uint8_t version;
  uint8_t speed;
  uint8_t walls;
  uint8_t music;
  uint8_t sound;
  char last_name[4];
  ScoreEntry scores[SCORE_SLOTS];
};

extern SaveData save;

void load_save();
void write_save_data();

// Rank (0-based) the score would take in the table, or -1 if it does not
// make it.
int score_rank(uint32_t score);
// Inserts a score at the given rank, pushing lower entries down.
void insert_score(int rank, const char *name, uint32_t score, uint16_t length, uint8_t speed, bool walls);

}
