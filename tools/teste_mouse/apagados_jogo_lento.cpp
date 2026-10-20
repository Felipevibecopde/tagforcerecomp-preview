#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
// Menu com itens APAGADOS (o jogo os pula no direcional), como o menu principal do Tag Force (Free Duel / Network Mode).
// modo 0: mouse vagueia e clica num item habilitado (deve confirmar ele, uma vez)
// modo 1: so passa o mouse e para num item habilitado (a selecao deve parar nele, sem ficar pulando)
int main(int argc,char**argv){
  int seed=atoi(argv[1]); int modo=argc>2?atoi(argv[2]):0; std::mt19937 rng(seed);
  int n=4+rng()%6, lag=5+rng()%3, close_anim=rng()%12;
  std::vector<bool> dis(n,false); int nd=rng()%3; for(int k=0;k<nd;k++) dis[1+rng()%(n-2)]=true;
  auto pick=[&](){ int i; do i=rng()%n; while(dis[i]); return i; };
  int sel=pick(), screen=0, closing=0; std::vector<int> hist; uint32_t prev=0, b=0;
  int wander_until=60+rng()%80, clickf=wander_until, hold=1+rng()%25; int target=pick();
  int cur=rng()%n; std::vector<int> conf; int moves=0;
  bool wrap=rng()%2; auto step=[&](int d){ int i=sel; for(int k=0;k<n;k++){ i+=d; if(wrap) i=(i+n)%n; if(i<0||i>=n) return; if(!dis[i]) break; } sel=i; moves++; };
  for(int f=1;f<40;f++){ for(int i=0;i<3;i++){ uint32_t tex=i==0?2:1; int y=150+i*26; mui_prim(0,140,y,156,y+20,0,0,16,20,tex,0); mui_prim(0,156,y,324,y+20,16,0,32,20,tex,0); mui_prim(0,324,y,340,y+20,32,0,48,20,tex,0);} mui_set_frame(0); MuiMouse m{}; mui_mouse(f,m,true,0); }
  for(int f=40;f<540;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    if(closing>0)closing--;
    if(screen==0||closing>0) for(int i=0;i<n;i++){ uint32_t tex=dis[i]?3:(i==ds?2:1); int y=40+i*26;
      mui_prim(0,100,y,116,y+20,0,0,16,20,tex,0); mui_prim(0,116,y,284,y+20,16,0,32,20,tex,0); mui_prim(0,284,y,300,y+20,32,0,48,20,tex,0);}
    mui_set_frame(0);
    if(f<wander_until && rng()%6==0) cur=rng()%n;
    int item; if(modo==0) item = f<clickf?cur: (f<clickf+hold+3?target: (int)(rng()%n)); else item = f<wander_until?cur:target;
    int mx=110+rng()%180, my=40+item*26+2+rng()%16;
    bool down= modo==0 && f>=clickf&&f<clickf+hold;
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=mx; m.my=my; m.down[0]=down; m.clicked[0]=down&&f==clickf;
    b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b;
      if(screen==0){ if(e&0x10)step(-1); if(e&0x40)step(1); if(e&0x4000){conf.push_back(sel); screen=1; closing=close_anim;} }
      else if(e&0x4000) conf.push_back(-1); }
  }
  bool ok = modo==0 ? (conf.size()==1 && conf[0]==target) : (conf.empty() && sel==target);
  int ideal=0; printf("%d %d modo=%d n=%d apagados=%d wrap=%d lag=%d conf=",ok,seed,modo,n,nd,(int)wrap,lag); for(int c:conf)printf("%d ",c); printf("sel=%d alvo=%d passos=%d\n",sel,target,moves);
}
