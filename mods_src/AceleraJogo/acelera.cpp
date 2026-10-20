/* Game Speed: acelera o jogo inteiro (padrao 1,3x) mantendo a musica e os efeitos sonoros na velocidade e no tom normais. Usa set_speed
 * (API de plugins v6: ritmo do limitador de quadros) e set_music_normal (v7: as trilhas de som do jogo esperam o tempo real de cada bloco).
 * No menu Mods da janela do jogo: a velocidade (100% = normal, ate 300%) e "Music at normal speed" (desligado = o som tambem acelera,
 * mais agudo). Guardado em config.txt, na pasta do mod. Nao usa nenhum endereco do jogo: vale para qualquer versao. */
#include "tfmod.h"
#include <stdio.h>
#include <wchar.h>

extern "C" {
__declspec(dllexport) const char *tfmod_game_ids = "ULES-00600,ULUS-10136";   /* sem enderecos do jogo: vale para as versoes que o projeto roda */
}

static const TfModApi *A;
static int g_pct = 130, g_music = 1;

static void path(wchar_t *out, size_t n) { swprintf(out, n, L"%ls\\config.txt", A->mod_dir); }
static void load(void) {
    wchar_t p[1024]; path(p, 1024);
    if (FILE *f = _wfopen(p, L"rb")) {
        int v, m;
        if (fscanf(f, "speed=%d\n", &v) == 1 && v >= 100 && v <= 300) g_pct = v;
        if (fscanf(f, "music_normal=%d", &m) == 1) g_music = m ? 1 : 0;
        fclose(f);
    }
}
static void save(void) {
    wchar_t p[1024]; path(p, 1024);
    if (FILE *f = _wfopen(p, L"wb")) { fprintf(f, "speed=%d\nmusic_normal=%d\n", g_pct, g_music); fclose(f); }
}
static int get_speed(void) { return g_pct; }
static void set_speed(int v) { g_pct = v; A->set_speed(g_pct); save(); A->log("velocidade: %d%%", g_pct); }
static int get_music(void) { return g_music; }
static void set_music(int on) { g_music = on ? 1 : 0; A->set_music_normal(g_music); save(); }

extern "C" __declspec(dllexport) int tfmod_init(const TfModApi *api) {
    A = api;
    if (api->version < 7) { api->log("este mod precisa da versao 7 da API (runtime mais novo); ignorado"); return 1; }
    load();
    api->set_music_normal(g_music);
    api->set_speed(g_pct);
    api->add_slider(L"Game speed (100 = normal)", L"%", 100, 300, 130, get_speed, set_speed);
    api->add_toggle(L"Music at normal speed", 0, get_music, set_music);
    api->log("ligado: jogo a %d%%, musica normal: %s", g_pct, g_music ? "sim" : "nao");
    return 0;
}
