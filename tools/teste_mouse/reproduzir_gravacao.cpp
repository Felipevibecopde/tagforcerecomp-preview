// Passa uma gravacao F4 de verdade pelo modulo e imprime o que ele detecta (cartas, cursor) a cada mudanca.
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
  std::string prev; int n=0, ni=0, ns=0;
  for(auto &f:frames){
    for(auto &l:f.second){ if(l.t=='S') mui_prim(0,l.a[0],l.a[1],l.a[2],l.a[3],l.uv[0],l.uv[1],l.uv[2],l.uv[3],l.tex,l.col); else if(l.t=='R') mui_run(0,l.a[0],l.a[1],l.a[2],l.a[3]); else mui_quad(0,l.a[0],l.a[1],l.a[2],l.a[3],l.tex); }
    mui_set_frame(0); n++; if(!g_items.empty()) ni++; if(g_isel>=0) ns++;
    char b[200]; snprintf(b,sizeof b,"modo=%d cartas=%zu cursor=%d",g_imode,g_items.size(),g_isel); std::string k=b;
    if(g_isel>=0){ snprintf(b,sizeof b," col=%d linha=%d x=%d",g_items[g_isel].col,g_items[g_isel].row,g_items[g_isel].x0); k+=b; }
    if(argc>2){ printf("%llu %d %d %d %d\n",(unsigned long long)f.first,g_imode,(int)g_items.size(),g_isel>=0?g_items[g_isel].x0:-1,g_isel>=0?g_items[g_isel].y0:-1); } else if(k!=prev){ printf("%llu %s\n",(unsigned long long)f.first,k.c_str()); prev=k; }
  }
  printf("# %d quadros, %d com cartas, %d com cursor\n",n,ni,ns);
}
