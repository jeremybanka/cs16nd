#ifndef CS64_BAKED_H
#define CS64_BAKED_H
#include "model.h"
typedef struct { uint16_t frames,vertices; const uint16_t *ticks; const int16_t (*positions)[3]; } BakedClip;
extern const BakedClip glock_baked[];
void model_baked(const Model *model,const BakedClip *clips,int sequence,float seconds,VertexPair *output);
#endif
