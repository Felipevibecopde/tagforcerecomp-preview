// Simula o campo do duelo com a geometria medida (F4): monstros do oponente, seus monstros, suas magicas/armadilhas e a mao.
// Modelo do jogo: cima/baixo vai para a fileira vizinha que tem carta, na carta de x mais proximo; esquerda/direita anda na fileira.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>
struct C { int row, x, y, w, h, px, py; };   // px,py = deslocamento da maozinha
int main(int argc,char**argv){
  int seed=atoi(argv[1]); std::mt19937 rng(seed); int lag=3+rng()%3;
  std::vector<C> cs;
  int zx[5]={183,226,270,314,357};
  auto zone=[&](int row,int z){ if(row==0) cs.push_back({0,zx[z]-17+0,102,33,15,17,-3}); if(row==1) cs.push_back({1,zx[z]-14,143,28,27,14,2}); if(row==2) cs.push_back({2,zx[z]-15,176,30,32,15,5}); };
  for(int r=0;r<3;r++) for(int z=0;z<5;z++) if(rng()%2) zone(r,z);
  int nh=3+rng()%4, hx0=240-nh*47/2; for(int i=0;i<nh;i++) cs.push_back({3,hx0+47*i,219,45,64,17,-13});
  int n=cs.size();
  auto rowcards=[&](int r){ std::vector<int> v; for(int i=0;i<n;i++) if(cs[i].row==r) v.push_back(i); std::sort(v.begin(),v.end(),[&](int a,int b){return cs[a].x<cs[b].x;}); return v; };
  int sel; { auto h=rowcards(3); sel=h[rng()%h.size()]; }
  int target=rng()%n; int clickf=40+rng()%30;
  std::vector<int> hist; uint32_t prev=0; int xs=0, moves=0; bool wrongx=false;
  for(int f=1;f<700;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<n;i++){ const C&c=cs[i]; int y=c.y-(c.row==3&&i==ds?13:0);
      mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x98b4e40); mui_quad(0,c.x,y,c.x+c.w,y+c.h,0x9d00000+i*0x800); }
    mui_quad(0,281,125,313,141,0x99f08c0); mui_quad(0,281,125,313,141,0x99f2080);   // enfeite da mesa (nao e carta)
    mui_quad(0,280,36,301,65,0x98b4e40);                                            // mao do oponente (nao clicavel)
    { const C&c=cs[ds]; int y=c.y-(c.row==3?13:0); mui_prim(0,c.x+c.px,y+c.py+(c.row==3?13:0),c.x+c.px+32,y+c.py+(c.row==3?13:0)+32,64,96,96,128,0x9925340,0); }
    mui_set_frame(0);
    const C&t=cs[target]; int mx=t.x+t.w/2, my=(t.row==3? 250 : t.y+t.h/2);
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=f<clickf?5:mx; m.my=f<clickf?5:my; m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf;
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b; int old=sel;
      if(e&0xa0){ auto v=rowcards(cs[sel].row); int k=std::find(v.begin(),v.end(),sel)-v.begin(); if(e&0x20&&k+1<(int)v.size()) sel=v[k+1]; if(e&0x80&&k>0) sel=v[k-1]; }
      if(e&0x50){ int d=(e&0x40)?1:-1; for(int r=cs[sel].row+d;r>=0&&r<=3;r+=d){ auto v=rowcards(r); if(v.empty()) continue; int bi=v[0]; for(int i:v) if(std::abs(cs[i].x+cs[i].w/2-(cs[sel].x+cs[sel].w/2))<std::abs(cs[bi].x+cs[bi].w/2-(cs[sel].x+cs[sel].w/2))) bi=i; sel=bi; break; } }
      if(sel!=old) moves++;
      if(e&0x4000){ xs++; if(sel!=target) wrongx=true; }
    }
  }
  bool ok = xs==1 && sel==target && !wrongx;
  printf("%d %d cartas=%d linha_alvo=%d alvo=%d final=%d X=%d passos=%d\n",ok,seed,n,cs[target].row,target,sel,xs,moves);
}
