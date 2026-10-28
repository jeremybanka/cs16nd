#ifndef CS64_MODEL_H
#define CS64_MODEL_H
#include "world.h"
#define MODEL_MAX_BONES 64
#define MODEL_MAX_VERTICES 1024
/* Original MDL local poses and compressed channel streams remain intact. */
typedef struct { int16_t parent,merge; uint8_t gait; float base[6],scale[6]; } ModelBone;
typedef struct { char name[32]; float fps; uint32_t offset; uint16_t frames,blends,motion,motionbone; uint32_t flags; float distance; } ModelSequence;
typedef struct { Vec3 pos; uint16_t bone; } ModelVertex;
typedef struct { uint16_t vertex; int16_t s,t; uint16_t shade; } ModelCorner;
typedef struct { uint16_t first,count,texture,triangle,triangles; } ModelBatch;
typedef struct { uint8_t v[3]; } ModelTriangle;
typedef struct { int bones,sequences,vertices,corners,batches,textures; const ModelBone *bone; const ModelSequence *sequence; const ModelVertex *vertex; const ModelCorner *corner; const ModelBatch *batch; const ModelTriangle *triangle; const uint16_t (*texture)[2048]; const uint8_t *animation; } Model;
typedef struct { float p[3],q[4]; } BonePose;
typedef struct { float m[3][4]; } BoneMatrix;
extern const Model terror_model,glock_view_model,glock_world_model;
int model_sequence(const Model *m,const char *name);
float model_duration(const Model *m,int seq);
void model_pose(const Model *m,int seq,float seconds,float yaw_blend,float pitch_blend,BonePose *pose);
void model_mix(BonePose *a,const BonePose *b,float t);
void model_matrices(const Model *m,const BonePose *pose,BoneMatrix *matrices,const BoneMatrix *merge);
void model_skin(const Model *m,const BoneMatrix *bones,VertexPair *output);
#endif
