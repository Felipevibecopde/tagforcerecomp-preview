#include "mouse_ui.h"
#include "hle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <algorithm>

namespace {
struct Spr { int x0, y0, x1, y1; float u0, v0, u1, v1; uint32_t tex, col; };
struct Run { int x0, y0, x1, y1; };   /* varios sprites num PRIM so (uma linha de texto) */
struct Bar { int x0, y0, x1, y1; uint64_t sig; size_t last; };

std::map<uint32_t, std::vector<Spr>> g_cur;   /* sprites desenhados em cada buffer desde que ele foi exibido pela ultima vez */
std::map<uint32_t, std::vector<Run>> g_cur_runs; std::vector<Run> g_runs;   /* linhas de texto (por buffer / do quadro exibido) */
std::vector<Spr> g_snap;                      /* sprites do quadro exibido */
std::vector<Bar> g_list;                      /* barras da lista de menu do quadro exibido (mesmo tamanho, >= 2) */
std::set<uint64_t> g_known;                   /* assinaturas ja vistas como 'selecionada' em listas com 3 ou mais barras (valem tambem para listas de 2) */
std::set<uint64_t> g_normal;                  /* assinaturas ja vistas em 2 ou mais barras de uma mesma lista (estilo 'nao selecionada') */
int g_sel = -1;                               /* indice em g_list da barra selecionada (-1 = nao deu para saber) */
std::vector<bool> g_skip;                     /* barras apagadas (item indisponivel, ex. 'Free Duel' no inicio): o jogo as PULA no direcional */
const bool g_mtrace = getenv("TF_MOUSE_TRACE") != nullptr;

uint64_t mix(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001b3ull; }

void trace_list() {
    if (!g_mtrace) return;
    static std::string last; char buf[4096]; int n = snprintf(buf, sizeof buf, "sel=%d known=%zu:", g_sel, g_known.size());
    for (size_t i = 0; i < g_list.size(); i++) { const Bar &b = g_list[i]; n += snprintf(buf + n, sizeof buf - n, " [%d,%d %dx%d %04x%s]", b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0, (unsigned)(b.sig & 0xffff), i < g_skip.size() && g_skip[i] ? " apagada" : ""); }
    if (last != buf) { last = buf; fprintf(stderr, "[mouse] lista: %s\n", buf); }
}

/* Lista de texto (mapa, menus claros): linhas de texto de mesmo x e passo constante; a selecionada e a que tem um realce (moldura 9 pedacos de uma cor so) atras. */
void analyze_text() {
    std::vector<Run> r;   /* junta o texto e a sombra dele (mesmo lugar, +-2 px) */
    for (const Run &a : g_runs) {
        if (a.y1 - a.y0 < 8 || a.y1 - a.y0 > 40) continue;
        bool merged = false;
        for (Run &b : r) if (std::abs(a.x0 - b.x0) <= 2 && std::abs(a.y0 - b.y0) <= 2) { b.x0 = std::min(b.x0, a.x0); b.y0 = std::min(b.y0, a.y0); b.x1 = std::max(b.x1, a.x1); b.y1 = std::max(b.y1, a.y1); merged = true; break; }
        if (!merged) r.push_back(a);
    }
    std::sort(r.begin(), r.end(), [](const Run &a, const Run &b) { return a.y0 < b.y0; });
    std::vector<Run> best;
    for (size_t i = 0; i < r.size(); i++) {
        std::vector<Run> ch = {r[i]};
        int pitch = 0;
        for (size_t j = i + 1; j < r.size(); j++) {
            if (std::abs(r[j].x0 - r[i].x0) > 2) continue;
            const int d = r[j].y0 - ch.back().y0;
            if (!pitch) { if (d >= r[i].y1 - r[i].y0 && d <= 60) { pitch = d; ch.push_back(r[j]); } }
            else if (std::abs(d - pitch) <= 1) ch.push_back(r[j]);
        }
        if (ch.size() > best.size()) best = ch;
    }
    if (best.size() < 3) return;
    const int pitch = best[1].y0 - best[0].y0;
    int left = best[0].x0, right = best[0].x1;
    for (const Run &b : best) { left = std::min(left, b.x0); right = std::max(right, b.x1); }
    /* realces: componentes de sprites de mesma textura e cor que se tocam */
    std::vector<int> comp(g_snap.size()); for (size_t i = 0; i < comp.size(); i++) comp[i] = (int)i;
    auto find = [&](int x) { while (comp[x] != x) x = comp[x] = comp[comp[x]]; return x; };
    for (size_t i = 0; i < g_snap.size(); i++) for (size_t j = i + 1; j < g_snap.size(); j++) {
        const Spr &a = g_snap[i], &b = g_snap[j];
        if (a.tex == b.tex && a.col == b.col && a.x0 <= b.x1 + 1 && b.x0 <= a.x1 + 1 && a.y0 <= b.y1 + 1 && b.y0 <= a.y1 + 1) comp[find((int)i)] = find((int)j);
    }
    std::map<int, std::tuple<int, int, int, int, int>> bb;   /* raiz -> x0,y0,x1,y1,n */
    for (size_t i = 0; i < g_snap.size(); i++) {
        const Spr &a = g_snap[i]; auto it = bb.find(find((int)i));
        if (it == bb.end()) bb[find((int)i)] = std::make_tuple(a.x0, a.y0, a.x1, a.y1, 1);
        else { auto &t = it->second; std::get<0>(t) = std::min(std::get<0>(t), a.x0); std::get<1>(t) = std::min(std::get<1>(t), a.y0); std::get<2>(t) = std::max(std::get<2>(t), a.x1); std::get<3>(t) = std::max(std::get<3>(t), a.y1); std::get<4>(t)++; }
    }
    int hx0 = 0, hx1 = 0, hn = 0; std::vector<std::pair<int, int>> hl;   /* realces plausiveis (y0,y1) */
    for (auto &kv : bb) {
        const int x0 = std::get<0>(kv.second), y0 = std::get<1>(kv.second), x1 = std::get<2>(kv.second), y1 = std::get<3>(kv.second), n = std::get<4>(kv.second);
        if (n >= 4 && y1 - y0 >= 14 && y1 - y0 <= pitch + 8 && x1 - x0 >= 40 && x1 - x0 <= 420 && x0 <= left + 2 && x1 >= left) { hl.push_back({y0, y1}); hx0 = x0; hx1 = x1; hn++; }
    }
    if (hn) right = std::max(right, hx1);
    const int rx0 = std::max(0, left - 6), rx1 = right + 4, top = best[0].y0 - (pitch - (best[0].y1 - best[0].y0)) / 2;
    for (size_t i = 0; i < best.size(); i++) g_list.push_back({rx0, top + (int)i * pitch, rx1, top + (int)(i + 1) * pitch, 0, 0});
    int n = 0;
    for (size_t i = 0; i < g_list.size(); i++) for (auto &h : hl) {
        const int ov = std::min(h.second, g_list[i].y1) - std::max(h.first, g_list[i].y0);
        if (ov * 10 >= (h.second - h.first) * 6) { g_sel = (int)i; n++; break; }
    }
    if (n != 1) g_sel = -1;
    (void)hx0;
}

/* barras = sequencias contiguas na horizontal de sprites de mesma altura (ponta, meio, ponta) com um meio largo */
/* Barras apagadas: com a selecionada conhecida, o estilo da maioria das outras e o 'normal'; um estilo minoritario (outra textura/cor) e de
 * item indisponivel, que o jogo pula ao apertar o direcional (no menu principal, 'Free Duel' e 'Network Mode' no comeco do jogo). Contar
 * essas barras como passos fazia a rajada passar do alvo e a selecao ficar pulando para cima e para baixo. */
std::set<uint64_t> g_enabled;                 /* estilos de barra que ja receberam o destaque (logo, de item habilitado: o jogo nunca seleciona um apagado) */
std::vector<Bar> g_prev_list;                 /* lista do quadro anterior (para ver qual estilo tinha a barra que acabou de ser selecionada) */
void classify_skip() {
    g_skip.assign(g_list.size(), false);
    if (g_sel >= 0)   /* a barra que agora esta selecionada, no quadro anterior, estava com o estilo de item normal habilitado */
        for (const Bar &b : g_prev_list) if (b.x0 == g_list[g_sel].x0 && b.y0 == g_list[g_sel].y0 && b.sig != g_list[g_sel].sig && b.sig) g_enabled.insert(b.sig);
    g_prev_list = g_list;
    if (g_sel < 0 || g_list.size() < 3) return;
    std::map<uint64_t, int> cnt;
    for (size_t i = 0; i < g_list.size(); i++) if ((int)i != g_sel && g_list[i].sig) cnt[g_list[i].sig]++;
    if (cnt.size() < 2) return;
    bool any_enabled = false; for (auto &c : cnt) any_enabled |= g_enabled.count(c.first) != 0;
    if (any_enabled) {   /* ja se sabe qual estilo e de item habilitado: os outros sao de item apagado */
        for (size_t i = 0; i < g_list.size(); i++) if ((int)i != g_sel && !g_enabled.count(g_list[i].sig)) g_skip[i] = true;
        return;
    }
    uint64_t major = 0; int mc = 0, second = 0;
    for (auto &c : cnt) { if (c.second > mc) { second = mc; mc = c.second; major = c.first; } else if (c.second > second) second = c.second; }
    if (mc == second) return;   /* empate: nao da para dizer qual e o estilo normal */
    for (size_t i = 0; i < g_list.size(); i++) if ((int)i != g_sel && g_list[i].sig != major) g_skip[i] = true;
}

void analyze() {
    g_list.clear(); g_sel = -1; g_skip.clear();
    std::vector<Spr> u;   /* sem repeticoes de retangulo, para achar as cadeias */
    for (const Spr &s : g_snap) {
        const int h = s.y1 - s.y0, w = s.x1 - s.x0;
        if (h < 14 || h > 48 || w < 2) continue;
        bool dup = false; for (const Spr &t : u) if (t.x0 == s.x0 && t.y0 == s.y0 && t.x1 == s.x1 && t.y1 == s.y1) { dup = true; break; }
        if (!dup) u.push_back(s);
    }
    std::sort(u.begin(), u.end(), [](const Spr &a, const Spr &b) { return a.y0 != b.y0 ? a.y0 < b.y0 : a.y1 != b.y1 ? a.y1 < b.y1 : a.x0 < b.x0; });
    std::vector<Bar> bars;
    for (size_t i = 0; i < u.size();) {
        size_t j = i + 1; int right = u[i].x1, widest = u[i].x1 - u[i].x0;
        while (j < u.size() && u[j].y0 == u[i].y0 && u[j].y1 == u[i].y1 && u[j].x0 >= right - 1 && u[j].x0 <= right + 1) { right = u[j].x1; widest = std::max(widest, u[j].x1 - u[j].x0); j++; }
        if (j - i >= 3 && widest >= 40 && right - u[i].x0 >= 80) {
            Bar b = {u[i].x0, u[i].y0, right, u[i].y1, 0xcbf29ce484222325ull, 0};
            for (size_t si = 0; si < g_snap.size(); si++) {   /* assinatura: so os sprites da altura inteira da barra, na ordem do desenho (o selecionado e desenhado de outro jeito) */
                const Spr &s = g_snap[si];
                if (s.y0 == b.y0 && s.y1 == b.y1 && s.x0 >= b.x0 && s.x1 <= b.x1) {
                    b.last = si;
                    b.sig = mix(b.sig, s.tex); b.sig = mix(b.sig, (uint64_t)(s.u0 * 16)); b.sig = mix(b.sig, (uint64_t)(s.v0 * 16)); b.sig = mix(b.sig, (uint64_t)(s.u1 * 16)); b.sig = mix(b.sig, (uint64_t)(s.v1 * 16));
                }
            }
            bars.push_back(b);
        }
        i = j;
    }
    /* a lista = o grupo de barras de mesmo tamanho e mesma coluna desenhado por ultimo (o que esta por cima: um submenu cobre o menu de tras) */
    std::map<std::tuple<int, int, int>, std::vector<int>> grp;
    for (size_t i = 0; i < bars.size(); i++) grp[std::make_tuple(bars[i].x1 - bars[i].x0, bars[i].y1 - bars[i].y0, bars[i].x0)].push_back((int)i);
    const std::vector<int> *best = nullptr; size_t best_last = 0;
    for (auto &g : grp) { size_t l = 0; for (int i : g.second) l = std::max(l, bars[i].last); if (g.second.size() >= 2 && (!best || l > best_last)) { best = &g.second; best_last = l; } }
    if (!best) { analyze_text(); g_skip.assign(g_list.size(), false); trace_list(); return; }
    for (int i : *best) g_list.push_back(bars[i]);
    std::sort(g_list.begin(), g_list.end(), [](const Bar &a, const Bar &b) { return a.y0 < b.y0; });
    /* selecionada: (1) lista de 3+ com so duas assinaturas e uma barra isolada = ela (e as outras sao o estilo 'normal'); (2) uma assinatura ja vista como
     * selecionada; (3) a unica barra cuja assinatura nao e de estilo normal (listas de 2: 'Deck Edit' / 'Recipe Viewer') */
    std::map<uint64_t, int> cnt; for (const Bar &b : g_list) cnt[b.sig]++;
    for (auto &c : cnt) if (c.second >= 2) g_normal.insert(c.first);
    if (cnt.size() == 2 && g_list.size() >= 3) {
        uint64_t odd = 0; int oc = 1 << 30;
        for (auto &c : cnt) if (c.second < oc) { oc = c.second; odd = c.first; }
        if (oc == 1) { for (size_t i = 0; i < g_list.size(); i++) if (g_list[i].sig == odd) g_sel = (int)i; g_known.insert(odd); }
    } else if (cnt.size() >= 2) {
        int n = 0; for (size_t i = 0; i < g_list.size(); i++) if (g_known.count(g_list[i].sig)) { g_sel = (int)i; n++; }
        if (n != 1) {
            n = 0; g_sel = -1;
            for (size_t i = 0; i < g_list.size(); i++) if (!g_normal.count(g_list[i].sig)) { g_sel = (int)i; n++; }
            if (n != 1) g_sel = -1; else g_known.insert(g_list[g_sel].sig);
        }
    }
    classify_skip();
    trace_list();
}

/* estado do controle */
int g_tkx = -1, g_tky = -1;          /* barra alvo (canto superior esquerdo), -1 = nenhuma */
bool g_pending;                       /* clique esperando a selecao chegar na barra */
uint64_t g_pending_until, g_next_tap, g_pulse_end; uint32_t g_pulse; int g_taps;
uint64_t g_sel_seen;                  /* ultimo quadro em que a selecao foi reconhecida (para distinguir 'sumiu agora' de 'nunca aparece nesta tela') */
int g_lmx = -1, g_lmy = -1;
int g_burst_n, g_burst_dir; uint64_t g_burst_start, g_burst_end;
const int kSettle = 12;   /* quadros de espera depois de uma rajada ate confiar no realce desenhado */
const int g_period = std::max(2, getenv("TF_MOUSE_PERIOD") ? atoi(getenv("TF_MOUSE_PERIOD")) : 4);   /* quadros por passo (metade apertado, metade solto) */

bool burst_running(uint64_t frame) { return g_burst_n && frame < g_burst_start + (uint64_t)g_burst_n * g_period; }
/* troca de alvo no meio de uma rajada: NAO corta o aperto atual no meio (um aperto de 1 quadro o jogo pode nao ver, e a contagem de passos
 * fica errada); termina o passo em andamento e para ai. A correcao ate o alvo novo vem depois, a partir da selecao que o jogo desenhar. */
void trim_burst(uint64_t frame) {
    if (!burst_running(frame)) return;
    const int started = (int)((frame - g_burst_start) / (uint64_t)g_period) + 1;
    if (started < g_burst_n) g_burst_n = started;
}
void set_target(int i, uint64_t frame) {
    g_tkx = g_list[i].x0; g_tky = g_list[i].y0; g_taps = 0; trim_burst(frame);
}
}

void mui_prim(uint32_t fb, int x0, int y0, int x1, int y1, float u0, float v0, float u1, float v1, uint32_t tex, uint32_t col) {
    if (x0 > x1) { std::swap(x0, x1); std::swap(u0, u1); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(v0, v1); }
    if (x1 <= 0 || y1 <= 0 || x0 >= 480 || y0 >= 272) return;
    std::vector<Spr> &v = g_cur[fb & 0x1ffff0u];
    if (v.size() < 6000) v.push_back({x0, y0, x1, y1, u0, v0, u1, v1, tex, col});
}

void mui_run(uint32_t fb, int x0, int y0, int x1, int y1) {
    if (x1 <= 0 || y1 <= 0 || x0 >= 480 || y0 >= 272) return;
    std::vector<Run> &v = g_cur_runs[fb & 0x1ffff0u];
    if (v.size() < 400) v.push_back({x0, y0, x1, y1});
}

/* ---- gravacao (F4) ---- */
namespace {
struct Quad { int x0, y0, x1, y1; uint32_t tex; };
std::map<uint32_t, std::vector<Quad>> g_cur_quads; std::vector<Quad> g_quads;
FILE *g_dump; bool g_dump_on; uint64_t g_dump_frame, g_dump_hash, g_dump_bytes; int g_dump_mx = -1, g_dump_my = -1; uint32_t g_dump_lastb = ~0u;
const uint64_t kDumpMax = 300ull << 20;   /* 300 MB: para sozinho (o texto compacta ~10x num zip) */
void dump_frame(uint32_t fb) {
    if (!g_dump_on || !g_dump) return;
    uint64_t h = 0xcbf29ce484222325ull;
    for (const Spr &a : g_snap) { h = mix(h, (uint64_t)a.x0 << 32 | (uint32_t)a.y0); h = mix(h, (uint64_t)a.x1 << 32 | (uint32_t)a.y1); h = mix(h, a.tex); h = mix(h, a.col); h = mix(h, (uint64_t)(a.u0 * 16) << 32 | (uint32_t)(a.v0 * 16)); }
    for (const Run &a : g_runs) { h = mix(h, (uint64_t)a.x0 << 32 | (uint32_t)a.y0); h = mix(h, (uint64_t)a.x1 << 32 | (uint32_t)a.y1); }
    for (const Quad &a : g_quads) { h = mix(h, (uint64_t)a.x0 << 32 | (uint32_t)a.y0); h = mix(h, (uint64_t)a.x1 << 32 | (uint32_t)a.y1); h = mix(h, a.tex); }
    int n;
    if (h == g_dump_hash) { n = fprintf(g_dump, "F %llu same\n", (unsigned long long)g_dump_frame); g_dump_bytes += n > 0 ? n : 0; return; }
    g_dump_hash = h;
    n = fprintf(g_dump, "F %llu fb=%x sprites=%zu runs=%zu quads=%zu sel=%d list=%zu\n", (unsigned long long)g_dump_frame, fb, g_snap.size(), g_runs.size(), g_quads.size(), g_sel, g_list.size());
    g_dump_bytes += n > 0 ? n : 0;
    for (const Spr &a : g_snap) { n = fprintf(g_dump, "S %d %d %d %d %.1f %.1f %.1f %.1f %x %x\n", a.x0, a.y0, a.x1, a.y1, a.u0, a.v0, a.u1, a.v1, a.tex, a.col); g_dump_bytes += n > 0 ? n : 0; }
    for (const Run &a : g_runs) { n = fprintf(g_dump, "R %d %d %d %d\n", a.x0, a.y0, a.x1, a.y1); g_dump_bytes += n > 0 ? n : 0; }
    for (const Quad &a : g_quads) { n = fprintf(g_dump, "Q %d %d %d %d %x\n", a.x0, a.y0, a.x1, a.y1, a.tex); g_dump_bytes += n > 0 ? n : 0; }
    if (g_dump_bytes > kDumpMax) { fprintf(g_dump, "# limite de tamanho: gravacao parada\n"); fclose(g_dump); g_dump = nullptr; g_dump_on = false; }
}
}
#ifdef _WIN32
std::wstring diag_session_prefix(void);
#endif
bool mui_dump_on(void) { return g_dump_on; }
bool mui_dump_toggle(void) {
    if (!g_dump) {
#ifdef _WIN32
        g_dump = _wfopen((diag_session_prefix() + L"_mouse.txt").c_str(), L"ab");
#else
        g_dump = fopen(getenv("TF_MOUSE_DUMP") ? getenv("TF_MOUSE_DUMP") : "mouse_dump.txt", "ab");
#endif
        if (!g_dump) return false;
        fprintf(g_dump, "# Tag Force Recomp: gravacao do mouse. F = quadro exibido; S = sprite 2D (x0 y0 x1 y1 u0 v0 u1 v1 textura cor);\n"
                        "# R = linha de texto; Q = quad 3D texturizado (carta); B = botoes do PSP (hex) e mouse 480x272 a partir daquele quadro\n");
    }
    g_dump_on = !g_dump_on; g_dump_hash = 0; g_dump_lastb = ~0u;
    fprintf(g_dump, "# gravacao %s no quadro %llu\n", g_dump_on ? "LIGADA" : "desligada", (unsigned long long)g_dump_frame); fflush(g_dump);
    return g_dump_on;
}
void mui_dump_buttons(uint64_t frame, uint32_t buttons) {
    g_dump_frame = frame;
    if (!g_dump_on || !g_dump) return;
    if (buttons != g_dump_lastb) { g_dump_lastb = buttons; int n = fprintf(g_dump, "B %llu %x %d %d\n", (unsigned long long)frame, buttons, g_dump_mx, g_dump_my); g_dump_bytes += n > 0 ? n : 0; }
}
void mui_quad(uint32_t fb, int x0, int y0, int x1, int y1, uint32_t tex) {
    if (x1 <= 0 || y1 <= 0 || x0 >= 480 || y0 >= 272) return;
    std::vector<Quad> &v = g_cur_quads[fb & 0x1ffff0u];
    if (v.size() < 2000) v.push_back({x0, y0, x1, y1, tex});
}

namespace { void analyze_items(); }
void mui_set_frame(uint32_t fb) {
    std::vector<Spr> &v = g_cur[fb & 0x1ffff0u];
    g_snap.swap(v); v.clear();
    std::vector<Run> &r = g_cur_runs[fb & 0x1ffff0u]; g_runs.swap(r); r.clear();
    std::vector<Quad> &q = g_cur_quads[fb & 0x1ffff0u]; g_quads.swap(q); q.clear();
    analyze();
    analyze_items();
    dump_frame(fb);
}

namespace {
int g_stab_sel = -2, g_stab_n;   /* selecao vista no quadro anterior e ha quantos quadros ela nao muda */
/* a cada quadro: quanto tempo a selecao ficou igual (o desenho atrasa em relacao ao aperto) */
void stab_update() {
    if (g_sel != g_stab_sel) { g_stab_sel = g_sel; g_stab_n = 0; } else g_stab_n++;
}
void confirm_pulse(uint64_t frame, const char *why, int bar) {
    g_pending = false; g_pulse = 0x4000; g_pulse_end = frame + 3; g_next_tap = frame + 10; g_tkx = g_tky = -1;
    if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: confirma barra %d (%s)\n", (unsigned long long)frame, bar, why);
}
}


/* ---- telas de cartas (nao sao listas de barras): Deck Edit (grade de miniaturas) e a mao no duelo ----
 * Medido em gravacoes (F4) do jogo europeu:
 *  - Deck Edit: miniaturas de carta = sprites 2D de 22x30 (desenhados duas vezes, sombra 1 px), grade de passo 28 x 34. O cursor e uma moldura
 *    de 9 pedacos (uma textura so) em volta da carta, ~4 px de folga; responde ao direcional ~5 quadros depois do aperto.
 *  - Duelo: as cartas da mao sao quads 3D de ~45x64 lado a lado na base da tela (passo ~47); a selecionada sobe ~13 px e uma maozinha (sprite
 *    32x32) fica em cima dela, ~17 px a direita da borda da carta. Sem a maozinha o jogador nao esta escolhendo carta (animacao, vez do CPU).
 * Aqui o mouse so age no CLIQUE (passar o mouse nao mexe o cursor: nessas telas abrem janelinhas que tambem usam o direcional). */
namespace {
struct Item { int x0, y0, x1, y1, col, row; };
std::vector<Item> g_items;    /* cartas clicaveis do quadro exibido */
int g_isel = -1;              /* carta onde o cursor do jogo esta (-1 = nao da para saber) */
int g_imode;                  /* 0 = nada, 1 = grade do Deck Edit, 2 = mao do duelo */

/* posicoes parecidas viram uma coluna/linha so (rank em ordem crescente) */
std::vector<int> ranks(const std::vector<int> &v, int tol) {
    std::vector<int> u = v; std::sort(u.begin(), u.end());
    std::vector<int> c; for (int x : u) if (c.empty() || x - c.back() > tol) c.push_back(x);
    std::vector<int> r(v.size());
    for (size_t i = 0; i < v.size(); i++) { int k = 0; for (size_t j = 0; j < c.size(); j++) if (v[i] - c[j] >= -tol) k = (int)j; r[i] = k; }
    return r;
}

void analyze_items() {
    g_items.clear(); g_isel = -1; g_imode = 0;
    if (!g_list.empty()) return;   /* lista de barras tem prioridade */
    /* 1) Deck Edit: miniaturas 22x30 */
    std::vector<Item> it;
    for (const Spr &a : g_snap) {
        const int w = a.x1 - a.x0, h = a.y1 - a.y0;
        if (w < 21 || w > 23 || h < 29 || h > 31) continue;
        bool dup = false; for (Item &b : it) if (std::abs(b.x0 - a.x0) <= 2 && std::abs(b.y0 - a.y0) <= 2) { b.x0 = std::min(b.x0, a.x0); b.y0 = std::min(b.y0, a.y0); dup = true; break; }
        if (!dup) it.push_back({a.x0, a.y0, a.x0 + w, a.y0 + h, 0, 0});
    }
    if (it.size() >= 2) {
        /* cursor: grupo de sprites da mesma textura que se tocam, cuja caixa envolve exatamente uma miniatura com 2..10 px de folga */
        std::vector<int> comp(g_snap.size()); for (size_t i = 0; i < comp.size(); i++) comp[i] = (int)i;
        auto find = [&](int x) { while (comp[x] != x) x = comp[x] = comp[comp[x]]; return x; };
        for (size_t i = 0; i < g_snap.size(); i++) for (size_t j = i + 1; j < g_snap.size(); j++) {
            const Spr &a = g_snap[i], &b = g_snap[j];
            if (a.tex == b.tex && a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1) comp[find((int)i)] = find((int)j);
        }
        std::map<int, std::tuple<int, int, int, int, int>> bb;
        for (size_t i = 0; i < g_snap.size(); i++) {
            const Spr &a = g_snap[i]; const int w = a.x1 - a.x0, h = a.y1 - a.y0;
            if (w >= 21 && w <= 23 && h >= 29 && h <= 31) continue;   /* as proprias miniaturas nao sao moldura */
            auto f = bb.find(find((int)i));
            if (f == bb.end()) bb[find((int)i)] = std::make_tuple(a.x0, a.y0, a.x1, a.y1, 1);
            else { auto &t = f->second; std::get<0>(t) = std::min(std::get<0>(t), a.x0); std::get<1>(t) = std::min(std::get<1>(t), a.y0); std::get<2>(t) = std::max(std::get<2>(t), a.x1); std::get<3>(t) = std::max(std::get<3>(t), a.y1); std::get<4>(t)++; }
        }
        int cur = -1, found = 0;
        for (auto &kv : bb) {
            const int bx0 = std::get<0>(kv.second), by0 = std::get<1>(kv.second), bx1 = std::get<2>(kv.second), by1 = std::get<3>(kv.second);
            if (std::get<4>(kv.second) < 4) continue;
            int inside = -1, n = 0;
            for (size_t i = 0; i < it.size(); i++) {
                const int l = it[i].x0 - bx0, t = it[i].y0 - by0, r = bx1 - it[i].x1, b = by1 - it[i].y1;
                if (l >= 2 && l <= 10 && t >= 2 && t <= 10 && r >= 2 && r <= 10 && b >= 2 && b <= 10) { inside = (int)i; n++; }
            }
            if (n == 1) { cur = inside; found++; }
        }
        std::vector<int> xs, ys; for (const Item &a : it) { xs.push_back(a.x0); ys.push_back(a.y0); }
        const std::vector<int> cx = ranks(xs, 3), cy = ranks(ys, 3);
        for (size_t i = 0; i < it.size(); i++) { it[i].col = cx[i]; it[i].row = cy[i]; it[i].x0 -= 3; it[i].x1 += 3; it[i].y0 -= 2; it[i].y1 += 2; }   /* clicavel ate o meio do vao */
        g_items = it; g_imode = 1; g_isel = found == 1 ? cur : -1;
        return;
    }
    /* 2) Duelo: cartas da mao (quads 3D na base da tela) e do campo, + maozinha 32x32 apontando para a escolhida.
     * Toda carta e desenhada duas vezes no mesmo retangulo: a moldura comum (uma textura igual para todas) e a arte. A textura da moldura e
     * descoberta pelas cartas da mao e serve para separar as cartas do campo dos enfeites da mesa (marcadores das zonas, icones). */
    static uint32_t frame_tex;
    it.clear();
    std::vector<int> hand_idx;
    std::map<uint32_t, int> tex_in_hand;
    for (const Quad &q : g_quads) {
        const int w = q.x1 - q.x0, h = q.y1 - q.y0;
        if (w < 38 || w > 54 || h < 54 || h > 74 || q.y1 < 250) continue;
        bool dup = false; for (Item &b : it) if (std::abs(b.x0 - q.x0) <= 3) { b.y0 = std::min(b.y0, q.y0); dup = true; break; }
        if (!dup) it.push_back({q.x0, q.y0, q.x1, std::max(q.y1, 272), 0, 1000});
    }
    if (it.size() >= 2) {   /* a moldura e a textura que aparece em todas as cartas da mao */
        for (const Quad &q : g_quads) { const int w = q.x1 - q.x0, h = q.y1 - q.y0; if (w >= 38 && w <= 54 && h >= 54 && h <= 74 && q.y1 >= 250) tex_in_hand[q.tex]++; }
        for (auto &kv : tex_in_hand) if (kv.second >= (int)it.size()) { frame_tex = kv.first; break; }
    }
    const size_t nhand = it.size();
    if (frame_tex) for (const Quad &q : g_quads) {   /* cartas no campo: retangulos com a moldura, menores que as da mao, entre a mao do oponente e a sua */
        const int w = q.x1 - q.x0, h = q.y1 - q.y0;
        if (q.tex != frame_tex || w < 16 || w > 40 || h < 12 || h > 40 || q.y1 < 72 || q.y0 > 214) continue;
        bool dup = false; for (Item &b : it) if (std::abs(b.x0 - q.x0) <= 2 && std::abs(b.y0 - q.y0) <= 2) { dup = true; break; }
        if (!dup) it.push_back({q.x0, q.y0, q.x1, q.y1, 0, 0});
    }
    if (it.size() < 2 && nhand < 2) return;
    /* fileiras do campo: y parecido = mesma fileira (de cima para baixo); a mao e a ultima */
    {
        std::vector<int> ys; std::vector<size_t> fi;
        for (size_t i = nhand; i < it.size(); i++) { ys.push_back((it[i].y0 + it[i].y1) / 2); fi.push_back(i); }
        const std::vector<int> ry = ranks(ys, 9);
        for (size_t k = 0; k < fi.size(); k++) it[fi[k]].row = ry[k];
    }
    std::sort(it.begin(), it.end(), [](const Item &a, const Item &b) { return a.row != b.row ? a.row < b.row : a.x0 < b.x0; });
    int cur = -1, best = 1 << 30;
    for (const Spr &a : g_snap) {
        if (a.x1 - a.x0 != 32 || a.y1 - a.y0 != 32) continue;
        for (size_t i = 0; i < it.size(); i++) {
            const int dx = a.x0 - it[i].x0, dy = a.y0 - it[i].y0;
            if (dx < 8 || dx > 24 || dy < -18 || dy > 10) continue;
            const int d = std::abs(dx - 16) + std::abs(dy + 4);
            if (d < best) { best = d; cur = (int)i; }
        }
    }
    /* coluna = ordem dentro da fileira */
    for (size_t i = 0; i < it.size(); i++) { int c = 0; for (size_t j = 0; j < it.size(); j++) if (it[j].row == it[i].row && it[j].x0 < it[i].x0) c++; it[i].col = c; }
    for (Item &a : it) if (a.row == 1000) a.y0 = std::min(a.y0, 210);   /* mao: clicavel do topo da carta levantada ate a borda da tela */
    for (size_t i = 0; i + 1 < it.size(); i++) if (it[i].row == it[i + 1].row && it[i].x1 > it[i + 1].x0) { const int m = (it[i].x1 + it[i + 1].x0) / 2; it[i].x1 = m; it[i + 1].x0 = m; }
    g_items = it; g_imode = 2; g_isel = cur;
}

int g_istab_sel = -2, g_istab_n, g_burst_from = -1;
int g_burst_axis, g_blocked_axis;   /* 1 = horizontal, 2 = vertical: eixo da ultima rajada / eixo em que o cursor nao andou */
uint32_t update_items(uint64_t frame, bool inside, int mx, int my, bool l_edge, uint32_t user_dpad, bool *eat_cross, uint32_t out) {
    if (g_isel != g_istab_sel) { g_istab_sel = g_isel; g_istab_n = 0; } else g_istab_n++;
    if (g_isel >= 0) g_sel_seen = frame;
    int hover = -1;
    if (inside) for (size_t i = 0; i < g_items.size(); i++) if (mx >= g_items[i].x0 && mx < g_items[i].x1 && my >= g_items[i].y0 && my < g_items[i].y1) hover = (int)i;
    if (user_dpad) { g_tkx = g_tky = -1; g_pending = false; return out; }
    int tgt = -1; for (size_t i = 0; i < g_items.size(); i++) if (g_items[i].col == g_tkx && g_items[i].row == g_tky) tgt = (int)i;
    if (l_edge && hover >= 0 && g_isel >= 0) {
        g_tkx = g_items[hover].col; g_tky = g_items[hover].row; g_taps = 0; trim_burst(frame);
        tgt = hover; g_pending = true; g_pending_until = frame + 150; *eat_cross = true; g_blocked_axis = 0; g_burst_from = -1;
        if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: clique na carta %d (coluna %d, linha %d); cursor na %d\n", (unsigned long long)frame, hover, g_tkx, g_tky, g_isel);
    }
    if (g_pending && frame > g_pending_until) g_pending = false;
    if (tgt < 0 || !g_pending) { g_tkx = g_tky = -1; g_pending = false; return out; }
    if (burst_running(frame)) return out;
    if (g_burst_n) { g_burst_end = g_burst_start + (uint64_t)g_burst_n * g_period; g_burst_n = 0; }
    if (frame < g_pulse_end + 2 || frame < g_next_tap) return out;
    if (g_isel < 0) { if (frame - g_sel_seen >= 12) { g_pending = false; g_tkx = g_tky = -1; } return out; }   /* cursor sumiu (janela, animacao): desiste */
    if (g_istab_n < 2 || frame < g_burst_end + kSettle) return out;
    if (g_burst_from >= 0) {
        /* a ultima rajada nao mexeu o cursor: ou aquele eixo esta bloqueado (linha incompleta no fim da grade: tenta o outro eixo), ou outra coisa
         * esta usando o direcional (janelinha aberta: os dois eixos falham, desiste sem confirmar nada) */
        const bool moved = g_isel != g_burst_from; g_burst_from = -1;
        if (moved) g_blocked_axis = 0;
        else if (tgt != g_isel) {
            if (g_blocked_axis && g_blocked_axis != g_burst_axis) { g_pending = false; g_tkx = g_tky = -1; g_blocked_axis = 0; if (g_mtrace) fprintf(stderr, "[mouse] o cursor nao andou: desisti\n"); return out; }
            g_blocked_axis = g_burst_axis;
        }
    }
    if (tgt == g_isel) { confirm_pulse(frame, "carta", tgt); return g_pulse; }
    if (g_taps >= 10) { g_pending = false; g_tkx = g_tky = -1; return out; }
    const Item &t = g_items[tgt], &c = g_items[g_isel];
    const int dx = t.col - c.col, dy = t.row - c.row;
    int axis = dx && g_blocked_axis != 1 ? 1 : dy && g_blocked_axis != 2 ? 2 : 0;
    if (g_imode == 2 && dy && g_blocked_axis != 2) axis = 2;   /* duelo: primeiro chega na fileira certa */
    if (!axis) { g_pending = false; g_tkx = g_tky = -1; g_blocked_axis = 0; if (g_mtrace) fprintf(stderr, "[mouse] sem caminho ate a carta: desisti\n"); return out; }
    if (axis == 1) { g_burst_dir = dx > 0 ? 0x20 : 0x80; g_burst_n = std::abs(dx); }
    else { g_burst_dir = dy > 0 ? 0x40 : 0x10; g_burst_n = g_imode == 2 ? 1 : std::abs(dy); }   /* duelo: as fileiras tem buracos e o jogo escolhe a coluna ao trocar de fileira: um passo por vez */
    g_burst_axis = axis; g_burst_start = frame; g_taps++; g_burst_from = g_isel;
    if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: carta %d -> %d, rajada de %d (%02x)\n", (unsigned long long)frame, g_isel, tgt, g_burst_n, g_burst_dir);
    return out | g_burst_dir;
}
}

uint32_t mui_update(uint64_t frame, bool inside, int mx, int my, bool l_edge, bool l_down, uint32_t user_dpad, bool *eat_cross) {
    (void)l_down;
    *eat_cross = false;
    const bool moved = inside && (mx != g_lmx || my != g_lmy);
    if (inside) { g_lmx = mx; g_lmy = my; }
    if (g_sel >= 0) g_sel_seen = frame;
    uint32_t out = frame < g_pulse_end ? g_pulse : 0;
    if (burst_running(frame) && frame >= g_burst_start && (frame - g_burst_start) % (uint64_t)g_period < (uint64_t)g_period / 2) out |= g_burst_dir;
    if (g_list.empty()) {
        if (!g_items.empty()) return update_items(frame, inside, mx, my, l_edge, user_dpad, eat_cross, out);
        g_tkx = g_tky = -1; g_pending = false; return out;
    }
    stab_update();
    int hover = -1;
    if (inside) for (size_t i = 0; i < g_list.size(); i++) if (mx >= g_list[i].x0 && mx < g_list[i].x1 && my >= g_list[i].y0 && my < g_list[i].y1) hover = (int)i;
    if (hover >= 0 && hover < (int)g_skip.size() && g_skip[hover]) {
        /* item apagado: o jogo nao deixa seleciona-lo. Nao vira alvo, e um clique nele nao faz nada (nao confirma o item que estiver selecionado) */
        if (l_edge) *eat_cross = true;
        hover = -1;
    }
    if (user_dpad) { g_tkx = g_tky = -1; g_pending = false; return out; }   /* o jogador voltou ao teclado/controle */
    int tgt = -1; for (size_t i = 0; i < g_list.size(); i++) if (g_list[i].x0 == g_tkx && g_list[i].y0 == g_tky) tgt = (int)i;
    if (l_edge && hover >= 0) {
        /* clique numa barra: este modulo e dono do clique (quem confirma e ele, quando a selecao chegar); o X cru fica bloqueado ate soltar o botao */
        if (hover != tgt) set_target(hover, frame);
        tgt = hover; g_pending = true; g_pending_until = frame + 120; *eat_cross = true;
    } else if (moved && hover >= 0 && hover != tgt && !g_pending) {
        /* so passar o mouse: leva a selecao ate a barra. Mexer DENTRO da mesma barra nao reinicia nada; com um clique pendente o alvo fica
         * travado (o mouse escorregar para a vizinha nao muda o item que vai ser confirmado) */
        set_target(hover, frame); tgt = hover;
    }
    if (g_pending && frame > g_pending_until) g_pending = false;
    if (tgt < 0) { g_pending = false; return out; }
    if (burst_running(frame)) return out;   /* rajada em andamento */
    if (g_burst_n) { g_burst_end = g_burst_start + (uint64_t)g_burst_n * g_period; g_burst_n = 0; }
    if (frame < g_pulse_end + 2 || frame < g_next_tap) return out;
    if (g_sel < 0) {
        /* nao da para saber qual barra esta selecionada. Se ela sumiu ha pouco (animacao, piscada do realce), espera ela voltar; so se ficar
         * desconhecida por 12 quadros seguidos (ou se nesta tela ela nunca aparece) faz o que um clique comum faria (X), como antes */
        if (g_pending && frame - g_sel_seen >= 12) { confirm_pulse(frame, "selecao desconhecida", tgt); out |= g_pulse; }
        return out;
    }
    if (g_stab_n < 2) return out;   /* espera o desenho refletir o ultimo aperto */
    /* o realce atrasa alguns quadros em relacao ao aperto: logo depois de uma rajada a barra desenhada como selecionada ainda pode ser a de antes
     * (confirmar ai abriria o item errado; corrigir ai daria passos a mais). So decide depois de esperar */
    if (frame < g_burst_end + kSettle) return out;
    if (tgt == g_sel) {
        if (g_pending) { confirm_pulse(frame, "selecao chegou", tgt); out = g_pulse; }
        else g_tkx = g_tky = -1;
        return out;
    }
    if (g_taps >= 6) { g_tkx = g_tky = -1; g_pending = false; if (g_mtrace) fprintf(stderr, "[mouse] desisti da barra %d\n", tgt); return out; }   /* nao esta chegando (deteccao errada): desiste */
    /* rajada: um aperto por item no ritmo mais rapido que o jogo aceita (o jogo le o controle a cada 2 quadros); cada passo toca o som e o realce do jogo */
    const Bar &t = g_list[tgt], &s = g_list[g_sel];
    g_burst_dir = t.y0 != s.y0 ? (t.y0 < s.y0 ? 0x10 : 0x40) : (t.x0 < s.x0 ? 0x80 : 0x20);
    int steps = 0;   /* um aperto por barra ate o alvo, sem contar as apagadas (o jogo as pula) */
    for (int i = std::min(tgt, g_sel); i <= std::max(tgt, g_sel); i++) if (i != g_sel && !(i < (int)g_skip.size() && g_skip[i])) steps++;
    g_burst_n = std::max(1, steps); g_burst_start = frame; g_taps++;
    if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: selecionada %d -> alvo %d, rajada de %d (%02x)\n", (unsigned long long)frame, g_sel, tgt, g_burst_n, g_burst_dir);
    return out | g_burst_dir;
}

/* ---- botoes do mouse -> botoes do PSP ---- */
namespace {
const uint32_t kMouseBit[MUI_NB] = {0x4000, 0x2000, 0x1000, 0x100, 0x200};   /* esquerdo = X, direito = O, meio = triangulo, laterais = L / R */
const int kMinHold = 3;   /* o jogo le o controle a cada 2 quadros: um clique mais curto que isso pode passar entre duas leituras */
bool g_mprev[MUI_NB]; uint64_t g_mhold_until[MUI_NB]; bool g_owned;
}

uint32_t mui_mouse(uint64_t frame, const MuiMouse &m, bool menus, uint32_t user_dpad) {
    g_dump_mx = m.inside ? m.mx : -1; g_dump_my = m.inside ? m.my : -1;
    bool down[MUI_NB], edge[MUI_NB];
    for (int i = 0; i < MUI_NB; i++) {
        const bool raw = m.in_client && m.down[i];
        bool e = raw && !g_mprev[i];
        /* clique inteiro entre duas leituras (a janela viu WM_xBUTTONDOWN, mas o botao ja estava solto aqui): vale como aperto. Se o botao estava
         * apertado na leitura anterior (ou o aperto esticado ainda vale), esse clique ja foi contado */
        if (!e && m.clicked[i] && !raw && !g_mprev[i] && frame >= g_mhold_until[i]) e = true;
        if (e) g_mhold_until[i] = frame + kMinHold;
        edge[i] = e; down[i] = raw || frame < g_mhold_until[i];
        g_mprev[i] = raw;
    }
    uint32_t b = 0;
    if (menus) {
        bool eat = false;
        b |= mui_update(frame, m.inside, m.mx, m.my, edge[MUI_BL], down[MUI_BL], user_dpad, &eat);
        if (edge[MUI_BL]) g_owned = eat;
    } else if (edge[MUI_BL]) g_owned = false;
    if (!down[MUI_BL]) g_owned = false;
    for (int i = 0; i < MUI_NB; i++) if (down[i] && !(i == MUI_BL && g_owned)) b |= kMouseBit[i];
    return b;
}
