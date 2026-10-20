"""Gera gen/portes/<VER>/portes_decomp.h a partir de tools/enderecos_<VER>.txt (tools/portar_mods.py): os enderecos que as partes reescritas
do jogo (decomp/<modulo>) usam, europeu -> outra versao, para compilar essas partes com os enderecos daquela versao (decomp/port.h).
Uso: python tools/gerar_portes.py ULUS10136
Para cada modulo do decomp diz se ele pode ser ligado nessa versao: todos os seus enderecos achados E deslocamentos coerentes. So reescreve
o arquivo quando muda (nao recompila a toa)."""
import os, sys, re, collections
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ver = sys.argv[1]
src = os.path.join(ROOT, 'tools', 'enderecos_%s.txt' % ver)
out_dir = os.path.join(ROOT, 'gen', 'portes', ver); os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, 'portes_decomp.h')
pares = {}; total = collections.Counter(); achados = collections.Counter(); estranhos = collections.defaultdict(list)
if os.path.isfile(src):
    for ln in open(src, encoding='utf-8'):
        m = re.match(r'\s*0x([0-9a-fA-F]{8}) -> (0x([0-9a-fA-F]{8})|NAO ACHADO).*usado por (.*)$', ln)
        if not m: continue
        eu = int(m.group(1), 16); us = int(m.group(3), 16) if m.group(3) else None
        for q in m.group(4).split(','):
            q = q.strip()
            if not q.startswith('decomp:'): continue
            mod = q[7:]; total[mod] += 1
            if us is None: continue
            achados[mod] += 1; pares[eu] = us
            # coerencia: dentro do proprio modulo/BOOT o deslocamento e pequeno; variaveis grandes podem andar mais
            lim = 0x4000 if eu < 0x09000000 else 0x20000
            if abs(us - eu) > lim: estranhos[mod].append('0x%08x -> 0x%08x' % (eu, us))
ok = {m: total[m] > 0 and achados[m] == total[m] and not estranhos[m] for m in total}
linhas = ['/* gerado por tools/gerar_portes.py a partir de tools/enderecos_%s.txt: NAO editar */' % ver, '#pragma once', '#include <stdint.h>',
          '#define TF_PORT_TABLE 1', 'struct TfPorte { uint32_t eu, outra; };',
          'static constexpr TfPorte kPortes[] = {']
for eu in sorted(pares): linhas.append('    {0x%08xu, 0x%08xu},' % (eu, pares[eu]))
if not pares: linhas.append('    {0u, 0u},')
linhas.append('};')
linhas.append('static constexpr int kPortesN = %d;' % (len(pares) if pares else 0))
for m in sorted(total): linhas.append('#define TF_PORT_%s_OK %d   /* %d de %d enderecos achados%s */' % (m.upper(), 1 if ok[m] else 0, achados[m], total[m], ', deslocamentos estranhos: ' + ' '.join(estranhos[m][:5]) if estranhos[m] else ''))
txt = '\n'.join(linhas) + '\n'
if not os.path.isfile(out) or open(out, encoding='utf-8').read() != txt: open(out, 'w', encoding='utf-8').write(txt)
for m in sorted(total): print('[portes %s] decomp/%s: %d de %d enderecos -> %s' % (ver, m, achados[m], total[m], 'LIGADO nesta versao' if ok[m] else 'desligado (falta endereco ou deslocamento estranho)'))
if not total: print('[portes %s] sem tabela (rode tools/portar_mods.py): as partes reescritas ficam desligadas nesta versao' % ver)
