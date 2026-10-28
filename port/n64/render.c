#include <libdragon.h>
#include <t3d/t3d.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "render.h"
#include "perf.h"
#include "baked.h"
#include "visibility.h"
_Static_assert(sizeof(VertexPair)==sizeof(T3DVertPacked),"Vertex layout mismatch");
enum { MAX_FACES=8192, MAX_PVS_BYTES=2048, BUFFER_COUNT=3 };
static struct { int leaf,count; uint16_t faces[MAX_FACES]; } candidates[2];
static uint16_t draw_order[MAX_FACES];
static int order_count,frame_slot;
static surface_t depth_buffer,*world_tex;
static T3DMat4FP *identity;
typedef struct { const Model *model; surface_t *texture; VertexPair *vertices[BUFFER_COUNT][2]; rspq_block_t *draw[BUFFER_COUNT][2]; int triangles,load_vertices; } Mesh;
static Mesh actors[3],held,viewgun;
typedef struct {
    T3DViewport world[2],weapon[2];
    T3DMat4FP *actor_matrix[2],*weapon_matrix;
    rspq_syncpoint_t fence;
} Frame;
static Frame frames[BUFFER_COUNT];
enum { FACE_CACHE_SIZE=1024 };
typedef struct { rspq_block_t *block; int face; unsigned used,slots; } FaceCommands;
static FaceCommands face_cache[FACE_CACHE_SIZE];
static int16_t face_command_index[MAX_FACES];
static unsigned command_stamp,active_slot;
static uint16_t available_faces[FACE_CACHE_SIZE];
static int available_count;
static void submit_face(const Face *f){
    t3d_vert_load((const T3DVertPacked*)&vertices[f->first],0,(f->count+1)&~1);
    for(int v=1;v<f->count-1;v++)t3d_tri_draw(0,v,v+1);
    t3d_tri_sync();
}
static void run_face(int face){
    int index=face_command_index[face];
    if(index<0){
        PERF_COUNTER(PC_FACE_MISS,1);
        while(available_count){
            int candidate=available_faces[--available_count];
            if(!face_cache[candidate].slots){index=candidate;break;}
        }
        if(index<0){submit_face(&faces[face]);return;}
        FaceCommands *entry=&face_cache[index];
        /* An entry can be freed only after every frame that borrowed it fenced. */
        if(entry->block){face_command_index[entry->face]=-1;rspq_block_free(entry->block);}
        rspq_block_begin();submit_face(&faces[face]);entry->block=rspq_block_end();
        entry->face=face;face_command_index[face]=index;
    }else PERF_COUNTER(PC_FACE_HIT,1);
    FaceCommands *entry=&face_cache[index];entry->used=++command_stamp;entry->slots|=1u<<active_slot;
    rspq_block_run(entry->block);
}

static void mesh_init(Mesh *mesh,const Model *model){
    mesh->model=model;mesh->texture=malloc(sizeof(surface_t)*model->textures);assert(mesh->texture);
    data_cache_hit_writeback((void*)model->texture,model->textures*4096);
    for(int i=0;i<model->textures;i++)mesh->texture[i]=surface_make_linear((void*)model->texture[i],FMT_RGBA16,64,32);
    for(int i=0;i<model->batches;i++){mesh->triangles+=model->batch[i].triangles;mesh->load_vertices+=(model->batch[i].count+1)&~1;}
    for(int s=0;s<BUFFER_COUNT;s++)for(int p=0;p<2;p++){
        mesh->vertices[s][p]=memalign(16,(model->corners/2)*sizeof(VertexPair));assert(mesh->vertices[s][p]);
        model_vertices_init(model,mesh->vertices[s][p]);
    }
}
void render_init(void){
    assertf(face_count<=MAX_FACES && (world_leaf_count+7)/8<=MAX_PVS_BYTES,"Map exceeds runtime capacity");
    candidates[0].leaf=candidates[1].leaf=-1;
    memset(face_command_index,255,sizeof(face_command_index));
    display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
    rdpq_init();t3d_init((T3DInitParams){});
#ifdef VALIDATE
    rdpq_debug_start();
#endif
    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO,rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
    depth_buffer=surface_alloc(FMT_RGBA16,320,240);
    identity=malloc_uncached(sizeof(*identity));assert(identity);t3d_mat4fp_identity(identity);
    for(int s=0;s<BUFFER_COUNT;s++){
        Frame *f=&frames[s];f->weapon_matrix=malloc_uncached(sizeof(T3DMat4FP));
        /* GoldSrc's source viewmodel is left-handed; mirror lateral Z for right hand. */
        t3d_mat4fp_from_srt_euler(f->weapon_matrix,(float[]){1.0f/32,1.0f/32,-1.0f/32},(float[]){0,0,0},(float[]){0,0,0});
        for(int p=0;p<2;p++){
            f->world[p]=t3d_viewport_create();f->weapon[p]=t3d_viewport_create();
            t3d_viewport_set_area(&f->world[p],0,p*120,320,120);
            t3d_viewport_set_area(&f->weapon[p],0,p*120,320,120);
            t3d_viewport_set_projection(&f->world[p],T3D_DEG_TO_RAD(50),4,8000);
            t3d_viewport_set_projection(&f->weapon[p],T3D_DEG_TO_RAD(62),.5f,128);
            t3d_viewport_look_at(&f->weapon[p],&(T3DVec3){{0,0,0}},&(T3DVec3){{1,0,0}},&(T3DVec3){{0,1,0}});
            f->actor_matrix[p]=malloc_uncached(sizeof(T3DMat4FP));assert(f->actor_matrix[p]);
        }
    }
    for(int m=0;m<texture_count;m++)for(int i=0;i<face_count;i++)if(faces[i].count && faces[i].texture==m)draw_order[order_count++]=i;
    data_cache_hit_writeback((void*)vertices,vertex_pairs*sizeof(VertexPair));
    data_cache_hit_writeback((void*)textures,texture_count*2048);
    world_tex=malloc(texture_count*sizeof(surface_t));assert(world_tex);
    for(int i=0;i<texture_count;i++)world_tex[i]=surface_make_linear((void*)textures[i],FMT_RGBA16,32,32);
    mesh_init(&actors[0],&terror_model);mesh_init(&actors[1],&terror_lod1_model);mesh_init(&actors[2],&terror_lod2_model);mesh_init(&held,&glock_world_model);mesh_init(&viewgun,&glock_view_model);
}
static void skin(Mesh *mesh,int slot,int id,BoneMatrix *bones){
    PERF_START(skin_time);
    model_skin(mesh->model,bones,mesh->vertices[slot][id]);
    PERF_END(PF_SKIN,skin_time);
    PERF_DETAIL(mesh==&held?PD_HELD_SKIN:mesh==&viewgun?PD_VIEW_SKIN:PD_ACTOR_SKIN,skin_time);
    PERF_COUNTER(PC_SKIN_VERTICES,mesh->model->vertices);PERF_COUNTER(PC_PACK_VERTICES,mesh->model->corners);
    PERF_START(flush_time);
    data_cache_hit_writeback(mesh->vertices[slot][id],mesh->model->corners/2*sizeof(VertexPair));
    PERF_END(PF_WRITEBACK,flush_time);
}
static void prepare_actor(Game *game,int slot,int i,int lod){
    Frame *frame=&frames[slot];BonePose pose[MODEL_MAX_BONES];BoneMatrix bones[MODEL_MAX_BONES],gun[MODEL_MAX_BONES];Fighter *f=&game->fighters[i];
    PERF_START(pose_time);game_pose(f,pose);PERF_END(PF_PLAYER_POSE,pose_time);
    PERF_START(bone_time);model_matrices(&terror_model,pose,bones,NULL);PERF_END(PF_MATRICES,bone_time);skin(&actors[lod],slot,i,bones);
    PERF_START(held_time);model_pose(&glock_world_model,0,0,.5f,.5f,pose);PERF_END(PF_HELD_POSE,held_time);
    PERF_START(gun_time);model_matrices(&glock_world_model,pose,gun,bones);PERF_END(PF_MATRICES,gun_time);skin(&held,slot,i,gun);
    PERF_START(transform_time);Vec3 p=f->player.pos;
    t3d_mat4fp_from_srt_euler(frame->actor_matrix[i],(float[]){1.0f/32,1.0f/32,1.0f/32},(float[]){0,f->body_yaw,0},(float[]){p.x,p.z,-p.y});
    PERF_END(PF_TRANSFORM,transform_time);
}
static void prepare_weapon(Game *game,int slot,int i){
    Fighter *f=&game->fighters[i];
    PERF_START(view_time);model_baked(&glock_view_model,glock_baked,f->view,f->view_age,viewgun.vertices[slot][i]);PERF_END(PF_VIEW_POSE,view_time);
    PERF_COUNTER(PC_PACK_VERTICES,glock_view_model.corners);
    PERF_START(flush_time);data_cache_hit_writeback(viewgun.vertices[slot][i],glock_view_model.corners/2*sizeof(VertexPair));PERF_END(PF_WRITEBACK,flush_time);
}
static void draw_mesh(Mesh *mesh,int slot,int id){
    int texture=-1;
    for(int i=0;i<mesh->model->batches;i++){
        const ModelBatch *b=&mesh->model->batch[i];
        if(b->texture!=texture){texture=b->texture;rdpq_sync_pipe();rdpq_tex_upload(TILE0,&mesh->texture[texture],NULL);}
        t3d_vert_load((T3DVertPacked*)&mesh->vertices[slot][id][b->first/2],0,(b->count+1)&~1);
        for(int v=0;v<b->triangles;v++){
            const ModelTriangle *tri=&mesh->model->triangle[b->triangle+v];
            t3d_tri_draw(tri->v[0],tri->v[1],tri->v[2]);
        }
        t3d_tri_sync();
    }
}
static void run_mesh(Mesh *mesh,int slot,int id){
    PERF_COUNTER(PC_MODEL_TRIANGLES,mesh->triangles);PERF_COUNTER(PC_MODEL_VERTICES,mesh->load_vertices);PERF_COUNTER(PC_MODEL_UPLOADS,mesh->model->textures);
    if(!mesh->draw[slot][id]){rspq_block_begin();draw_mesh(mesh,slot,id);mesh->draw[slot][id]=rspq_block_end();}
    rspq_block_run(mesh->draw[slot][id]);
}
static void draw_world(const Player *player,int view_id,int fullscreen){
    PERF_START(pvs_time);int leaf=world_leaf(player_eye(player));
    if(candidates[view_id].leaf!=leaf){
        candidates[view_id].count=world_candidates(leaf,draw_order,order_count,candidates[view_id].faces);
        candidates[view_id].leaf=leaf;
    }
    PERF_END(PF_PVS,pvs_time);
    PERF_COUNTER(PC_CANDIDATES,candidates[view_id].count);
    PERF_START(world_time);
    Vec3 eye=player_eye(player),dir=player_direction(player);
    Vec3 right={sinf(player->yaw),-cosf(player->yaw),0};
    Vec3 up={-cosf(player->yaw)*sinf(player->pitch),-sinf(player->yaw)*sinf(player->pitch),cosf(player->pitch)};
    int bound=-1;
#ifdef PROFILE
    unsigned drawn=0,triangles=0,loaded=0,uploads=0;
#endif
    for(int at=0;at<candidates[view_id].count;at++){
        int i=candidates[view_id].faces[at];const Face *f=&faces[i];
        const Plane *pl=&planes[f->plane];float facing=dot(eye,pl->n)-pl->d;
        if((f->side?-facing:facing)<-.1f)continue;
        Vec3 diff=sub(f->center,eye);float depth=dot(diff,dir);
        if(depth+f->radius<2)continue;
        /* 50 degree vertical / 102 degree horizontal, conservative sphere margin. */
        if(fabsf(dot(diff,right))>depth*(fullscreen?.96f:1.26f)+f->radius*(fullscreen?1.4f:1.65f) || fabsf(dot(diff,up))>depth*(fullscreen?.73f:.48f)+f->radius*(fullscreen?1.3f:1.2f))continue;
        if(bound!=f->texture){bound=f->texture;rdpq_sync_pipe();rdpq_tex_upload(TILE0,&world_tex[bound],&(rdpq_texparms_t){.s.repeats=REPEAT_INFINITE,.t.repeats=REPEAT_INFINITE});
#ifdef PROFILE
            uploads++;
#endif
        }
        run_face(i);
#ifdef PROFILE
        drawn++;triangles+=f->count-2;loaded+=(f->count+1)&~1;
#endif
    }
    PERF_END(view_id?PF_WORLD1:PF_WORLD0,world_time);
    PERF_COUNTER(PC_WORLD_FACES,drawn);PERF_COUNTER(PC_WORLD_TRIANGLES,triangles);PERF_COUNTER(PC_WORLD_VERTICES,loaded);PERF_COUNTER(PC_WORLD_UPLOADS,uploads);
    (void)view_id;
}
static void hud(const Fighter *f,int id,int connected){
    int y=id*120,cx=160,cy=y+60;
    rdpq_sync_pipe();rdpq_set_mode_fill(f->hit_marker>0?RGBA32(255,65,40,255):RGBA32(175,235,80,255));
    if(f->health>0){
        rdpq_fill_rectangle(cx-6,cy,cx-2,cy+1);rdpq_fill_rectangle(cx+3,cy,cx+7,cy+1);
        rdpq_fill_rectangle(cx,cy-6,cx+1,cy-2);rdpq_fill_rectangle(cx,cy+3,cx+1,cy+7);
    }
    if(f->hurt>0){rdpq_set_mode_fill(RGBA32(190,35,25,255));rdpq_fill_rectangle(0,y,320,y+2);}
    rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,5,y+12,"P%d TERRORIST",id+1);
    if(!connected)rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,185,y+12,"CONNECT PAD %d",id+1);
    if(f->health<=0)rdpq_text_print(NULL,FONT_BUILTIN_DEBUG_MONO,94,y+59,"RESPAWNING...");
    else if(f->reloading>0)rdpq_text_print(NULL,FONT_BUILTIN_DEBUG_MONO,126,y+85,"RELOADING");
    rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,5,y+114,"HP %3d  K %d",f->health,f->kills);
    rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,208,y+114,"GLOCK %02d/%03d",f->ammo,f->reserve);
}
void render_frame(Game *game,float fps,int connected[2]){
    int slot=frame_slot;frame_slot=(slot+1)%BUFFER_COUNT;Frame *frame=&frames[slot];
    PerfOptions options=perf_options();static int frozen[BUFFER_COUNT];
    PERF_START(fence_time);
    if(frame->fence)rspq_syncpoint_wait(frame->fence);
    PERF_END(PF_FENCE,fence_time);
    active_slot=slot;
    available_count=0;
    for(int i=FACE_CACHE_SIZE-1;i>=0;i--){
        face_cache[i].slots&=~(1u<<slot);
        if(!face_cache[i].slots)available_faces[available_count++]=i;
    }
    int update_models=!options.freeze || !frozen[slot];
    frozen[slot]=options.freeze;
    PERF_START(display_time);surface_t *surface=display_get();PERF_END(PF_DISPLAY,display_time);
#ifdef PROFILE
    perf_graphics_begin(surface);
#endif
    PERF_START(setup_time);
    rdpq_attach(surface,&depth_buffer);t3d_frame_start();
    t3d_screen_clear_color(RGBA32(174,188,195,255));t3d_screen_clear_depth();
    t3d_light_set_count(0);t3d_light_set_ambient((uint8_t[]){255,255,255,255});
    PERF_END(PF_SETUP,setup_time);perf_drain(PF_DRAIN_SETUP);
    for(int i=0;i<options.views;i++){
        PERF_START(camera_time);
        Fighter *f=&game->fighters[i];Vec3 eye=player_eye(&f->player),dir=player_direction(&f->player);
        T3DVec3 camera={{eye.x,eye.z,-eye.y}},target={{eye.x+dir.x,eye.z+dir.z,-eye.y-dir.y}};
        T3DViewport *view=&frame->world[i];
#ifdef PROFILE
        t3d_viewport_set_area(view,0,i*120,320,options.fullscreen?240:120);
        t3d_viewport_set_projection(view,T3D_DEG_TO_RAD(options.fullscreen?70:50),4,8000);
#endif
        t3d_viewport_look_at(view,&camera,&target,&(T3DVec3){{0,1,0}});
        t3d_viewport_attach(view);t3d_matrix_push(identity);
        rdpq_set_mode_standard();rdpq_mode_zbuf(true,true);rdpq_mode_persp(true);
        rdpq_mode_filter(FILTER_BILINEAR);rdpq_mode_alphacompare(1);rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
        t3d_state_set_drawflags(T3D_FLAG_DEPTH|T3D_FLAG_TEXTURED|T3D_FLAG_SHADED);
        PERF_END(PF_SETUP,camera_time);
        draw_world(&f->player,i,options.fullscreen);perf_drain(i?PF_DRAIN_WORLD1:PF_DRAIN_WORLD0);
        /* Submit terrain first so RSP work overlaps CPU model preparation. */
        rspq_flush();
        Fighter *other=&game->fighters[1-i];
        int actor_visible=options.actors && sphere_visible(&f->player,other->player.pos,actor_bound_radius,options.fullscreen);
        float nearest=dot(sub(other->player.pos,player_eye(&f->player)),dir)-actor_bound_radius;
        int lod=nearest>900?2:nearest>400?1:0;
        if(actor_visible && update_models)prepare_actor(game,slot,1-i,lod);
        if(!actor_visible){other->pose_valid=0;PERF_COUNTER(PC_ACTOR_CULLED,1);}
        else if(lod==1)PERF_COUNTER(PC_LOD1,1);
        else if(lod==2)PERF_COUNTER(PC_LOD2,1);
        PERF_START(actor_time);
        if(actor_visible){
            t3d_state_set_drawflags(T3D_FLAG_DEPTH|T3D_FLAG_TEXTURED|T3D_FLAG_SHADED|T3D_FLAG_CULL_FRONT);
            t3d_matrix_set(frame->actor_matrix[1-i],true);run_mesh(&actors[lod],slot,1-i);run_mesh(&held,slot,1-i);
        }
        t3d_matrix_pop(1);
        PERF_END(PF_ACTORS,actor_time);perf_drain(PF_DRAIN_ACTORS);
        if(f->health>0 && options.weapons){
            if(update_models)prepare_weapon(game,slot,i);
            PERF_START(weapon_setup_time);
            t3d_viewport_attach(&frame->weapon[i]);
            /* Clear this viewport's depth only: hands retain self-occlusion. */
            rdpq_sync_pipe();t3d_screen_clear_depth();
            t3d_state_set_drawflags(T3D_FLAG_DEPTH|T3D_FLAG_TEXTURED|T3D_FLAG_SHADED|T3D_FLAG_CULL_BACK);
            PERF_END(PF_WEAPON_SETUP,weapon_setup_time);
            PERF_START(weapon_time);
            t3d_matrix_push(frame->weapon_matrix);run_mesh(&viewgun,slot,i);t3d_matrix_pop(1);
            PERF_END(PF_WEAPONS,weapon_time);
        }
        perf_drain(PF_DRAIN_WEAPONS);
        PERF_START(hud_time);if(options.hud)hud(f,i,connected[i]);PERF_END(PF_HUD,hud_time);
        perf_drain(PF_DRAIN_HUD);
    }
    PERF_START(present_time);
    frame->fence=rspq_syncpoint_new();
    rdpq_sync_pipe();rdpq_set_scissor(0,0,320,240);rdpq_set_mode_fill(RGBA32(20,22,24,255));rdpq_fill_rectangle(0,119,320,121);
#ifdef SMOKE_TEST
#ifdef PROFILE
    rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,125,12,"TEST %4u PROFILE",game->ticks);
    (void)fps;
#else
    rdpq_text_printf(NULL,FONT_BUILTIN_DEBUG_MONO,125,12,"TEST %4u %2.0fFPS",game->ticks,fps);
#endif
#else
    (void)fps;
#endif
#ifdef PROFILE
    perf_present();
#else
    rdpq_detach_show();
#endif
    PERF_END(PF_PRESENT,present_time);
}
