// Campo do duelo com ZONAS (geometria medida nas gravacoes F4): suas 5 zonas de monstro e 5 de magia/armadilha sao sempre navegaveis
// (vazias ou nao), e as 10 do oponente tambem. Modo 0 = clicar; modo 1 = passar o mouse; modo 2 = ESCOLHER ALVO DE ATAQUE: o cursor so anda
// entre os monstros do oponente; modo 3 = INICIO DO TURNO: a maozinha comeca no deck (x=448, y=175) e o mouse para numa carta/zona; (esquerda/direita), o mouse passeia por zonas vazias e para num monstro (tem que chegar, sem travar, sem X).
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>
struct S { int row, px, py; bool card; int x, y, w, h; };   // px,py = maozinha
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed); int lag=3+rng()%3;
  std::vector<S> sl;
  int ox[5]={236,275,314,352,391}, ost[5]={242,278,314,349,385}, mxz[5]={226,270,314,358,401}, sx[5]={218,266,314,361,409};
  for(int z=0;z<5;z++){ bool c=rng()%3==0; sl.push_back({-1,ost[z],75,c,ost[z]-11,77,23,19}); }
  for(int z=0;z<5;z++){ bool c=modo==2? (z%2==0 || rng()%2) : rng()%3==0; sl.push_back({0,ox[z],99,c,ox[z]-17,102,33,15}); }
  for(int z=0;z<5;z++){ bool c=rng()%3==0; sl.push_back({1,mxz[z],145,c,mxz[z]-14,143,28,27}); }
  for(int z=0;z<5;z++){ bool c=rng()%3==0; sl.push_back({2,sx[z],181,c,sx[z]-15,176,30,32}); }
  int deck=-1; if(modo==3){ deck=sl.size(); sl.push_back({2,448,175,false,0,0,0,0}); }
  int nh=2+rng()%5, hx0=240-nh*47/2; for(int i=0;i<nh;i++) sl.push_back({3,hx0+47*i+17,193,true,hx0+47*i,219,45,64});
  int n=sl.size();
  auto row=[&](int r){ std::vector<int> v; for(int i=0;i<n;i++) if(sl[i].row==r) v.push_back(i); return v; };
  std::vector<int> oppm; for(int i=0;i<n;i++) if(sl[i].row==0&&sl[i].card) oppm.push_back(i);
  int sel; if(modo==2) sel=oppm[rng()%oppm.size()]; else if(modo==3) sel=deck; else { auto h=row(3); sel=h[rng()%h.size()]; }
  int target= modo==2 ? oppm[rng()%oppm.size()] : (int)(rng()%n); if(modo==3) while(target==deck) target=rng()%n; int clickf=60+rng()%30;
  std::vector<int> hist; uint32_t prev=0; int xs=0, moves=0;
  for(int f=1;f<900;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<n;i++){ const S&c=sl[i]; if(!c.card) continue; int y=c.y-(c.row==3&&i==ds?13:0); mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x98b4e40); mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x9d00000+i*0x800); }
    mui_quad(0,281,125,313,141,0x99f08c0); mui_quad(0,205,123,461,140,0x99efb90);   // enfeites da mesa
    mui_prim(0,sl[ds].px,sl[ds].py,sl[ds].px+32,sl[ds].py+32,64,96,96,128,0x9925340,0);
    mui_set_frame(0);
    const S&t=sl[target]; int mx=t.px, my=(t.row==3?250:t.py+(t.row==1?8:10));
    if((modo==1||modo==2) && f<clickf){ int k=(f/11)%n; if(modo==2){ k=(f/11)%5; k=5+k; } mx=sl[k].px; my=(sl[k].row==3?250:sl[k].py+8); }
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=(modo==0&&f<clickf)?5:mx; m.my=(modo==0&&f<clickf)?5:my;
    if(modo==0){ m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf; }
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; int old=sel;
      if(modo==2){ if(e&0xa0){ int k=std::find(oppm.begin(),oppm.end(),sel)-oppm.begin(); if(e&0x20&&k+1<(int)oppm.size()) sel=oppm[k+1]; if(e&0x80&&k>0) sel=oppm[k-1]; } }
      else if(e&0xa0){ auto v=row(sl[sel].row); int k=std::find(v.begin(),v.end(),sel)-v.begin(); if(e&0x20&&k+1<(int)v.size()) sel=v[k+1]; if(e&0x80&&k>0) sel=v[k-1]; }
      if(modo!=2 && (e&0x50)){ int d=(e&0x40)?1:-1; for(int r=sl[sel].row+d;r>=-1&&r<=3;r+=d){ auto v=row(r); if(v.empty()) continue; int bi=v[0]; for(int i:v) if(std::abs(sl[i].px-sl[sel].px)<std::abs(sl[bi].px-sl[sel].px)) bi=i; sel=bi; break; } }
      if(sel!=old) moves++;
      if(e&0x4000) xs++;
    }
  }
  bool ok = sel==target && xs==(modo==0?1:0);   // modo 3: comeca no deck (inicio do turno) e so passa o mouse
  printf("%d %d modo=%d slots=%d linha_alvo=%d alvo=%d vazio=%d final=%d X=%d passos=%d\n",ok,seed,modo,n,sl[target].row,target,(int)!sl[target].card,sel,xs,moves);
}
