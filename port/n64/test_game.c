#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static void check_model(const Model *m){
    for(int i=0;i<m->batches;i++){
        const ModelBatch *b=&m->batch[i];assert(b->count<=64 && b->first%2==0 && b->first+b->count<=m->corners);
        for(int t=0;t<b->triangles;t++)for(int v=0;v<3;v++)assert(m->triangle[b->triangle+t].v[v]<b->count);
    }
    BonePose pose[MODEL_MAX_BONES];BoneMatrix bones[MODEL_MAX_BONES];
    VertexPair *vertices=malloc(m->corners/2*sizeof(VertexPair));assert(vertices);
    int samples=0;
    for(int s=0;s<m->sequences;s++)for(int frame=0;frame<m->sequence[s].frames;frame++){
        model_pose(m,s,frame/m->sequence[s].fps,.5f,.5f,pose);
        model_matrices(m,pose,bones,NULL);model_skin(m,bones,vertices);
        for(int b=0;b<m->bones;b++){
            float len=0;for(int j=0;j<4;j++){assert(isfinite(pose[b].q[j]));len+=pose[b].q[j]*pose[b].q[j];}
            assert(fabsf(len-1)<.001f);
            for(int j=0;j<3;j++)for(int k=0;k<4;k++)assert(isfinite(bones[b].m[j][k]));
        }
        samples++;
    }
    /* Every source aim blend, including adjacent-cell interpolation. */
    for(int s=0;s<m->sequences;s++)if(m->sequence[s].blends==9)
        for(int y=0;y<=4;y++)for(int x=0;x<=4;x++)model_pose(m,s,.017f,x*.25f,y*.25f,pose);
    model_pose(m,0,0,.5f,.5f,pose);model_matrices(m,pose,bones,NULL);model_skin(m,bones,vertices);
    printf("PASS: %d clips / %d frames, %d bones, animation decoding and skinning\n",m->sequences,samples,m->bones);
    free(vertices);
}
int main(void){
    check_model(&terror_model);check_model(&glock_view_model);check_model(&glock_world_model);
    Game g;game_init(&g);Controls c[2];int saw_crouch=0,saw_jump=0,saw_death=0,saw_reload=0;
    BonePose pose[MODEL_MAX_BONES];
    for(unsigned tick=0;tick<1200;tick++){
        game_smoke_controls(&g,c,tick);game_tick(&g,c,1.0f/60);
        for(int i=0;i<2;i++){
            Fighter *f=&g.fighters[i];game_pose(f,pose);
            assert(!trace_player(f->player.pos,f->player.pos,f->player.crouched).startsolid);
            saw_crouch|=f->player.crouched;saw_jump|=!f->player.grounded && f->player.vel.z>100;
            saw_death|=f->health==0;saw_reload|=f->reloading>0;
        }
    }
    printf("weapon: shots %d hits %d kills %d ammo %d reserve %d target %d\n",g.fighters[0].shots,g.fighters[0].hits,g.fighters[0].kills,g.fighters[0].ammo,g.fighters[0].reserve,g.fighters[1].health);
    assert(saw_crouch && saw_jump && saw_death && saw_reload);
    assert(g.fighters[0].shots==4 && g.fighters[0].hits==4 && g.fighters[0].kills==1);
    assert(g.fighters[0].ammo==20 && g.fighters[0].reserve==116 && g.fighters[1].health==100);
    /* Wall occlusion: place the other player across the map, aim precisely. */
    game_init(&g);player_spawn(&g.fighters[1].player,0);
    Controls neutral[2]={0};for(int i=0;i<120;i++)game_tick(&g,neutral,1.0f/60);
    Vec3 a=player_eye(&g.fighters[0].player),b=g.fighters[1].player.pos,d=sub(b,a);
    assert(trace_bullet(a,b).fraction<1);
    g.fighters[0].player.yaw=atan2f(d.y,d.x);g.fighters[0].player.pitch=atan2f(d.z,hypotf(d.x,d.y));
    neutral[0].fire=1;game_tick(&g,neutral,1.0f/60);assert(g.fighters[0].ammo==19 && g.fighters[1].health==100);
    /* No firing during reload, no ammunition duplication. */
    neutral[0].fire=0;for(int i=0;i<15;i++)game_tick(&g,neutral,1.0f/60);
    neutral[0].reload=1;game_tick(&g,neutral,1.0f/60);neutral[0].reload=0;neutral[0].fire=1;
    game_tick(&g,neutral,1.0f/60);assert(g.fighters[0].ammo==19);neutral[0].fire=0;
    for(int i=0;i<140;i++)game_tick(&g,neutral,1.0f/60);
    assert(g.fighters[0].ammo==20 && g.fighters[0].reserve==119);
    game_init(&g);g.fighters[1].player.pos=g.fighters[0].player.pos;game_respawn(&g,0);
    Vec3 separation=sub(g.fighters[0].player.pos,g.fighters[1].player.pos);
    assert(fabsf(separation.x)>=40 || fabsf(separation.y)>=40);
    puts("PASS: two-player movement, crouch, jump, hits, wall occlusion, reload, death and respawn");
}
