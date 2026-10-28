#include "perf.h"
#ifdef PROFILE
#include <string.h>
#include <stdlib.h>
#include <math.h>
enum { WARMUP=36, SAMPLES=120, TOTAL=WARMUP+SAMPLES };
typedef struct {
    uint32_t start,end,graphics,submitted,ready,stage[PF_COUNT],counter[PC_COUNT];
    uint32_t detail[PD_COUNT],audio_start,audio,audio_calls_start,audio_calls,vi_start,vi,flips_start,flips;
    surface_t *surface;
    volatile int complete;
} Sample;
static Sample samples[TOTAL],*current;
static int frame,scenario=-1;
static volatile uint32_t audio_ticks,audio_calls,vi_count,flip_count,last_origin;
static const char *detail_names[PD_COUNT]={"skin_transform","skin_pack","actor_skin","held_skin","viewmodel_skin"};
static const char *stage_names[PF_COUNT]={
    "input","simulation","sound_events","rsp_buffer_wait","player_pose","held_pose","view_pose",
    "bone_matrices","skinning","vertex_writeback","actor_transform","display_wait","frame_setup","pvs",
    "world0_cull_submit","world1_cull_submit","actor_submit","weapon_setup","weapon_submit","hud","present_submit",
    "serial_setup_drain","serial_world0_drain","serial_world1_drain","serial_actor_drain","serial_weapon_drain","serial_hud_drain"
};
static const char *counter_names[PC_COUNT]={
    "simulation_steps","world_faces","world_triangles","world_vertices","world_texture_uploads",
    "model_triangles","model_vertices","model_texture_uploads","skinned_vertices","packed_vertices","pose_cache_hits","pose_cache_misses","world_candidates","face_cache_hits","face_cache_misses","actors_culled","actors_lod1","actors_lod2"
};
static const PerfOptions scenarios[]={
    {"two_full",2,0,1,1,0,1,1},
    {"two_world",2,0,0,0,0,1,1},
    {"one_world_split",1,0,0,0,0,1,1},
    {"one_world_fullscreen",1,1,0,0,0,1,1},
    {"two_frozen",2,0,1,1,1,1,1},
    {"two_no_viewmodels",2,0,1,0,0,1,1},
    {"two_no_actors",2,0,0,1,0,1,1},
    {"two_silent",2,0,1,1,0,1,0},
    {"two_no_hud",2,0,1,1,0,0,1},
    {"two_full_repeat",2,0,1,1,0,1,1},
    {"two_mid_lod",2,0,1,1,0,1,1},
    {"two_far_lod",2,0,1,1,0,1,1},
    {"two_away",2,0,1,1,0,1,1}
};
static void vi_tick(void){
    /* Read-only VI_ORIGIN, same register as libdragon src/vi.h. NTSC noninterlaced.
       Observation may trail display's VI handler by one interrupt. */
    uint32_t origin=*(volatile uint32_t*)0xA4400004;
    if(last_origin && origin!=last_origin)flip_count++;
    last_origin=origin;vi_count++;
}
void perf_init(void){
    register_VI_handler(vi_tick);
    debugf("PERF_META {\"version\":1,\"warmup\":%d,\"samples\":%d,\"serial\":%d,\"benchmark\":%d,\"timer\":\"emulated CPU Count\"}\n",WARMUP,SAMPLES,
#ifdef PROFILE_SERIAL
        1,
#else
        0,
#endif
#ifdef BENCHMARK
        1
#else
        0
#endif
    );
}
PerfOptions perf_options(void){return scenarios[scenario<0?0:scenario];}
void perf_add(PerfStage stage,uint32_t ticks){if(current)current->stage[stage]+=ticks;}
void perf_count(PerfCounter counter,unsigned amount){if(current)current->counter[counter]+=amount;}
void perf_detail(PerfDetail detail,uint32_t ticks){if(current)current->detail[detail]+=ticks;}
void perf_audio(uint32_t ticks){audio_ticks+=ticks;audio_calls++;}
void perf_drain(PerfStage stage){
#ifdef PROFILE_SERIAL
    PERF_START(t);rspq_wait();PERF_END(stage,t);
#else
    (void)stage;
#endif
}
void perf_graphics_begin(surface_t *surface){current->surface=surface;current->graphics=get_ticks();}
static void completed(void *arg){
    Sample *sample=arg;sample->ready=get_ticks();sample->complete=1;
    display_show(sample->surface);
}
void perf_present(void){current->submitted=get_ticks();rdpq_detach_cb(completed,current);}
static int compare_u32(const void *a,const void *b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return (x>y)-(x<y);}
static double milliseconds(uint64_t ticks){return (double)ticks*(1000.0/TICKS_PER_SECOND);}
static void report(void){
    uint64_t total=0,stages[PF_COUNT]={0},counts[PC_COUNT]={0},details[PD_COUNT]={0},audio=0,calls=0,vi=0,flips=0,latency=0,tail=0;
    uint32_t durations[SAMPLES];
    for(int i=0;i<SAMPLES;i++){
        const Sample *s=&samples[WARMUP+i];assertf(s->complete,"Profiling frame not completed by RDP");
        durations[i]=s->end-s->start;total+=durations[i];audio+=s->audio;calls+=s->audio_calls;vi+=s->vi;flips+=s->flips;
        latency+=(uint32_t)(s->ready-s->graphics);tail+=(uint32_t)(s->ready-s->submitted);
        for(int j=0;j<PF_COUNT;j++)stages[j]+=s->stage[j];
        for(int j=0;j<PC_COUNT;j++)counts[j]+=s->counter[j];
        for(int j=0;j<PD_COUNT;j++)details[j]+=s->detail[j];
    }
    qsort(durations,SAMPLES,sizeof(*durations),compare_u32);
    uint32_t elapsed=samples[TOTAL-1].end-samples[WARMUP].start;
    double seconds=milliseconds(elapsed)/1000;uint64_t accounted=0;
    for(int j=0;j<PF_COUNT;j++)accounted+=stages[j];
    assertf(accounted<=total,"Overlapping CPU profiler scopes");
    debugf("PERF {\"scenario\":\"%s\",\"frames\":%d,\"elapsed_ms\":%.4f,\"fps\":%.4f,\"frame_ms\":%.4f,\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"max_ms\":%.4f,",perf_options().name,SAMPLES,milliseconds(elapsed),SAMPLES/seconds,milliseconds(total)/SAMPLES,milliseconds(durations[SAMPLES/2]),milliseconds(durations[(SAMPLES*95/100)-1]),milliseconds(durations[SAMPLES-1]));
    debugf("\"rdp_frame_latency_ms\":%.4f,\"rdp_queue_tail_ms\":%.4f,\"vi_hz\":%.4f,\"vi_buffer_changes_hz\":%.4f,\"audio_isr_ms\":%.4f,\"cpu_ms\":{",milliseconds(latency)/SAMPLES,milliseconds(tail)/SAMPLES,vi/seconds,flips/seconds,milliseconds(audio)/SAMPLES);
    for(int j=0;j<PF_COUNT;j++)debugf("\"%s\":%.4f,",stage_names[j],milliseconds(stages[j])/SAMPLES);
    debugf("\"other\":%.4f},\"counts\":{",milliseconds(total-accounted)/SAMPLES);
    for(int j=0;j<PC_COUNT;j++)debugf("%s\"%s\":%.4f",j?",":"",counter_names[j],(double)counts[j]/SAMPLES);
    debugf("},\"detail_ms\":{");
    for(int j=0;j<PD_COUNT;j++)debugf("%s\"%s\":%.4f",j?",":"",detail_names[j],milliseconds(details[j])/SAMPLES);
    heap_stats_t heap;sys_get_heap_stats(&heap);
    debugf("},\"audio_callbacks\":%lu,\"heap_free_bytes\":%d}\n",(unsigned long)calls,heap.total-heap.used);
}
int perf_begin_frame(Game *game){
    int reset=0;
    if(scenario<0 || frame==TOTAL){
        /* Batch boundary only: drain before reusing callback storage and logging.
           No per-frame drain in the default profiler. Excluded from timings. */
        current=NULL;rspq_wait();
        if(scenario>=0)report();
#ifdef BENCHMARK
        scenario++;
        if(scenario==(int)(sizeof(scenarios)/sizeof(scenarios[0]))){
            debugf("PERF_DONE\n");scenario=0;
        }
        game_init(game);Controls neutral[2]={0};
        if(scenario>=10){
            int a=-1,b=-1;
            for(int i=0;i<spawn_count && a<0;i++)for(int j=i+1;j<spawn_count;j++){
                Vec3 d=sub(spawns[i].pos,spawns[j].pos);float distance=sqrtf(dot(d,d));
                if((scenario==10 && distance>570 && distance<1000) || (scenario>10 && distance>1200)){a=i;b=j;break;}
            }
            assertf(a>=0,"Missing model LOD benchmark spawn pair");
            player_spawn(&game->fighters[0].player,a);player_spawn(&game->fighters[1].player,b);
            for(int i=0;i<2;i++){
                Player *p=&game->fighters[i].player;Vec3 d=sub(game->fighters[1-i].player.pos,player_eye(p));
                p->yaw=atan2f(d.y,d.x)+(scenario==12?3.14159265358979323846f:0);
                p->pitch=atan2f(d.z,hypotf(d.x,d.y));
                game->fighters[i].body_yaw=p->yaw;
            }
        }
        for(int i=0;i<120;i++)game_tick(game,neutral,1.0f/60);
        audio_pause(!perf_options().audio);
#else
        (void)game;scenario=0;
#endif
        frame=0;memset(samples,0,sizeof(samples));reset=1;
        debugf("PERF_BEGIN %s\n",perf_options().name);
    }
    current=&samples[frame];current->start=get_ticks();
    current->audio_start=audio_ticks;current->audio_calls_start=audio_calls;current->vi_start=vi_count;current->flips_start=flip_count;
    return reset;
}
void perf_end_frame(void){
    current->end=get_ticks();current->audio=audio_ticks-current->audio_start;
    current->audio_calls=audio_calls-current->audio_calls_start;
    current->vi=vi_count-current->vi_start;current->flips=flip_count-current->flips_start;
    frame++;current=NULL;
}
#endif
