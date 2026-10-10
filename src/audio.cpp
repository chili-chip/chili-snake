#include "audio.hpp"

#include "32blit.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace blit;

namespace snek::audio {

// ---------------------------------------------------------------------------
// Soundtrack
//
// Original chiptune written for snek (same licence as the rest of the repo).
// Each track is a list of 16th-note steps separated by spaces:
//   C4, F#5 ...  start a note      -  hold the previous note      .  rest
// Drum tracks use K (kick), S (snare) and H (hi-hat).
// ---------------------------------------------------------------------------

struct SongData {
  uint16_t step_ms;
  bool loop;
  const char *lead;
  const char *bass;
  const char *drums;
};

// Title theme: A minor, laid back.
static const SongData title_song = {
  140, true,
  // lead
  "A4 - - - C5 - E5 - D5 - C5 - B4 - C5 - "
  "A4 - - - F4 - A4 - C5 - - - . . . . "
  "B4 - - - D5 - G5 - F5 - E5 - D5 - B4 - "
  "G#4 - - - B4 - E5 - - - - - . . . . "
  "E5 - - - A5 - G5 - E5 - D5 - C5 - D5 - "
  "C5 - - - A4 - C5 - F5 - - - E5 - D5 - "
  "D5 - - - B4 - D5 - G5 - - - F5 - D5 - "
  "E5 - - - - - - - B4 - - - G#4 - - - ",
  // bass
  "A2 - - - E3 - - - A3 - - - E3 - - - "
  "F2 - - - C3 - - - F3 - - - C3 - - - "
  "G2 - - - D3 - - - G3 - - - D3 - - - "
  "E2 - - - B2 - - - E3 - - - G#3 - - - "
  "A2 - - - E3 - - - A3 - - - E3 - - - "
  "F2 - - - C3 - - - F3 - - - C3 - - - "
  "G2 - - - D3 - - - G3 - - - D3 - - - "
  "E2 - - - B2 - - - E3 - - - G#3 - - - ",
  // drums
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . . H . S S "
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . . H . . H "
  "K . . . H . . . S . . S S . S S ",
};

// In-game loop: Am F C G / Am F C E, driving eighth-note bass.
static const SongData game_song = {
  115, true,
  "E5 - . E5 A5 - . G5 E5 - D5 - C5 - D5 - "
  "C5 - . C5 F5 - . E5 C5 - A4 - C5 - . . "
  "G4 - C5 - E5 - G5 - E5 - G5 - C6 - B5 - "
  "D6 - - - B5 - G5 - D5 - G5 - B5 - . . "
  "A5 - . A5 G5 - E5 - A5 - . C6 B5 - A5 - "
  "F5 - . F5 E5 - C5 - A4 - C5 - F5 - E5 - "
  "E5 - G5 - C6 - . E5 G5 - C6 - E6 - D6 - "
  "B5 - - - G#5 - - - E5 - B4 - E5 - G#5 - ",
  "A2 - A3 - A2 - A3 - A2 - A3 - A2 - A3 - "
  "F2 - F3 - F2 - F3 - F2 - F3 - F2 - F3 - "
  "C3 - C4 - C3 - C4 - C3 - C4 - C3 - C4 - "
  "G2 - G3 - G2 - G3 - G2 - G3 - G2 - G3 - "
  "A2 - A3 - A2 - A3 - A2 - A3 - A2 - A3 - "
  "F2 - F3 - F2 - F3 - F2 - F3 - F2 - F3 - "
  "C3 - C4 - C3 - C4 - C3 - C4 - C3 - C4 - "
  "E2 - E3 - E2 - E3 - E2 - E3 - G#2 - B2 - ",
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K K H . S . H H "
  "K . H . S . H . K . S . S S S S ",
};

static const SongData game_over_song = {
  130, false,
  "E5 - D5 - C5 - B4 - A4 - - - - - - - ",
  "C3 - - - B2 - - - A2 - - - - - - - ",
  "K . . . . . . . K . . . . . . . ",
};

static const SongData fanfare_song = {
  85, false,
  "C5 E5 G5 C6 - G5 C6 - E6 - - - - - - - ",
  "C3 - - - - - - - G2 - - - C3 - - - ",
  "K . . . S . . . K . S . K . . . ",
};

// Parsed step codes.
static const uint8_t STEP_REST = 0;
static const uint8_t STEP_HOLD = 1;
static const uint8_t DRUM_KICK = 2;
static const uint8_t DRUM_SNARE = 3;
static const uint8_t DRUM_HAT = 4;

struct ParsedSong {
  const SongData *data = nullptr;
  std::vector<uint8_t> lead, bass, drums;
  size_t length = 0;
};

static ParsedSong songs[5];

static std::vector<uint8_t> parse_track(const char *text) {
  std::vector<uint8_t> steps;
  const char *p = text;
  while(*p) {
    while(*p == ' ') p++;
    if(!*p) break;

    const char *tok = p;
    while(*p && *p != ' ') p++;
    size_t n = p - tok;

    if(tok[0] == '-') steps.push_back(STEP_HOLD);
    else if(tok[0] == '.') steps.push_back(STEP_REST);
    else if(tok[0] == 'K') steps.push_back(DRUM_KICK);
    else if(tok[0] == 'S') steps.push_back(DRUM_SNARE);
    else if(tok[0] == 'H') steps.push_back(DRUM_HAT);
    else {
      static const int8_t semis[] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
      int semi = semis[tok[0] - 'A'];
      size_t i = 1;
      if(i < n && tok[i] == '#') { semi++; i++; }
      int octave = i < n ? tok[i] - '0' : 4;
      steps.push_back(uint8_t(12 * (octave + 1) + semi)); // MIDI note number
    }
  }
  return steps;
}

static void parse_song(ParsedSong &out, const SongData &data) {
  out.data = &data;
  out.lead = parse_track(data.lead);
  out.bass = parse_track(data.bass);
  out.drums = parse_track(data.drums);
  out.length = std::max(out.lead.size(), std::max(out.bass.size(), out.drums.size()));
}

static uint16_t midi_to_hz(uint8_t note) {
  return uint16_t(440.0f * powf(2.0f, (note - 69) / 12.0f) + 0.5f);
}

// Sequencer state
static Song song = Song::NONE;
static size_t step_index = 0;
static float step_timer = 0;
static float tempo_scale = 1.0f;
static bool song_paused = false;
static bool song_done = false;
static bool music_on = true;
static bool sfx_on = true;
static uint32_t last_time = 0;

// Kick drum pitch sweep, applied every update.
static float kick_freq = 0;

enum MusicChannel { CH_LEAD = 0, CH_BASS = 1, CH_KICK = 2, CH_NOISE = 3, CH_SFX_GAME = 4, CH_SFX_UI = 5 };

static void setup_music_channels() {
  AudioChannel &lead = channels[CH_LEAD];
  lead.waveforms = Waveform::SQUARE;
  lead.pulse_width = 0x4000; // 25% duty for a classic chip lead
  lead.attack_ms = 4;
  lead.decay_ms = 140;
  lead.sustain = 0x9000;
  lead.release_ms = 60;
  lead.volume = 0x3800;

  AudioChannel &bass = channels[CH_BASS];
  bass.waveforms = Waveform::TRIANGLE;
  bass.attack_ms = 2;
  bass.decay_ms = 90;
  bass.sustain = 0xa000;
  bass.release_ms = 30;
  bass.volume = 0x7000;

  AudioChannel &kick = channels[CH_KICK];
  kick.waveforms = Waveform::SINE;
  kick.attack_ms = 1;
  kick.decay_ms = 90;
  kick.sustain = 0;
  kick.release_ms = 10;
  kick.volume = 0x9000;

  AudioChannel &noise = channels[CH_NOISE];
  noise.waveforms = Waveform::NOISE;
  noise.attack_ms = 1;
  noise.sustain = 0;
  noise.release_ms = 5;
}

static void release_music_channels() {
  for(int c = CH_LEAD; c <= CH_NOISE; c++) channels[c].trigger_release();
}

static void play_melodic(AudioChannel &ch, uint8_t code) {
  if(code == STEP_HOLD) return;
  if(code == STEP_REST) {
    ch.trigger_release();
    return;
  }
  ch.frequency = midi_to_hz(code);
  ch.trigger_attack();
}

static void play_drum(uint8_t code) {
  if(code == DRUM_KICK) {
    kick_freq = 160;
    channels[CH_KICK].frequency = uint16_t(kick_freq);
    channels[CH_KICK].trigger_attack();
  } else if(code == DRUM_SNARE) {
    AudioChannel &n = channels[CH_NOISE];
    n.decay_ms = 110;
    n.volume = 0x3000;
    n.trigger_attack();
  } else if(code == DRUM_HAT) {
    AudioChannel &n = channels[CH_NOISE];
    n.decay_ms = 25;
    n.volume = 0x1400;
    n.trigger_attack();
  }
}

static void play_step() {
  const ParsedSong &s = songs[int(song)];
  auto at = [](const std::vector<uint8_t> &t, size_t i) { return i < t.size() ? t[i] : STEP_REST; };

  play_melodic(channels[CH_LEAD], at(s.lead, step_index));
  play_melodic(channels[CH_BASS], at(s.bass, step_index));
  play_drum(at(s.drums, step_index));
}

// ---------------------------------------------------------------------------
// Sound effects: short frequency sequences on a single channel.
// ---------------------------------------------------------------------------

struct SfxStep {
  uint16_t freq;
  uint16_t ms;
};

struct SfxData {
  uint8_t channel;
  uint8_t waveform;
  uint16_t volume;
  uint16_t pulse_width;
  uint16_t release_ms;
  uint8_t count;
  SfxStep steps[8];
};

static const SfxData sfx_table[] = {
  // MENU_MOVE
  {CH_SFX_UI, Waveform::SQUARE, 0x2800, 0x7fff, 20, 1, {{880, 30}}},
  // MENU_SELECT
  {CH_SFX_UI, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{660, 40}, {990, 70}}},
  // MENU_BACK
  {CH_SFX_UI, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{660, 40}, {440, 70}}},
  // EAT
  {CH_SFX_GAME, Waveform::SQUARE, 0x4000, 0x4000, 50, 4, {{523, 30}, {659, 30}, {784, 30}, {1047, 70}}},
  // GOLDEN
  {CH_SFX_GAME, Waveform::SQUARE, 0x4000, 0x2000, 80, 6, {{784, 35}, {988, 35}, {1175, 35}, {1568, 35}, {1976, 35}, {2093, 110}}},
  // GOLDEN_SPAWN
  {CH_SFX_GAME, Waveform::SINE, 0x3000, 0x7fff, 40, 4, {{1568, 35}, {2093, 35}, {1568, 35}, {2093, 60}}},
  // DIE
  {CH_SFX_GAME, Waveform::SAW, 0x4800, 0x7fff, 120, 7, {{440, 60}, {392, 60}, {330, 60}, {294, 60}, {247, 80}, {196, 100}, {147, 160}}},
  // COUNTDOWN
  {CH_SFX_UI, Waveform::SQUARE, 0x3000, 0x7fff, 40, 1, {{440, 90}}},
  // GO
  {CH_SFX_UI, Waveform::SQUARE, 0x3800, 0x7fff, 80, 1, {{880, 200}}},
  // PAUSE
  {CH_SFX_UI, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{523, 40}, {392, 70}}},
};

struct SfxPlayer {
  const SfxData *sfx = nullptr;
  uint8_t step = 0;
  int32_t timer = 0;
};

static SfxPlayer sfx_players[2];

static void update_sfx(uint32_t dt) {
  for(auto &pl : sfx_players) {
    if(!pl.sfx) continue;
    pl.timer -= dt;
    if(pl.timer > 0) continue;

    AudioChannel &ch = channels[pl.sfx->channel];
    pl.step++;
    if(pl.step >= pl.sfx->count) {
      ch.trigger_release();
      pl.sfx = nullptr;
      continue;
    }
    ch.frequency = pl.sfx->steps[pl.step].freq;
    pl.timer += pl.sfx->steps[pl.step].ms;
  }
}

// ---------------------------------------------------------------------------

void init() {
  parse_song(songs[int(Song::TITLE)], title_song);
  parse_song(songs[int(Song::GAME)], game_song);
  parse_song(songs[int(Song::GAME_OVER)], game_over_song);
  parse_song(songs[int(Song::FANFARE)], fanfare_song);
  setup_music_channels();
}

void update(uint32_t time) {
  uint32_t dt = last_time ? time - last_time : 0;
  last_time = time;
  if(dt > 100) dt = 100; // don't race to catch up after a stall

  update_sfx(dt);

  if(kick_freq > 45) {
    kick_freq *= 0.82f;
    channels[CH_KICK].frequency = uint16_t(kick_freq);
  }

  if(song == Song::NONE || song_paused || song_done) return;

  const ParsedSong &s = songs[int(song)];
  step_timer -= dt;
  while(step_timer <= 0) {
    step_index++;
    if(step_index >= s.length) {
      if(!s.data->loop) {
        song_done = true;
        release_music_channels();
        return;
      }
      step_index = 0;
    }
    if(music_on) play_step();
    step_timer += s.data->step_ms * tempo_scale;
  }
}

void play_song(Song new_song) {
  release_music_channels();
  song = new_song;
  song_paused = false;
  song_done = false;
  tempo_scale = 1.0f;
  if(song == Song::NONE) return;

  setup_music_channels();
  step_index = 0;
  step_timer = songs[int(song)].data->step_ms;
  if(music_on) play_step();
}

void stop_song() {
  play_song(Song::NONE);
}

void pause_song(bool paused) {
  if(paused && !song_paused) release_music_channels();
  song_paused = paused;
}

bool song_finished() {
  return song == Song::NONE || song_done;
}

Song current_song() {
  return song;
}

void set_tempo_scale(float scale) {
  tempo_scale = scale;
}

void play_sfx(Sfx id) {
  if(!sfx_on) return;
  const SfxData &s = sfx_table[int(id)];
  SfxPlayer &pl = sfx_players[s.channel == CH_SFX_GAME ? 0 : 1];

  // Don't let a menu blip or a turn tick cut off the death sound.
  if(pl.sfx == &sfx_table[int(Sfx::DIE)] && id != Sfx::DIE) return;

  AudioChannel &ch = channels[s.channel];
  ch.off();
  ch.waveforms = s.waveform;
  ch.volume = s.volume;
  ch.pulse_width = s.pulse_width;
  ch.attack_ms = 2;
  ch.decay_ms = 10;
  ch.sustain = 0xd000;
  ch.release_ms = s.release_ms;
  ch.frequency = s.steps[0].freq;
  ch.trigger_attack();

  pl.sfx = &s;
  pl.step = 0;
  pl.timer = s.steps[0].ms;
}

void set_music_enabled(bool enabled) {
  music_on = enabled;
  if(!enabled) release_music_channels();
}

void set_sfx_enabled(bool enabled) {
  sfx_on = enabled;
  if(!enabled) {
    for(auto &pl : sfx_players) pl.sfx = nullptr;
    channels[CH_SFX_GAME].trigger_release();
    channels[CH_SFX_UI].trigger_release();
  }
}

}
