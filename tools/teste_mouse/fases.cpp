// Selecao de fase (geometria medida): 6 icones 20x32 em y=124, passo 40, x=204..404; realce = moldura de 9 pedacos com 5 px de folga.
// Fases indisponiveis (desenhadas com v=32) sao puladas pelo jogo. modo 0: clicar numa fase; modo 1: passar o mouse e parar numa fase.
#include "mouse_ui.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
int main(int argc,char**argv){
  int seed=atoi(argv[1]), modo=atoi(argv[2]); std::mt19937 rng(seed); int lag=2+rng()%4;
  bool dis[6]; int sel=2+rng()%2; for(int i=0;i<6;i++) dis[i]= i!=sel && rng()%3==0;
  int target; do target=rng()%6; while(dis[target]);
  std::vector<int> hist; uint32_t prev=0; int xs=0; int clickf=40+rng()%30;
  for(int f=1;f<500;f++){
    hist.push_back(sel); int ds=hist.size()>(size_t)lag?hist[hist.size()-1-lag]:hist[0];
    for(int i=0;i<6;i++){ int x=204+40*i; float v=dis[i]?32:0; mui_prim(0,x,124,x+20,156,i*20.f,v,i*20.f+20,v+32,0x99ba7c0,0xffffffff); if(i<5) mui_prim(0,x+27,135,x+34,146,120,32,127,43,0x99ba7c0,0xffffffff); }
    int hx=204+40*ds-5; uint32_t T=0x88951f0, C=0xa70080ff;
    mui_prim(0,hx,120,hx+4,124,0,112,4,116,T,C); mui_prim(0,hx+4,120,hx+26,124,4,112,8,116,T,C); mui_prim(0,hx+26,120,hx+30,124,8,112,12,116,T,C);
    mui_prim(0,hx,124,hx+4,156,0,116,4,120,T,C); mui_prim(0,hx+4,124,hx+26,156,4,116,8,120,T,C); mui_prim(0,hx+26,124,hx+30,156,8,116,12,120,T,C);
    mui_prim(0,hx,156,hx+4,160,0,120,4,124,T,C); mui_prim(0,hx+4,156,hx+26,160,4,120,8,124,T,C); mui_prim(0,hx+26,156,hx+30,160,8,120,12,124,T,C);
    mui_quad(0,267,219,312,283,0x98b4e40); mui_quad(0,314,219,360,283,0x98b4e40);   // cartas da mao ao fundo
    mui_set_frame(0);
    int mx=204+40*target+10, my=140;
    if(modo==1 && f<clickf){ int k=(f/9)%6; mx=204+40*k+10; }
    MuiMouse m{}; m.in_client=m.inside=true; m.mx=(modo==0&&f<clickf)?5:mx; m.my=(modo==0&&f<clickf)?5:my;
    if(modo==0){ m.down[0]=f>=clickf&&f<clickf+4; m.clicked[0]=f==clickf; }
    uint32_t b=mui_mouse(f,m,true,0);
    if(f%2==0){ uint32_t e=b&~prev; prev=b;
      if(e&0x20){ int i=sel+1; while(i<6&&dis[i]) i++; if(i<6) sel=i; }
      if(e&0x80){ int i=sel-1; while(i>=0&&dis[i]) i--; if(i>=0) sel=i; }
      if(e&0x4000){ xs++; if(sel!=target) printf("X FORA "); }
    }
  }
  bool ok= sel==target && xs==(modo==0?1:0);
  printf("%d %d modo=%d alvo=%d final=%d X=%d\n",ok,seed,modo,target,sel,xs);
}
