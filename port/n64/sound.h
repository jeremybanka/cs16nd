#ifndef CS64_SOUND_H
#define CS64_SOUND_H
#include "game.h"
typedef struct { const int8_t *samples; int count; } Sound;
extern const Sound gun_sounds[4];
void sound_init(void);
void sound_update(const Game *game);
#endif
