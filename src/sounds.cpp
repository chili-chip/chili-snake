#include "sounds.hpp"

#include "32blit.hpp"

using namespace blit;
using chili::audio::Sfx;
using chili::audio::Song;
using chili::audio::SFX1;
using chili::audio::SFX2;

namespace snek::sounds {

// ---------------------------------------------------------------------------
// Soundtrack
//
// Original chiptune written for snek (same licence as the rest of the repo).
// The note format is the chili game SDK's: C4, F#5 start a note, - holds,
// . rests; K, S and H are kick, snare and hi-hat.
// ---------------------------------------------------------------------------

// Title theme: A minor, laid back.
const Song title = {
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
const Song game = {
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

const Song game_over = {
  130, false,
  "E5 - D5 - C5 - B4 - A4 - - - - - - - ",
  "C3 - - - B2 - - - A2 - - - - - - - ",
  "K . . . . . . . K . . . . . . . ",
};

const Song fanfare = {
  85, false,
  "C5 E5 G5 C6 - G5 C6 - E6 - - - - - - - ",
  "C3 - - - - - - - G2 - - - C3 - - - ",
  "K . . . S . . . K . S . K . . . ",
};

// ---------------------------------------------------------------------------
// Sound effects: game sounds on SFX1, menu sounds on SFX2. The death sound has
// priority 1 so a menu blip or an apple cannot cut it off.
// ---------------------------------------------------------------------------

const Sfx menu_move = {SFX2, Waveform::SQUARE, 0x2800, 0x7fff, 20, 1, {{880, 30}}};
const Sfx menu_select = {SFX2, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{660, 40}, {990, 70}}};
const Sfx menu_back = {SFX2, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{660, 40}, {440, 70}}};
const Sfx eat = {SFX1, Waveform::SQUARE, 0x4000, 0x4000, 50, 4, {{523, 30}, {659, 30}, {784, 30}, {1047, 70}}};
const Sfx golden = {SFX1, Waveform::SQUARE, 0x4000, 0x2000, 80, 6, {{784, 35}, {988, 35}, {1175, 35}, {1568, 35}, {1976, 35}, {2093, 110}}};
const Sfx golden_spawn = {SFX1, Waveform::SINE, 0x3000, 0x7fff, 40, 4, {{1568, 35}, {2093, 35}, {1568, 35}, {2093, 60}}};
const Sfx die = {SFX1, Waveform::SAW, 0x4800, 0x7fff, 120, 7, {{440, 60}, {392, 60}, {330, 60}, {294, 60}, {247, 80}, {196, 100}, {147, 160}}, 1};
const Sfx countdown = {SFX2, Waveform::SQUARE, 0x3000, 0x7fff, 40, 1, {{440, 90}}};
const Sfx go = {SFX2, Waveform::SQUARE, 0x3800, 0x7fff, 80, 1, {{880, 200}}};
const Sfx pause = {SFX2, Waveform::SQUARE, 0x3000, 0x7fff, 40, 2, {{523, 40}, {392, 70}}};

}
