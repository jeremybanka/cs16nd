#include "game.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#define PI 3.14159265358979323846f
static float clampf(float a,float lo,float hi){return fmaxf(lo,fminf(hi,a));}
static float angle(float a){while(a>PI)a-=2*PI;while(a<-PI)a+=2*PI;return a;}
Vec3 player_direction(const Player *p){return (Vec3){cosf(p->yaw)*cosf(p->pitch),sinf(p->yaw)*cosf(p->pitch),sinf(p->pitch)};}
void game_respawn(Game *g,int id){
    Fighter *f=&g->fighters[id];int kills=f->kills,shots=f->shots,hits=f->hits;
    memset(f,0,sizeof(*f));f->kills=kills;f->shots=shots;f->hits=hits;
    f->spawn=id?31:25;
    const Fighter *other=&g->fighters[1-id];
    for(int attempt=0;attempt<20;attempt++){
        int index=20+(f->spawn-20+attempt)%20;
        Vec3 delta=sub(spawns[index].pos,other->player.pos);
        if(other->health<=0 || fabsf(delta.x)>=40 || fabsf(delta.y)>=40 || fabsf(delta.z)>=80){f->spawn=index;break;}
    }
    player_spawn(&f->player,f->spawn);f->player.yaw=id?PI:0;
    f->health=100;f->ammo=20;f->reserve=120;f->shot_age=100;
    f->view=8;f->cooldown=model_duration(&glock_view_model,8);f->gait=1;f->upper=19;f->body_yaw=f->player.yaw;
}
void game_init(Game *g){memset(g,0,sizeof(*g));for(int i=0;i<2;i++)game_respawn(g,i);}
/* A basic player hitbox, occluded by the map's unexpanded point BSP. */
static float hitbox(Vec3 start,Vec3 delta,const Player *p){
    float half=p->crouched?18:36,near=0,far=1;
    float origin[3]={start.x,start.y,start.z},dir[3]={delta.x,delta.y,delta.z};
    float center[3]={p->pos.x,p->pos.y,p->pos.z},extent[3]={16,16,half};
    for(int a=0;a<3;a++){
        if(fabsf(dir[a])<.00001f){if(fabsf(origin[a]-center[a])>extent[a])return 1;continue;}
        float lo=(center[a]-extent[a]-origin[a])/dir[a],hi=(center[a]+extent[a]-origin[a])/dir[a];
        if(lo>hi){float tmp=lo;lo=hi;hi=tmp;}
        near=fmaxf(near,lo);far=fminf(far,hi);if(near>far)return 1;
    }
    return near;
}
static void shoot(Game *g,int id){
    Fighter *f=&g->fighters[id],*other=&g->fighters[1-id];
    if(f->cooldown>0 || f->reloading>0 || !f->ammo)return;
    f->ammo--;f->shots++;f->cooldown=.15f;f->shot_age=0;f->view_age=0;f->view=f->ammo?3:6;
    Vec3 eye=player_eye(&f->player),delta=mul(player_direction(&f->player),8192);
    Trace wall=trace_bullet(eye,add(eye,delta));
    if(other->health>0 && hitbox(eye,delta,&other->player)<wall.fraction){
        f->hits++;f->hit_marker=.18f;other->hurt=.25f;other->health-=25;
        if(other->health<=0){
            other->health=0;other->death_age=0;other->death=other->player.crouched?110:101+(g->ticks%3);
            other->reloading=0;other->player.vel=(Vec3){0};f->kills++;
        }
    }
}
static void animate(Fighter *f,float dt){
    Player *p=&f->player;float speed=hypotf(p->vel.x,p->vel.y),sign=1;
    if(speed>5){
        f->body_yaw=atan2f(p->vel.y,p->vel.x);
        if(fabsf(angle(p->yaw-f->body_yaw))>PI*2/3){f->body_yaw=angle(f->body_yaw+PI);sign=-1;}
    }else f->body_yaw=angle(f->body_yaw+angle(p->yaw-f->body_yaw)*fminf(1,dt*5));
    int gait=!p->grounded?6:p->crouched?(speed>5?5:2):speed>150?4:speed>5?3:1;
    if(gait!=f->gait){f->gait=gait;f->gait_age=0;}
    const ModelSequence *seq=&terror_model.sequence[gait];
    float rate=seq->distance>0?speed*model_duration(&terror_model,gait)/seq->distance:1;
    f->gait_age+=dt*clampf(rate,.1f,3)*sign;
    int upper=f->health<=0?f->death:f->reloading>0?(p->crouched?18:21):f->shot_age<.4f?(p->crouched?17:20):(p->crouched?16:19);
    if(upper!=f->upper){
        f->upper=upper;f->upper_age=0;f->transition=0;
        if(f->pose_valid)memcpy(f->from_pose,f->last_pose,sizeof(f->from_pose));
    }else f->upper_age+=dt;
    f->transition=fminf(1,f->transition+dt*8);
}
void game_tick(Game *g,const Controls controls[2],float dt){
    for(int i=0;i<2;i++){
        Fighter *f=&g->fighters[i];const Controls *c=&controls[i];
        f->cooldown=fmaxf(0,f->cooldown-dt);f->shot_age+=dt;f->view_age+=dt;
        f->hit_marker=fmaxf(0,f->hit_marker-dt);f->hurt=fmaxf(0,f->hurt-dt);
        if(f->health<=0){
            f->death_age+=dt;player_move(&f->player,0,0,0,dt);
            if(f->death_age>4 || (c->respawn && f->death_age>1))game_respawn(g,i);
            continue;
        }
        if(c->respawn){game_respawn(g,i);continue;}
        f->player.yaw=angle(f->player.yaw-c->turn*2.2f*dt);
        f->player.pitch=clampf(f->player.pitch+c->look*1.5f*dt,-1.35f,1.35f);
        player_crouch(&f->player,c->crouch);
        Player before=f->player;player_move(&f->player,c->forward,c->side,c->jump,dt);
        Fighter *other=&g->fighters[1-i];Vec3 difference=sub(f->player.pos,other->player.pos);
        if(other->health>0 && fabsf(difference.x)<32 && fabsf(difference.y)<32 && fabsf(difference.z)<(f->player.crouched?18:36)+(other->player.crouched?18:36)){
            f->player.pos.x=before.pos.x;f->player.pos.y=before.pos.y;f->player.vel.x=f->player.vel.y=0;
        }
        if(f->reloading>0){
            f->reloading=fmaxf(0,f->reloading-dt);
            if(f->reloading==0){int amount=20-f->ammo;if(amount>f->reserve)amount=f->reserve;f->ammo+=amount;f->reserve-=amount;}
        }
        if(c->reload && f->ammo<20 && f->reserve>0 && f->reloading==0 && f->cooldown==0){f->reloading=2.2f;f->view=7;f->view_age=0;}
        if(c->fire)shoot(g,i);
        if(f->view_age>model_duration(&glock_view_model,f->view) && f->reloading==0){f->view=0;f->view_age=0;}
    }
    for(int i=0;i<2;i++)animate(&g->fighters[i],dt);
    g->ticks++;
}
void game_pose(Fighter *f,BonePose *pose){
    float yaw=.5f-angle(f->player.yaw-f->body_yaw)/PI;
    float pitch=.5f+f->player.pitch/PI;
    float time=f->health<=0?f->death_age:f->reloading>0?(2.2f-f->reloading)*(2.0f/2.2f):f->shot_age<.4f?f->shot_age:f->upper_age;
    model_pose(&terror_model,f->upper,time,yaw,pitch,pose);
    if(f->health>0){
        BonePose gait[MODEL_MAX_BONES];model_pose(&terror_model,f->gait,f->gait_age,.5f,.5f,gait);
        for(int b=0;b<terror_model.bones;b++)if(terror_model.bone[b].gait)pose[b]=gait[b];
    }
    if(f->pose_valid && f->transition<1)for(int b=0;b<terror_model.bones;b++){
        BonePose p=f->from_pose[b];model_mix(&p,&pose[b],f->transition);pose[b]=p;
    }
    memcpy(f->last_pose,pose,sizeof(f->last_pose));f->pose_valid=1;
}
/* Deterministic acceptance fixture. Normal cartridges never call this. */
void game_smoke_controls(Game *g,Controls *c,unsigned tick){
    memset(c,0,2*sizeof(*c));unsigned t=tick%1200;
    if(t==0){for(int i=0;i<2;i++)game_respawn(g,i);}
    if(t>=120 && t<210){c[0].side=.65f;c[1].side=-.45f;}
    if(t>=210 && t<300){c[0].side=-.65f;c[1].side=.45f;}
    if(t==310)c[0].jump=1;
    if(t>=360 && t<420)c[1].crouch=1;
    if(t==450){for(int i=0;i<2;i++)game_respawn(g,i);}
    /* Draw, four shots, visible death, reload, automatic respawn. */
    if(t==540 || t==570 || t==600 || t==630)c[0].fire=1;
    if(t==650)c[0].reload=1;
    if(t>=930 && t<1050){c[0].forward=.6f;c[1].side=.7f;c[1].turn=.25f;}
}
