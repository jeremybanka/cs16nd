#ifndef CS64_GAME_H
#define CS64_GAME_H
#include "model.h"
typedef struct {
    float forward,side,turn,look;
    int jump,crouch,fire,reload,respawn;
} Controls;
typedef struct {
    Player player;
    int health,ammo,reserve,kills,shots,hits,spawn;
    float cooldown,reloading,shot_age,death_age,view_age;
    float gait_age,body_yaw,upper_age,transition;
    int gait,upper,view,death;
    BonePose last_pose[MODEL_MAX_BONES],from_pose[MODEL_MAX_BONES];
    int pose_valid;
    float hit_marker,hurt;
} Fighter;
typedef struct { Fighter fighters[2]; unsigned ticks; } Game;
void game_init(Game *g);
void game_respawn(Game *g,int id);
void game_tick(Game *g,const Controls controls[2],float dt);
void game_pose(Fighter *f,BonePose *pose);
Vec3 player_direction(const Player *p);
void game_smoke_controls(Game *g,Controls *controls,unsigned tick);
#endif
