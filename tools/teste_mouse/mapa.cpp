// Mapa da escola (Modo Historia): 12 lugares nas posicoes da gravacao F4, quads 16x16 desenhados 2x; setinha 16x16 balancando 16-20 px acima
// do lugar escolhido, que some 5 quadros enquanto anda. Direcional: vai ao lugar mais perto naquela direcao (dentro de 60 graus).
// modo 0 = clicar num lugar; modo 1 = passar o mouse e parar num lugar.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed); int lag=3+rng()%3;
  int P[12][2]={{311,41},{202,78},{113,82},{81,121},{315,128},{391,132},{118,139},{23,146},{197,157},{266,168},{340,179},{8,216}};
  int sel=rng()%12, target; do target=rng()%12; while(target==sel);
  auto step=[&](int s,int dx,int dy){ int best=-1; double bd=1e9; for(int i=0;i<12;i++){ if(i==s) continue; double vx=P[i][0]-P[s][0], vy=P[i][1]-P[s][1], L=std::hypot(vx,vy); double cosv=(vx*dx+vy*dy)/L; if(cosv<0.5) continue; if(L<bd){bd=L;best=i;} } return best<0?s:best; };
  std::vector<int> hist; uint32_t prev=0; int xs=0, moved_at=-100, clickf=40+rng()%30;
  for(int f=1;f<900;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<12;i++){ mui_quad(0,P[i][0],P[i][1],P[i][0]+16,P[i][1]+16,0x9867fc0); mui_quad(0,P[i][0],P[i][1],P[i][0]+16,P[i][1]+16,0x9868230); }
    mui_quad(0,0,0,511,257,0x9876530); mui_quad(0,201,199,265,263,0x9874a50);
    bool anim = f-moved_at < lag+5;
    if(!anim){ int bob=(f/6)%5; mui_quad(0,P[ds][0],P[ds][1]-16-bob,P[ds][0]+16,P[ds][1]-bob,0x9868dd0); mui_quad(0,P[ds][0]-8,P[ds][1]-8,P[ds][0]+24,P[ds][1]+24,0x9868a60); }
    mui_set_frame(0);
    int mx=P[target][0]+8, my=P[target][1]+8;
    if(modo==1 && f<clickf){ int k=(f/10)%12; mx=P[k][0]+8; my=P[k][1]+8; }
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=(modo==0&&f<clickf)?470:mx; m.my=(modo==0&&f<clickf)?260:my;
    if(modo==0){ m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf; }
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; int old=sel;
      if(e&0x20) sel=step(sel,1,0); else if(e&0x80) sel=step(sel,-1,0); else if(e&0x40) sel=step(sel,0,1); else if(e&0x10) sel=step(sel,0,-1);
      if(sel!=old) moved_at=f;
      if(e&0x4000){ xs++; if(sel!=target) printf("X FORA "); }
    }
  }
  bool ok= sel==target && xs==(modo==0?1:0);
  printf("%d %d modo=%d alvo=%d final=%d X=%d\n",ok,seed,modo,target,sel,xs);
}
