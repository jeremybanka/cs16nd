#include "world.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void){
    float total=0;int blocked=0;
    for(int i=0;i<spawn_count;i++){
        Player p;player_spawn(&p,i);
        assert(!trace_world(p.pos,p.pos).startsolid);
        float initial=p.pos.z;
        for(int j=0;j<120;j++)player_move(&p,0,0,0,1.0f/60);
        assert(p.grounded);assert(p.pos.z<=initial+1);assert(p.pos.z>initial-200);
        float floor=p.pos.z;player_move(&p,0,0,1,1.0f/60);
        for(int j=0;j<20;j++)player_move(&p,0,0,0,1.0f/60);
        assert(p.pos.z>floor+10);
        for(int j=0;j<100;j++)player_move(&p,0,0,0,1.0f/60);
        assert(p.grounded);assert(fabsf(p.pos.z-floor)<0.2f);
        player_crouch(&p,1);assert(p.crouched);assert(fabsf(p.pos.z-(floor-18))<1.0f);
        for(int j=0;j<30;j++)player_move(&p,0,0,0,1.0f/60);
        assert(p.grounded && !trace_player(p.pos,p.pos,1).startsolid);
        player_crouch(&p,0);assert(!p.crouched);assert(fabsf(p.pos.z-floor)<1.0f);
        Vec3 start=p.pos;
        Trace wall=trace_world(p.pos,add(p.pos,(Vec3){8000,0,0}));
        assert(wall.fraction<1);assert(!trace_world(wall.end,wall.end).startsolid);
        for(int j=0;j<600;j++){
            player_move(&p,1,0,0,1.0f/60);
            assert(isfinite(p.pos.x) && isfinite(p.pos.y) && isfinite(p.pos.z));
            assert(!trace_world(p.pos,p.pos).startsolid);
            assert(p.pos.z>-400 && p.pos.z<1000);
        }
        float dist=hypotf(p.pos.x-start.x,p.pos.y-start.y);total+=dist;
        if(dist<2400)blocked++;
        printf("spawn %02d: floor %.2f, moved %.1f, leaf %d\n",i,floor,dist,world_leaf(add(p.pos,(Vec3){0,0,28})));
    }
    assert(total>1000);assert(blocked>0);
    printf("PASS: %d spawns, gravity/jump/landing, 24000 movement ticks, wall sweeps\n",spawn_count);
}
