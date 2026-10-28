#ifndef CS64_PERF_H
#define CS64_PERF_H
#include <stdint.h>
#include "game.h"
typedef enum {
    PF_INPUT,PF_SIM,PF_SOUND,PF_FENCE,PF_PLAYER_POSE,PF_HELD_POSE,PF_VIEW_POSE,
    PF_MATRICES,PF_SKIN,PF_WRITEBACK,PF_TRANSFORM,PF_DISPLAY,PF_SETUP,PF_PVS,
    PF_WORLD0,PF_WORLD1,PF_ACTORS,PF_WEAPON_SETUP,PF_WEAPONS,PF_HUD,PF_PRESENT,
    PF_DRAIN_SETUP,PF_DRAIN_WORLD0,PF_DRAIN_WORLD1,PF_DRAIN_ACTORS,PF_DRAIN_WEAPONS,PF_DRAIN_HUD,PF_COUNT
} PerfStage;
typedef enum {
    PC_STEPS,PC_WORLD_FACES,PC_WORLD_TRIANGLES,PC_WORLD_VERTICES,PC_WORLD_UPLOADS,
    PC_MODEL_TRIANGLES,PC_MODEL_VERTICES,PC_MODEL_UPLOADS,PC_SKIN_VERTICES,
    PC_PACK_VERTICES,PC_CACHE_HIT,PC_CACHE_MISS,PC_COUNT
} PerfCounter;
typedef enum { PD_SKIN_TRANSFORM,PD_SKIN_PACK,PD_ACTOR_SKIN,PD_HELD_SKIN,PD_VIEW_SKIN,PD_COUNT } PerfDetail;
typedef struct { const char *name; int views,fullscreen,actors,weapons,freeze,hud,audio; } PerfOptions;
#if defined(PROFILE) && defined(N64)
#include <libdragon.h>
void perf_init(void);
int perf_begin_frame(Game *game);
void perf_end_frame(void);
void perf_add(PerfStage stage,uint32_t ticks);
void perf_count(PerfCounter counter,unsigned amount);
void perf_detail(PerfDetail detail,uint32_t ticks);
void perf_audio(uint32_t ticks);
PerfOptions perf_options(void);
void perf_drain(PerfStage stage);
void perf_graphics_begin(surface_t *surface);
void perf_present(void);
#define PERF_START(name) uint32_t name=get_ticks()
#define PERF_END(stage,name) perf_add(stage,get_ticks()-(name))
#define PERF_COUNTER(counter,amount) perf_count(counter,amount)
#define PERF_DETAIL(detail,name) perf_detail(detail,get_ticks()-(name))
#else
static inline PerfOptions perf_options(void){return (PerfOptions){"normal",2,0,1,1,0,1,1};}
static inline void perf_drain(PerfStage stage){(void)stage;}
#define PERF_START(name) ((void)0)
#define PERF_END(stage,name) ((void)0)
#define PERF_COUNTER(counter,amount) ((void)0)
#define PERF_DETAIL(detail,name) ((void)0)
#endif
#endif
