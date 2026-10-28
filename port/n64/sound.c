#include <libdragon.h>
#include "sound.h"
#include "perf.h"
#include <string.h>
typedef struct { const Sound *sound; int cursor,player; } Voice;
static Voice voices[4];
static int next_voice,last_shots[2];static float last_reload[2];
static void fill(short *buffer,size_t frames){
    PERF_START(audio_time);
    int active=0;for(int v=0;v<4;v++)active|=voices[v].sound!=NULL;
    if(!active)memset(buffer,0,frames*2*sizeof(*buffer));
    else
    for(size_t i=0;i<frames;i++){
        int left=0,right=0;
        for(int v=0;v<4;v++){
            Voice *voice=&voices[v];
            if(!voice->sound)continue;
            int sample=voice->sound->samples[voice->cursor++];
            left+=sample*(voice->player?36:60);right+=sample*(voice->player?60:36);
            if(voice->cursor>=voice->sound->count)voice->sound=NULL;
        }
        buffer[i*2]=left;buffer[i*2+1]=right;
    }
#ifdef PROFILE
    perf_audio(get_ticks()-audio_time);
#endif
}
static void play(int sound,int player){
    disable_interrupts();voices[next_voice]=(Voice){.sound=&gun_sounds[sound],.player=player};next_voice=(next_voice+1)%4;enable_interrupts();
}
void sound_init(void){audio_init(22050,4);audio_set_buffer_callback(fill);audio_write_silence();}
void sound_update(const Game *game){
    for(int i=0;i<2;i++){
        const Fighter *f=&game->fighters[i];
        if(f->shots!=last_shots[i])play(0,i);
        if(f->reloading>last_reload[i])play(1,i);
        if(f->reloading>0 && f->reloading<=1.1f && last_reload[i]>1.1f)play(2,i);
        if(f->reloading>0 && f->reloading<=.4f && last_reload[i]>.4f)play(3,i);
        last_shots[i]=f->shots;last_reload[i]=f->reloading;
    }
}
