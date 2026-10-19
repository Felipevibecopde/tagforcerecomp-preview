// imprime, por quadro: lista de texto/barras (tamanho, selecionada) e modo das cartas
#include "mouse_ui.cpp"
#include <fstream>
#include <sstream>
struct L { char t; int a[4]; float uv[4]; uint32_t tex, col; };
int main(int argc,char**argv){
  std::ifstream in(argv[1]); std::string line;
  std::vector<std::pair<uint64_t,std::vector<L>>> frames; std::vector<L> last;
  while(std::getline(in,line)){
    if(line.empty()||line[0]=='#') continue;
    std::istringstream ss(line); char t; ss>>t;
    if(t=='F'){ uint64_t fr; std::string a; ss>>fr>>a; if(a=="same") frames.push_back({fr,last}); else frames.push_back({fr,{}}); continue; }
    if(frames.empty()) continue;
    L l{}; l.t=t; std::string tx,cl;
    if(t=='S'){ ss>>l.a[0]>>l.a[1]>>l.a[2]>>l.a[3]>>l.uv[0]>>l.uv[1]>>l.uv[2]>>l.uv[3]>>tx>>cl; l.tex=strtoul(tx.c_str(),0,16); l.col=strtoul(cl.c_str(),0,16); }
    else if(t=='R'){ ss>>l.a[0]>>l.a[1]>>l.a[2]>>l.a[3]; }
    else if(t=='Q'){ ss>>l.a[0]>>l.a[1]>>l.a[2]>>l.a[3]>>tx; l.tex=strtoul(tx.c_str(),0,16); }
    else continue;
    frames.back().second.push_back(l); last=frames.back().second;
  }
  std::string prev; int nl=0, ni=0;
  for(auto &f:frames){
    for(auto &l:f.second){ if(l.t=='S') mui_prim(0,l.a[0],l.a[1],l.a[2],l.a[3],l.uv[0],l.uv[1],l.uv[2],l.uv[3],l.tex,l.col); else if(l.t=='R') mui_run(0,l.a[0],l.a[1],l.a[2],l.a[3]); else mui_quad(0,l.a[0],l.a[1],l.a[2],l.a[3],l.tex); }
    mui_set_frame(0);
    if(!g_list.empty()) nl++; if(!g_items.empty()) ni++;
    char b[300]; snprintf(b,sizeof b,"lista=%zu sel=%d%s | cartas=%zu cursor=%d", g_list.size(), g_sel, g_list.empty()?"":(" ["+std::to_string(g_list[0].x0)+","+std::to_string(g_list[0].y0)+" "+std::to_string(g_list[0].x1-g_list[0].x0)+"x"+std::to_string(g_list[0].y1-g_list[0].y0)+"]").c_str(), g_items.size(), g_isel);
    std::string k=b; if(k!=prev){ printf("%llu %s\n",(unsigned long long)f.first,k.c_str()); prev=k; }
  }
  printf("# %zu quadros: %d com lista, %d com cartas\n",frames.size(),nl,ni);
}
