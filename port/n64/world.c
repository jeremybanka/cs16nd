#include "world.h"
#include <math.h>
#include <string.h>
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static int contents(const Node *tree,int root,Vec3 p){
    while(root>=0){const Node *n=&tree[root];const Plane *pl=&planes[n->plane];root=n->child[dot(p,pl->n)<pl->d];}
    return root;
}
int point_contents(int root,Vec3 p){return contents(clips,root,p);}
int world_leaf(Vec3 p){
    int root=0;
    while(root>=0){const Node *n=&nodes[root];const Plane *pl=&planes[n->plane];root=n->child[dot(p,pl->n)<pl->d];}
    return -1-root;
}
/* Sweep the center through the BSP's pre-expanded standing-player hull.
   Child contents -1 = empty, -2 = solid. Other contents are traversable here. */
static int trace_node(const Node *tree,int root,int node,float f0,float f1,Vec3 a,Vec3 b,Trace *t){
    if(node<0)return node!=-2;
    const Node *n=&tree[node]; const Plane *pl=&planes[n->plane];
    float da=dot(a,pl->n)-pl->d,db=dot(b,pl->n)-pl->d;
    if(da>=0 && db>=0)return trace_node(tree,root,n->child[0],f0,f1,a,b,t);
    if(da<0 && db<0)return trace_node(tree,root,n->child[1],f0,f1,a,b,t);
    int side=da<0;
    float fraction=clamp((da+(side?0.03125f:-0.03125f))/(da-db),0,1);
    float midf=f0+(f1-f0)*fraction;
    Vec3 mid=add(a,mul(sub(b,a),fraction));
    if(!trace_node(tree,root,n->child[side],f0,midf,a,mid,t))return 0;
    if(contents(tree,n->child[!side],mid)!=-2)
        return trace_node(tree,root,n->child[!side],midf,f1,mid,b,t);
    t->normal=mul(pl->n,side?-1:1);
    /* Roundoff can put the split into solid; back up a bounded fraction. */
    while(contents(tree,root,mid)==-2 && fraction>0){
        fraction=fmaxf(0,fraction-0.01f);midf=f0+(f1-f0)*fraction;
        mid=add(a,mul(sub(b,a),fraction));
    }
    t->fraction=midf;t->end=mid;return 0;
}
static Trace trace_hulls(const Node *tree,const int *roots,Vec3 start,Vec3 end){
    Trace best={.fraction=1,.end=end};
    for(int i=0;i<hull_count;i++){
        if(contents(tree,roots[i],start)==-2){best.startsolid=1;best.fraction=0;best.end=start;return best;}
        Trace t={.fraction=1,.end=end};
        trace_node(tree,roots[i],roots[i],0,1,start,end,&t);
        if(t.fraction<best.fraction)best=t;
    }
    return best;
}
Trace trace_world(Vec3 start,Vec3 end){return trace_hulls(clips,hulls,start,end);}
Trace trace_player(Vec3 start,Vec3 end,int crouched){return trace_hulls(clips,crouched?crouch_hulls:hulls,start,end);}
Trace trace_bullet(Vec3 start,Vec3 end){return trace_hulls(point_nodes,point_hulls,start,end);}
Vec3 player_eye(const Player *p){return add(p->pos,(Vec3){0,0,p->crouched?12:28});}
void player_crouch(Player *p,int crouched){
    if(crouched==p->crouched)return;
    /* Hull 3 is 36 units high; preserve the feet when changing hulls. */
    Vec3 pos=add(p->pos,(Vec3){0,0,crouched?-18:18});
    if(crouched){
        Trace down=trace_player(p->pos,pos,1);
        if(down.startsolid)return;
        pos=down.end;
    }
    if(!trace_player(pos,pos,crouched).startsolid){p->pos=pos;p->crouched=crouched;}
}
void player_spawn(Player *p,int index){
    memset(p,0,sizeof(*p));p->pos=spawns[index%spawn_count].pos;p->yaw=spawns[index%spawn_count].yaw;
}
static void slide(Player *p,float dt){
    Vec3 normals[5],original=p->vel;int count=0;
    for(int bump=0;bump<4;bump++){
        Trace t=trace_player(p->pos,add(p->pos,mul(p->vel,dt)),p->crouched);
        if(t.startsolid){p->vel=(Vec3){0};return;}
        p->pos=t.end;
        if(t.fraction==1)break;
        dt*=1-t.fraction;normals[count++]=t.normal;
        Vec3 input=p->vel;
        int i;
        for(i=0;i<count;i++){
            p->vel=sub(input,mul(normals[i],dot(input,normals[i])));
            int j;for(j=0;j<count;j++)if(j!=i && dot(p->vel,normals[j])<-0.001f)break;
            if(j==count)break;
        }
        if(i==count){p->vel=(Vec3){0};break;}
        if(dot(p->vel,original)<=0){p->vel=(Vec3){0};break;}
    }
}
void player_move(Player *p,float forward,float side,int jump,float dt){
    Trace floor=trace_player(p->pos,add(p->pos,(Vec3){0,0,-2}),p->crouched);
    p->grounded=floor.fraction<1 && floor.normal.z>0.7f && p->vel.z<=0;
    if(p->grounded){
        p->pos=floor.end;p->vel.z=0;
        float speed=hypotf(p->vel.x,p->vel.y);
        if(speed>0){float next=fmaxf(0,speed-fmaxf(speed,100)*6*dt);p->vel.x*=next/speed;p->vel.y*=next/speed;}
    }
    float len=hypotf(forward,side);if(len>1){forward/=len;side/=len;}
    Vec3 wish={cosf(p->yaw)*forward+sinf(p->yaw)*side,sinf(p->yaw)*forward-cosf(p->yaw)*side,0};
    float wishlen=hypotf(wish.x,wish.y),speed=(p->crouched?85:250)*wishlen;
    if(wishlen>0){
        wish=mul(wish,1/wishlen);
        float gain=fminf((p->grounded?10:1)*speed*dt,speed-dot(p->vel,wish));
        if(gain>0)p->vel=add(p->vel,mul(wish,gain));
    }
    if(jump && p->grounded){p->vel.z=268;p->grounded=0;}
    if(!p->grounded)p->vel.z-=800*dt;
    Player start=*p;slide(p,dt);
    if(start.grounded && !jump){
        Player step=start;
        Trace up=trace_player(step.pos,add(step.pos,(Vec3){0,0,18}),step.crouched);
        if(!up.startsolid){
            step.pos=up.end;slide(&step,dt);
            Trace down=trace_player(step.pos,add(step.pos,(Vec3){0,0,-18}),step.crouched);
            Vec3 a=sub(p->pos,start.pos),b=sub(down.end,start.pos);
            if(down.fraction<1 && down.normal.z>0.7f && b.x*b.x+b.y*b.y>a.x*a.x+a.y*a.y){
                step.pos=down.end;step.vel.z=p->vel.z;*p=step;
            }
        }
    }
}
