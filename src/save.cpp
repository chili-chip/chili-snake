#include "save.hpp"

#include "32blit.hpp"

#include <cstring>

using namespace blit;

namespace snek {

static const uint32_t SAVE_MAGIC = 0x4b4e5332; // "2SNK"
static const uint8_t SAVE_VERSION = 1;
// Save file written by v0.0.1, holding a single int high score.
static const char *LEGACY_SAVE_FILE = "snek_save_data.bin";

SaveData save;

static void set_defaults() {
  memset(&save, 0, sizeof(save));
  save.magic = SAVE_MAGIC;
  save.version = SAVE_VERSION;
  save.speed = NORMAL;
  save.walls = 0;
  save.music = 1;
  save.sound = 1;
  strcpy(save.last_name, "AAA");
}

static void import_legacy_highscore() {
  File legacy(LEGACY_SAVE_FILE, OpenMode::read);
  if(!legacy.is_open()) return;

  int32_t old_score = 0;
  if(legacy.read(0, sizeof(old_score), (char *)&old_score) == sizeof(old_score) && old_score > 0) {
    // v0.0.1 gave 100 points per apple on a wrapping board, same as NORMAL now.
    insert_score(0, "OLD", old_score, old_score / 100 + 1, NORMAL, false);
  }
  legacy.close();
}

void load_save() {
  if(read_save(save) && save.magic == SAVE_MAGIC && save.version == SAVE_VERSION) {
    for(auto &e : save.scores) e.name[3] = 0;
    save.last_name[3] = 0;
    if(save.speed > FAST) save.speed = NORMAL;
    return;
  }

  set_defaults();
  import_legacy_highscore();
  write_save_data();
}

void write_save_data() {
  write_save(save);
}

int score_rank(uint32_t score) {
  if(score == 0) return -1;
  for(int i = 0; i < SCORE_SLOTS; i++) {
    if(score > save.scores[i].score) return i;
  }
  return -1;
}

void insert_score(int rank, const char *name, uint32_t score, uint16_t length, uint8_t speed, bool walls) {
  if(rank < 0 || rank >= SCORE_SLOTS) return;
  for(int i = SCORE_SLOTS - 1; i > rank; i--) save.scores[i] = save.scores[i - 1];

  ScoreEntry &e = save.scores[rank];
  memset(&e, 0, sizeof(e));
  memcpy(e.name, name, 3);
  e.score = score;
  e.length = length;
  e.speed = speed;
  e.walls = walls ? 1 : 0;
}

}
