/* Mouse nos menus: acha as barras de menu desenhadas no quadro exibido (sprites 2D em 3 pedacos: ponta, meio, ponta), descobre qual esta
 * selecionada pelo desenho (a barra cujo conjunto de sprites difere das outras do mesmo tamanho) e, quando o mouse mexe/clica numa barra,
 * aperta o direcional ate a selecao chegar nela (e confirma com X no clique). O jogo so conhece direcional e botoes: nada e escrito na
 * memoria dele. See docs/ARCHITECTURE.md. */
#pragma once
#include <stdint.h>

/* chamado pelo GE a cada PRIM de sprite 2D texturizado (dois vertices, modo through) */
void mui_prim(uint32_t fb, int x0, int y0, int x1, int y1, float u0, float v0, float u1, float v1, uint32_t tex, uint32_t col);
/* varios sprites num PRIM so (linha de texto): retangulo que os envolve */
void mui_run(uint32_t fb, int x0, int y0, int x1, int y1);
/* sceDisplaySetFrameBuf: o buffer vira o quadro exibido; seus sprites viram a tela atual */
void mui_set_frame(uint32_t fb);
/* uma vez por quadro (leitura do teclado/mouse). (mx,my) = mouse em coordenadas 480x272 (inside = sobre a imagem), l_edge = clique esquerdo novo,
 * l_down = botao esquerdo apertado (nao usado), user_dpad = direcional que o jogador esta apertando de verdade. Devolve botoes do PSP a acrescentar;
 * *eat_cross = true (so no quadro do clique): o modulo tomou este clique; ele NAO deve virar X ate o botao ser solto (o modulo confirma quando a
 * selecao chegar na barra). */
uint32_t mui_update(uint64_t frame, bool inside, int mx, int my, bool l_edge, bool l_down, uint32_t user_dpad, bool *eat_cross);

/* Botoes do mouse do esquema de PC (esquerdo = X, direito = O, meio = triangulo, laterais = L/R), mais o mouse nos menus. Uma vez por quadro.
 * down[] = botao (logico) apertado agora; clicked[] = a janela recebeu WM_xBUTTONDOWN desde a leitura anterior (pega o clique curto que comeca e
 * termina entre duas leituras). Garante que todo clique fica apertado pelo menos alguns quadros (o jogo le o controle a cada 2) e que um clique
 * tomado pelo mouse nos menus nunca vira X por conta propria (nem depois que a tela troca, com o botao ainda apertado). */
enum { MUI_BL, MUI_BR, MUI_BM, MUI_BX1, MUI_BX2, MUI_NB };
struct MuiMouse { bool in_client, inside; int mx, my; bool down[MUI_NB], clicked[MUI_NB]; };
uint32_t mui_mouse(uint64_t frame, const MuiMouse &m, bool menus, uint32_t user_buttons);   /* user_buttons = botoes do teclado/controle deste quadro */

/* Gravacao para desenvolver o mouse em telas que nao sao listas (Deck Edit, duelo): F4 liga/desliga. Grava, por quadro exibido, os sprites 2D,
 * as linhas de texto e os quads 3D texturizados (cartas), mais os botoes e o mouse, em logs/<sessao>_mouse.txt. */
void mui_quad(uint32_t fb, int x0, int y0, int x1, int y1, uint32_t tex);   /* quad 3D texturizado (carta no campo...), retangulo na tela */
bool mui_dump_toggle(void);    /* devolve o estado novo */
bool mui_dump_on(void);
void mui_dump_buttons(uint64_t frame, uint32_t buttons);   /* uma vez por quadro: botoes do PSP que foram para o jogo */
