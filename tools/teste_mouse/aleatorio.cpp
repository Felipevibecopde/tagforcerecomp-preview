#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
// Um clique por execucao: o mouse vagueia pela lista (com tremor), clica num item com duracao aleatoria, escorrega depois.
int main(int argc,char**argv){
  int seed=atoi(argv[1]); std::mt19937 rng(seed);
  int n=2+rng()%6, lag=1+rng()%4, close_anim=rng()%12; bool blink=rng()%3==0;
  int sel=rng()%n, screen=0, closing=0; std::vector<int> hist; uint32_t prev=0, b=0;
  int wander_until=20+rng()%80, clickf=wander_until, hold=1+rng()%25; int target=rng()%n;
  int cur=rng()%n; std::vector<std::pair<int,int>> conf; 
  for(int f=1;f<400;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    if(closing>0)closing--;
    if(screen==0||closing>0) for(int i=0;i<n;i++){ uint32_t tex=(i==ds&&!(blink&&(f%11)>=9))?2:1; int y=50+i*30;
      mui_prim(0,100,y,116,y+20,0,0,16,20,tex,0); mui_prim(0,116,y,284,y+20,16,0,32,20,tex,0); mui_prim(0,284,y,300,y+20,32,0,48,20,tex,0);}
    mui_set_frame(0);
    if(f<wander_until && rng()%6==0) cur=rng()%n;
    int item = f<clickf?cur: (f<clickf+hold+3?target: (int)(rng()%n));
    int mx=110+rng()%180, my=50+item*30+2+rng()%16;
    bool down=f>=clickf&&f<clickf+hold;
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=mx; m.my=my; m.down[0]=down; m.clicked[0]=(f==clickf);
    b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b;
      if(screen==0){ if(e&0x10&&sel>0)sel--; if(e&0x40&&sel<n-1)sel++; if(e&0x4000){conf.push_back({sel,0}); screen=1; closing=close_anim;} }
      else if(e&0x4000) conf.push_back({-1,1}); }
  }
  bool ok = conf.size()==1 && conf[0].first==target;
  printf("%d %d n=%d lag=%d anim=%d blink=%d hold=%d alvo=%d conf=",ok,seed,n,lag,close_anim,blink,hold,target);
  for(auto&c:conf)printf("%d ",c.first); printf("\n");
}
