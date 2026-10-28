#ifndef CS64_VISIBILITY_H
#define CS64_VISIBILITY_H
#include "game.h"
/* Conservative sphere/frustum test. Game coordinates are Z-up. */
int sphere_visible(const Player *camera,Vec3 center,float radius,int fullscreen);
/* Returned list retains the original texture/face order and exact BSP PVS. */
int world_candidates(int leaf,const uint16_t *order,int count,uint16_t *output);
#endif
