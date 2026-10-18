// Simula o Deck Edit (grade 11x4, 42 cartas) e a mao do duelo (4..6 cartas) com a geometria e o atraso medidos nas gravacoes F4.
// modo 0 = Deck Edit, 1 = mao do duelo, 2 = mao com janelinha aberta (o direcional mexe a janela, nao o cursor; X confirma a janela)
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed);
  int lag=3+rng()%4;
  int ncards = modo==0 ? 30+rng()%13 : 4+rng()%3;   // Deck Edit: 30..42 cartas; mao: 4..6
  int cols = modo==0 ? 11 : ncards;
  int sel=rng()%ncards; std::vector<int> hist; uint32_t prev=0;
  int target; do target=rng()%ncards; while(modo==2 && target==sel);
  int clickf=40+rng()%40, hold=2+rng()%15;
  int xs=0, popup_moves=0, cursor_moves=0, at_click=-1;
  for(int f=1;f<520;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    if(modo==0){
      for(int i=0;i<ncards;i++){ int x=156+28*(i%11), y=46+34*(i/11); mui_prim(0,x,y,x+22,y+30,0,0,22,30,0x9a00000+(i%5)*0x40000,0); mui_prim(0,x+1,y+1,x+23,y+31,0,0,22,30,0x9a00000+(i%5)*0x40000,0); }
      int x=156+28*(ds%11)-4, y=46+34*(ds/11)-4, X=x+33, Y=y+41; uint32_t T=0x9cb2de0;
      mui_prim(0,x,y,x+6,y+6,213,122,219,128,T,0); mui_prim(0,X-6,y,X,y+6,250,122,256,128,T,0); mui_prim(0,x,Y-6,x+6,Y,213,159,219,165,T,0); mui_prim(0,X-6,Y-6,X,Y,250,159,256,165,T,0);
      mui_prim(0,x,y+6,x+6,Y-6,213,128,219,129,T,0); mui_prim(0,X-6,y+6,X,Y-6,250,128,256,129,T,0); mui_prim(0,x+6,y,X-6,y+6,219,122,220,128,T,0); mui_prim(0,x+6,Y-6,X-6,Y,219,159,220,165,T,0);
    } else {
      int x0=240-ncards*47/2;
      for(int i=0;i<ncards;i++){ int x=x0+47*i, y=(i==ds)?206:219; mui_quad(0,x,y,x+45,y+64,0x98b4e40); mui_quad(0,x,y,x+45,y+64,0x98b4e40); }
      mui_prim(0,x0+47*ds+17,193,x0+47*ds+49,225,0,0,32,32,0x9925340,0);   // maozinha
    }
    mui_set_frame(0);
    // mouse: em cima do alvo a partir do clique
    int tx,ty; if(modo==0){ tx=156+28*(target%11)+11; ty=46+34*(target/11)+15; } else { int x0=240-ncards*47/2; tx=x0+47*target+22; ty=245; }
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=f<clickf?10:tx; m.my=f<clickf?10:ty; m.down[0]=f>=clickf&&f<clickf+hold; m.clicked[0]=f==clickf;
    uint32_t b=mui_mouse(f,m,true,0);
    if(f==clickf) at_click=sel;
    if(f%2==0){ uint32_t e=b&~prev; prev=b;
      if(modo==2){ if(e&0xf0) popup_moves++; if(e&0x4000) xs++; }
      else {
        int r=sel/cols, c=sel%cols, old=sel;
        if(e&0x20 && c<cols-1 && sel+1<ncards) sel++;
        if(e&0x80 && c>0) sel--;
        if(e&0x40 && modo==0 && r<3){ sel=std::min(ncards-1, sel+11); }
        if(e&0x10 && modo==0 && r>0) sel-=11;
        if(sel!=old) cursor_moves++;
        if(e&0x4000){ xs++; if(sel!=target) printf("X FORA DO ALVO "); }
      }
    }
  }
  bool ok;
  if(modo==2) ok = xs==0;   // janelinha: nada de X, e desiste logo
  else ok = xs==1 && sel==target;
  printf("%d %d modo=%d cartas=%d lag=%d de=%d alvo=%d final=%d X=%d passos=%d janela=%d\n",ok,seed,modo,ncards,lag,at_click,target,sel,xs,cursor_moves,popup_moves);
}
