# Game Speed 1.3x

Acelera o jogo inteiro em 1,3x: duelos, animações, menus e Modo História, **mantendo a música e os efeitos sonoros na velocidade e no tom normais**.

- Liga e desliga na página **Mods** do launcher.
- No menu **Mods** da janela do jogo:
  - **Game speed**: 100% = normal, até 300% (padrão 130%).
  - **Music at normal speed**: ligado (padrão) = música e efeitos normais; desligado = o som também acelera (mais agudo).
- As escolhas ficam em `config.txt`.

Usa `set_speed` (API de plugins v6) e `set_music_normal` (v7), em `mods_sdk/tfmod.h`.
