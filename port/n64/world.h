#ifndef CS64_WORLD_H
#define CS64_WORLD_H
#include <stdint.h>
typedef struct { float x,y,z; } Vec3;
typedef struct { Vec3 n; float d; } Plane;
typedef struct { int plane; int16_t child[2]; } Node;
typedef struct { int vis; uint16_t first,count; } Leaf;
typedef struct { int16_t posA[3]; uint16_t normA; int16_t posB[3]; uint16_t normB; uint32_t rgbaA,rgbaB; int16_t stA[2],stB[2]; } VertexPair;
typedef struct { int first; uint16_t count,texture,plane,side; Vec3 center; float radius; } Face;
typedef struct { Vec3 pos; float yaw; } Spawn;
extern const Plane planes[];
extern const Node nodes[],clips[],point_nodes[];
extern const Leaf leaves[];
extern const uint16_t marks[];
extern const uint8_t visibility[];
extern const VertexPair vertices[];
extern const Face faces[];
extern const uint16_t textures[][1024];
extern const int hulls[],crouch_hulls[],point_hulls[];
extern const Spawn spawns[];
extern const int face_count,world_leaf_count,world_faces,leaf_count,texture_count,hull_count,spawn_count,vertex_pairs;
typedef struct { float fraction; Vec3 end,normal; int startsolid; } Trace;
typedef struct { Vec3 pos,vel; float yaw,pitch; int grounded,crouched; } Player;
Vec3 add(Vec3 a,Vec3 b);
Vec3 sub(Vec3 a,Vec3 b);
Vec3 mul(Vec3 a,float b);
float dot(Vec3 a,Vec3 b);
int point_contents(int root,Vec3 p);
Trace trace_world(Vec3 start,Vec3 end);
Trace trace_player(Vec3 start,Vec3 end,int crouched);
Trace trace_bullet(Vec3 start,Vec3 end);
int world_leaf(Vec3 p);
void player_spawn(Player *p,int index);
void player_crouch(Player *p,int crouched);
Vec3 player_eye(const Player *p);
void player_move(Player *p,float forward,float side,int jump,float dt);
#endif
