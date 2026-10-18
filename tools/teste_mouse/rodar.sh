#!/bin/sh
# Testa runtime/mouse_ui.cpp sem o jogo: um menu simulado (le o controle a cada 2 quadros, realce com atraso) recebe o mouse.
# Uso (Git Bash/MSYS2 ou Linux): sh tools/teste_mouse/rodar.sh [quantidade de casos aleatorios, padrao 500]
# Gravacao de verdade (F4 no jogo): g++ -std=c++17 -Iruntime tools/teste_mouse/reproduzir_gravacao.cpp -o /tmp/rep && /tmp/rep gen/logs/<sessao>_mouse.txt
set -e
cd "$(dirname "$0")"
R=../../runtime; N=${1:-500}; T=${TMPDIR:-/tmp}/teste_mouse; mkdir -p "$T"
g++ -std=c++17 -O1 -I$R cenarios.cpp $R/mouse_ui.cpp -o "$T/cenarios"
g++ -std=c++17 -O1 -I$R aleatorio.cpp $R/mouse_ui.cpp -o "$T/aleatorio"
g++ -std=c++17 -O1 -I$R apagados.cpp $R/mouse_ui.cpp -o "$T/apagados"
g++ -std=c++17 -O1 -I$R cartas.cpp $R/mouse_ui.cpp -o "$T/cartas"
g++ -std=c++17 -O1 -I$R campo.cpp $R/mouse_ui.cpp -o "$T/campo"
echo "== cenarios =="; for i in 1 2 3 4 5 6 7 8 9 10 11; do "$T/cenarios" $i; done
echo "== $N casos aleatorios (um processo por caso: o modulo guarda estado) =="
: > "$T/r.txt"; s=1; while [ $s -le $N ]; do "$T/aleatorio" $s >> "$T/r.txt"; s=$((s+1)); done
ok3=$(grep -v 'n=2 ' "$T/r.txt" | grep -c '^1 ' || true); tot3=$(grep -vc 'n=2 ' "$T/r.txt" || true)
ok2=$(grep 'n=2 ' "$T/r.txt" | grep -c '^1 ' || true); tot2=$(grep -c 'n=2 ' "$T/r.txt" || true)
echo "listas de 3+ itens: $ok3/$tot3 corretos (deve ser 100%)"
echo "listas de 2 itens:  $ok2/$tot2 (sem uma lista maior antes, o modulo nao tem como reconhecer o realce)"
echo "== $N menus com itens apagados (o jogo os pula), clicando e so passando o mouse =="
: > "$T/a.txt"; s=1; while [ $s -le $N ]; do "$T/apagados" $s 0 >> "$T/a.txt"; "$T/apagados" $s 1 >> "$T/a.txt"; s=$((s+1)); done
oka=$(grep -c '^1 ' "$T/a.txt" || true); tota=$(wc -l < "$T/a.txt")
echo "menus com itens apagados: $oka/$tota corretos (deve ser 100%)"
echo "== $N cliques em cartas: Deck Edit, mao do duelo, mao com janelinha aberta =="
: > "$T/c.txt"; s=1; while [ $s -le $N ]; do "$T/cartas" $s 0 >> "$T/c.txt"; "$T/cartas" $s 1 >> "$T/c.txt"; "$T/cartas" $s 2 >> "$T/c.txt"; s=$((s+1)); done
okc=$(grep -c '^1 ' "$T/c.txt" || true); totc=$(wc -l < "$T/c.txt")
echo "cartas: $okc/$totc corretos (deve ser 100%)"
echo "== $N cliques no campo do duelo (monstros e magicas/armadilhas seus, monstros do oponente, mao) =="
: > "$T/f.txt"; s=1; while [ $s -le $N ]; do "$T/campo" $s >> "$T/f.txt"; s=$((s+1)); done
okf=$(grep -c '^1 ' "$T/f.txt" || true); totf=$(wc -l < "$T/f.txt")
echo "campo: $okf/$totf corretos (deve ser 100%)"
[ "$ok3" = "$tot3" ] && [ "$oka" = "$tota" ] && [ "$okc" = "$totc" ] && [ "$okf" = "$totf" ]
