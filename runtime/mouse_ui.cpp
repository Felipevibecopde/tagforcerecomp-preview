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
#include <cmath>

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
    /* todas as colunas de linhas alinhadas com passo constante (candidatas), da maior para a menor */
    std::vector<std::vector<Run>> cands;
    for (size_t i = 0; i < r.size(); i++) {
        std::vector<Run> ch = {r[i]};
        int pitch = 0;
        for (size_t j = i + 1; j < r.size(); j++) {
            if (std::abs(r[j].x0 - r[i].x0) > 2) continue;
            const int d = r[j].y0 - ch.back().y0;
            /* linhas que se sobrepoem um pouco (passo menor que a altura do texto, ex. a janelinha de opcoes do duelo: passo 15, texto 17)
             * tambem valem, mas so se um realce marcar exatamente uma delas (verificado abaixo) */
            if (!pitch) { if (d >= std::min(r[i].y1 - r[i].y0, 12) && d <= 60) { pitch = d; ch.push_back(r[j]); } }
            else if (std::abs(d - pitch) <= 1) ch.push_back(r[j]);
        }
        if (ch.size() >= 2) cands.push_back(ch);
    }
    std::stable_sort(cands.begin(), cands.end(), [](const std::vector<Run> &a, const std::vector<Run> &b) { return a.size() > b.size(); });
    if (cands.empty()) return;
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
    /* a primeira candidata que for menu de verdade: uma coluna apertada ou de 2 linhas so vale se um realce marcar exatamente uma linha.
     * (Antes so a maior era testada: o texto de descricao da carta, 3 linhas a esquerda, ganhava da janelinha 'Set'/'Ativar' de 2 opcoes,
     * era descartado por nao ter realce, e a janelinha nao era reconhecida) */
    for (const std::vector<Run> &best : cands) {
        const int pitch = best[1].y0 - best[0].y0;
        const bool tight = pitch < best[0].y1 - best[0].y0 || best.size() < 3;
        int left = best[0].x0, right = best[0].x1;
        for (const Run &b : best) { left = std::min(left, b.x0); right = std::max(right, b.x1); }
        int hx1 = 0, hn = 0; std::vector<std::pair<int, int>> hl;   /* realces plausiveis (y0,y1) */
        for (auto &kv : bb) {
            const int x0 = std::get<0>(kv.second), y0 = std::get<1>(kv.second), x1 = std::get<2>(kv.second), y1 = std::get<3>(kv.second), n = std::get<4>(kv.second);
            if (n >= 4 && y1 - y0 >= 14 && y1 - y0 <= pitch + 8 && x1 - x0 >= 40 && x1 - x0 <= 420 && x0 <= left + 2 && x1 >= left) { hl.push_back({y0, y1}); hx1 = x1; hn++; }
        }
        if (hn) right = std::max(right, hx1);
        const int rx0 = std::max(0, left - 6), rx1 = right + 4, top = best[0].y0 - (pitch - (best[0].y1 - best[0].y0)) / 2;
        std::vector<Bar> list;
        for (size_t i = 0; i < best.size(); i++) list.push_back({rx0, top + (int)i * pitch, rx1, top + (int)(i + 1) * pitch, 0, 0});
        int sel = -1, n = 0;
        for (size_t i = 0; i < list.size(); i++) for (auto &h : hl) {
            const int ov = std::min(h.second, list[i].y1) - std::max(h.first, list[i].y0);
            if (ov * 10 >= (h.second - h.first) * 6) { sel = (int)i; n++; break; }
        }
        if (n != 1) sel = -1;
        if (tight && sel < 0) continue;   /* sem realce: texto corrido (descricao da carta, 2 linhas soltas), nao e menu; tenta a proxima */
        g_list = list; g_sel = sel;
        return;
    }
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

/* Fileira de icones iguais com uma moldura de realce em volta de um deles: a escolha de fase do duelo (Draw, Standby, Main 1, Battle,
 * Main 2, End). Medido (F4): icones de 20x32 de uma textura so, lado a lado com passo 40, em y=124; o realce e uma moldura de 9 pedacos
 * (outra textura) com ~4-5 px de folga. Fase indisponivel = mesmo icone desenhado com outra parte da textura (v diferente da escolhida);
 * o jogo pula essas no direcional. Vira uma lista horizontal comum (passar o mouse realca, clicar confirma). */
/* Tambem a escolha de pedra/papel/tesoura antes do duelo: 3 maos de 80x116 (passo 90); a escolhida SOBE 20 px e ganha uma moldura de
 * 9 pedacos da mesma textura das maos. */
bool analyze_icons() {
    std::map<std::tuple<uint32_t, int, int>, std::vector<int>> rows;   /* (textura, w, h) -> sprites (a escolhida pode estar mais alta) */
    for (size_t i = 0; i < g_snap.size(); i++) {
        const Spr &a = g_snap[i]; const int w = a.x1 - a.x0, h = a.y1 - a.y0;
        if (w < 12 || w > 120 || h < 16 || h > 140) continue;
        rows[std::make_tuple(a.tex, w, h)].push_back((int)i);
    }
    for (auto &kv : rows) {
        std::vector<int> v = kv.second;
        if (v.size() < 3 || v.size() > 10) continue;
        { int ylo = 1 << 30, yhi = -(1 << 30); for (int k : v) { ylo = std::min(ylo, g_snap[k].y0); yhi = std::max(yhi, g_snap[k].y0); } if (yhi - ylo > 30) continue; }
        if (v.size() == 3 && std::get<1>(kv.first) < 40) continue;   /* so 3 icones: so os grandes (maos); fileiras pequenas pedem 4+ */
        std::sort(v.begin(), v.end(), [](int a, int b) { return g_snap[a].x0 < g_snap[b].x0; });
        const int pitch = g_snap[v[1]].x0 - g_snap[v[0]].x0;
        if (pitch < 24 || pitch > 120) continue;   /* fases: 40; pedra/papel/tesoura: 90 */
        bool even = true; for (size_t k = 1; k < v.size(); k++) if (std::abs(g_snap[v[k]].x0 - g_snap[v[k - 1]].x0 - pitch) > 2) even = false;
        if (!even) continue;
        /* realce: componente (mesma textura e cor, se tocando, >= 4 pedacos, sem contar os proprios icones) que envolve exatamente um icone */
        std::vector<char> is_icon(g_snap.size(), 0); for (int k : v) is_icon[k] = 1;
        std::vector<int> comp(g_snap.size()); for (size_t i = 0; i < comp.size(); i++) comp[i] = (int)i;
        auto find = [&](int x) { while (comp[x] != x) x = comp[x] = comp[comp[x]]; return x; };
        for (size_t i = 0; i < g_snap.size(); i++) for (size_t j = i + 1; j < g_snap.size(); j++) {
            const Spr &a = g_snap[i], &b = g_snap[j];
            if (!is_icon[i] && !is_icon[j] && a.tex == b.tex && a.col == b.col && a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1) comp[find((int)i)] = find((int)j);
        }
        std::map<int, std::tuple<int, int, int, int, int>> bb;
        for (size_t i = 0; i < g_snap.size(); i++) {
            const Spr &a = g_snap[i]; if (is_icon[i]) continue;
            auto f = bb.find(find((int)i));
            if (f == bb.end()) bb[find((int)i)] = std::make_tuple(a.x0, a.y0, a.x1, a.y1, 1);
            else { auto &t = f->second; std::get<0>(t) = std::min(std::get<0>(t), a.x0); std::get<1>(t) = std::min(std::get<1>(t), a.y0); std::get<2>(t) = std::max(std::get<2>(t), a.x1); std::get<3>(t) = std::max(std::get<3>(t), a.y1); std::get<4>(t)++; }
        }
        int sel = -1, found = 0;
        for (auto &b : bb) {
            if (std::get<4>(b.second) < 4) continue;
            int in = -1, n = 0;
            for (size_t k = 0; k < v.size(); k++) {
                const Spr &a = g_snap[v[k]];
                const int l = a.x0 - std::get<0>(b.second), t = a.y0 - std::get<1>(b.second), r = std::get<2>(b.second) - a.x1, bo = std::get<3>(b.second) - a.y1;
                if (l >= 2 && l <= 10 && t >= 2 && t <= 10 && r >= 2 && r <= 10 && bo >= 2 && bo <= 10) { in = (int)k; n++; }
            }
            if (n == 1) { sel = in; found++; }
        }
        if (found != 1) continue;
        const int half = std::max(2, (pitch - (g_snap[v[0]].x1 - g_snap[v[0]].x0)) / 2);
        int ybot = -(1 << 30); for (int k : v) ybot = std::max(ybot, g_snap[k].y1);
        g_list.clear(); g_skip.clear();
        for (size_t k = 0; k < v.size(); k++) {
            const Spr &a = g_snap[v[k]];
            /* mesma faixa para todos, presa na base das que estao paradas (a escolhida sobe/desce animando; a faixa nao pode mexer junto, senao o alvo se perde) */
            Bar b{}; b.x0 = a.x0 - half; b.x1 = a.x1 + half; b.y0 = ybot - (a.y1 - a.y0) - 26; b.y1 = ybot + 6; b.sig = 0;
            g_list.push_back(b);
            g_skip.push_back((int)k != sel && (std::abs(a.v0 - g_snap[v[sel]].v0) > 1 || std::abs(a.v1 - g_snap[v[sel]].v1) > 1));
        }
        g_sel = sel;
        return true;
    }
    return false;
}

/* Botoes lado a lado (ex.: a janelinha Sim/Nao do duelo: "ativar a armadilha?"): sprites de 40-120 x 16-40 da mesma textura na mesma
 * altura; o escolhido e desenhado com camadas extras por cima (o mesmo retangulo repetido: medido, 3 sprites no escolhido e 1 no outro).
 * Vira uma lista horizontal (passar o mouse realca, clicar confirma). */
bool analyze_buttons() {
    std::map<std::tuple<uint32_t, int, int, int>, std::map<int, int>> rows;   /* (textura, y0, w, h) -> x0 -> quantos sprites no retangulo */
    for (const Spr &a : g_snap) {
        const int w = a.x1 - a.x0, h = a.y1 - a.y0;
        if (w < 40 || w > 120 || h < 16 || h > 40) continue;
        rows[std::make_tuple(a.tex, a.y0, w, h)][a.x0]++;
    }
    for (auto &kv : rows) {
        const std::map<int, int> &bx = kv.second;
        if (bx.size() < 2 || bx.size() > 5) continue;
        int maxc = 0, minc = 1 << 30, nmax = 0;
        for (auto &b : bx) { maxc = std::max(maxc, b.second); minc = std::min(minc, b.second); }
        for (auto &b : bx) if (b.second == maxc) nmax++;
        if (maxc == minc || nmax != 1) continue;   /* exatamente um botao com camadas a mais = o escolhido */
        const int w = std::get<2>(kv.first), h = std::get<3>(kv.first), y0 = std::get<1>(kv.first);
        std::vector<int> xs; for (auto &b : bx) xs.push_back(b.first);
        bool sep = true; for (size_t i = 1; i < xs.size(); i++) if (xs[i] - xs[i - 1] < w) sep = false;   /* botoes separados, sem sobrepor */
        if (!sep) continue;
        g_list.clear(); g_skip.clear(); g_sel = -1;
        for (size_t i = 0; i < xs.size(); i++) {
            Bar b{}; b.x0 = xs[i] - 4; b.x1 = xs[i] + w + 4; b.y0 = y0 - 4; b.y1 = y0 + h + 4; b.sig = 0;
            g_list.push_back(b); g_skip.push_back(false);
            if (bx.at(xs[i]) == maxc) g_sel = (int)i;
        }
        return true;
    }
    return false;
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
    if (!best) {
        if (analyze_icons()) { trace_list(); return; }
        if (analyze_buttons()) { trace_list(); return; }
        analyze_text(); g_skip.assign(g_list.size(), false);
        /* janelinha de texto cujo realce pisca (ex.: confirmando 'Attack'): enquanto as mesmas linhas de texto continuarem no lugar, por ate
         * 12 quadros, a lista continua valendo com a selecao desconhecida (um clique espera o realce voltar), em vez de o campo embaixo dela
         * virar alvo do clique */
        static std::vector<Bar> held; static int held_age;
        if (!g_list.empty()) { held = g_list; held_age = 0; }
        else if (!held.empty() && ++held_age <= 12) {
            int lines = 0;
            for (const Bar &b : held) for (const Run &r : g_runs) if (std::abs(r.x0 - (b.x0 + 6)) <= 4 && r.y0 >= b.y0 - 3 && r.y0 <= b.y1) { lines++; break; }
            if (lines == (int)held.size()) { g_list = held; g_sel = -1; g_skip.assign(g_list.size(), false); }
            else held.clear();
        } else held.clear();
        trace_list(); return;
    }
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
int g_burst_n, g_burst_dir; uint64_t g_burst_start, g_burst_end; int g_burst_sel0 = -9;   /* selecao quando a rajada comecou (listas) */
int g_last_burst_n;   /* quantos apertos a ultima rajada mandou */
uint64_t g_list_moved_until;   /* ate quando o ultimo movimento do ponteiro ainda vale como 'passar o mouse' numa lista nova */
/* quadros de espera depois de uma rajada ate confiar no realce desenhado. Medido nas gravacoes: o realce dos menus muda 4-5 quadros depois
 * do aperto (o ultimo aperto da rajada comeca 4 quadros antes do fim dela); 6 deixa folga e ainda exige 2 quadros estaveis (era 12) */
const int kSettle = 6;
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
    { static uint64_t last_flush; if (g_dump_frame - last_flush >= 60) { last_flush = g_dump_frame; fflush(g_dump); } }   /* grava no disco ~1x/s: nada se perde se o jogo fechar sem o 2o F4 */
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
                        "# R = linha de texto; Q = quad 3D texturizado (carta); B = botoes do PSP (hex) e mouse 480x272 a partir daquele quadro; M = mouse mudou de lugar;\n"
                        "# U = estado do mouse nas cartas: modo(1 Deck Edit, 2 duelo) cartas cursor fileira_do_cursor pausado alvo_coluna alvo_fileira\n");
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

namespace { void analyze_items(); void analyze_tabs(); }
void mui_set_frame(uint32_t fb) {
    std::vector<Spr> &v = g_cur[fb & 0x1ffff0u];
    g_snap.swap(v); v.clear();
    std::vector<Run> &r = g_cur_runs[fb & 0x1ffff0u]; g_runs.swap(r); r.clear();
    std::vector<Quad> &q = g_cur_quads[fb & 0x1ffff0u]; g_quads.swap(q); q.clear();
    analyze();
    analyze_items();
    analyze_tabs();
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
 * Passar o mouse leva o cursor (com pausa enquanto alguma janelinha que nao virou menu pode estar usando o direcional). */
namespace {
struct Item { int x0, y0, x1, y1, col, row, ax = -999, ay = -999; bool zone = false; };   /* ax,ay = onde fica a maozinha quando o cursor esta nesta carta/zona */
std::vector<Item> g_items;    /* cartas clicaveis do quadro exibido */
int g_isel = -1;              /* carta onde o cursor do jogo esta (-1 = nao da para saber) */
int g_imode; bool g_is_shop;   /* a grade e a da loja (sempre completa) */                  /* 0 = nada, 1 = grade do Deck Edit, 2 = duelo, 3 = mapa do Modo Historia */

/* posicoes parecidas viram uma coluna/linha so (rank em ordem crescente) */
std::vector<int> ranks(const std::vector<int> &v, int tol) {
    std::vector<int> u = v; std::sort(u.begin(), u.end());
    std::vector<int> c; for (int x : u) if (c.empty() || x - c.back() > tol) c.push_back(x);
    std::vector<int> r(v.size());
    for (size_t i = 0; i < v.size(); i++) { int k = 0; for (size_t j = 0; j < c.size(); j++) if (v[i] - c[j] >= -tol) k = (int)j; r[i] = k; }
    return r;
}

void analyze_items() {
    g_items.clear(); g_isel = -1; g_imode = 0; g_is_shop = false;
    if (!g_list.empty()) return;   /* lista de barras tem prioridade */
    /* 0) loja (mouse-fix-44, gravacao 143802): grade de pacotes de booster. Cada casa tem um divisor 2x8 no alto (12 por fileira, a cada 22 px;
     * fileiras a cada 47 px) e o cursor e uma moldura de 4 cantos 12x12 da MESMA textura (canto da arte em u 0/12, v 214/226) que pulsa
     * (cresce e diminui) com o centro fixo na casa escolhida. O direcional anda uma casa (passa tambem pelas vazias) e o desenho responde
     * ~5 quadros depois, como no Deck Edit: a casa vira um item de grade (mesma navegacao) */
    {
        std::map<uint32_t, std::vector<std::pair<int, int>>> divs;   /* textura -> divisores (x, y) */
        for (const Spr &a : g_snap) if (a.x1 - a.x0 == 2 && a.y1 - a.y0 == 8) divs[a.tex].push_back({a.x0, a.y0});
        for (auto &kv : divs) {
            std::vector<std::pair<int, int>> &d = kv.second;
            if (d.size() < 16) continue;
            std::sort(d.begin(), d.end()); d.erase(std::unique(d.begin(), d.end()), d.end());
            std::map<int, std::vector<int>> rows; for (auto &p : d) rows[p.second].push_back(p.first);
            std::vector<int> ry; std::vector<int> cxs;
            for (auto &r : rows) if (r.second.size() >= 8) { ry.push_back(r.first); if (cxs.empty() || r.second.size() > cxs.size()) cxs = r.second; }
            if (ry.size() < 2 || cxs.size() < 8) continue;
            std::sort(cxs.begin(), cxs.end());
            bool pitch_ok = true; for (size_t i = 1; i < cxs.size(); i++) if (cxs[i] - cxs[i - 1] < 20 || cxs[i] - cxs[i - 1] > 24) pitch_ok = false;
            bool rows_ok = true; for (size_t i = 1; i < ry.size(); i++) if (ry[i] - ry[i - 1] < 44 || ry[i] - ry[i - 1] > 50) rows_ok = false;
            if (!pitch_ok || !rows_ok) continue;
            /* cursor: cantos 12x12 da mesma textura com a arte do canto (u0 0 ou 12, v0 214 ou 226) */
            int n = 0, bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);
            for (const Spr &a : g_snap) {
                if (a.tex != kv.first || a.x1 - a.x0 != 12 || a.y1 - a.y0 != 12) continue;
                const int u = (int)std::lround(std::min(a.u0, a.u1)), v = (int)std::lround(std::min(a.v0, a.v1));
                if (u != 0 || v != 214) continue;   /* a arte do canto ocupa u 0..12, v 214..226 (espelhada nos 4 cantos) */
                n++; bx0 = std::min(bx0, a.x0); by0 = std::min(by0, a.y0); bx1 = std::max(bx1, a.x1); by1 = std::max(by1, a.y1);
            }
            std::vector<Item> it; int cur = -1;
            const int ccx = (bx0 + bx1) / 2, ccy = (by0 + by1) / 2;
            for (size_t r = 0; r < ry.size(); r++) for (size_t c = 0; c < cxs.size(); c++) {
                Item q{};
                q.x0 = cxs[c] - 9; q.x1 = q.x0 + 22; q.y0 = ry[r] + 1; q.y1 = q.y0 + 47;   /* a casa inteira e clicavel */
                q.col = (int)c; q.row = (int)r; q.ax = q.x0 + 10; q.ay = q.y0 + 20;
                if (n == 4 && ccx >= q.x0 && ccx < q.x1 && ccy >= q.y0 && ccy < q.y1) cur = (int)it.size();
                it.push_back(q);
            }
            g_items = it; g_imode = 1; g_isel = cur; g_is_shop = true;
            return;
        }
    }
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
        for (size_t i = 0; i < it.size(); i++) { it[i].col = cx[i]; it[i].row = cy[i]; it[i].ax = it[i].x0 + 11; it[i].ay = it[i].y0 + 15; it[i].x0 -= 3; it[i].x1 += 3; it[i].y0 -= 2; it[i].y1 += 2; }   /* clicavel ate o meio do vao; ax,ay = centro (passar o mouse lembra por ele) */
        g_items = it; g_imode = 1; g_isel = found == 1 ? cur : -1;
        return;
    }
    /* 1b) Mapa da escola (Modo Historia): cada lugar e um quad 16x16 desenhado duas vezes no mesmo ponto (duas texturas); uma setinha 16x16
     * (terceira textura) balanca logo ACIMA do lugar escolhido (16-20 px). O direcional leva ao lugar vizinho naquela direcao (nao e grade). */
    {
        std::vector<Item> nodes;
        std::map<std::pair<int, int>, std::set<uint32_t>> at;
        for (const Quad &q : g_quads) if (q.x1 - q.x0 >= 14 && q.x1 - q.x0 <= 18 && q.y1 - q.y0 >= 14 && q.y1 - q.y0 <= 18) at[{q.x0, q.y0}].insert(q.tex);
        std::map<std::pair<uint32_t, uint32_t>, int> pairs;   /* par de texturas mais comum = os lugares */
        for (auto &kv : at) if (kv.second.size() == 2) pairs[{*kv.second.begin(), *kv.second.rbegin()}]++;
        std::pair<uint32_t, uint32_t> best_pair{0, 0}; int bn = 0;
        for (auto &kv : pairs) if (kv.second > bn) { bn = kv.second; best_pair = kv.first; }
        if (bn >= 3) {
            for (auto &kv : at) if (kv.second.size() == 2 && *kv.second.begin() == best_pair.first && *kv.second.rbegin() == best_pair.second) {
                Item n{kv.first.first - 10, kv.first.second - 12, kv.first.first + 26, kv.first.second + 26, 0, 0};
                n.ax = kv.first.first + 8; n.ay = kv.first.second + 8; nodes.push_back(n);
            }
            int cur = -1, found = 0;
            for (const Quad &q : g_quads) {
                if (q.x1 - q.x0 < 14 || q.x1 - q.x0 > 18 || q.y1 - q.y0 < 14 || q.y1 - q.y0 > 18 || q.tex == best_pair.first || q.tex == best_pair.second) continue;
                for (size_t i = 0; i < nodes.size(); i++) {
                    const int dx = (q.x0 + 8) - nodes[i].ax, dy = (q.y0 + 8) - nodes[i].ay;
                    if (std::abs(dx) <= 3 && dy >= -24 && dy <= -10) { cur = (int)i; found++; }
                }
            }
            /* sem setinha (ela some ~5 quadros enquanto anda de um lugar para outro): o mapa continua valendo, com o cursor desconhecido */
            for (size_t i = 0; i < nodes.size(); i++) { nodes[i].col = (int)i; nodes[i].row = 0; }
            g_items = nodes; g_imode = 3; g_isel = found == 1 ? cur : -1;
            return;
        }
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
    /* onde a maozinha fica em cada carta (medido): na mao, 17 px a direita da borda e em y=193; no campo, no meio da carta, ~2 px abaixo do topo */
    for (size_t i = 0; i < it.size(); i++) {
        if (i < nhand) { it[i].ax = it[i].x0 + 17; it[i].ay = 193; }
        else { it[i].ax = (it[i].x0 + it[i].x1) / 2; it[i].ay = it[i].y0 + 2; }
    }
    /* Zonas VAZIAS do seu lado: o cursor do jogo passa por elas (para invocar, baixar, olhar), entao tambem sao alvos. Posicoes da maozinha
     * medidas nas gravacoes (visao normal do campo): monstros em y=145, x 226/270/314/358/401 (passo ~44); magias/armadilhas em y=181,
     * x 218/266/314/361/409 (passo ~47). As fileiras sao centradas em x=314. Uma zona com carta ja e a propria carta. */
    if (nhand >= 1 || it.size() > nhand) {
        static const int kMonX[5] = {226, 270, 314, 358, 401}, kStX[5] = {218, 266, 314, 361, 409};
        auto zone = [&](int px, int py, int half, int y0, int y1) {
            for (const Item &c : it) if (c.ax != -999 && std::abs(c.ax - px) <= 8 && std::abs(c.ay - py) <= 10) return;   /* ja tem carta ali */
            Item z{px - half, y0, px + half, y1, 0, 0}; z.ax = px; z.ay = py; z.zone = true; it.push_back(z);
        };
        for (int k = 0; k < 5; k++) zone(kMonX[k], 145, 21, 141, 173);
        for (int k = 0; k < 5; k++) zone(kStX[k], 181, 23, 174, 205);
        /* lado do oponente (mais longe da camera, mais estreito): monstros em y=99, x 236/275/314/352/391 (passo ~39); magias/armadilhas em
         * y=75, x 242/278/314/349/385 (passo ~36). O cursor passa por elas ao olhar o campo; ao escolher alvo de ataque so para nos monstros */
        static const int kOppMonX[5] = {236, 275, 314, 352, 391}, kOppStX[5] = {242, 278, 314, 349, 385};
        for (int k = 0; k < 5; k++) zone(kOppMonX[k], 99, 19, 99, 122);
        for (int k = 0; k < 5; k++) zone(kOppStX[k], 75, 18, 73, 99);
        /* pilhas: o SEU deck (canto direito; a maozinha comeca nele no inicio do seu turno, x=448 y=175), o seu cemiterio logo acima e a pilha do oponente (canto de
         * cima a esquerda, x=208 y=61). Retangulos clicaveis em volta; o deck fica na fileira das magias/armadilhas, a direita delas */
        auto pile = [&](int px, int py, int x0, int y0, int x1, int y1) {
            for (const Item &c : it) if (c.ax != -999 && std::abs(c.ax - px) <= 8 && std::abs(c.ay - py) <= 10) return;
            Item z{x0, y0, x1, y1, 0, 0}; z.ax = px; z.ay = py; z.zone = true; it.push_back(z);
        };
        pile(448, 175, 434, 166, 480, 206);
        pile(450, 161, 434, 138, 480, 166);   /* seu cemiterio: logo acima do deck (a maozinha fica em x=450 y=161), na fileira dos monstros */
        pile(439, 129, 420, 110, 470, 138);   /* sua zona de banimento: acima do cemiterio (x=439 y=129) */
        pile(208, 61, 186, 40, 232, 72);
    }
    if (it.size() < 2) return;
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
            const int dx = a.x0 - it[i].ax, dy = a.y0 - it[i].ay;
            if (std::abs(dx) > 6 || dy < -10 || dy > (it[i].row == 1000 ? 14 : 10)) continue;   /* na mao a maozinha desce ~12 px enquanto a carta sobe */
            const int d = std::abs(dx) + std::abs(dy);
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
int g_chain_dir, g_chain_n, g_chain_col = -9, g_chain_row = -9;   /* loja (grade sempre completa): a parte vertical sai logo depois da horizontal, sem esperar o desenho */
int g_burst_axis, g_blocked_axis;   /* 1 = horizontal, 2 = vertical: eixo da ultima rajada / eixo em que o cursor nao andou */
/* Passar o mouse pelas cartas do duelo leva o cursor do jogo junto (como no direcional). Fica PAUSADO enquanto uma janelinha pode estar
 * aberta (Invocar/Baixar, escolher zona...), porque ela tambem usa o direcional: pausa quando o jogador confirma (X, do mouse ou do teclado)
 * ou quando o cursor nao anda; volta quando a maozinha muda de carta ou some e reaparece (janela fechou, vez passou), ou com O (cancelar). */
bool g_hover_pause; int g_pause_sel = -2; uint64_t g_pause_until; bool g_pause_timed; int g_nomove;   /* rajadas seguidas em que o cursor nao andou (qualquer alvo) */
int g_nm_from = -1; uint64_t g_nm_check; bool g_nm_moved;   /* conferencia independente do alvo: cursor no inicio da ultima rajada e quando conferir */   /* pausa do X: so ate g_pause_until (janelinha aparecer) */
std::vector<std::pair<int, int>> g_unreach;   /* cartas/zonas que o cursor nao alcancou desde a ultima vez que ele andou (ex.: zona vazia do oponente ao escolher alvo) */
int g_unreach_sel = -2, g_burst_dist = -1; bool g_escape_burst;   /* a ultima rajada foi um desvio para sair de um lugar isolado (banimento...) */
bool unreachable(const Item &a) { for (auto &u : g_unreach) if (u.first == a.ax && u.second == a.ay) return true; return false; }
int item_dist(const Item &t, const Item &c) {
    if (g_imode == 3) return (int)std::lround(std::hypot((double)(t.ax - c.ax), (double)(t.ay - c.ay)));
    return t.row != c.row ? 100 * std::abs((t.row == 1000 ? 9 : t.row) - (c.row == 1000 ? 9 : c.row)) + 50 : std::abs(t.col - c.col);
}
uint32_t update_items(uint64_t frame, bool inside, bool moved, int mx, int my, bool l_edge, uint32_t user_dpad, bool *eat_cross, uint32_t out) {
    if (g_isel != g_istab_sel) { g_istab_sel = g_isel; g_istab_n = 0; } else g_istab_n++;
    if (g_isel >= 0) g_sel_seen = frame;
    /* a pausa do X dura meio segundo: o tempo de uma janelinha aparecer (e virar menu, que tem prioridade). Sem isso, escolher varios
     * monstros seguidos (tributo) ficava pausado, porque a maozinha nao sai do monstro ja escolhido */
    if (g_hover_pause && g_pause_timed && frame > g_pause_until) { g_hover_pause = false; g_pause_timed = false; }
    /* duas rajadas seguidas sem o cursor andar, mesmo com o mouse passando por varias cartas (cada uma um alvo novo): alguma janelinha que
     * nao virou menu esta usando o direcional. Passar o mouse pausa (volta quando a maozinha mudar de lugar ou sumir e voltar); clicar vale */
    if (g_nm_from >= 0 && g_isel >= 0 && g_isel != g_nm_from) g_nm_moved = true;   /* andou em algum momento (mesmo que tenha voltado) */
    if (g_nm_from >= 0 && frame >= g_nm_check && g_isel >= 0) {
        g_nomove = g_nm_moved ? 0 : g_nomove + 1; g_nm_from = -1;
        if (g_nomove >= 2 && !g_pending && (g_imode == 1 || g_imode == 2)) {
            g_hover_pause = true; g_pause_sel = g_isel; g_pause_timed = false; g_tkx = g_tky = -1; g_blocked_axis = 0; g_nomove = 0;
            if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: o cursor nao anda: passar o mouse pausado\n", (unsigned long long)frame);
        }
    }
    if (g_hover_pause && g_isel != g_pause_sel && !g_pending) {   /* a maozinha mudou de lugar (ou sumiu e voltou) sem ser por nos */
        if (g_isel >= 0) { g_hover_pause = false; if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: passar o mouse liberado de novo\n", (unsigned long long)frame); }
        g_pause_sel = g_isel;
    }
    if (g_isel != g_unreach_sel) { g_unreach_sel = g_isel; g_unreach.clear(); }   /* o cursor andou: tenta de novo o que antes nao deu */
    int hover = -1;
    if (inside) for (size_t i = 0; i < g_items.size(); i++) if (mx >= g_items[i].x0 && mx < g_items[i].x1 && my >= g_items[i].y0 && my < g_items[i].y1) hover = (int)i;
    /* onde o mouse parou (por ate 30 quadros depois do ultimo movimento): se ele parou numa carta/lugar num momento em que o cursor do jogo
     * estava escondido (andando, animacao), o alvo e pego assim que o cursor reaparecer, mesmo sem o mouse mexer de novo */
    static int want_ax = -999, want_ay = -999; static uint64_t want_until;
    if (moved) { if (hover >= 0) { want_ax = g_items[hover].ax; want_ay = g_items[hover].ay; want_until = frame + 30; } else want_ax = -999; }
    if (user_dpad) { g_tkx = g_tky = -1; g_pending = false; want_ax = -999; return out; }
    int want = -1;
    if (frame <= want_until && want_ax != -999) for (size_t i = 0; i < g_items.size(); i++) if (g_items[i].ax == want_ax && g_items[i].ay == want_ay) want = (int)i;
    int tgt = -1; for (size_t i = 0; i < g_items.size(); i++) if (g_items[i].col == g_tkx && g_items[i].row == g_tky) tgt = (int)i;
    if (l_edge && hover >= 0 && g_isel >= 0 && g_imode >= 2 && unreachable(g_items[hover])) {
        *eat_cross = true;   /* clique numa zona que o cursor nao alcanca agora (ex.: vazia, ao escolher alvo): nao faz nada (nao confirma a carta atual) */
    } else if (l_edge && hover >= 0 && g_isel >= 0) {
        g_tkx = g_items[hover].col; g_tky = g_items[hover].row; g_taps = 0; trim_burst(frame);
        tgt = hover; g_pending = true; g_pending_until = frame + 150; *eat_cross = true; g_blocked_axis = 0; g_burst_from = -1; g_hover_pause = false;
        if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: clique na carta %d (coluna %d, linha %d); cursor na %d\n", (unsigned long long)frame, hover, g_tkx, g_tky, g_isel);
    } else if (g_imode >= 1 && !g_hover_pause && !g_pending && want >= 0 && want != tgt && want != g_isel && g_isel >= 0 && !unreachable(g_items[want])) {   /* passar o mouse: Deck Edit, duelo e mapa */
        g_tkx = g_items[want].col; g_tky = g_items[want].row; g_taps = 0; trim_burst(frame); g_blocked_axis = 0; g_burst_from = -1;
        tgt = want;
    }
    if (g_pending && frame > g_pending_until) g_pending = false;
    if (tgt < 0) { g_tkx = g_tky = -1; g_pending = false; return out; }
    if (burst_running(frame)) return out;
    if (g_burst_n && g_chain_n > 0 && g_tkx == g_chain_col && g_tky == g_chain_row) {
        /* loja: a horizontal terminou, a vertical sai ja (a grade nao tem buracos: subir/descer sempre anda); confere tudo no fim */
        g_burst_dir = g_chain_dir; g_burst_n = g_chain_n; g_burst_start = frame; g_burst_axis = 2; g_chain_n = 0;
        if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: loja: rajada vertical de %d encadeada (%02x)\n", (unsigned long long)frame, g_burst_n, g_burst_dir);
        return out | g_burst_dir;
    }
    g_chain_n = 0;
    if (g_burst_n) { g_burst_end = g_burst_start + (uint64_t)g_burst_n * g_period; g_last_burst_n = g_burst_n; g_burst_n = 0; }
    if (frame < g_pulse_end + 2 || frame < g_next_tap) return out;
    if (g_isel < 0) { if (frame - g_sel_seen >= 12) { g_pending = false; g_tkx = g_tky = -1; } return out; }   /* cursor sumiu: espera ate 12 quadros (animacao de um passo) e desiste */
    /* a maozinha/moldura responde 5-6 quadros depois do aperto (medido em 6 gravacoes de duelo); a rajada termina 4 quadros depois do aperto,
     * entao 3 quadros + 2 estaveis cobrem com folga (era 8: cada passo no campo levava 12 quadros, agora ~8) */
    /* mouse-fix-43: se o cursor ja mostrou todos os passos da ultima rajada, nao espera os 3 quadros extras (so 2 quadros estaveis) */
    bool ja_mostrou = false;
    if (g_burst_from >= 0 && g_isel >= 0 && g_burst_from < (int)g_items.size() && g_isel < (int)g_items.size()) {
        const Item &bf0 = g_items[g_burst_from];
        /* quantos passos o cursor ja mostrou: na mesma fileira, as colunas andadas; mudando de fileira, as fileiras andadas (grade do Deck
         * Edit); no duelo/mapa as fileiras nao sao seguidas (1000, -1...): ai 1. So segue cedo com TODOS os passos da rajada visiveis */
        const Item &ci = g_items[g_isel];
        int sh = 0;
        if (g_isel != g_burst_from) {
            if (ci.row == bf0.row) sh = std::abs(ci.col - bf0.col);
            else if (g_imode == 1) sh = std::abs(ci.row - bf0.row);
            else sh = 1;
        }
        ja_mostrou = g_isel != g_burst_from && sh >= (g_imode == 1 ? g_last_burst_n : (g_burst_axis == 2 || g_imode == 3 ? 1 : g_last_burst_n));
        if (g_imode != 1 && g_burst_axis == 2 && g_last_burst_n > 1) ja_mostrou = false;   /* varias fileiras de uma vez fora do Deck Edit: espera como antes */
    }
    if (g_imode != 2) ja_mostrou = false;   /* so no duelo (mao/campo): no Deck Edit e no mapa o desenho pode atrasar mais (testes com atraso 6), fica o ritmo antigo */
    if (g_istab_n < 2 || frame < g_burst_end + (ja_mostrou ? 1 : 3)) return out;   /* ja chegou: 1 quadro de folga (era sempre 3) */
    auto give_up = [&](const char *why) {
        if (g_imode >= 2 && tgt >= 0) {
            g_unreach.push_back({g_items[tgt].ax, g_items[tgt].ay});   /* nao insiste nesta ate o cursor andar */
            /* passando o mouse por uma CARTA e o cursor nao anda: alguma janelinha usa o direcional, pausa. Zona vazia que nao da para alcancar
             * (escolhendo alvo de ataque o cursor so para nos monstros) so fica marcada, sem pausar o resto */
            if (!g_pending && !g_items[tgt].zone && (g_imode == 1 || g_imode == 2)) { g_hover_pause = true; g_pause_sel = g_isel; g_pause_timed = false; }
        }
        g_pending = false; g_tkx = g_tky = -1; g_blocked_axis = 0;
        if (g_mtrace) fprintf(stderr, "[mouse] %s: desisti\n", why);
    };
    /* espera adaptativa: se o cursor ainda nao mudou desde a rajada, da mais tempo antes de concluir que ele nao andou (algum lugar do jogo pode
     * demorar mais que o medido); se ja mudou, segue no ritmo rapido */
    if (g_burst_from >= 0 && frame < g_burst_end + 10) {
        const Item &bf = g_items[std::min((size_t)g_burst_from, g_items.size() - 1)];
        /* Deck Edit (grade): mudando de fileira conta as fileiras andadas, e espera TODAS as da rajada (descer 2 fileiras e conferir na 1a
         * mandava outro passo e passava do ponto) */
        const int shown = g_isel == g_burst_from ? 0 : (g_items[g_isel].row == bf.row ? std::abs(g_items[g_isel].col - bf.col)
                                                         : (g_imode == 1 ? std::abs(g_items[g_isel].row - bf.row) : 1));
        if (shown < (g_imode == 1 ? g_last_burst_n : (g_burst_axis == 2 || g_imode == 3 ? 1 : g_last_burst_n))) return out;   /* o cursor ainda nao mostrou todos os passos mandados */
    }
    if (g_burst_from >= 0) {
        /* a ultima rajada nao mexeu o cursor: ou aquele eixo esta bloqueado (linha incompleta no fim da grade: tenta o outro eixo), ou outra coisa
         * esta usando o direcional (janelinha aberta: os dois eixos falham, desiste sem confirmar nada) */
        const bool mv = g_isel != g_burst_from; g_burst_from = -1;

        if (mv && g_imode == 2 && tgt != g_isel && g_burst_dist >= 0 && !g_escape_burst && item_dist(g_items[tgt], g_items[g_isel]) >= g_burst_dist) {
            give_up("o cursor andou mas nao chegou mais perto (o jogo pula esta zona)"); return out;
        }
        if (mv) g_blocked_axis = 0;
        else if (tgt != g_isel) {
            if (g_blocked_axis && g_blocked_axis != g_burst_axis) { give_up("o cursor nao andou"); return out; }
            g_blocked_axis = g_burst_axis;
        }
    }
    if (tgt == g_isel) {
        if (g_pending) { confirm_pulse(frame, "carta", tgt); return g_pulse; }
        g_tkx = g_tky = -1; return out;   /* passando o mouse: chegou, nao confirma */
    }
    if (g_taps >= (g_imode == 3 ? 17 : 10)) { give_up("passos demais"); return out; }
    const Item &t = g_items[tgt], &c = g_items[g_isel];
    if (g_imode == 3) {
        /* mapa: um passo na direcao (das que ainda nao falharam a partir deste lugar) que mais aponta para o alvo */
        /* pode precisar de desvio (o vizinho naquela direcao nem sempre aproxima): lembra, por lugar, as direcoes ja tentadas e os lugares
         * visitados neste caminho; voltar a um lugar ja visitado marca como tentada a direcao que trouxe de volta */
        static int path_tgt = -1, last_from = -1, last_dir = 0; static std::map<int, int> tried; static std::set<int> visited;
        if (path_tgt != tgt || g_taps == 0) { path_tgt = tgt; tried.clear(); visited.clear(); last_from = -1; }
        if (g_blocked_axis) { tried[g_isel] |= g_blocked_axis; g_blocked_axis = 0; }
        if (last_from >= 0 && last_from != g_isel && visited.count(g_isel)) tried[last_from] |= last_dir;
        visited.insert(g_isel);
        const int vx = t.ax - c.ax, vy = t.ay - c.ay;
        const int dirs[4] = {0x20, 0x80, 0x40, 0x10}; const int score[4] = {vx, -vx, vy, -vy};
        int bd = -1; for (int k = 0; k < 4; k++) if (!(tried[g_isel] & dirs[k]) && score[k] > 0 && (bd < 0 || score[k] > score[bd])) bd = k;
        if (bd < 0) for (int k = 0; k < 4; k++) if (!(tried[g_isel] & dirs[k]) && (bd < 0 || score[k] > score[bd])) bd = k;   /* desvio: afasta um pouco */
        if (bd < 0 || g_taps >= 16) { give_up("nenhuma direcao leva ao lugar"); return out; }
        tried[g_isel] |= dirs[bd]; last_from = g_isel; last_dir = dirs[bd];
        g_burst_dir = dirs[bd]; g_burst_n = 1; g_burst_axis = dirs[bd];
        g_burst_start = frame; g_taps++; g_burst_from = g_isel; g_burst_dist = item_dist(t, c);
        if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: lugar %d -> %d, passo (%02x)\n", (unsigned long long)frame, g_isel, tgt, g_burst_dir);
        return out | g_burst_dir;
    }
    const int dx = t.col - c.col, dy = t.row - c.row;
    int axis = dx && g_blocked_axis != 1 ? 1 : dy && g_blocked_axis != 2 ? 2 : 0;
    if (!axis && g_imode == 2 && t.row != c.row && g_blocked_axis == 2 && t.ax != c.ax) axis = 1;   /* nao sobe/desce daqui: tenta pelo lado */
    if (g_imode == 2 && dy && g_blocked_axis != 2) axis = 2;   /* duelo: primeiro chega na fileira certa */
    if (!axis) { give_up("sem caminho ate a carta"); return out; }
    /* no campo do duelo, um passo por vez: conforme o momento o jogo para nas zonas vazias (olhando o campo) ou pula elas (escolhendo alvo de
     * ataque), entao contar colunas pode passar do alvo. Na mao (sem buracos) vai de uma vez */
    if (axis == 1 && g_imode == 2 && t.row != c.row) {   /* fileiras diferentes e subir/descer nao deu (ex.: saindo do banimento): vai para o lado em que o alvo esta na tela */
        if (t.ax == c.ax) { give_up("sem caminho ate a carta"); return out; }
        g_burst_dir = t.ax > c.ax ? 0x20 : 0x80; g_burst_n = 1; g_escape_burst = true;   /* desvio: pode afastar de fileira; a conferencia de 'chegou mais perto' nao vale para ele */
    } else g_escape_burst = false;
    if (axis == 1 && g_imode == 2 && t.row != c.row) { /* ja feito acima */ }
    else if (axis == 1) {
        g_burst_dir = dx > 0 ? 0x20 : 0x80; g_burst_n = (g_imode == 2 && c.row != 1000) ? 1 : std::abs(dx);
        if (g_is_shop && dy && !g_blocked_axis) { g_chain_dir = dy > 0 ? 0x40 : 0x10; g_chain_n = std::abs(dy); g_chain_col = t.col; g_chain_row = t.row; }
    }
    else { g_burst_dir = dy > 0 ? 0x40 : 0x10; g_burst_n = g_imode == 2 ? 1 : std::abs(dy); }   /* duelo: as fileiras tem buracos e o jogo escolhe a coluna ao trocar de fileira: um passo por vez */
    g_burst_axis = axis; g_burst_start = frame; g_taps++; g_burst_from = g_isel; g_burst_dist = item_dist(t, c);
    /* so tentativas em direcao a CARTAS contam (zona vazia que o cursor nao alcanca e normal, ex.: escolhendo alvo de ataque); nao adia */
    if (g_nm_from < 0 && !t.zone) { g_nm_from = g_isel; g_nm_moved = false; g_nm_check = frame + (uint64_t)g_burst_n * g_period + 10; }
    if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: carta %d -> %d, rajada de %d (%02x)%s\n", (unsigned long long)frame, g_isel, tgt, g_burst_n, g_burst_dir, g_pending ? "" : " (passando o mouse)");
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
        if (!g_items.empty()) return update_items(frame, inside, moved, mx, my, l_edge, user_dpad, eat_cross, out);
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
    /* mouse-fix-44: o ponteiro mexeu ha pouco (ate 20 quadros) e esta parado numa opcao de uma lista que acabou de aparecer (janelinha abrindo,
     * menu redesenhado): leva o realce ate ela sem precisar mexer de novo (como as cartas ja faziam). Ponteiro largado ha mais tempo: nao mexe */
    if (moved) g_list_moved_until = frame + 20;
    if (user_dpad) { g_tkx = g_tky = -1; g_pending = false; g_list_moved_until = 0; return out; }   /* o jogador voltou ao teclado/controle */
    int tgt = -1; for (size_t i = 0; i < g_list.size(); i++) if (g_list[i].x0 == g_tkx && g_list[i].y0 == g_tky) tgt = (int)i;
    if (l_edge && hover >= 0) {
        /* clique numa barra: este modulo e dono do clique (quem confirma e ele, quando a selecao chegar); o X cru fica bloqueado ate soltar o botao */
        if (hover != tgt) set_target(hover, frame);
        tgt = hover; g_pending = true; g_pending_until = frame + 120; *eat_cross = true;
    } else if (hover >= 0 && hover != tgt && !g_pending && (moved || (frame <= g_list_moved_until && hover != g_sel))) {   /* mexendo: como antes; parado ha pouco: so se a opcao ainda nao esta selecionada */
        /* so passar o mouse: leva a selecao ate a barra. Mexer DENTRO da mesma barra nao reinicia nada; com um clique pendente o alvo fica
         * travado (o mouse escorregar para a vizinha nao muda o item que vai ser confirmado) */
        set_target(hover, frame); tgt = hover;
    }
    if (g_pending && frame > g_pending_until) g_pending = false;
    if (tgt < 0) { g_pending = false; return out; }
    if (burst_running(frame)) return out;   /* rajada em andamento */
    if (g_burst_n) { g_burst_end = g_burst_start + (uint64_t)g_burst_n * g_period; g_last_burst_n = g_burst_n; g_burst_n = 0; }
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
    /* espera adaptativa: com o realce ja tendo mudado desde a rajada, kSettle basta; se ainda nao mudou, espera como antes (12) para nao
     * apertar de novo e passar do ponto num menu mais lento */
    int progressed = 0;   /* quantos dos apertos da ultima rajada o realce ja mostrou */
    if (g_burst_sel0 >= 0 && g_sel >= 0) for (int i = std::min(g_sel, g_burst_sel0); i <= std::max(g_sel, g_burst_sel0); i++) if (i != g_burst_sel0 && !(i < (int)g_skip.size() && g_skip[i])) progressed++;
    /* mouse-fix-43: com o realce ja mostrando TODOS os passos da rajada (e 2 quadros estaveis, conferido acima), segue na hora: a folga de
     * kSettle so valia para o caso de o realce ainda nao ter mudado. Medido (sessao 141220): o realce muda 5 quadros depois do aperto e o
     * mouse esperava ate o quadro 10; agora cada passo/confirmacao sai ~4 quadros antes. Sem rajada conhecida (sel0 < 0), como antes */
    if (g_burst_sel0 >= 0 && progressed >= g_last_burst_n) { if (frame < g_burst_end) return out; }
    else if (frame < g_burst_end + (progressed < g_last_burst_n ? 12 : kSettle)) return out;
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
    g_burst_n = std::max(1, steps); g_burst_start = frame; g_taps++; g_burst_sel0 = g_sel;
    if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: selecionada %d -> alvo %d, rajada de %d (%02x)\n", (unsigned long long)frame, g_sel, tgt, g_burst_n, g_burst_dir);
    return out | g_burst_dir;
}

namespace {
/* ---- abas do Deck Edit (Deck, Fusion, Side, Trunk) ----
 * Medido em 4 gravacoes: quatro quads 3D no topo (altura 33; Deck/Fusion/Side com 65 de largura, Trunk com 129), a aba ativa desenhada 2 px mais
 * ALTA (y 22 em vez de 24). O quadrado (botao 0x8000) passa para a proxima, sempre na ordem Deck -> Fusion -> Side -> Trunk -> Deck. Clicar numa aba:
 * aperta o quadrado as vezes que faltam e confere. A area clicavel fica so na faixa dos nomes (y 20-44), acima da primeira fileira de cartas. */
struct Tab { int x0, x1; };
std::vector<Tab> g_tabs; int g_tab_active = -1;
void analyze_tabs() {
    g_tabs.clear(); g_tab_active = -1;
    std::vector<Quad> t;
    for (const Quad &q : g_quads) {
        const int w = q.x1 - q.x0, h = q.y1 - q.y0;
        if (h < 32 || h > 34 || q.y0 < 18 || q.y0 > 28 || w < 50 || w > 140 || q.x0 < 120 || q.x0 > 360) continue;
        bool dup = false; for (const Quad &o : t) if (std::abs(o.x0 - q.x0) <= 2) dup = true;
        if (!dup) t.push_back(q);
    }
    if (t.size() != 4) return;
    std::sort(t.begin(), t.end(), [](const Quad &a, const Quad &b) { return a.x0 < b.x0; });
    int ymin = 1 << 30, nmin = 0, act = -1;
    for (size_t i = 0; i < 4; i++) { if (t[i].y0 < ymin) { ymin = t[i].y0; nmin = 1; act = (int)i; } else if (t[i].y0 == ymin) nmin++; }
    if (nmin != 1) return;   /* nenhuma (ou mais de uma) mais alta: nao da para saber a ativa */
    for (size_t i = 0; i < 4; i++) {   /* as abas se sobrepoem um pouco: cada uma fica com a sua metade */
        const int a = i == 0 ? t[i].x0 : (t[i - 1].x1 + t[i].x0) / 2, b = i == 3 ? std::min(t[i].x1, 470) : (t[i].x1 + t[i + 1].x0) / 2;
        g_tabs.push_back({a, b});
    }
    g_tab_active = act;
}
int g_tab_target = -1, g_tab_tries; uint64_t g_tab_next, g_tab_until;
/* um clique esquerdo na faixa das abas (devolve true se tomou o clique); a cada quadro, os apertos do quadrado que faltam */
bool tab_click(uint64_t frame, int mx, int my) {
    if (g_tabs.size() != 4 || g_tab_active < 0 || my < 20 || my >= 44) return false;
    for (int i = 0; i < 4; i++) if (mx >= g_tabs[i].x0 && mx < g_tabs[i].x1) {
        if (i != g_tab_active) { g_tab_target = i; g_tab_tries = 0; g_tab_next = frame; g_tab_until = frame + 240; }
        if (g_mtrace) fprintf(stderr, "[mouse] quadro %llu: clique na aba %d (ativa %d)\n", (unsigned long long)frame, i, g_tab_active);
        return true;
    }
    return false;
}
uint32_t tab_step(uint64_t frame) {
    if (g_tab_target < 0) return 0;
    if (frame > g_tab_until || g_tabs.size() != 4 || g_tab_active < 0) { if (frame > g_tab_until || g_tabs.empty()) g_tab_target = -1; return 0; }
    if (g_tab_active == g_tab_target) { g_tab_target = -1; return 0; }
    if (frame < g_tab_next) return frame < g_tab_next - 8 ? 0x8000u : 0u;   /* aperto em andamento (3 quadros) e espera o desenho mudar */
    if (++g_tab_tries > 8) { g_tab_target = -1; return 0; }
    g_tab_next = frame + 3 + 8;   /* 3 quadros apertado + 8 de espera (a aba muda ~4 quadros depois do aperto) */
    return 0x8000u;
}
}
/* ---- botoes do mouse -> botoes do PSP ---- */
namespace {
const uint32_t kMouseBit[MUI_NB] = {0x4000, 0x2000, 0x1000, 0x100, 0x200};   /* esquerdo = X, direito = O, meio = triangulo, laterais = L / R */
const int kMinHold = 3;   /* o jogo le o controle a cada 2 quadros: um clique mais curto que isso pode passar entre duas leituras */
bool g_mprev[MUI_NB]; uint64_t g_mhold_until[MUI_NB]; bool g_owned;
}

uint32_t mui_mouse(uint64_t frame, const MuiMouse &m, bool menus, uint32_t user_buttons) {
    const uint32_t user_dpad = user_buttons & 0xf0;
    {   /* gravacao F4: posicao do mouse a cada mudanca (para estudar o 'passar o mouse') */
        const int nx = m.inside ? m.mx : -1, ny = m.inside ? m.my : -1;
        if (g_dump_on && g_dump && (nx != g_dump_mx || ny != g_dump_my)) { int n = fprintf(g_dump, "M %llu %d %d\n", (unsigned long long)frame, nx, ny); g_dump_bytes += n > 0 ? n : 0; }
        g_dump_mx = nx; g_dump_my = ny;
    }
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
        if (edge[MUI_BL] && m.inside && tab_click(frame, m.mx, m.my)) { eat = true; b |= mui_update(frame, m.inside, m.mx, m.my, false, down[MUI_BL], user_dpad, &eat); eat = true; }
        else b |= mui_update(frame, m.inside, m.mx, m.my, edge[MUI_BL], down[MUI_BL], user_dpad, &eat);
        b |= tab_step(frame);
        if (edge[MUI_BL]) g_owned = eat;
    } else if (edge[MUI_BL]) g_owned = false;
    if (!down[MUI_BL]) g_owned = false;
    for (int i = 0; i < MUI_NB; i++) if (down[i] && !(i == MUI_BL && g_owned)) b |= kMouseBit[i];
    /* cartas do duelo: confirmar (X) pode abrir uma janelinha -> pausa o 'passar o mouse'; cancelar (O) fecha -> libera */
    static uint32_t prev_all;
    const uint32_t all = b | user_buttons, rise = all & ~prev_all; prev_all = all;
    if (rise & 0x4000) { g_hover_pause = true; g_pause_sel = g_isel; g_pause_timed = true; g_pause_until = frame + 30; }
    if (rise & 0x2000) g_hover_pause = false;
    if (g_dump_on && g_dump) {   /* gravacao F4: o que o modulo enxergou e decidiu (cartas, cursor, pausa, alvo), a cada mudanca */
        static std::string last;
        char st[160]; snprintf(st, sizeof st, "%d %zu %d %d %d %d %d", g_imode, g_items.size(), g_isel, g_isel >= 0 ? g_items[g_isel].row : -1, (int)g_hover_pause, g_tkx, g_tky);
        if (last != st) { last = st; int n = fprintf(g_dump, "U %llu %s\n", (unsigned long long)frame, st); g_dump_bytes += n > 0 ? n : 0; }
    }
    return b;
}
