#include <libdragon.h>
#include <math.h>
#include <stdlib.h>
#include "game.h"
#include "render.h"
#include "sound.h"
#include "perf.h"
static float axis(int value){return abs(value)<8?0:fmaxf(-1,fminf(1,value/80.0f));}
static Controls read_controls(int id,int *connected){
    joypad_port_t port=(joypad_port_t)id;*connected=joypad_is_connected(port);
    joypad_inputs_t in=joypad_get_inputs(port);joypad_buttons_t press=joypad_get_buttons_pressed(port);
    return (Controls){.forward=axis(in.stick_y)+(in.btn.d_up-in.btn.d_down),
        .side=(in.btn.c_right-in.btn.c_left)+(in.btn.d_right-in.btn.d_left),
        .turn=axis(in.stick_x),.look=in.btn.c_up-in.btn.c_down,
        .jump=press.a,.crouch=in.btn.l,.fire=press.z,.reload=press.b,.respawn=press.start};
}
#if defined(SMOKE_TEST) && defined(VALIDATE)
static void validate_model_views(void){
    static Game qa;Controls neutral[2]={0};int connected[2]={1,1};
    for(int mode=0;mode<3;mode++){
        game_init(&qa);int a=-1,b=-1;
        for(int i=0;i<spawn_count && a<0;i++)for(int j=i+1;j<spawn_count;j++){
            Vec3 d=sub(spawns[i].pos,spawns[j].pos);float distance=sqrtf(dot(d,d));
            if((mode==0 && distance>570 && distance<1000) || (mode>0 && distance>1200)){a=i;b=j;break;}
        }
        assertf(a>=0,"Missing LOD validation spawn pair");
        player_spawn(&qa.fighters[0].player,a);player_spawn(&qa.fighters[1].player,b);
        for(int i=0;i<2;i++){
            Player *p=&qa.fighters[i].player;Vec3 d=sub(qa.fighters[1-i].player.pos,player_eye(p));
            p->yaw=atan2f(d.y,d.x)+(mode==2?3.14159265358979323846f:0);
            p->pitch=atan2f(d.z,hypotf(d.x,d.y));qa.fighters[i].body_yaw=p->yaw;
        }
        for(int tick=0;tick<120;tick++)game_tick(&qa,neutral,1.0f/60);
        for(int frame=0;frame<12;frame++){game_tick(&qa,neutral,1.0f/60);render_frame(&qa,0,connected);}
        rspq_wait();debugf("LOD/CULL VALIDATION %d PASS\n",mode);
    }
}
#endif
int main(void){
    debug_init_isviewer();debug_init_usblog();
    if(get_memory_size()<8*1024*1024){
        console_init();console_set_render_mode(RENDER_AUTOMATIC);
        printf("CS64 / TWO PLAYERS\n\nEnable the 8 MiB Expansion Pak\nin ares, then reload this ROM.\n");
        for(;;){}
    }
    joypad_init();render_init();sound_init();static Game game;game_init(&game);
#ifdef PROFILE
    perf_init();
#endif
    uint32_t last=get_ticks(),timer=last;float accumulator=0,fps=0;int frames=0,connected[2]={0};Controls controls[2]={0};
    debugf("CS64 2P ready: Terrorist + Glock, %lu bytes RAM\n",(unsigned long)get_memory_size());
    for(;;){
#ifdef PROFILE
        if(perf_begin_frame(&game)){last=get_ticks();accumulator=0;}
#endif
        PERF_START(input_time);
        joypad_poll();
        for(int i=0;i<2;i++){
            Controls c=read_controls(i,&connected[i]);
            c.jump|=controls[i].jump;c.fire|=controls[i].fire;c.reload|=controls[i].reload;c.respawn|=controls[i].respawn;controls[i]=c;
        }
        PERF_END(PF_INPUT,input_time);
        uint32_t now=get_ticks();float elapsed=fminf(TICKS_TO_US(now-last)*.000001f,.1f);last=now;accumulator+=elapsed;
        while(accumulator>=1.0f/60){
#ifdef BENCHMARK
            controls[0]=controls[1]=(Controls){0};
#endif
#ifdef SMOKE_TEST
            game_smoke_controls(&game,controls,game.ticks);connected[0]=connected[1]=1;
#endif
            PERF_START(sim_time);game_tick(&game,controls,1.0f/60);PERF_END(PF_SIM,sim_time);
            PERF_COUNTER(PC_STEPS,1);
            PERF_START(sound_time);sound_update(&game);PERF_END(PF_SOUND,sound_time);
            for(int i=0;i<2;i++){controls[i].jump=controls[i].fire=controls[i].reload=controls[i].respawn=0;}
            accumulator-=1.0f/60;
#ifdef SMOKE_TEST
            if(game.ticks%120==0){
                for(int i=0;i<2;i++)assertf(!trace_player(game.fighters[i].player.pos,game.fighters[i].player.pos,game.fighters[i].player.crouched).startsolid,"Player %d entered solid",i+1);
                debugf("2P TEST tick %u: HP %d/%d shots %d hits %d kills %d ammo %d/%d\n",game.ticks,game.fighters[0].health,game.fighters[1].health,game.fighters[0].shots,game.fighters[0].hits,game.fighters[0].kills,game.fighters[0].ammo,game.fighters[0].reserve);
            }
            if(game.ticks==1200){
                assertf(game.fighters[0].hits==4 && game.fighters[0].kills==1 && game.fighters[0].reserve==116 && game.fighters[1].health==100,"2P weapon acceptance failed");
#ifdef VALIDATE
                validate_model_views();
#endif
                debugf("2P SMOKE PASS: independent movement, jump/crouch, 4 hits, kill/death, reload, respawn\n");
            }
#endif
        }
        render_frame(&game,fps,connected);
#ifdef PROFILE
        perf_end_frame();
        (void)timer;(void)frames;
#else
        if(++frames==60){
            uint32_t tick=get_ticks();fps=60000000.0f/TICKS_TO_US(tick-timer);timer=tick;frames=0;
            heap_stats_t heap;sys_get_heap_stats(&heap);
            debugf("2P FPS %.1f, tick %u, heap %d/%d bytes\n",fps,game.ticks,heap.used,heap.total);
        }
#endif
    }
}
