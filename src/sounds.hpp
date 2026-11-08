#pragma once

#include "chili/audio.hpp"

// Snek's soundtrack and sound effects. The chili game SDK plays them:
// music on channels 0-3, game sounds on SFX1, menu sounds on SFX2.
namespace snek::sounds {

extern const chili::audio::Song title;
extern const chili::audio::Song game;
extern const chili::audio::Song game_over;
extern const chili::audio::Song fanfare;

extern const chili::audio::Sfx menu_move;
extern const chili::audio::Sfx menu_select;
extern const chili::audio::Sfx menu_back;
extern const chili::audio::Sfx eat;
extern const chili::audio::Sfx golden;
extern const chili::audio::Sfx golden_spawn;
extern const chili::audio::Sfx die;
extern const chili::audio::Sfx countdown;
extern const chili::audio::Sfx go;
extern const chili::audio::Sfx pause;

}
