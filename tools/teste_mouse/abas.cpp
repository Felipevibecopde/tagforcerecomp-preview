// Abas do Deck Edit (geometria das gravacoes): Deck/Fusion/Side (65x33 em x=147/207/267) e Trunk (129x33 em x=327); a ativa 2 px mais alta.
// O quadrado passa para a proxima (Deck->Fusion->Side->Trunk->Deck), desenhado ~4 quadros depois. Clica numa aba: tem que chegar nela, sem X.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]); std::mt19937 rng(seed); int lag=2+rng()%4;
  int act=rng()%4, target; do target=rng()%4; while(target==act);
  int X0[4]={147,207,267,327}, W[4]={65,65,65,129}; uint32_t T[4]={0x9c2ed30,0x9c2afe0,0x9c2ce10,0x9c289b0};
  std::vector<int> hist; uint32_t prev=0; int xs=0, sq=0, clickf=30+rng()%20;
  for(int f=1;f<300;f++){
    hist.push_back(act); int da=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<4;i++){ int y=i==da?22:24; mui_quad(0,X0[i],y,X0[i]+W[i],y+33,T[i]); }
    for(int r=0;r<4;r++) for(int c=0;c<11;c++){ int x=156+28*c,y=46+34*r; mui_prim(0,x,y,x+22,y+30,0,0,22,30,0x9a00000,0); mui_prim(0,x+1,y+1,x+23,y+31,0,0,22,30,0x9a00000,0); }
    { int x=152,y=42,X=x+33,Y=y+41; uint32_t C=0x9cb2de0; mui_prim(0,x,y,x+6,y+6,213,122,219,128,C,0); mui_prim(0,X-6,y,X,y+6,250,122,256,128,C,0); mui_prim(0,x,Y-6,x+6,Y,213,159,219,165,C,0); mui_prim(0,X-6,Y-6,X,Y,250,159,256,165,C,0); mui_prim(0,x,y+6,x+6,Y-6,213,128,219,129,C,0); mui_prim(0,X-6,y+6,X,Y-6,250,128,256,129,C,0); mui_prim(0,x+6,y,X-6,y+6,219,122,220,128,C,0); mui_prim(0,x+6,Y-6,X-6,Y,219,159,220,165,C,0); }
    mui_set_frame(0);
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=f<clickf?5:X0[target]+W[target]/2; m.my=f<clickf?5:32; m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf;
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; if(e&0x8000){ act=(act+1)%4; sq++; } if(e&0x4000) xs++; }
  }
  bool ok= act==target && xs==0;
  printf("%d %d de=%d alvo=%d final=%d quadrados=%d X=%d\n",ok,seed,(target-((target+4-0)%4))>=0?0:0,target,act,sq,xs);
}
