// Tributo: escolher 2 monstros seguidos. Seus monstros em zonas de y=145 (geometria medida); depois de confirmar o 1o (X) a maozinha
// FICA nele e o direcional continua andando entre os monstros (sem janelinha). modo 0 = clicando nos dois; modo 1 = clica no 1o, passa o
// mouse no 2o e clica. Tem que chegar em cada um e mandar exatamente 2 X, cada um no monstro certo.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed); int lag=3+rng()%3;
  int mx5[5]={226,270,314,358,401}; std::vector<int> mons; for(int z=0;z<5;z++) if(rng()%3!=0) mons.push_back(z); while(mons.size()<3){ int z=rng()%5; if(std::find(mons.begin(),mons.end(),z)==mons.end()) mons.push_back(z);} std::sort(mons.begin(),mons.end());
  int n=mons.size(); int sel=0; int a=rng()%n, b; do b=rng()%n; while(b==a);
  int nh=4, hx0=240-nh*47/2;
  std::vector<int> hist; uint32_t prev=0; std::vector<int> xs; int at_click=-1;
  int t1=40, t2=40+90+rng()%60;
  for(int f=1;f<500;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<n;i++){ int x=mx5[mons[i]]-14; mui_quad(0,x,143,x+28,170,0x98b4e40); mui_quad(0,x,143,x+28,170,0x9d00000+i*0x800); }
    for(int i=0;i<nh;i++){ int x=hx0+47*i; mui_quad(0,x,219,x+45,283,0x98b4e40); mui_quad(0,x,219,x+45,283,0x9e00000+i*0x800); }
    mui_prim(0,mx5[mons[ds]],145,mx5[mons[ds]]+32,177,64,96,96,128,0x9925340,0);
    mui_set_frame(0);
    int tgt = f<t2-30 ? a : b; int mx=mx5[mons[tgt]], my=156;
    MuiMouse m{}; m.in_client=m.inside=true; m.mx= f<t1?5:mx; m.my= f<t1?5:my;
    bool click = (f>=t1&&f<t1+4) || (f>=t2&&f<t2+4);
    if(modo==1 && f>=t1+4 && f<t2) { }   // so passa o mouse ate o 2o
    m.down[0]=click; m.clicked[0]=(f==t1||f==t2);
    uint32_t bb=mui_mouse(f,m,true,0);
    if(f==t2-1) at_click=sel;
    if(f%2==0){ uint32_t e=bb&~prev; prev=bb; if(e&0x20&&sel<n-1) sel++; if(e&0x80&&sel>0) sel--; if(e&0x4000) xs.push_back(sel); }
  }
  bool ok = xs.size()==2 && xs[0]==a && xs[1]==b && (modo==0 || at_click==b);
  printf("%d %d modo=%d monstros=%d primeiro=%d segundo=%d X=",ok,seed,modo,n,a,b); for(int x:xs) printf("%d ",x); printf("\n");
}
