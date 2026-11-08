#include "save.hpp"

#include "32blit.hpp"

#include <cstring>

using namespace blit;

namespace snek {

static const uint32_t SAVE_MAGIC = 0x4b4e5332;  // "2SNK"
// Version 2 is the chili game SDK's save format. Version 1 was snek v1.0.0's
// own layout, read below.
static const uint16_t SAVE_VERSION = 2;

chili::Save<SaveData> save(SAVE_MAGIC, SAVE_VERSION);

// Save written by snek v1.0.0: the same fields, after a magic and a 1-byte
// version.
struct SaveV1 {
  uint32_t magic;
  uint8_t version;
  uint8_t speed;
  uint8_t walls;
  uint8_t music;
  uint8_t sound;
  char last_name[4];
  ScoreEntry scores[SCORE_SLOTS];
};

// Save file written by v0.0.1, holding a single int high score.
static const char *LEGACY_SAVE_FILE = "snek_save_data.bin";

static bool import_v1() {
  SaveV1 old;
  if(!read_save(old) || old.magic != SAVE_MAGIC || old.version != 1) return false;

  SaveData &d = save.data;
  d.speed = old.speed;
  d.walls = old.walls;
  d.music = old.music;
  d.sound = old.sound;
  memcpy(d.last_name, old.last_name, sizeof(d.last_name));
  for(int i = 0; i < SCORE_SLOTS; i++) d.scores[i] = old.scores[i];
  return true;
}

static void import_legacy_highscore() {
  File legacy(LEGACY_SAVE_FILE, OpenMode::read);
  if(!legacy.is_open()) return;

  int32_t old_score = 0;
  if(legacy.read(0, sizeof(old_score), (char *)&old_score) == sizeof(old_score) && old_score > 0) {
    // v0.0.1 gave 100 points per apple on a wrapping board, same as NORMAL now.
    ScoreEntry e{};
    memcpy(e.name, "OLD", 3);
    e.score = old_score;
    e.length = old_score / 100 + 1;
    e.speed = NORMAL;
    save.data.scores.insert(0, e);
  }
  legacy.close();
}

void load_save() {
  if(!save.load()) {
    if(!import_v1()) import_legacy_highscore();
    save.store();
  }

  SaveData &d = save.data;
  for(int i = 0; i < SCORE_SLOTS; i++) d.scores[i].name[3] = 0;
  d.last_name[3] = 0;
  if(d.speed > FAST) d.speed = NORMAL;
}

}
