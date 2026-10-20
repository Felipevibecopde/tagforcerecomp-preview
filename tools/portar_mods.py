"""Porta os enderecos que os mods (mods_src/*) usam do jogo europeu (ULES-00600) para o americano (ULUS-10136).

Uso (MSYS2, na pasta do projeto, com as DUAS versoes instaladas: gen/jogo e gen/jogo_ULUS10136):
    python tools/portar_mods.py
Escreve tools/enderecos_ULUS10136.txt (uma linha por endereco: europeu -> americano, e como foi achado).

Como acha:
  * funcao (gancho, chamada): o 'formato' das instrucoes a partir do endereco europeu, ignorando o que muda entre as versoes (alvos de jal/j e
    os imediatos de lui/addiu/ori/load/store, que sao enderecos), e procurado no mesmo modulo americano; vale so se houver UM lugar igual.
  * dado (tabela, variavel, save): acha no codigo europeu as instrucoes que montam esse endereco (par lui + addiu/load/store), acha a mesma
    vizinhanca de codigo no americano e le o endereco que ela monta la. Varios lugares votam; vale a resposta da maioria.
Os modulos sao carregados com tools/pspmod.py (o mesmo carregador da traducao)."""
import os, sys, re, struct, glob, collections
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pspmod import load_module

def rows(tsv):
    seen = set(); out = []
    for ln in open(os.path.join(ROOT, 'tools', tsv), encoding='utf-8'):
        if ln.startswith('#') or not ln.strip(): continue
        n, base, iso = ln.rstrip('\n').split('\t')[:3]
        if n in seen: continue   # variantes de base: a 1a basta para comparar
        seen.add(n); out.append((n, int(base, 16), iso))
    return out

def carregar(jogo, tsv):
    mods = {}
    for n, b, iso in rows(tsv):
        p = os.path.join(jogo, 'PSP_GAME', *iso.split('/'))
        if not os.path.isfile(p): continue
        try:
            img, _, _ = load_module(p, b)
        except SystemExit as e:
            print('  aviso: nao carreguei %s (%s)' % (n, e)); continue
        w = list(struct.unpack_from('<%dI' % (len(img) // 4), img))
        mods[n] = (b, w)
    return mods

MASK_IMM = {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2b, 0x31, 0x35, 0x39, 0x3d}
def mascara(w):
    op = w >> 26
    if op in (2, 3): return w & 0xfc000000
    if op in MASK_IMM: return w & 0xffff0000
    return w

class Mod:
    def __init__(self, base, words):
        self.base, self.w = base, words
        self.m = [mascara(x) for x in words]
        self.idx = collections.defaultdict(list)
        for i, x in enumerate(self.m): self.idx[x].append(i)
    def has(self, a): return self.base <= a < self.base + 4 * len(self.w)
    def i(self, a): return (a - self.base) // 4
    def find(self, pat):
        if not pat: return []
        res = []
        for i in self.idx.get(pat[0], ()):
            if self.m[i:i + len(pat)] == pat: res.append(i)
            if len(res) > 1: break
        return res

def funcoes_traduzidas(gen_dir):
    """enderecos que a traducao do projeto sabe CHAMAR em cada modulo: a tabela <modulo>_fns[] de gen[/<VER>]/<modulo>.c (comecos de funcao
    E entradas extras de funcoes com mais de uma entrada). E o que o jogo usa para chamar/prender uma funcao."""
    fns = collections.defaultdict(set)
    for f in glob.glob(os.path.join(gen_dir, '*.c')):
        txt = open(f, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'const FnEntry (\w+)_fns\[\] = \{(.*?)\};', txt, re.S):
            for a in re.finditer(r'\{0x([0-9a-f]{8}),', m.group(2)): fns[m.group(1)].add(int(a.group(1), 16))
    return fns

def portar_codigo(eu, us, a):
    i = eu.i(a)
    for n in (24, 48, 96, 160):
        pat = eu.m[i:i + n]
        if len(pat) < n: pat = eu.m[i:]
        r = us.find(pat)
        if len(r) == 1: return us.base + 4 * r[0], 'codigo (%d instrucoes iguais)' % len(pat)
        if not r: return None, None
    return None, None

def portar_parecido(eu, us, a, deltas):
    """Sem igual exato: preve o lugar pelo deslocamento das funcoes vizinhas ja portadas (mesmo modulo) e procura, ali perto, o trecho mais
    PARECIDO (64 instrucoes, mascaradas). Vale so se for claramente o melhor: >= 80% iguais e 10 pontos acima do segundo."""
    viz = sorted(deltas, key=lambda t: abs(t[0] - a))[:6]
    if not viz: return None, None
    i = eu.i(a); N = 64; pat = eu.m[i:i + N]
    if len(pat) < 16: return None, None
    import difflib
    cands = sorted({d for _, d in viz})
    melhor = []; vistos = set()
    for d in cands:
        c0 = us.i(a + d)
        for j in range(max(0, c0 - 1024), min(len(us.m) - len(pat), c0 + 1024)):
            if j in vistos or us.m[j] != pat[0]: continue
            vistos.add(j)
            # alinhado (instrucoes a mais/a menos no meio nao 'escorregam' o resto): quantas instrucoes do trecho europeu aparecem em ordem
            sm = difflib.SequenceMatcher(None, pat, us.m[j:j + len(pat) + 16], autojunk=False)
            igual = sum(b.size for b in sm.get_matching_blocks())
            melhor.append((igual, j))
    if not melhor: return None, None
    melhor = sorted(set(melhor), reverse=True)
    top = melhor[0]; seg = next((m for m in melhor[1:] if abs(m[1] - top[1]) > 2), (0, -1))
    pct = 100 * top[0] // len(pat); pct2 = 100 * seg[0] // len(pat)
    ULTIMO_PARECIDO[a] = (pct, pct2)
    if (pct >= 75 and pct - pct2 >= 15) or (pct >= 65 and pct - pct2 >= 35):   # bem parecido, ou o melhor MUITO a frente do 2o
        return us.base + 4 * top[1], 'codigo parecido (%d%% igual, o 2o melhor %d%%)' % (pct, pct2)
    return None, None
ULTIMO_PARECIDO = {}

def e_funcao(EU, a):
    """o endereco e o comeco de uma funcao em algum modulo europeu (prologo addiu sp,sp,-N nas 2 primeiras instrucoes)?"""
    for eu in EU.values():
        if eu.has(a):
            i = eu.i(a)
            for k in (0, 1):
                if i + k < len(eu.w):
                    w = eu.w[i + k]
                    if (w & 0xffff0000) == 0x27bd0000 and (w & 0x8000): return True
    return False

def refs_dado(mod, a, perto=0x40):
    """lugares (indice da instrucao 'lo', delta) onde o codigo monta a (ou a - ate 'perto' bytes: base de estrutura + campo)"""
    out = []
    w = mod.w; last_lui = {}
    for i, x in enumerate(w):
        op = x >> 26
        if op == 0x0f:
            last_lui[(x >> 16) & 31] = (i, (x & 0xffff) << 16); continue
        if op in (0x09, 0x0d, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2b, 0x31, 0x39):
            rs = (x >> 21) & 31
            if rs in last_lui and i - last_lui[rs][0] <= 12:
                lo = x & 0xffff
                v = last_lui[rs][1] + (lo | (0 if op == 0x0d else (-0x10000 if lo & 0x8000 else 0))) if op != 0x0d else last_lui[rs][1] | lo
                v &= 0xffffffff
                if 0 <= a - v <= perto: out.append((i, last_lui[rs][0], a - v))
        if op == 0 and (x & 0x3f) == 8 and ((x >> 21) & 31) == 31: last_lui.clear()   # jr ra: fim de funcao
    return out

def valor_em(mod, i_lo, i_lui):
    x, h = mod.w[i_lo], mod.w[i_lui]
    if (h >> 26) != 0x0f: return None
    op = x >> 26; lo = x & 0xffff
    if op == 0x0d: return ((h & 0xffff) << 16) | lo
    return (((h & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xffffffff

def portar_dado(EU, US, a, perto=0x40, unanime=False, minimo=2):
    votos = collections.Counter(); onde = {}
    for nome, eu in EU.items():
        us = US.get(nome)
        if not us: continue
        for i_lo, i_lui, delta in refs_dado(eu, a, perto)[:12]:
            for n in (16, 32, 64):
                ini = max(0, i_lui - n // 2); pat = eu.m[ini:i_lo + n // 2]
                r = us.find(pat)
                if len(r) == 1:
                    j0 = r[0]; v = valor_em(us, j0 + (i_lo - ini), j0 + (i_lui - ini))
                    if v is not None: votos[(v + delta) & 0xffffffff] += 1; onde.setdefault((v + delta) & 0xffffffff, nome)
                    break
                if not r: break
    if not votos: return None, None
    v, k = votos.most_common(1)[0]
    tot = sum(votos.values())
    if unanime and (k != tot or tot < minimo): return None, None
    return v, 'dado (%d de %d lugares do codigo concordam, %s)' % (k, tot, onde[v])

EXTRAS = {   # enderecos europeus usados pelo proprio programa (runtime), portados junto
    0x088ad000: 'runtime:icone de carregamento (inicio)', 0x088b3800: 'runtime:icone de carregamento (fim)',
}
DECOMP = ['libtitle']   # partes reescritas do jogo (decomp/<modulo>) portadas junto: o menu de titulo (Graphics, Exit to Desktop)
def enderecos_dos_mods():
    achados = collections.OrderedDict()
    for a, quem in EXTRAS.items(): achados.setdefault(a, set()).add(quem)
    for d in DECOMP:
        for f in sorted(glob.glob(os.path.join(ROOT, 'decomp', d, '*.cpp')) + glob.glob(os.path.join(ROOT, 'decomp', d, '*.h'))):
            for m in re.finditer(r'0x0[89a][0-9a-fA-F]{6}', open(f, encoding='utf-8', errors='replace').read()):
                a = int(m.group(0), 16)
                if a in (0x08000000, 0x08800000, 0x0a000000) or (a & 0xffff) == 0: continue
                achados.setdefault(a, set()).add('decomp:' + d)
    for f in sorted(glob.glob(os.path.join(ROOT, 'mods_src', '*', '*.c*'))):
        mod = os.path.basename(os.path.dirname(f))
        for m in re.finditer(r'0x0[89a][0-9a-fA-F]{6}', open(f, encoding='utf-8', errors='replace').read()):
            a = int(m.group(0), 16)
            if a in (0x08000000, 0x08800000, 0x0a000000) or (a & 0xffff) == 0: continue
            achados.setdefault(a, set()).add(mod)
    return achados

def main():
    eu_dir = os.path.join(ROOT, 'gen', 'jogo'); us_dir = os.path.join(ROOT, 'gen', 'jogo_ULUS10136')
    for d, nome in ((eu_dir, 'europeia (gen/jogo)'), (us_dir, 'americana (gen/jogo_ULUS10136)')):
        if not os.path.isdir(os.path.join(d, 'PSP_GAME')): sys.exit('falta a versao %s: instale com sh tools/recomp_tudo.sh <ISO> (veja docs/VERSIONS.md)' % nome)
    print('carregando os modulos europeus...'); EUw = carregar(eu_dir, 'modules.tsv')
    print('carregando os modulos americanos...'); USw = carregar(us_dir, 'modules_ULUS10136.tsv')
    EU = {n: Mod(*v) for n, v in EUw.items()}; US = {n: Mod(*v) for n, v in USw.items()}
    FN_EU = funcoes_traduzidas(os.path.join(ROOT, 'gen')); FN_US = funcoes_traduzidas(os.path.join(ROOT, 'gen', 'ULUS10136'))
    if not FN_EU or not FN_US: print('  aviso: sem a lista de funcoes traduzidas (gen/*.c e gen/ULUS10136/*.c); a checagem de comeco de funcao fica mais fraca')
    def comeco_eu(a): return any(a in v for v in FN_EU.values())
    def mods_da_funcao(a): return [m for m in EU if a in FN_EU.get(m, ())]   # modulos europeus onde a e chamavel
    def chamavel_us(nome, r): return not FN_US or r in FN_US.get(nome, ())
    linhas = []; ok = 0; todos = enderecos_dos_mods(); resumo = collections.defaultdict(lambda: [0, 0])
    achado = {}; deltas = collections.defaultdict(list)   # modulo -> [(endereco europeu, deslocamento)] das funcoes ja portadas
    def so_decomp(a): return all(q.startswith('decomp:') for q in todos[a])
    def modulos_para(a):
        """mods: todos os modulos que cobrem o endereco, na ordem da tabela (como sempre funcionou). Menu reescrito (decomp:<modulo>): SO o
        modulo dele (no PSP varios modulos ocupam o mesmo lugar da memoria, um de cada vez: procurar nos outros acharia a funcao errada)"""
        if so_decomp(a):
            ds = sorted({q[7:] for q in todos[a]})
            return [d for d in ds if d in EU and EU[d].has(a)]
        return [n for n, eu in EU.items() if eu.has(a)]
    def exige_chamavel(a): return so_decomp(a) and comeco_eu(a)   # so o menu reescrito: chamar/prender endereco que nao e funcao fecha o jogo
    rejeitado = {}
    for a in todos:
        for nome in modulos_para(a):
            if nome not in US: continue
            r, c = portar_codigo(EU[nome], US[nome], a)
            if r is not None and exige_chamavel(a) and not chamavel_us(nome, r): rejeitado[a] = (nome, r); r = None
            if r is not None: achado[a] = (r, c + ', ' + nome); deltas[nome].append((a, r - a)); break
    def bate_vizinhas(a, r):
        """o deslocamento achado fica perto do das funcoes vizinhas ja portadas no mesmo modulo?"""
        for nome, eu in EU.items():
            if eu.has(a) and deltas.get(nome):
                d = min(deltas[nome], key=lambda t: abs(t[0] - a))[1]
                return abs((r - a) - d) <= 0x800
        return True
    for a, quem in todos.items():
        res, como = achado.get(a, (None, None))
        funcao = e_funcao(EU, a) or comeco_eu(a)   # prologo, ou a traducao do projeto diz que ali comeca uma funcao (inclusive as curtas, sem prologo)
        if res is None and not funcao: res, como = portar_dado(EU, US, a)
        if res is None:   # funcao que mudou um pouco entre as versoes
            for nome in modulos_para(a):
                if nome not in US: continue
                r, c = portar_parecido(EU[nome], US[nome], a, deltas[nome])
                if r is not None and exige_chamavel(a) and not chamavel_us(nome, r): rejeitado[a] = (nome, r); r = None
                if r is not None and bate_vizinhas(a, r): res, como = r, c + ', ' + nome; break
        if res is None and not funcao: res, como = portar_dado(EU, US, a, perto=0x4000, unanime=True)   # campo longe do comeco (ex.: o save)
        if res is None and funcao and so_decomp(a):
            # funcao do menu reescrito guardada em tabela/ponteiro: o codigo DO PROPRIO modulo que monta o endereco dela, na mesma vizinhanca
            # americana, monta o endereco americano. Vale so se ali o jogo americano chama funcao E o deslocamento bate com o das vizinhas.
            for nome in modulos_para(a):
                if nome not in US: continue
                r, c = portar_dado({nome: EU[nome]}, {nome: US[nome]}, a, perto=0, unanime=True, minimo=1)   # todos concordam (pode ser 1 so: as outras checagens seguram)
                if r is not None and chamavel_us(nome, r) and bate_vizinhas(a, r):
                    res, como = r, 'funcao por ponteiro: ' + c; break
        if res is None and a in ULTIMO_PARECIDO: como = 'sem correspondencia segura (o mais parecido: %d%% igual, o 2o: %d%%)' % ULTIMO_PARECIDO[a]
        if res is None and a in rejeitado:
            nome, r = rejeitado[a]; vs = sorted(FN_US.get(nome, ()))
            antes = [x for x in vs if x < r][-2:]; depois = [x for x in vs if x > r][:2]
            como = 'o codigo igual fica em 0x%08x, mas ali o jogo americano NAO tem entrada de funcao (vizinhas: %s | %s)%s' % (
                r, ' '.join('0x%08x' % x for x in antes), ' '.join('0x%08x' % x for x in depois), '; ' + como if como else '')
        ok += res is not None
        linhas.append('0x%08x -> %s   # %s; usado por %s' % (a, '0x%08x' % res if res is not None else 'NAO ACHADO', como or 'sem correspondencia', ','.join(sorted(quem))))
        if not all(q.startswith('decomp:') for q in quem): print(linhas[-1])
        for q in quem: resumo[q][0] += 1; resumo[q][1] += res is not None
    out = os.path.join(ROOT, 'tools', 'enderecos_ULUS10136.txt')
    open(out, 'w', encoding='utf-8').write('# enderecos dos mods: europeia (ULES-00600) -> americana (ULUS-10136). Gerado por tools/portar_mods.py\n' + '\n'.join(linhas) + '\n')
    print('\n== resumo por mod / parte do jogo (achados / total) ==')
    for q in sorted(resumo): print('  %-28s %4d / %4d' % (q, resumo[q][1], resumo[q][0]))
    faltam = [l for l in linhas if 'NAO ACHADO' in l and 'decomp:' in l]
    if faltam:
        print('\nenderecos do menu reescrito (decomp) NAO achados: %d (os primeiros 40):' % len(faltam))
        for l in faltam[:40]: print('  ' + l)
    print('\n%d de %d enderecos portados -> %s' % (ok, len(todos), out))

if __name__ == '__main__':
    main()
