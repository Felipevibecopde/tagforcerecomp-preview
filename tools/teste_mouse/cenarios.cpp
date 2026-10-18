// Simulador: um "jogo" PSP com uma lista vertical de barras, lido pelo modulo do mouse do mesmo jeito que no runtime.
#include "mouse_ui.h"
#include <cstdio>
#include <vector>
#include <functional>
#include <string>
#include <cstdlib>
struct Ev { bool inside=false; int mx=0,my=0; bool ldown=false; bool lclick=false; };
struct Result { std::vector<std::pair<int,int>> confirms; /* (item, tela) */ int final_sel; int moves; };
struct Game; static int bar_y(int i){ return 50+i*30; }
static int center_y(int i){ return bar_y(i)+10; }
struct Game {
  int n=5, sel=0, screen=0, lag=2; int closing=0; static int close_anim; std::vector<int> hist; uint32_t prevpoll=0; int moves=0;
  std::function<bool(int)> blink=[](int){return false;};
  Result r;
  void draw(int f){
    hist.push_back(sel);
    int ds = hist.size()>(size_t)lag ? hist[hist.size()-1-lag] : hist[0];
    uint32_t fb=0x04000000;
    if(closing>0) closing--;
    if(screen==0 || closing>0) for(int i=0;i<n;i++){
      uint32_t tex = (i==ds && !blink(f)) ? 2 : 1; int y=bar_y(i);
      mui_prim(fb,100,y,116,y+20,0,0,16,20,tex,0xffffffff);
      mui_prim(fb,116,y,284,y+20,16,0,32,20,tex,0xffffffff);
      mui_prim(fb,284,y,300,y+20,32,0,48,20,tex,0xffffffff);
    }
    mui_set_frame(fb);
  }
  void input(int f,uint32_t b){
    if(f%2) return;            // o jogo le o controle a cada 2 quadros
    uint32_t e=b&~prevpoll; prevpoll=b;
    if(screen==0){
      if(e&0x10 && sel>0){sel--;moves++;} if(e&0x40 && sel<n-1){sel++;moves++;}
      if(e&0x4000){ r.confirms.push_back({sel,screen}); screen=1; closing=close_anim; }
    } else if(e&0x4000) r.confirms.push_back({-1,screen});
  }
};
int Game::close_anim=0;
static Result run(std::function<Ev(int)> script,int frames=300,std::function<bool(int)> blink=nullptr,bool menus=true){
  Game g; if(blink) g.blink=blink;
    uint32_t b=0;
  for(int f=1;f<frames;f++){
    g.draw(f);
    Ev e=script(f);
    MuiMouse m{}; m.in_client=true; m.inside=e.inside; m.mx=e.mx; m.my=e.my; m.down[MUI_BL]=e.ldown; m.clicked[MUI_BL]=e.lclick;
    b=mui_mouse(f,m,menus,0);
    g.input(f,b);
  }
  g.r.final_sel=g.sel; g.r.moves=g.moves; return g.r;
}
static void show(const char*name,const Result&r,const char*expect){
  std::string s; for(auto&c:r.confirms){ char t[40]; snprintf(t,sizeof t,"%s%d@tela%d",s.empty()?"":", ",c.first,c.second); s+=t; }
  printf("%-58s confirmou: [%s]  (esperado: %s)\n",name,s.c_str(),expect);
}
int main(int argc,char**argv){ int only=argc>1?atoi(argv[1]):0; int k=0;
#define SC if(++k==only)
  // S1: passa o mouse no item 3, espera, clique normal (6 quadros)
  SC show("S1 hover no 3 e clique normal",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=center_y(3); e.ldown=(f>=80&&f<86); e.lclick=(f==80); return e;}),"3 uma vez");
  // S2: clique direto no 4 segurando ~250 ms (15 quadros) sem hover antes
  SC show("S2 clique direto no 4 segurando 250 ms",run([](int f){Ev e; e.inside=f>=40; e.mx=200; e.my=center_y(4); e.ldown=(f>=40&&f<55); e.lclick=(f==40); return e;}),"4 uma vez");
  // S3: clique segurado 400 ms no item ja selecionado
  SC show("S3 hover no 2, clique segurado 400 ms",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=center_y(2); e.ldown=(f>=80&&f<104); e.lclick=(f==80); return e;}),"2 uma vez");
  // S4: clica no 4 e o mouse escorrega para o 1 logo depois
  SC show("S4 clica no 4, mouse escorrega para o 1",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=center_y(f<44?4:1); e.ldown=(f>=40&&f<44); e.lclick=(f==40); return e;}),"4 uma vez");
  // S5: clique muito curto (1 quadro, entre leituras do jogo) sem menus (so X)
  SC show("S5 clique de 1 quadro, mouse nos menus desligado",run([](int f){Ev e; e.ldown=(f==41); e.lclick=(f==41); return e;},300,nullptr,false),"1 X (item 0)");
  // S6: clique em quadro impar com botao ja solto na leitura (clique so visto pela mensagem)
  SC show("S6 clique inteiro entre leituras (so a mensagem viu)",run([](int f){Ev e; e.lclick=(f==41); return e;},300,nullptr,false),"1 X (item 0)");
  // S7: realce pisca (some 2 de cada 8 quadros) e o clique cai num quadro sem realce
  SC show("S7 realce piscando, clique no 3",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=(f<70?center_y(0):center_y(3)); e.ldown=(f>=70&&f<74); e.lclick=(f==70); return e;},300,[](int f){return (f%8)>=6 || (f>=66&&f<=72);}),"3 uma vez");
  // S8: mouse varrendo a lista de cima a baixo devagar, mexendo 1 px por quadro dentro das barras (sem clique)
  SC show("S4b vai ao 4 so no clique e escorrega para o 1",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=center_y(f<40?0:(f<44?4:1)); e.ldown=(f>=40&&f<44); e.lclick=(f==40); return e;}),"4 uma vez");
  SC { Game::close_anim=10; show("S9 clique segurado 300 ms, menu fecha com animacao",run([](int f){Ev e; e.inside=true; e.mx=200; e.my=center_y(2); e.ldown=(f>=80&&f<98); e.lclick=(f==80); return e;}),"2 uma vez (sem X extra na tela seguinte)"); }
  SC { Result r=run([](int f){Ev e; e.inside=true; e.mx=200+(f%3); e.my=(f<30?center_y(0):center_y(4)); return e;},200);
    printf("%-58s selecao final %d, %d passos dados pelo jogo (esperado: 4, 4 passos)\n","S10 pula para o 4 e o mouse treme dentro da barra",r.final_sel,r.moves); }
  SC { Result r=run([](int f){Ev e; e.inside=true; e.mx=150+(f%40); e.my=50+f/2; if(e.my>190)e.my=190; return e;},400);
    printf("%-58s selecao final %d, %d passos dados pelo jogo (esperado: 4, 4 passos)\n","S8 varredura lenta com micro-movimentos",r.final_sel,r.moves); }
  return 0;
}
