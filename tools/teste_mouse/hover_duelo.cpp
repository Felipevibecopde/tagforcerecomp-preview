// Passar o mouse no duelo. Modelo do campo igual ao campo.cpp (geometria medida).
// Roteiro: 1) mouse passeia pelas cartas e para numa: a maozinha tem que ir ate ela, sem nenhum X.
//          2) jogador aperta X no teclado: abre janelinha (o direcional passa a mexer a janela). Mouse passeia: NENHUM toque deve chegar a janela.
//          3) jogador aperta O: janela fecha. Mouse para numa carta: a maozinha tem que ir ate ela de novo.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>
struct C { int row, x, y, w, h, px, py; };
int main(int argc,char**argv){
  int seed=atoi(argv[1]); std::mt19937 rng(seed); int lag=3+rng()%3;
  std::vector<C> cs; int zx[5]={183,226,270,314,357};
  for(int r=0;r<3;r++) for(int z=0;z<5;z++) if(rng()%2){ if(r==0) cs.push_back({0,zx[z]-17,102,33,15,17,-3}); if(r==1) cs.push_back({1,zx[z]-14,143,28,27,14,2}); if(r==2) cs.push_back({2,zx[z]-15,176,30,32,15,5}); }
  int nh=3+rng()%4, hx0=240-nh*47/2; for(int i=0;i<nh;i++) cs.push_back({3,hx0+47*i,219,45,64,17,-13});
  int n=cs.size();
  auto rowcards=[&](int r){ std::vector<int> v; for(int i=0;i<n;i++) if(cs[i].row==r) v.push_back(i); std::sort(v.begin(),v.end(),[&](int a,int b){return cs[a].x<cs[b].x;}); return v; };
  int sel; { auto h=rowcards(3); sel=h[rng()%h.size()]; }
  int t1=rng()%n, t3=rng()%n;
  std::vector<int> hist; uint32_t prev=0; int xs=0, popup_moves=0; bool popup=false; int sel_at_1=-1;
  auto mouse_on=[&](int i,int&mx,int&my){ const C&c=cs[i]; mx=c.x+c.w/2; my=(c.row==3?250:c.y+c.h/2); };
  for(int f=1;f<900;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<n;i++){ const C&c=cs[i]; int y=c.y-(c.row==3&&i==ds?13:0); mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x98b4e40); mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x9d00000+i*0x800); }
    { const C&c=cs[ds]; mui_prim(0,c.x+c.px,c.y+c.py,c.x+c.px+32,c.y+c.py+32,64,96,96,128,0x9925340,0); }
    mui_set_frame(0);
    int mx=5,my=5, kb=0;
    if(f<200){ if(f<150){ int k=(f/9)%n; mouse_on(k,mx,my); mx+= (f%3)-1; } else mouse_on(t1,mx,my); }
    else if(f==260) kb=0x4000;                                  // X no teclado: abre a janelinha
    else if(f>270 && f<500){ int k=(f/7)%n; mouse_on(k,mx,my); mx+=(f%5)-2; }   // passeia com a janela aberta
    else if(f==520) kb=0x2000;                                  // O: fecha
    else if(f>540){ mouse_on(t3,mx,my); if(f<560) mx+=f%4; }
    if(f==200) sel_at_1=sel;
    MuiMouse m{}; m.in_client=true; m.inside=mx>20; m.mx=mx; m.my=my;
    uint32_t b=mui_mouse(f,m,true,kb) | kb;
    if(f%2==0){ uint32_t e=b&~prev; prev=b;
      if(e&0x4000){ xs++; if(f>=255&&f<=265) popup=true; }
      if(e&0x2000) popup=false;
      if(popup){ if(e&0xf0) popup_moves++; }
      else {
        if(e&0xa0){ auto v=rowcards(cs[sel].row); int k=std::find(v.begin(),v.end(),sel)-v.begin(); if(e&0x20&&k+1<(int)v.size()) sel=v[k+1]; if(e&0x80&&k>0) sel=v[k-1]; }
        if(e&0x50){ int d=(e&0x40)?1:-1; for(int r=cs[sel].row+d;r>=0&&r<=3;r+=d){ auto v=rowcards(r); if(v.empty()) continue; int bi=v[0]; for(int i:v) if(std::abs(cs[i].x+cs[i].w/2-(cs[sel].x+cs[sel].w/2))<std::abs(cs[bi].x+cs[bi].w/2-(cs[sel].x+cs[sel].w/2))) bi=i; sel=bi; break; } }
      }
    }
  }
  bool ok = sel_at_1==t1 && sel==t3 && popup_moves==0 && xs==1;
  printf("%d %d cartas=%d alvo1=%d chegou1=%d alvo3=%d final=%d toques_na_janela=%d X=%d\n",ok,seed,n,t1,sel_at_1,t3,sel,popup_moves,xs);
}
