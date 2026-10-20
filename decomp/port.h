/* Enderecos do jogo europeu -> versao que este executavel roda, na hora de COMPILAR (as partes reescritas usam enderecos em constexpr).
 * No executavel europeu nao ha tabela: PORT(a) == a. Nos de outra versao o build passa -Igen/portes/<VER> e portes_decomp.h (gerado por
 * tools/gerar_portes.py a partir de tools/enderecos_<VER>.txt, feito por tools/portar_mods.py) traz os pares e diz que modulos podem ligar. */
#pragma once
#include <stdint.h>
#if defined(__has_include)
#if __has_include("portes_decomp.h")
#include "portes_decomp.h"
#endif
#endif
constexpr uint32_t PORT(uint32_t a) {
#ifdef TF_PORT_TABLE
    int lo = 0, hi = kPortesN - 1;
    while (lo <= hi) { const int m = (lo + hi) / 2; if (kPortes[m].eu == a) return kPortes[m].outra; if (kPortes[m].eu < a) lo = m + 1; else hi = m - 1; }
#endif
    return a;   /* europeu, ou valor que nao e endereco */
}
#ifndef TF_PORT_LIBTITLE_OK
#define TF_PORT_LIBTITLE_OK 0
#endif
