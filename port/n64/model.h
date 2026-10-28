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
extern const Model terror_lod1_model,terror_lod2_model;
extern const float actor_bound_radius;
typedef union { int16_t xyz[4]; uint64_t word; } ModelPosition;
typedef uint64_t ModelPositionWord __attribute__((__may_alias__));
/* Position + zero normal are a single aligned VR4300 load/store. */
static inline void model_scatter(const Model *m,const ModelPosition *positions,VertexPair *out){
    ModelPositionWord *words=(ModelPositionWord*)out;
    const ModelCorner *corner=m->corner,*end=corner+m->corners;
    for(;corner<end;corner+=2,words+=4){
        words[0]=positions[corner[0].vertex].word;
        words[1]=positions[corner[1].vertex].word;
    }
}
int model_sequence(const Model *m,const char *name);
float model_duration(const Model *m,int seq);
void model_pose(const Model *m,int seq,float seconds,float yaw_blend,float pitch_blend,BonePose *pose);
void model_mix(BonePose *a,const BonePose *b,float t);
void model_matrices(const Model *m,const BonePose *pose,BoneMatrix *matrices,const BoneMatrix *merge);
/* Initialize immutable UV/color/normal fields once for each fenced buffer. */
void model_vertices_init(const Model *m,VertexPair *output);
/* Finite coordinates in the signed 16-bit range; rounds ties away from zero. */
int16_t model_round_position(float value);
void model_skin(const Model *m,const BoneMatrix *bones,VertexPair *output);
#endif
