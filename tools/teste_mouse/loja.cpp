// Simula a loja (grade de pacotes 12x4, casas a cada 22x47 px, divisores 2x8 no alto de cada casa) com a moldura de 4 cantos que pulsa,
// casas vazias (sem pacote desenhado, mas o cursor passa por elas) e o atraso medido na gravacao 143802 (~5 quadros).
// modo 0 = clicando num pacote; modo 1 = so passando o mouse (o cursor tem que terminar na casa, sem X)
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed);
  const int C=12, R=4; int lag=4+rng()%3; int sel=rng()%(C*R), target; do target=rng()%(C*R); while(target==sel);
  std::vector<int> hist; uint32_t prev=0; int xs=0, fx=-1, fchega=-1;
  bool vazia[C*R]; for(int i=0;i<C*R;i++) vazia[i]=rng()%3==0;
  int clickf=30+rng()%40, hold=2+rng()%15;
  for(int f=1;f<500;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    mui_prim(0,0,0,64,256,0,0,64,256,9,0);                    // fundo
    for(int r=0;r<R;r++) for(int c=0;c<C;c++){ int x=86+22*c, y=5+47*r;
      mui_prim(0,x+9,y-1,x+11,y+7,131,144,133,152,7,0);       // divisor 2x8
      if(!vazia[r*C+c]){ mui_prim(0,x,y,x+20,y+40,0,0,20,40,8,0); mui_prim(0,x,y,x+24,y+44,50,164,74,208,7,0); } }
    { int c=ds%C, r=ds/C, x=86+22*c, y=5+47*r, p=(f/4)%5-2;   // moldura de 4 cantos pulsando, centro fixo
      int l=x-3-p, t=y-3-p, rr=x+23+p, b=y+43+p;
      mui_prim(0,l,t,l+12,t+12,0,214,12,226,7,0); mui_prim(0,rr-12,t,rr,t+12,12,214,0,226,7,0);
      mui_prim(0,l,b-12,l+12,b,0,226,12,214,7,0); mui_prim(0,rr-12,b-12,rr,b,12,226,0,214,7,0); }
    mui_set_frame(0);
    int tx=86+22*(target%C)+11, ty=5+47*(target/C)+20;
    MuiMouse m{}; m.in_client=m.inside=true;
    if(f<clickf){ int k=(f/7)%(C*R); m.mx=modo==1?86+22*(k%C)+11:10; m.my=modo==1?5+47*(k/C)+20:250; } else { m.mx=tx+(f%3)-1; m.my=ty; }
    if(modo==0){ m.down[0]=f>=clickf&&f<clickf+hold; m.clicked[0]=f==clickf; }
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; int r=sel/C, c=sel%C, old=sel;
      if(e&0x20 && c<C-1) sel++; if(e&0x80 && c>0) sel--; if(e&0x40 && r<R-1) sel+=C; if(e&0x10 && r>0) sel-=C;
      if(e&0x4000){ xs++; if(sel!=target) printf("X FORA DO ALVO "); if(fx<0) fx=f; }
      if(sel!=old && sel!=target) fchega=-1; if(f>=clickf && sel==target && fchega<0) fchega=f; }
  }
  bool ok = modo==0 ? (xs==1 && sel==target) : (xs==0 && sel==target);
  printf("%d %d modo=%d lag=%d alvo=%d final=%d X=%d", ok, seed, modo, lag, target, sel, xs);
  if(getenv("TF_TEMPO")) printf(" tempo=%d", modo==0 ? (fx<0?-1:fx-clickf) : (fchega<0?-1:fchega-clickf));
  printf("\n");
}
