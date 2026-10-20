// Pedra/papel/tesoura (geometria da gravacao F4): 3 maos 80x116 em x=110/200/290, base y=268; a escolhida sobe 20 px (anima 4 px/quadro)
// e ganha uma moldura de 9 pedacos da mesma textura. modo 0 = clicar; modo 1 = passar o mouse e parar numa mao.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed); int lag=2+rng()%3;
  int sel=rng()%3, target=rng()%3; int y[3]={152,152,152}; uint32_t T=0x8f88a50; std::vector<int> hist; uint32_t prev=0; int xs=0, clickf=30+rng()%30;
  for(int f=1;f<400;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<3;i++){ int goal= i==ds?132:152; if(y[i]>goal) y[i]-=4; else if(y[i]<goal) y[i]+=4; }
    for(int i=0;i<3;i++){ int x=110+90*i; mui_prim(0,x,y[i],x+80,y[i]+116,80.f*i,0,80.f*i+80,116,T,0xffffffff); }
    for(int i=0;i<3;i++) mui_prim(0,175+45*i,35,215+45*i,93,240,116,320,232,T,0xffffffff);
    { int x=110+90*ds-4, yy=y[ds]-4, X=x+89, Y=yy+125;
      mui_prim(0,x,yy,x+6,yy+6,288,232,294,238,T,0xffffffff); mui_prim(0,x+6,yy,X-6,yy+6,294,232,295,238,T,0xffffffff); mui_prim(0,X-6,yy,X,yy+6,306,232,312,238,T,0xffffffff);
      mui_prim(0,x,yy+6,x+6,Y-6,288,238,294,239,T,0xffffffff); mui_prim(0,X-6,yy+6,X,Y-6,306,238,312,239,T,0xffffffff);
      mui_prim(0,x,Y-6,x+6,Y,288,250,294,256,T,0xffffffff); mui_prim(0,x+6,Y-6,X-6,Y,294,250,295,256,T,0xffffffff); mui_prim(0,X-6,Y-6,X,Y,306,250,312,256,T,0xffffffff); }
    mui_set_frame(0);
    int mx=150+90*target, my=210; if(modo==1&&f<clickf){ int k=(f/8)%3; mx=150+90*k; }
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=(modo==0&&f<clickf)?20:mx; m.my=(modo==0&&f<clickf)?20:my;
    if(modo==0){ m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf; }
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; if(e&0x20&&sel<2) sel++; if(e&0x80&&sel>0) sel--; if(e&0x4000){ xs++; if(sel!=target) printf("X FORA "); } }
  }
  bool ok= sel==target && xs==(modo==0?1:0);
  printf("%d %d modo=%d alvo=%d final=%d X=%d\n",ok,seed,modo,target,sel,xs);
}
