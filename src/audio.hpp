#pragma once

#include <cstdint>

// Soundtrack sequencer and sound effects, all synthesised on the 32blit
// audio channels. Channels 0-3 play music (lead, bass, kick, noise), 4-5 play
// sound effects.
namespace snek::audio {

enum class Song : uint8_t {
  NONE,
  TITLE,
  GAME,
  GAME_OVER,
  FANFARE,
};

enum class Sfx : uint8_t {
  MENU_MOVE,
  MENU_SELECT,
  MENU_BACK,
  EAT,
  GOLDEN,
  GOLDEN_SPAWN,
  DIE,
  COUNTDOWN,
  GO,
  PAUSE,
};

void init();
// Call from blit's update(); time is the update time in ms.
void update(uint32_t time);

void play_song(Song song);
void stop_song();
void pause_song(bool paused);
// Whether a non-looping song (jingle) has played to the end.
bool song_finished();
Song current_song();
// Scales the song's step length; values below 1 play faster.
void set_tempo_scale(float scale);

void play_sfx(Sfx sfx);

void set_music_enabled(bool enabled);
void set_sfx_enabled(bool enabled);

}
