#include <cstdio>
#include <cstdlib>
#include "sonic_levels.h"
const char *NAMES[4][2] = {{"GH1","GH2"},{"CP1","CP2"},{"CN1","CN2"},{"SB1","SB2"}};
const ActDef *A;
int terr(int x){ const TerrainPt*t=A->ter; int n=A->nTer;
  if(x<=t[0].x) return t[0].y; if(x>=t[n-1].x) return t[n-1].y;
  for(int i=0;i<n-1;i++) if(x>=t[i].x && x<t[i+1].x) return t[i].y + (t[i+1].y-t[i].y)*(x-t[i].x)/(t[i+1].x-t[i].x);
  return t[n-1].y; }
int main(){
  int bad=0;
  for(int z=0;z<4;z++) for(int a=0;a<2;a++){
    A=&ACTS[z][a]; int maxCliff=0, nCliff=0, objs=0, plats=0, rings=0, maxSlope=0, hazards=0;
    for(int i=1;i<A->nTer;i++){
      int dx=A->ter[i].x-A->ter[i-1].x, dy=abs(A->ter[i].y-A->ter[i-1].y);
      if(dx<=0){printf("  %s terrain not increasing at %d\n",NAMES[z][a],A->ter[i].x);bad++;}
      else if(dx==1 && dy>6){ // cliff
        int x=A->ter[i].x; bool up=A->ter[i].y<A->ter[i-1].y; int springNear=0;
        for(int j=0;j<A->nObj;j++){ const LvlObj&o=A->obj[j]; int ox=o.x, ex=o.x;
          if(o.kind==L_SPRING && ox<x && ox>=x-8) springNear=1;   // flush against the face
          if(o.kind==L_SPIKES||o.kind==L_RINGS||o.kind==L_ARC) ex=o.x+12*(o.arg-1);
          if(o.kind==L_PLAT) ex=o.x+o.arg;
          int near = (o.kind==L_CRAB)?30 : (o.kind==L_SPIKES||o.kind==L_SPIKE||o.kind==L_WALL||o.kind==L_FLAME)?14 : (o.kind==L_SPRING)?0 : 0;   // springs belong right at the face
          if(near && ex>x-near && ox<x+near){printf("  %s kind %d at %d too close to cliff at %d\n",NAMES[z][a],o.kind,o.x,x);bad++;}
          if(o.kind==L_BRIDGE && x>=o.x-8 && x<=o.x+o.arg*4+8){printf("  %s cliff %d inside bridge\n",NAMES[z][a],x);bad++;} }
        if(up && dy>30 && !springNear){printf("  %s up-cliff %d px at %d needs a spring flush against it (x-6)\n",NAMES[z][a],dy,x);bad++;}
        if(dy>maxCliff) maxCliff=dy; nCliff++;
      }
      else { int s=dy*32/dx; if(s>maxSlope)maxSlope=s; if(dy*32>12*dx){printf("  %s steep slope %d->%d (%d px over %d)\n",NAMES[z][a],A->ter[i-1].x,A->ter[i].x,dy,dx);bad++;} }
    }
    if(A->ter[A->nTer-1].x < A->len+200){printf("  %s terrain ends before len+200\n",NAMES[z][a]);bad++;}
    for(int i=0;i<A->nObj;i++){ const LvlObj&o=A->obj[i];
      if(o.x>=A->len-20){printf("  %s object kind %d at x %d is past/at the end (%d)\n",NAMES[z][a],o.kind,o.x,A->len);bad++;}
      switch(o.kind){
        case L_RINGS: case L_ARC: objs+=o.arg; rings+=o.arg; break;
        case L_SPIKES: objs+=o.arg; hazards+=o.arg; break;
        case L_PLAT: plats++; if(o.arg>48){printf("  %s plat too wide at %d\n",NAMES[z][a],o.x);bad++;} break;
        case L_LOOP: { int g=terr(o.x); for(int x=o.x-110;x<=o.x+140;x+=2) if(terr(x)!=g){printf("  %s loop at %d: ground not flat at %d (%d vs %d)\n",NAMES[z][a],o.x,x,terr(x),g);bad++;break;} } break;
        case L_CHEMX: { objs+=4+2+1; plats+=14; int g=terr(o.x+40); for(int x=o.x;x<=o.x+130;x+=2) if(terr(x)!=g){printf("  %s chemx at %d not flat at %d\n",NAMES[z][a],o.x,x);bad++;break;} } break;
        case L_RING: objs++; rings++; break;
        case L_MONITOR: objs++; break;
        case L_STARPOST: objs++; break;
        default: objs++; if(o.kind!=L_DECO) hazards++; break;
      }
    }
    // spikes need open sky: no platform lower than 2 Sonic heights (32 px) above them
    for(int i=0;i<A->nObj;i++){ const LvlObj&p=A->obj[i]; if(p.kind!=L_PLAT||p.h>=32) continue;
      for(int j=0;j<A->nObj;j++){ const LvlObj&k=A->obj[j]; int n= k.kind==L_SPIKES?k.arg : k.kind==L_SPIKE?1:0;
        for(int c=0;c<n;c++){ int sx=k.x+c*12; if(sx+9>p.x && sx<p.x+p.arg){ printf("  %s plat x%d w%d h%d covers spike at %d\n",NAMES[z][a],p.x,p.arg,p.h,sx); bad++; } } } }
    for(int i=0;i<A->nObj;i++){ const LvlObj&b=A->obj[i]; if(b.kind!=L_BRIDGE) continue;
      int x0=b.x, x1=b.x+b.arg*4, y=terr(x0);
      if(terr(x1)!=y) { printf("  %s bridge %d-%d: far end ground %d != deck %d\n",NAMES[z][a],x0,x1,terr(x1),y); bad++; }
      int deep=-9999; for(int x=x0;x<=x1;x+=4) if(terr(x)>deep) deep=terr(x);
      printf("  %s bridge %d-%d deck y%d pool floor y%d\n",NAMES[z][a],x0,x1,y,deep);
      for(int j=0;j<A->nObj;j++){ const LvlObj&c=A->obj[j];
        if(c.kind==L_CRAB && c.x+18>x0 && c.x-18<x1){ printf("  %s crab %d patrols into the pool\n",NAMES[z][a],c.x); bad++; }
        if(c.kind==L_FISH && (c.x<x0+8||c.x>x1-8) && c.x>x0-40 && c.x<x1+40){ printf("  %s fish %d not under the deck\n",NAMES[z][a],c.x); bad++; } } }
    // Landing zones: the top of every up-cliff, the foot of every drop, and every spring.
    // Hazards there are hidden traps (you're mid-air or just landed). Green Hill 1 teaches the
    // game, so it must be clean; other acts are only counted.
    { int traps=0; const int LZ=80;
      for(int i=1;i<A->nTer;i++){ int dx=A->ter[i].x-A->ter[i-1].x, dy=abs(A->ter[i].y-A->ter[i-1].y); if(dx!=1||dy<=6) continue;
        int x=A->ter[i].x;
        for(int j=0;j<A->nObj;j++){ const LvlObj&o=A->obj[j]; int x0=o.x, x1=o.x;
          if(o.kind==L_CRAB){x0-=18;x1+=18;} else if(o.kind==L_BEE){x0-=30;x1+=30;} else if(o.kind==L_SPIKES) x1=o.x+12*o.arg; else if(!(o.kind==L_SPIKE||o.kind==L_WALL||o.kind==L_SAW||o.kind==L_FLAME)) continue;
          if(x1>x && x0<x+LZ){ traps++; if(z==0&&a==0){printf("  %s kind %d at %d sits in the landing zone of the cliff at %d\n",NAMES[z][a],o.kind,o.x,x);bad++;} } } }
      if(traps) printf("  %s: %d hazard(s) in cliff landing zones%s\n",NAMES[z][a],traps,(z==0&&a==0)?"":" (allowed outside Green Hill 1)"); }
    int lo=999,hi=-999; for(int i=0;i<A->nTer;i++){ if(A->ter[i].y<lo)lo=A->ter[i].y; if(A->ter[i].y>hi)hi=A->ter[i].y; }
    int topPlat=lo; for(int i=0;i<A->nObj;i++) if(A->obj[i].kind==L_PLAT){ int t=terr(A->obj[i].x)-A->obj[i].h; if(t<topPlat)topPlat=t; }
    printf("  %s ground y %d..%d (%d px), highest ledge y %d -> total vertical %d px (%.1f screens)\n",NAMES[z][a],lo,hi,hi-lo,topPlat,hi-topPlat,(hi-topPlat)/64.0);
    for(int i=0;i<A->nObj;i++) if(A->obj[i].kind==L_DECO) for(int j=i+1;j<A->nObj;j++) if(A->obj[j].kind==L_DECO && abs(A->obj[j].x-A->obj[i].x)<32){ printf("  %s deco at %d and %d closer than 32 px\n",NAMES[z][a],A->obj[i].x,A->obj[j].x); bad++; }
    objs++; // goal
    printf("%s: len %5d  par %2ds  objects %3d/384  plats %2d/64  rings %3d  hazards/enemies %3d  max slope %d/32  cliffs %d (max %d)\n",
      NAMES[z][a],A->len,A->parSec,objs,plats,rings,hazards,maxSlope,nCliff,maxCliff);
    if(objs>384||plats>64){printf("  OVER CAPACITY\n");bad++;}
  }
  printf(bad? "\n%d problems\n":"\nall checks passed\n",bad);
}
