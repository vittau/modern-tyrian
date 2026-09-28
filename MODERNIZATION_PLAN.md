# Modern Tyrian — Plano de Modernização e Journal

> **Princípio:** fazer o Tyrian parecer e se comportar como um jogo moderno sem virar um remake.
> **Fronteira arquitetural:** o gameplay conhece o playfield original; o render conhece o display moderno.

Este arquivo tem duas partes:

1. **Plano** — análise de viabilidade, arquitetura, riscos e fases. É atualizado quando uma decisão muda.
2. **Journal** — registro cronológico do processo (no fim do arquivo). Só cresce; nada é apagado.

Base: OpenTyrian (SDL2, C) neste repo. Dados do jogo original (Tyrian 2.1, freeware) em `/Users/vitor/Tyrian`.

---

## 1. Veredito

Viável como camada de apresentação, mas **o código não tem a fronteira "gameplay × renderização"** que a proposta pressupõe. Criar essa fronteira é o trabalho central; depois dela, a maioria dos itens é barata ou média. Os itens caros são iluminação que "interage com o ambiente" e frame pacing moderno. Alguns itens já existem no jogo original.

## 2. O que o código mostra

1. **Renderização 100% em software, 8 bits indexado, 320×200** (`src/video.c:71`). A paleta é organizada em 16 matizes × 16 brilhos, e blends e filtros operam nesses nibbles (`src/varz.c:1136`, `src/tyrian2.c:117`). A saída passa por um scaler em CPU → `SDL_Texture` → `SDL_Renderer`, sem shaders (`src/video.c:394`).
2. **Lógica e desenho estão entrelaçados** em `JE_main` (`src/tyrian2.c` ~1190–2280). `JE_drawEnemy` "actually does a whole lot more than just drawing" (`src/tyrian2.c:177`). Explosões e superpixels se movem e desenham no mesmo passo (`src/tyrian2.c:1837`, `src/varz.c:1125`).
3. **RNG compartilhado em caminhos de desenho.** `mt_rand()` é usado nas explosões repetidas (`src/tyrian2.c:1848`) e nos superpixels (`JE_doSP`). As demos (`demo.1`–`5`) e o netplay dependem de determinismo; qualquer efeito novo que chame `mt_rand` quebra os dois.
4. **A lógica roda a ~34,8 Hz.** O PIT está em 0x4300 (≈69,6 Hz) com `frameCountMax = 2` (`src/config.c:595`), e o jogo desenha um quadro por tick. No DOS, cada quadro durava exatamente 2 refreshes de um VGA de 70 Hz; em 60 Hz a imagem trepida (35 não divide 60).
5. **O playfield visível mede 264×184** (`src/tyrian2.c:106`, copiado de `game_screen` com offset de +24). O resto é HUD, mas parte do HUD é desenhada **dentro** do playfield: dinheiro, vidas e superbombs (`src/mainint.c:2847`).
6. **O parallax já existe.** São 3 camadas de background com velocidades diferentes, mais um pan horizontal que segue o jogador em 3 velocidades (`src/mainint.c:4443`), starfield e "smoothies" (lava, água, blur). Os mapas têm 336–360 px de largura contra 264 visíveis, então **existe arte original além da borda**.
7. **Já há iluminação e flash em 8 bits**: um holofote que segue o jogador (`starShowVGASpecialCode == 2`, `src/tyrian2.c:109`), filtros de cor por fase (`JE_filterScreen`) e `fade_white`. O original não tem screen shake.
8. **O timing tem dependências sutis de tempo real.** Além de marcar o ritmo dos quadros, `getFrameCount2Ticks()` é usado como acumulador de fase: a animação de aviso (`src/fonthand.c:260`) e a de respawn (`src/mainint.c:2410`) só avançam quando o contador chega a 0. Qualquer mudança no ritmo dos quadros, como interpolação ou rodar acima de 35 Hz, precisa preservar isso.
9. **Arquivos de usuário ficam fora do diretório de dados.** `tyrian.cfg` e `tyrian.sav` ao lado dos dados do DOS nunca são lidos; o OpenTyrian usa `~/.config/opentyrian`, ou o diretório do executável quando já existe um `opentyrian.cfg` lá (`src/file.c`).
10. **O padrão de `processorType` é inconsistente.** Sem arquivo de config, o motor usa 3 ("High Detail"), mas `JE_initProcessorType()` documenta o 2 como padrão. Na prática, os níveis 2, 3 e 5 produzem saída idêntica nas demos, porque `smoothScroll` é forçado para true depois.
11. **Mapa dos "smoothies" nos níveis.** Estes efeitos são ligados por eventos de fase (tipo 64): lava (bit 1) em E1/L16 e em várias fases do episódio 4; água (bit 2) em E1/L17 e em muitas do E4; blur (bit 4) e iced blur (bits 3 e 5) só no E4; holofote (bit 6) em E1/L15 e E1/L16; flip vertical (bit 9) em E4/L12 e E4/L13. Os bits 7 e 8 nunca são usados. Uma varredura estática dos níveis não basta, porque eventos de salto (tipo 54) e saltos condicionais (61/66/70/71) pulam parte dos eventos. `tools/scan_smoothies.py` lista os candidatos, e só a execução confirma quais rodam.
12. **Estrutura dos arquivos `.lvl`.** `lvlPos` guarda duas entradas por fase (`JE_loadMap()` usa `lvlPos[(lvlFileNum-1)*2]`), e a última entrada do episódio 4 é o bloco de itens (`src/episodes.c:87`).
13. **Pipeline de áudio.** A saída é mono, S16, 44100 Hz (`11025 × OUTPUT_QUALITY`), com buffer de 1024 amostras. A música é um emulador OPL mais o player LDS em C puro, sem SDL. Os efeitos sonoros são 29 em `tyrian.snd` e 9 vozes em `voices.snd`, em 8 bits a 11025 Hz. A única reamostragem acontece em `loadSndFile()` (`src/nortsong.c`), via `SDL_AudioCVT`. A percussão do OPL usa `rand()` da libc, não `mt_rand`.
14. **Os baselines dependem da máquina.** `powf`/`pow`/`sin` (volume, OPL) e `cosf`/`sinf` (superpixels) passam por ponto flutuante da libm. Por isso os baselines valem para esta máquina e toolchain (macOS arm64, clang), e não como golden files multiplataforma. Uma CI em outro sistema precisaria de baselines próprios.
15. **A rede vazava para o gameplay.** Havia 38 chamadas `SDLNet_Read16/Write16` espalhadas por `mainint.c`, `tyrian2.c` e `game_menu.c`, lendo e escrevendo pacotes. Hoje elas usam `network_read16/write16` (`src/network.h`). O protocolo é UDP ponto a ponto, 2 jogadores, em lock-step com o loop do jogo; o SDL3_net não tem bind, então o filtro por endereço e porta do oponente é explícito.
16. **Contrato dos passes do modo Modern** (`src/modern.h`): `void pass(ModernFrame *)`, rodam na ordem de registro, uma vez por quadro apresentado, sobre o canvas na grade lógica. Regras: determinísticos, sem RNG, sem tocar estado de jogo; podem ler `src` (índices de 8 bits) e `palette` e escrever `pixels`. O canvas pode ser mais largo que o quadro de 8 bits, o que prepara o widescreen.
17. **GCC é mais estrito que o clang do macOS.** Com `-std=c99 -pedantic -Werror`, o GCC rejeita ponteiro-para-array com qualificador diferente (`const Palette *` a partir de `&palette`), e o clang aceita. A CI de Linux e Windows x86_64 usa GCC, então mudanças em `src/` devem ser auditadas também com `gcc-16` (Homebrew), usando as flags da CI. **Complemento (2026-09-27):** a auditoria feita no macOS também não pega header padrão faltando (ex.: `snprintf` sem `<stdio.h>`), porque os headers do SDL no macOS já puxam esses headers. Só a CI em Linux/Windows pega; empurrar cedo e olhar a CI.
18. **SDL3: eventos de hot-plug.** `SDL_EVENT_GAMEPAD_ADDED/REMOVED` chegam mesmo com os eventos desabilitados, mas `SDL_EVENT_JOYSTICK_ADDED/REMOVED` não chegam. Um gamepad gera os dois pares, então é preciso deduplicar por instance id.
19. **O harness pode receber input real.** No macOS, mesmo com o driver `dummy`, o processo de teste é um app Cocoa chamado `opentyrian`. Um `osascript` que traz "o processo opentyrian" para a frente e manda teclas (como nas capturas de tela do widescreen) pode acertar um teste em andamento. Isso aconteceu: o cenário `flip` falhou por volta do quadro 855 só enquanto outro agente capturava telas, e passou 3/3 isolado, tanto na árvore limpa quanto na do gamepad. A correção está pendente (§6). **Correção (2026-09-27):** a causa provável da falha no quadro 855 não era input real. O diretório `data/` do repositório, ignorado pelo git, tem um `newsh9.shp` diferente do Tyrian 2.1 em `/Users/vitor/Tyrian`. Rodar a regressão com ele diverge exatamente no quadro 855 do cenário flip, que é quando esse sprite aparece. A proteção contra input real continua valendo como endurecimento. Falta uma trava: a regressão deve recusar dados diferentes dos esperados.
20. **Arte além da borda do playfield (medido).** Cada camada de fundo é desenhada como uma janela de 12 tiles (288 px) deslizando sobre um mapa de 336 px (bg1/bg2) ou 360 px (bg3). Somando todo o range do pan, há arte em x ∈ [-45,315) na bg1, [-41,342) na bg2 e [-69,369) na bg3, com o playfield em [24,288). A margem garantida em qualquer pan é assimétrica: ~45 px à esquerda e só 3 px à direita (7 px onde há bg2/bg3 densa). Seis fases não têm fundo nenhum (E1:L1, E2:L5, L7, L8, L9 e E4:L14). Com 24 px por lado, a faixa esquerda tem em média 94 % de arte real e a direita 64 %. Inimigos já simulados aparecem nessas faixas em 11–21 % dos quadros. Porém `JE_drawEnemy` só anima e desenha inimigos em x ∈ (-29,300), o que limita a revelação à direita a ~12 px, e tiros somem visivelmente na borda direita em ~1,5 % dos quadros. Dois cuidados para qualquer implementação: `blit_sprite2*` usam o `VGAScreen->pitch` global em vez do pitch da superfície (`sprite.c:559,636,672,708`), e `draw_background_2/3` avançam o scroll dentro da função de desenho, então não podem ser chamadas duas vezes por quadro. Relatório completo: `.worker-reports/phase1-extview-investigation.md`.
21. **O desenho do HUD tem efeitos na lógica.** `JE_inGameDisplays` escreve na global `tempW`, que o pan lê, e `draw_boss_bar` atualiza `boss_bar[]` durante o desenho. `blit_sprite2*` usavam o `VGAScreen->pitch` global; foi corrigido para o pitch da superfície de destino. Para vigiar isso, `--regress-state-out` grava um hash por quadro do estado do jogo: RNG, jogadores, inimigos, tiros, barras de chefe, `tempW`, eventos e explosões. Os casos `state-*` rodam o Modern 16:9 contra baselines gerados pelo Classic, então qualquer mudança do Modern que altere o estado do jogo falha ali. Limitação: o hash inclui bytes de padding das structs, o que é determinístico no mesmo binário (e os baselines já são por máquina).
22. **Depois de um reboot, sessões opencode ocupadas voltam sozinhas.** O serviço compartilhado do opencode retomou a sessão do agente do HUD, que estava no meio de um turno, sem terminal nenhum ligado a ela. Ela ficou rodando regressões no checkout principal e disputando o `test/regress/actual/` com outras execuções: toda execução começa com `rm -rf` desse diretório, então uma apaga a saída da outra ("no output written", divergências falsas). Solução: localizar com `opencode api session.list` e interromper com `opencode api session.interrupt` (registrado no CLAUDE.md global). Nunca rodar duas regressões no mesmo checkout.
20. **Display e input**: fullscreen desktop, janela, scalers e os modos Center/Integer/8:5/4:3 já existem, assim como o remapeamento de teclado (`src/config.c:297`). O joystick usa a API legada `SDL_Joystick`, não `SDL_GameController`.

## 3. Viabilidade por item

Esforço: P = pequeno, M = médio, G = grande.

| Item | Viabilidade | Esforço | Observação |
|---|---|---|---|
| Preservar o gameplay | Alta | — | Exige três regras: a camada moderna é só leitura, usa RNG próprio e as demos viram teste de regressão. |
| Alta resolução, nearest e integer | Alta | P | Quase tudo existe. Falta a proporção de pixel (PAR) do 4:3 original, em que o pixel era 1,2× mais alto que largo. Em integer isso dá 5×v/4×h em 1080p, 7×/6× em 1440p e 10×/8× em 4K. Oferecer também pixel quadrado. |
| Frame pacing e VSync | Média/baixa | G | A trepidação de 35 Hz em 60 Hz é o problema. VRR/ProMotion resolve de graça onde existir. Interpolação real exige separar as camadas, porque o quadro de 8 bits já sai composto. |
| Bloom e glow | Alta | P→M | A versão simples usa uma máscara pelo nibble de brilho da paleta, o que faz fundos claros brilharem também. A versão boa usa o tag buffer (§4). |
| Iluminação dinâmica | Média | M/G | Luz aditiva a partir do snapshot de tiros e explosões. A cor da luz pode ser derivada automaticamente da matiz da paleta de cada sprite, sem curadoria manual por arma. Sem normal maps nem profundidade, "interagir com o ambiente" se limita a clarear o albedo. |
| VFX (faíscas, trilhas, shockwave, distorção) | Alta | M | Os gatilhos são ganchos em `JE_setupExplosion`, disparos e acertos. Há uma decisão estética: partículas em alta resolução ou presas à grade de 320×200. |
| Parallax e ambiente | Alta (procedural) | P (ambiente) / M (camadas) | O parallax já existe. Camadas novas só procedurais: névoa, poeira, starfields ou derivações dos tiles originais (§7). |
| Widescreen com playfield fixo | Alta | M | Recortar os 264×184 e redesenhar fora deles o HUD que hoje fica dentro. As laterais podem mostrar, escurecida, a arte original que existe além da borda. Menus, loja e cutscenes (320×200) ficam centralizados com moldura. |
| HUD expandido | Alta | M | Todo o estado está em globais (`player[]`, armas, escudo, armadura), então é só um desenho novo lendo esses dados. Os modos 2P, arcade e rede multiplicam os layouts. |
| Controle, remapeamento e autofire | Alta | P/M | Migrar para a API de gamepad do SDL dá mapeamento padrão e hot-plug. O Tyrian já atira segurando o botão, então turbo é trivial. |
| Acessibilidade | Alta | P/M | Reduzir flashes = interceptar `fade_white` e os filtros de fase. Cores alternativas de projéteis dependem do tag buffer. Reduzir partículas e shake vale só para os efeitos novos. |
| Modos Classic e Modern | Alta | — | O Classic é o caminho atual, mantido intacto. |

## 4. Arquitetura

```text
(atualizado em 2026-09-27: a camada moderna roda na CPU, ver §7)
Tick de lógica original (~35 Hz, intocado)
   │  desenha no framebuffer 8-bit 320×200 (como hoje)
   │  + tag buffer 8-bit paralelo (categoria de cada pixel)
   │  + snapshot só-leitura do estado + fila de eventos
   ▼
Camada moderna (CPU, canvas XRGB na grade lógica)
   ├── paleta aplicada na conversão 8-bit → XRGB (efeitos 8-bit preservados)
   ├── recorte do playfield 264×184 + layout widescreen
   ├── luzes / bloom / VFX / partículas (RNG próprio)
   └── HUD expandido (lê o snapshot)
   ▼
Display moderno (16:9 / 21:9 / 32:9)
```

Peças novas necessárias:

1. **Backend GPU.** O buffer de 8 bits e a paleta sobem como texturas e a conversão de cor acontece no shader. Candidatos: SDL3 com a API de GPU (Metal/Vulkan/D3D12, que também traz a API nova de gamepad) ou SDL2 com OpenGL 3.3 (ver decisões em aberto).
2. **Snapshot e fila de eventos por tick.** Uma estrutura só-leitura com jogador, `playerShot`, `enemyShot`, `explosions`, `enemy[]` e flags de fase. Os eventos (explosão criada, acerto, disparo) vêm de ganchos que **nunca** chamam `mt_rand` nem alteram estado.
3. **Tag buffer.** Um buffer de 8 bits paralelo, preenchido pelas funções `blit_sprite*` com a categoria de cada pixel: fundo, inimigo, tiro do jogador, tiro inimigo, explosão ou HUD. Com uma mudança pequena, ele habilita bloom seletivo, luz só em projéteis, recolorir tiros inimigos e não iluminar o HUD.
4. **Teste de regressão por demos.** Rodar as 5 demos sem janela e comparar um hash do framebuffer de 8 bits e do estado a cada tick. É a prova, a cada commit, de que o gameplay não mudou.

### Regras invioláveis

- A camada moderna **nunca escreve** em estado de jogo e **nunca chama** `mt_rand`/`mt_rand_1`/`mt_rand_lt1`.
- O modo Classic tem de produzir exatamente o mesmo framebuffer de 8 bits que o upstream (validado pelo teste de regressão).
- Efeitos "secundários" (explosões extras, partículas) são puramente visuais; nunca usam `JE_setupExplosion` ou `JE_doSP`.

### Como rodar o teste de regressão

```sh
make regress TYRIAN_DATA=/caminho/para/Tyrian   # padrão: ./data (baixa com ./get_data.sh)
tools/regress.sh --update                       # regenera os baselines (só quando a mudança de saída é intencional)
```

O modo de teste (`--regress-demo=N --regress-detail=M --regress-out=FILE`) ignora a config e os saves do usuário, fixa as opções que afetam a saída e troca o relógio real por um relógio virtual, então roda na velocidade máxima e de forma determinística.

**Cobertura:** as 5 demos (× 6 níveis de detalhe) nunca ativam `smoothies[]`. Os 5 cenários sintéticos cobrem esses caminhos: água em E4/L9, flip e lava em E4/L12, iced blur em E4/L8, holofote e lava em E1/L16, blur em E4/L19. Nos cenários o jogador é invencível, pela flag `youAreCheating` do próprio motor e só nesse modo, e a entrada fica neutra. Por isso o texto "Cheaters always prosper." aparece nesses baselines, o que é esperado.

## 5. Riscos

| Risco | Impacto | Mitigação |
|---|---|---|
| Quebra de determinismo (demos e netplay) | Alto e silencioso | Teste de regressão por demos em cada mudança; RNG separado para os efeitos. |
| Trepidação a 35 Hz em telas de 60 Hz | É o que mais "parece antigo" | VRR primeiro; a interpolação fica para a Fase 3. |
| ~~Mistura de resoluções~~ | — | Resolvido: todos os efeitos ficam na grade de 320×200 (§7). |
| Divergência do upstream OpenTyrian | Correções do upstream difíceis de trazer | Concentrar as mudanças em módulos novos; tocar o mínimo em `tyrian2.c` e `mainint.c`. |
| ~~Contradições na proposta (sprites em alta resolução, parallax extra)~~ | — | Resolvido: arte nova só procedural; nada de sprites em alta resolução (§7). |

## 6. Fases

### Fase 0 — Fundação (visualmente não muda nada)
- [x] Build local funcionando (macOS, `make`; Homebrew com sdl2-compat sobre SDL3)
- [x] Teste de regressão: 5 demos × 6 níveis de detalhe, sem janela, hash por quadro de 8 bits + paleta, baselines em `test/regress/`, `make regress` (~26 s)
- [x] Cobrir os caminhos que as demos não exercitam: 5 cenários sintéticos (`--regress-level=E:L --regress-frames=N`) cobrem lava, água, blur, iced blur, flip vertical e holofote. Total: 52 pares, ~35 s
- [x] Baseline de áudio offline (`--regress-audio`): 38 efeitos convertidos, as 41 músicas (10 s cada) e uma mixagem fixa. Total: 53 casos, ~37 s
- [x] Migração SDL2 → SDL3, núcleo: vídeo, eventos, input, áudio e build. Linka só libSDL3, e os 53 baselines passam sem ser regenerados. Rede desligada temporariamente
- [x] Rede via SDL3_net (handshake validado com dois peers locais; o lock-step em jogo ainda não foi testado)
- [x] Migração SDL2 → SDL3: CI (`.github/workflows`) e scripts de release (`make_macos.sh`, `make_linux.sh`, Windows/`visualc`). **CI verde em Linux (x86_64/arm64), macOS e Windows (x86_64/arm64)** desde `e5a6d11`. O projeto MSVC (`visualc/`) não é exercitado pela CI
- [x] Pequenos ajustes de áudio pós-SDL3: buffer estático no callback e `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES=1024`
- [ ] ~~Backend GPU com a paleta aplicada no shader~~ → substituído pelo pipeline de composição na CPU (§7)
- [x] Pipeline de composição moderna na CPU (`src/modern.c`): canvas XRGB na grade lógica, lista de passes de efeito (vazia), textura própria com nearest. Setting `presentation` (cfg + `--presentation`), hash do canvas no harness (`--regress-modern`). Total: 63 casos. Custo ~37 µs/quadro

### Fase 1 — Ganhos visíveis e baratos
- [x] Correção de PAR (4:3) com integer scaling por eixo: `pixel_aspect` = `original` (1,2) ou `square` (`b1e4313`)
- [x] Widescreen: `aspect` = 4:3, 16:10, 16:9, 21:9, 32:9 ou auto. O quadro de 320×200 fica centralizado e nunca é ampliado, e as laterais recebem um preenchimento procedural escurecido ("ambilight") tirado das bordas do playfield, sem revelar nada fora dele (`b1e4313`)
- [x] HUD que fica dentro do playfield (dinheiro, vidas e nomes, superbombas, arma especial, barras de chefe, timer da fase, aviso de cheat) movido para os painéis laterais no modo Modern quando cada painel tem ≥ 51 px (16:9 original ou mais largo). Jogador 1 à esquerda, jogador 2 à direita. Merge `1292c30`
- [x] Gamepad via API de Gamepad do SDL3, com hot-plug e remapeamento por nome no cfg; autoteste `--selftest-gamepad` com gamepad virtual (51 checks). Integrado a `modernization` no merge `85f741a`
- [x] Harness imune a input real: no modo regress só `SDL_EVENT_QUIT` passa, joysticks ficam inertes, o foco fica fixo e no macOS o processo sobe com `SDL_HINT_MAC_BACKGROUND_APP` (§2.19)
- [ ] Bloom simples pela máscara de brilho da paleta
- [—] ~~Visão estendida~~: descartada pelo usuário depois das medições (§2.20, §7). As laterais ficam só com o preenchimento procedural do widescreen

### Fase 2 — O "Modern"
- [ ] **Taxa de quadros independente da lógica (etapas 1–3 feitas; falta a 4) (pedido do usuário, prioridade logo após as telas de menu):** a lógica continua no tick fixo de ~35 Hz, o que preserva demos, rede e regressão, e o desenho vai para a taxa do monitor (≥ 60 Hz, idealmente o refresh da tela, com vsync) com interpolação entre ticks. Etapas:
  1. [x] Lista de desenho por tick: cada blit com sprite, posição, blend, camada e identidade do objeto, mais o scroll das camadas. Só observa, sem tocar a lógica (`src/drawlist.c`, `f35198f`).
  2. [x] Renderizador que reproduz o quadro a partir da lista. Prova: `--regress-replay-check`, 0 divergências em 82/82 casos (todas as demos e cenários em todos os detalhes); ~0,17 ms por quadro.
  3. [x] (`09441a8`, merge `b9b65da`) Interpolação entre o tick anterior e o atual na taxa do monitor, obrigatória para a suavidade (pedido do usuário). Vale para todo movimento: inimigos (inclusive os de chão, que andam com o scroll), nave e sidekicks, tiros do jogador e dos inimigos, explosões que seguem objetos e scroll das três camadas. Nascimento, remoção, reuso de slot ou salto maior que um limiar entram direto na posição nova, sem deslizar. Os quadros de animação dos sprites não são misturados: só as posições são interpoladas.
  4. Efeitos, paleta e HUD sobre o quadro interpolado.
  Custo: ~1 tick (~28 ms) de atraso de imagem por interpolar, configurável. A infraestrutura das etapas 1 e 2 é a mesma do snapshot por tick e do tag buffer abaixo.
- [ ] Snapshot e fila de eventos por tick
- [ ] Tag buffer nas funções `blit_sprite*`
- [ ] Luzes dinâmicas, com cor derivada da matiz da paleta
- [ ] VFX (na grade de 320×200): faíscas, destroços, fumaça, shockwave, trilhas, muzzle flash e impactos
- [ ] Partículas ambiente (poeira, névoa, energia)
- [x] HUD expandido, etapa 2 do HUD modernizado (§7), commit `5a5952b`. Pendente, pedido do usuário: no modo Modern, só o HUD novo, sem a barra lateral original
- [x] Só o HUD novo no Modern (commit desta entrada): a barra lateral e a faixa de baixo originais saem do canvas nos quadros de jogo, e o HUD novo assume toda a informação delas e usa o espaço liberado
- [x] Fallback de widescreen para telas fora do jogo: laterais com a própria tela desfocada e escurecida (média 40×25 + esticamento bilinear, ~0,08 ms em 16:9)
- [x] Telas fora do jogo em widescreen com conteúdo real. S1 em `296cf7e` (Vert-, painel alargado e borda sólida, mais `--regress-screen`); S2 em `2b27f5e`, merge `38ab9f1` (jukebox, ship specs e créditos na largura do canvas; o mapa de navegação usa o painel alargado). Inventário em `.worker-reports/screens-inventory.md`, ferramenta `tools/dump_screens.py`):
  - Vert- (fundo ampliado ~1,33× e cortado em cima e embaixo, elementos do menu nítidos por cima em 1×) no **título** (pic 4) e nos **menus** sobre a pic 2 (seleção de jogo, episódio e dificuldade, setup, ajuda, load/save, recordes). A pic 2 perde a linha "AN EPIC MEGAGAMES PRODUCTION ©1994" embutida no rodapé; tentar recompor essa faixa em 1×
  - Extensão real, montada por código: **mapa de navegação** (o espaço de coordenadas dos planetas já passa de 320), **ship specs** (grid procedural), **jukebox** e **simulador de armas** (starfields), **créditos**
  - Preenchimento sólido onde a borda da imagem é lisa (pic 5, pic 11/Destruct, telas pretas); overlays sobre o jogo com a sombra na largura toda
  - Continua com o desfoque: logos de abertura (pics 10 e 12), imagens da história (7, 8, 9, 13, tshp2) e animação final
  - Loja, armamentos, dados e demais telas da moldura mecânica (pic 1): alargar o painel liso da direita repetindo uma faixa de colunas internas dele até preencher o canvas (arte original em 1×, sem desfoque). A moldura com a nave fica à esquerda. É só composição: o jogo segue desenhando 320 px, o compositor corta numa coluna livre do painel e insere a faixa. Ajustes: recentralizar o título da caixa, mapear o mouse através da faixa inserida e checar tela por tela se a coluna de corte fica livre. Pedido do usuário, que prefere isso ao desfoque (Vert- não serve aqui: abre a moldura e desalinha as janelas)
- [x] Trava de dados na regressão (`7909049`); referência no zip oficial e regressão na CI, verde em Linux, macOS e Windows (`b4f42b6`)
- [ ] (ref.) HUD expandido original do plano: nomes e nível das armas, munição e carga dos sidekicks, escudo e armadura numéricos, layouts 1P, 2P, arcade e rede. Protótipo com capturas para aprovação do usuário
- [ ] Acessibilidade: menos flashes, menos partículas, cores alternativas de projéteis, intensidade dos efeitos ajustável
- [ ] HUD Modern, layout (pedido do usuário, 2026-09-28): inverter os lados, com escudo, armadura e gerador/força no painel **direito**, como no original, e armas e sidekicks à esquerda. As barras desses três ficam **verticais** e um pouco mais compridas. Vale para 1P, 2P e arcade e para 16:10/16:9/21:9. Vai depois do merge de `hud-fade`, porque os dois mexem em `src/modern_hud.c`.

### Fase 3 — Opcional e cara
- [ ] Separar as camadas de fundo (bg1, bg2, bg3, inimigos) em buffers próprios, preservando a matemática dos blends
- [ ] (movido para a Fase 2, "Taxa de quadros independente da lógica")

## 7. Decisões

### Em aberto
_(nenhuma)_

### Tomadas
- **2026-09-26 — Fase 0 aprovada; backend = SDL3.** A migração SDL2 → SDL3 (API de GPU/renderer com shaders e API nova de gamepad) foi a recomendação aceita. SDL3 3.4.16 já está instalado via Homebrew.
- **2026-09-26 — Efeitos na resolução original.** Partículas, luzes, bloom, trilhas e demais VFX são calculados na grade lógica de 320×200 (um valor por pixel do jogo) e ampliados com o mesmo scaling dos sprites. Nenhum efeito é desenhado em resolução de tela. Com isso somem o risco de "mistura de resoluções" (§5) e a decisão estética que estava prevista para a Fase 2.
- **2026-09-26 — Arte nova só procedural.** O projeto não terá artista. Arte nova é permitida desde que seja gerada por código: ruído, gradientes, derivação/recoloração/composição dos sprites e tiles originais, partículas, shaders. Nada desenhado à mão e nada de sprites redesenhados em alta resolução. Isso resolve a contradição de §5: camadas extras de parallax (névoa, poeira, starfields, versões desfocadas ou escurecidas dos tiles existentes) viram viáveis se forem procedurais, sempre na grade de 320×200.
- **2026-09-27 — Composição moderna na CPU, sem backend GPU próprio** (decisão técnica do coordenador; pode ser revista). Como todos os efeitos ficam na grade de 320×200, o custo de luz, bloom, partículas e widescreen na CPU é trivial: 64 mil pixels por quadro, bem menos de 1 ms. Fazer na CPU mantém tudo portátil, sem shaders para Metal/Vulkan/D3D, e determinístico, então a saída do modo Modern pode ser coberta por baselines de hash como o resto. O `SDL_Renderer` do SDL3 continua só para subir a textura e escalar com nearest. O modo Classic mantém o caminho atual intacto. Se no futuro algum efeito exigir resolução de tela, reavaliamos.
- **2026-09-27 — Quadros acima de 35 Hz, sem atrelar a lógica.** O usuário quer ao menos 60 fps, de preferência o refresh da tela, e a lógica não pode depender da taxa de quadros. Decisão: lógica no tick fixo original, desenho por interpolação a partir de uma lista de desenho gravada por tick (Fase 2). Entra logo depois das telas de menu.
- **2026-09-27 — No modo Modern, só o HUD novo.** O usuário aprovou o visual da etapa 2 ("bem bonito"), mas não quer o HUD original e o novo juntos, porque é redundante. Com o HUD novo ativo, a barra lateral e a faixa de baixo originais saem da tela e o espaço vai para os painéis. As vidas seguem o comportamento original (aparecem só em arcade e 2P), e escudo e armadura ficam com barra e número.
- **2026-09-27 — Splash screens e menus: conteúdo real onde for viável.** O usuário prefere conteúdo real ao desfoque, citando zoom com corte (Vert-: ampliar a imagem até a largura e cortar em cima e embaixo, ~25 % da altura em 16:9). Ele autorizou implementar onde for viável, nas splash screens, na tela principal e nos menus do jogo (armamentos, dados). Um inventário decide tela por tela entre Vert-, extensão real/reorganização e desfoque (que fica como fallback). Custos a pesar: conteúdo perdido no corte e pixels do fundo ~1,33× maiores que os dos elementos do menu.
- **2026-09-27 — Vert- só no título e nos menus sobre a pic 2.** O inventário mostrou que nenhuma imagem tem arte além de 320 px. O Vert- funciona no título (o corte leva só céu e a parte de baixo do planeta, e logo e menu são camadas separadas) e é aceitável na pic 2 dos menus. Na pic 1 (loja, armamentos, dados) ele quebra a moldura e desalinha as janelas, nos logos corta arte e na história corta arte única. O mapa de navegação, o ship specs, os starfields e os créditos ganham extensão real.
- **2026-09-27 — Splash screens e menus em widescreen, de forma procedural.** O usuário perguntou se dava. Resposta: sim, preenchendo as laterais com uma cópia ampliada, desfocada e escurecida da própria tela. Reorganizar cada menu para usar a largura exigiria refazer tela por tela, com ganho pequeno, e fica fora por enquanto.
- **2026-09-27 — HUD modernizado, em duas etapas.** Por sugestão do coordenador, aceita pelo usuário, o espaço lateral do widescreen vai para HUD, em vez de fundo. (1) Os elementos que hoje ficam dentro do playfield (dinheiro, vidas, superbombas, arma especial, barras de chefe) vão para os painéis laterais no modo Modern quando houver largura. (2) Painéis completos: nomes e nível das armas, munição e carga dos sidekicks, escudo e armadura numéricos, e o layout de 2 jogadores, com protótipo e capturas para o usuário aprovar. Tudo na grade de 320×200, com as fontes e os sprites do jogo e molduras procedurais. Fica de fora informação que o original não dava (progresso da fase, vida de inimigos comuns).
- **2026-09-27 — Visão estendida descartada.** As medições (§2.20) mostraram só ~24 px de arte real por lado, com a direita 36 % vazia em média, e mostrar os objetos nessas faixas daria ao jogador informação que o original não dava. Das três opções (só fundo, fundo com objetos, desistir), o usuário escolheu desistir. As laterais do widescreen ficam só com o preenchimento procedural.
- **2026-09-27 — (substituída pela anterior) Visão estendida é desejada, dentro das regras de gameplay.** O usuário prefere usar a área extra da tela mostrando mais do campo do que só decoração, desde que viável. Limites conhecidos: os mapas têm só 336–360 px de largura contra 264 visíveis, e o pan horizontal existe só como parallax das 3 camadas; os tiros inimigos somem em x ≤ 0 / > 275 e os do jogador em x < -34 / > 290; os inimigos existem de -80 a 340. Por isso a ampliação fica restrita à arte real disponível, estimada em ~20–24 px por lado, e o restante segue com as laterais procedurais. Mudar limites de despawn para ampliar mais está fora de escopo, porque alteraria o balanceamento.
- **2026-09-26 — Ordem da Fase 0:** o teste de regressão por demos vem antes da migração para SDL3, porque é a rede de segurança dela.

## 8. Processo

- **Branch:** todo o trabalho acontece na branch `modernization` (remote `origin` = `github.com/vittau/modern-tyrian`).
- **Implementação:** feita por agentes OpenCode (DeepSeek V4.1 Flash) orquestrados pelo Orca, no checkout principal, uma tarefa por vez quando as tarefas mexem nos mesmos arquivos.
- **Contexto limpo por tarefa:** cada tarefa nova vai para um agente novo (ou com sessão limpa). Só correções pedidas na revisão da *mesma* tarefa voltam para o mesmo agente.
- **Revisão, commits e pushes:** feitos pelo coordenador (Claude). Os agentes não fazem commit.
- **Relatórios dos agentes:** cada agente grava descobertas em `.worker-reports/<tarefa>.md` (ignorado localmente via `.git/info/exclude`); o que for relevante é incorporado a este arquivo.
- **Este arquivo** é atualizado a cada marco: decisões em §7, checklist em §6, descobertas em §2 e entradas no Journal.

---

## Journal

Formato: uma entrada por sessão ou marco, em ordem cronológica (mais recente no fim). Cada entrada registra o que foi feito, o que foi decidido e o que ficou pendente.

### 2026-09-26 — Análise de viabilidade
- Analisado o código do OpenTyrian contra a proposta de modernização (renderização, loop principal, timing, RNG, backgrounds, HUD, input).
- Conclusão: viável como camada de apresentação, desde que se criem primeiro a fronteira gameplay/render (snapshot, eventos e tag buffer) e o teste de regressão por demos.
- Descobertas relevantes: o parallax já existe (3 camadas + pan horizontal); há arte de fundo além da borda visível; a lógica roda a ~35 Hz, acoplada ao desenho; `mt_rand` é compartilhado com caminhos de desenho.
- Plano criado neste arquivo.
- **Pendente:** decisão do backend (SDL3 × SDL2+OpenGL) e o aval para iniciar a Fase 0.

### 2026-09-26 — Início da Fase 0
- Decidido: Fase 0 aprovada, backend SDL3, e o fluxo de trabalho de §8 (agentes OpenCode com DeepSeek V4.1 Flash via Orca; o coordenador revisa, faz commit e push).
- Branch `modernization` criada e publicada em `origin`.
- Orca run `run_e0cad877e2fd`. Primeira tarefa despachada: teste de regressão headless por demos (task `task_9f4f56a107b2`).
- Descoberta: o Homebrew desta máquina fornece `sdl2-compat` 2.32 sobre SDL3 3.4.16, ou seja, o build atual já roda sobre SDL3 por meio da camada de compatibilidade. SDL2_net não está instalado, então o build sai sem rede.
- Descoberta: as demos usam semente fixa (`src/demo.c:42`, `mt_srand(32402394)`), o que as torna candidatas naturais a teste determinístico.

### 2026-09-26 — Teste de regressão entregue (Fase 0a)
- O agente entregou o harness em duas rodadas; o commit é `682abeb`.
- Na revisão da 1ª rodada devolvi dois problemas: (1) ele tinha movido `JE_paramCheck()` para antes de `loadConfiguration()`, o que quebrava `--xmas`/`--no-xmas` fora do modo de teste; (2) com `processorType` fixo em 2, lava, água e blend `wild` ficavam de fora. A correção restaurou a ordem original (com um pré-scan dos argumentos) e passou a varrer os níveis de detalhe 1–6.
- Verificado pelo coordenador: os 30 pares (demo × nível) passam, não sobrou sonda temporária no código, e o teste negativo (1 pixel alterado) falha em todos os pares a partir do quadro 51.
- Descobertas registradas em §2 (itens 8–10) e §4: o timing depende de `getFrameCount2Ticks()` como acumulador de fase; os arquivos de usuário ficam fora do diretório de dados; o padrão de `processorType` é inconsistente; as demos nunca ativam `smoothies[]`, o que é uma lacuna de cobertura.
- Próximo: cenários sintéticos para cobrir os smoothies e, depois, a migração para SDL3. Cada tarefa vai para um agente novo.

### 2026-09-26 — Decisão: efeitos na resolução original
- O usuário decidiu que os efeitos modernos (VFX, luz, bloom) ficam na mesma resolução do jogo original, a grade de 320×200. Isso foi registrado em §7 e remove um risco de §5.

### 2026-09-26 — Decisão: arte nova só procedural
- O projeto não terá artista. Arte nova é permitida se for gerada por código, na grade de 320×200 (§7). Com isso, camadas extras de parallax viram viáveis e ficam fora os sprites redesenhados. Não resta nenhuma decisão em aberto.

### 2026-09-26 — Cobertura dos smoothies entregue (Fase 0b)
- Um agente novo entregou 5 cenários sintéticos que cobrem os 6 caminhos de renderização que as demos não exercitavam; aprovado na primeira revisão. Commit em seguida ao `d09f352`.
- Verificado pelo coordenador: os 52 pares passam (~34 s), não há caminho fixo nos arquivos, e o scanner roda. O teste negativo (constante alterada em `water_filter`) derruba só os 4 cenários de água.
- Descobertas em §2 (itens 11–12): mapa de quais fases ligam cada smoothie; os saltos de evento tornam a varredura estática não confiável; `lvlPos` guarda duas entradas por fase. Sem invencibilidade, o jogador morre em 358–1185 quadros, antes da maioria dos eventos.
- Incidente do coordenador: um `git commit -a` para ajustar o plano publicou o trabalho do agente ainda sem revisão. Desfeito com reset e `--force-with-lease`. Regra adotada: só adicionar arquivos por caminho explícito.
- Próximo: migração SDL2 → SDL3 (Fase 0c), com um agente novo.

### 2026-09-27 — Baseline de áudio entregue (Fase 0c-1)
- Um agente novo entregou `--regress-audio`, aprovado na primeira revisão. O callback de áudio foi extraído sem mudanças para `audio_mix()`, que o harness chama diretamente, sem abrir dispositivo. Os testes negativos (volume, taxa de conversão) quebram exatamente as linhas esperadas.
- A migração para SDL3 foi dividida em três tarefas sequenciais: (1) baseline de áudio [feito]; (2) núcleo; (3) rede, CI e release.
- Descobertas em §2 (itens 13–14): o formato do pipeline de áudio, onde fica a única reamostragem, e o fato de os baselines dependerem da libm e toolchain desta máquina.

### 2026-09-27 — Núcleo migrado para SDL3 (Fase 0c-2)
- Um agente novo portou os 44 arquivos para a API nativa do SDL3, aprovado na primeira revisão. Os 53 baselines passam sem regeneração, inclusive o áudio byte a byte (`SDL_ConvertAudioSamples` produz os mesmos bytes que o `SDL_AudioCVT` do sdl2-compat).
- Pontos de atenção tratados: checagens de erro invertidas (`bool`), `SDL_GetTicks()` truncado para Uint32, escala nearest explícita, text input por janela, fullscreen desktop via `SDL_DisplayID`, áudio via `SDL_AudioStream`, e drivers headless com os novos nomes (`SDL_VIDEO_DRIVER`/`SDL_AUDIO_DRIVER`).
- Verificado pelo coordenador com janela real no macOS: a tela de título aparece com pixels nítidos e a paleta correta; o teclado navega nos menus (setas e Enter); Alt+Enter alterna o fullscreen, ida e volta.
- Ainda não verificado de forma manual: digitação de texto (nome do high score, nome do save), mouse, joystick e o som num dispositivo real.
- A CI (`.github/workflows`) está quebrada nesta branch até a Fase 0c-3, porque ainda instala SDL2.
- Próximo: Fase 0c-3, com rede via SDL3_net, CI e scripts de release para SDL3.

### 2026-09-27 — Rede em SDL3_net (Fase 0c-3a)
- Um agente novo portou `network.c` para o SDL3_net com o mesmo protocolo, aprovado na primeira revisão. Duas instâncias headless em 127.0.0.1 completam o handshake CONNECT/ACK. O lock-step em jogo não foi exercitado, porque precisa de input dos dois lados.
- Mudança de comportamento intencional: um host que não resolve agora gera erro após 10 s; antes o jogo travava em silêncio.
- No mesmo pacote entraram os dois ajustes de áudio pendentes: buffer estático no callback e hint de 1024 frames. Os 53 casos continuam passando.
- Atenção para a Fase 0c-3b: o `sdl3-net.pc` do Homebrew vem com `prefix=` vazio. Por isso o Makefile pega os caminhos de `sdl3` e só os nomes de biblioteca de `sdl3-net`, o que pode falhar em sistemas com os dois pacotes em prefixos diferentes, como a build estática do Linux.
- Instalado nesta máquina: `brew install sdl3_net` (3.2.0).

### 2026-09-27 — CI e empacotamento em SDL3 (Fase 0c-3b)
- Um agente novo migrou `make_macos.sh` (frameworks oficiais SDL3 3.4.16 e SDL3_net 3.2.0, fixados por SHA-256, app universal), `make_linux.sh` (SDL3 estático compilado do código-fonte, fixado e verificado), os 3 workflows, o projeto MSVC e o README. Aprovado na primeira revisão; commit `55baaae`.
- Validado localmente: o app gerado é universal (x86_64 + arm64), carrega os frameworks de dentro do bundle (nenhum caminho do Homebrew), abre sem janela e chega à tela de título; `codesign --verify` passa; 53/53 no teste de regressão.
- **Pendente:** o GitHub Actions nunca rodou neste fork (0 runs, o padrão em forks). Linux, Windows e o projeto MSVC só serão validados depois que o dono ligar o Actions. Ponto a acompanhar: se `make debug` linka contra o SDL3 estático no Ubuntu 22.04.

### 2026-09-27 — Decisão: composição moderna na CPU
- O "backend GPU com paleta no shader" foi substituído por um pipeline de composição na CPU (§7), porque os efeitos ficam na grade de 320×200. A próxima tarefa (Fase 0d) cria esse pipeline e os modos Classic/Modern.

### 2026-09-27 — Pipeline moderno na CPU (Fase 0d) e primeira CI real
- Um agente novo entregou o pipeline moderno e o setting Classic/Modern, aprovado na primeira revisão. Os 63 casos passam. Sem passes, o modo Modern sai byte a byte igual ao scaler "None" do Classic; o custo medido é ~37 µs por quadro. Verificado com janela real em modo Modern.
- O GitHub Actions foi disparado manualmente pela API (em forks, o push não disparava). 1ª rodada: macOS verde; Linux quebrou por falta do `libxtst-dev` e Windows por um banco MSYS2 desatualizado sem `sdl3-net`. Um agente corrigiu os dois (`108e69a`).
- 2ª rodada: macOS e Windows arm64 (clang) verdes. Linux e Windows x86_64 (GCC) falham num erro real de código, `palette.c:87` (ponteiro-para-array `const`, rejeitado pelo GCC com `-pedantic`). Uma auditoria com GCC 16 nos 55 arquivos confirmou que é o único. A correção está em andamento com o mesmo agente.

### 2026-09-27 — CI verde nas três plataformas
- Com o Actions disparado pela API (em forks o push não disparava na primeira vez; hoje o push já dispara), foram 4 rodadas até ficar tudo verde: (1) `libxtst-dev` e o banco MSYS2 desatualizado; (2) `palette.c:87`, que o GCC rejeita com `-pedantic`; (3) `PKG_CONFIG_PATH` só no passo de compilação, o que quebrava `make install`; (4) verde em Linux x86_64/arm64, macOS e Windows x86_64/arm64 (`e5a6d11`).
- Lição registrada em §2.17: auditar com `gcc-16` e as flags da CI toda mudança em `src/`, porque o clang do macOS aceita construções que o GCC rejeita.
- Fase 0 concluída. A Fase 1 começou em paralelo: widescreen/PAR no checkout principal e gamepad numa worktree local separada (`gamepad`), que será incorporada a `modernization` após revisão.

### 2026-09-27 — Pergunta sobre ampliar o campo horizontal
- O usuário perguntou se dá para aproveitar o scroll horizontal nativo para mostrar mais campo no widescreen. Análise: o "scroll" é só o parallax do fundo (camada da frente ~70 px, meio ~47, fundo ~24), os mapas têm 336–360 px, e tiros e inimigos têm limites de remoção próximos da borda original. Só ~20–24 px por lado são viáveis sem inventar arte nem mudar o gameplay.
- Decisão (§7): seguir com a visão estendida dentro desse limite. Começa com uma investigação de medições numa worktree separada; a implementação vem depois do widescreen.

### 2026-09-27 — Gamepad entregue; harness vulnerável a input real
- O agente do gamepad (worktree `gamepad`) caiu uma vez por HTTP 400 do provedor e foi retomado. Entregou a API de Gamepad do SDL3 com mapeamento padrão, hot-plug e remapeamento por nome no cfg; o caminho legado de joystick não mudou. Aprovado; commit `1a33820` na branch local `gamepad`. A integração em `modernization` fica para depois que o widescreen for commitado, porque os dois mexem em `README.md`, `params.c` e `opentyr.c`.
- Na verificação apareceu falha intermitente no cenário `flip` (5 casos, por volta do quadro 855). A causa provável é input real injetado no processo de teste pelas capturas com `osascript` do agente de widescreen (§2.19). Isolado, o cenário passa 3/3. A correção do harness entra na fila.
- O agente de widescreen também encerrou o turno antes de terminar e foi retomado. A técnica para destravar workers opencode foi registrada no CLAUDE.md global.

### 2026-09-27 — Visão estendida medida
- Um agente numa worktree separada instrumentou o renderizador de 8 bits (depois reverteu tudo), varreu o pan inteiro e rodou as 62 fases dos 4 episódios mais as 5 demos. Números em §2.20.
- Resultado: dá para mostrar ~24 px de arte real por lado, com a direita mais pobre (64 % coberta em média). Mostrar também os inimigos nessas faixas esbarra no portão de desenho de `JE_drawEnemy` e exigiria alargar o framebuffer das fases. A escolha entre só fundo e fundo com objetos foi para o usuário (§7, em aberto). A implementação vem depois do widescreen, qualquer que seja a opção.

### 2026-09-27 — Widescreen e PAR entregues (Fase 1a); visão estendida descartada
- O agente de widescreen entregou os ajustes `aspect` e `pixel_aspect` do modo Modern. O canvas tem largura `round(200 × PAR × aspect)`, com o quadro original centralizado e laterais "ambilight" procedurais, determinísticas e escurecidas. Durante o jogo elas usam a coluna 263 do playfield, pulando o HUD. O mapeamento do mouse leva em conta o deslocamento do quadro, e o custo é de ~65–72 µs por quadro em 16:9.
- Na revisão: diff lido, capturas de tela conferidas (4:3 e 16:9, original e square), `make regress` com 66/66 (os 63 antigos com baselines intactos, mais 3 `modern-wide-*` em 16:9) e auditoria GCC 16 limpa. Commit `b1e4313`.
- Pendências menores anotadas pelo agente: no modo Integer, com PAR 1,2, uma janela pequena (640×400) fica com escala quase quadrada; e a janela ainda segue o tamanho do scaler Classic. Um tamanho de janela padrão próprio do Modern resolveria as duas coisas. Fica para depois.
- O usuário escolheu desistir da visão estendida (§7). Próximos passos: integrar a branch `gamepad` e deixar o harness imune a input real.

### 2026-09-27 — Gamepad integrado
- A branch `gamepad` entrou em `modernization` no merge `85f741a`. Os conflitos foram só de adições: as duas seções do README ficaram, e `--selftest-gamepad` virou a opção 269, depois das opções do widescreen (266–268). Depois do merge: build ok, auditoria GCC limpa, `--selftest-gamepad` passou e `make regress` fechou 66/66. A branch local e a ref de proteção foram removidas.
- Obs.: o projeto do Visual Studio em `visualc/` não lista nem `modern.c` nem `regress.c` (desde a Fase 0), então já não compila sem ajuste. A CI do Windows usa MSYS2 e não é afetada. Fica anotado para uma tarefa de manutenção.
- Despachado para um agente novo: harness imune a input real (§2.19).

### 2026-09-27 — HUD modernizado, etapa 1 despachada
- O usuário aprovou o HUD modernizado em duas etapas (§7). A etapa 1 foi para um agente novo na worktree `hud`, em paralelo com o agente do harness no checkout principal.
- Achado na preparação: o desenho do HUD tem efeitos colaterais na lógica. `JE_inGameDisplays` (`mainint.c:2847`) escreve na global `tempW`, que o cálculo do pan lê, e `draw_boss_bar` (`tyrian2.c:~5176`) atualiza `boss_bar[]` enquanto desenha. Mover o desenho exige separar atualização e desenho. A tarefa também cria uma prova permanente: um hash do estado do jogo por quadro (`--regress-state-out`), que tem de ser idêntico entre Classic e Modern.

### 2026-09-27 — Harness imune a input real
- O agente fechou o vazamento na fronteira de entrada:
  - `handleSdlEvents` descarta tudo menos `SDL_EVENT_QUIT` no modo regress.
  - `poll_joysticks` fica inerte.
  - `windowHasFocus` fica fixo em true, porque o build de release pausa sozinho sem foco.
  - `SDL_HINT_MAC_BACKGROUND_APP` é ligado antes do `SDL_Init`.
- Nesta sessão o `osascript` não conseguiu mais entregar teclas ao processo, que nunca virou frontmost. Por isso a prova usou um injetor temporário dentro do processo: uma tecla Up via `SDL_PushEvent` no quadro 855 do cenário flip. Sem o descarte, 3/3 divergem a partir do quadro 858; com o descarte, 3/3 batem com o baseline.
- Revisão: diff de 4 arquivos, build ok, auditoria GCC limpa, autoteste do gamepad ok, `make regress` 66/66.

### 2026-09-27 — HUD modernizado, etapa 1 entregue
- O agente da worktree `hud` desenha os elementos que ficavam dentro do playfield em duas superfícies de 8 bits fora da tela, uma por painel, e o `modern_build_frame` compõe essas superfícies sobre o ambilight. A largura mínima de 51 px vem da barra de chefe. Em 16:10 original (32 px) e em 16:9 square (18 px), tudo continua dentro do playfield, para não deixar um HUD pela metade.
- Prova (§2.21): os fluxos de hash de estado de demo 1, demo 3 e spotlight são byte a byte iguais entre Classic e Modern 16:9 com o HUD movido. Isso virou os 3 casos `state-*`. Só os 3 baselines `modern-wide-*` mudaram, como esperado.
- Revisão: diff conferido (todas as escritas em `tempW` preservadas; a atualização da barra de chefe continua separada do desenho), capturas 1P real e 2P com barras de chefe (sonda headless). Depois do merge em `modernization`: build ok, auditoria GCC limpa, autoteste do gamepad ok, `make regress` 69/69.
- Para a etapa 2: em 16:9 os painéis têm só ~53 px, então cabe um layout de uma coluna; em 21:9 cabe mais. Nomes longos de jogador em rede precisam de truncamento. O P2 deveria ficar alinhado à borda externa.

### 2026-09-27 — Reboot, sessão fantasma e HUD completo (etapa 2)
- O Mac reiniciou no meio das correções da etapa 2 e derrubou o Orca, o agente e o scratchpad. Um agente novo terminou as correções: vidas e dinheiro em linhas separadas, nomes com espaços sobrando aparados antes do corte, a reserva do ícone especial só no 2P, e o caso `state-scenario-spotlight-2p-d3`, o primeiro que roda o bloco de vidas, e portanto as escritas em `tempW`.
- Falhas intermitentes na revisão ("no output written" em demo1-d6, divergência em iced) vinham de uma sessão fantasma (§2.22) disputando o `test/regress/actual/`. Com ela interrompida: 70/70 duas vezes, auditoria GCC limpa. A sessão fantasma também revelou que a falha antiga do quadro 855 era o `data/` errado (§2.19).
- O usuário aprovou o visual. Commit `5a5952b`. Próximo: só o HUD novo no Modern, e splash screens e menus com laterais desfocadas (§7).

### 2026-09-27 — Só o HUD novo na partida; desfoque nos menus
- Nos quadros de jogo com painéis, o compositor copia só o playfield de 264×184. A barra lateral e a faixa de baixo originais continuam sendo desenhadas no quadro de 8 bits, o que garante lógica intacta, mas não entram no canvas. Os painéis ficam com 81/82 px em 16:9, e 16:10 passa a ter painéis (60 px). Os 16 pixels sob o playfield viram uma faixa com o nome da fase e a mensagem do jogo, que chega por um gancho só de leitura em `JE_drawTextWindow` e no apagamento da mensagem. O HUD ganhou a barra de energia das armas (`PWR`), o único dado vital que só a barra original mostrava.
- As telas fora do jogo trocaram o ambilight pelo desfoque da própria tela.
- Revisão: inventário barra→HUD completo, capturas conferidas (1P e 2P em 16:9, 16:10, 21:9, arcade, telas reais de título e menus), ganchos conferidos (o `--textErase` continua avaliado igual), auditoria GCC limpa, `make regress` 71/71 duas vezes. Os `state-*` e os baselines Classic e 4:3 não mudaram.
- Próximo: telas fora do jogo com conteúdo real (§6): Vert- no título e nos menus da pic 2, o painel direito da moldura da loja alargado e a extensão real do mapa, do ship specs, dos starfields e dos créditos.

### 2026-09-27 — Pausa: máquina vai ser desligada
- **Em andamento:** telas fora do jogo, etapa S1. O painel direito da moldura da loja é alargado, e o título e os menus da pic 2 ganham Vert-. O agente foi interrompido de propósito antes do desligamento, para o serviço do opencode não retomar a sessão sozinho (§2.22).
- **Onde está o trabalho:** no working tree de `modernization`, sem commit. Arquivos: `src/modern.c/.h`, `src/video.c/.h`, `src/picload.c`, `src/keyboard.c`, `src/mouse.c`. Há um backup em `refs/keep/screens-s1-wip` (stash `507e703`). A nota de passagem do agente está em `.worker-reports/phase1-screens-s1-handoff.md`.
- **CI vermelha no Linux e no Windows:** falta `#include <stdio.h>` em `src/modern.c`. A correção já está no working tree do S1 e entra junto com ele.
- **Ao retomar:**
  - Conferir que nenhuma sessão opencode ficou ocupada (`opencode api session.list`).
  - Passar o S1 para um agente novo com a nota de passagem. Remover os ganchos de teste, com atenção a eventos de teclado injetados em `keyboard.c`/`mouse.c`, que ficam fora do escopo combinado.
  - Revisar, fazer o commit e ver a CI ficar verde.
- **Depois:** S2 (mapa, ship specs, starfields, créditos) e taxa de quadros independente da lógica com interpolação (Fase 2).

### 2026-09-27 — Retomada: três agentes em paralelo
- Depois do desligamento, tudo intacto e nenhuma sessão fantasma. O S1 das telas foi para um agente novo, com a nota de passagem, no checkout principal. Ele também vai criar a opção `--regress-screen` para abrir telas de menu direto, sem janela, porque a navegação por teclas simuladas não funcionou nesta máquina.
- O usuário pediu prioridade para 60 fps ou mais e quer muito ver a iluminação melhorada. Em paralelo, em worktrees separadas:
  - `framerate`: design e etapas 1–2 da taxa de quadros independente da lógica (lista de desenho por tick e renderizador que reproduz o quadro, com a prova de hash idêntico).
  - `lighting`: bloom mais um mapa de luz dinâmica a partir dos pixels emissivos da paleta. Tiros, explosões e chamas iluminam o terreno com a própria cor. As luzes por objeto virão depois, alimentadas pela lista de desenho.
- As telas S2 (mapa, ship specs, starfields, créditos) esperam o S1, porque usam o mesmo compositor.

### 2026-09-28 — S1 das telas e etapas 1–2 dos 60 fps no branch principal
- **S1 entregue (`296cf7e`).** Título e menus da pic 2 com Vert-; todas as telas da moldura da loja (menu do jogo, upgrade, compra, opções, data cubes, teclado, joystick, load/save) com o painel direito alargado; pic 5/11 com borda sólida. Nenhuma tela da pic 1 caiu no desfoque.
  - As colunas de corte fixas da primeira tentativa (158/310) bloqueavam quase todas as telas: a linha de ajuda do rodapé cruza a tela inteira e o leitor de data cube vai até x=310. A solução foi um corte único, decidido pelo conteúdo: logo depois do elemento mais à direita do painel (y < 184), com a faixa de baixo tratada à parte (o fundo acompanha, o texto de ajuda fica no lugar).
  - Efeito colateral aceito: o título da caixa ("Game Menu") não é recentralizado, porque um segundo corte à esquerda cortaria a linha de ajuda. A faixa de destaque do item selecionado termina onde o painel original acabava.
  - `--regress-screen=NAME` renderiza 13 telas sem janela com as funções do próprio jogo (em modo tela, `hasInput()` diz que há entrada, e o limite de quadros encerra). São 28 casos novos: Modern 16:9 e 21:9, mais os Clássicos, que provam que o harness não altera as telas de 8 bits. A regressão passou a ter 99 casos.
  - Mouse: o mapeamento por partes foi verificado por uma checagem temporária (a faixa inserida cai na coluna de corte, dentro da área clicável dos itens).
- **Etapas 1–2 dos 60 fps (`f35198f`, merge `f7535d7`).** Lista de desenho por tick no nível das primitivas, com identidade de objeto, e replay com prova de igualdade byte a byte (82/82 casos). No merge, `--regress-replay-check` virou a opção 275, ao lado de `--regress-screen` (274). A regressão ficou com 103 casos.
- **Etapa 3 despachada** para um agente novo na worktree `framerate`: interpolação na taxa do monitor, só no modo Modern e em jogo (o Clássico segue idêntico ao original), com opção "Smooth motion" ligada por padrão. Ponto de atenção passado ao agente: a identidade das linhas de fundo da etapa 1 (camada e linha de tela) salta 28 px quando o mapa avança uma linha de tiles; para interpolar o scroll, casar pela linha do mapa, não pela linha da tela.
- Lição de processo: depois de um `worker_done`, a correção da revisão tem que ir por `worker-start --terminal` no mesmo terminal. Um `terminal send` simples funciona, mas o `worker_done` seguinte é rejeitado por capacidade revogada, e só a tela confirma o término.

### 2026-09-28 — 60 fps ou mais: etapa 3 entregue
- **Modern em jogo apresenta na taxa do monitor com movimento interpolado** (`09441a8`, merge `b9b65da`). Na espera do tick fixo (`delayUntilElapsed` em `JE_starShowVGA`, agora `interp_present_gameplay()` em `src/interp.c`), o jogo apresenta quadros a `alpha = decorrido / período do tick`, com vsync. Sem vsync, dorme até o próximo refresh; nunca gira. O deadline continua na grade absoluta do tick.
- Medido numa janela real neste Mac (painel de 120 Hz, embora `SDL_GetCurrentDisplayMode` reporte 60): **~120 fps** apresentados, p99 de 9–9,5 ms, lógica a ~34,8 ticks/s, igual ao original. Custo de ~1,1 ms por quadro apresentado em 16:9.
- Casamento por identidade + ordem dentro da identidade, com índice hash fixo. Snap em nascimento, salto > 64 px e troca de sheet (reuso de slot). As linhas de fundo casam pela linha do mapa com o pan removido (`row_key`), então o scroll não salta na virada de tile. Starfield e superpixels são interpolados por índice. HUD, textos e barras de chefe usam o tick atual.
- O renderizador mantém uma referência do quadro do tick: os filtros de água, lava, blur e iced misturam com o destino e partem dela. Efeito: a 120 fps esses filtros são aplicados sobre quadros intermediários; o quadro do tick continua exato.
- Provas: `--regress-interp-check`, com alpha = 1 byte a byte igual ao quadro real em 52 casos (demos e cenários em todos os detalhes) e 0 overshoots. `--regress-interp-alpha=A` captura quadros intermediários, e `--regress-realtime` mede o ritmo. Regressão com 107 casos.
- Opção `smooth_motion` no `[video]` do cfg (ligada por padrão) e `--smooth-motion=on|off`. Só vale no Modern; o Clássico não muda.
- Pendente: etapa 4 (a iluminação entra por cima do quadro interpolado quando for integrada); README sem as opções novas; opção no menu de setup para ligar e desligar.

### 2026-09-28 — Manutenção entregue; referência dos testes passa a ser o zip oficial
- **Manutenção (`7909049`).** O menu Setup → Graphics ganhou Presentation, Aspect, Pixel Aspect e Smooth Motion; antes, o Modern só era alcançável pelo cfg ou pela linha de comando. As opções valem na hora e ficam esmaecidas no Clássico. O Modern abre uma janela no formato escolhido, a ~80% da área útil da tela. O README documenta todas as opções. A trava de dados (`test/regress/data-manifest.txt`, tamanho + `cksum` POSIX) recusa dados diferentes e diz qual arquivo não bate. O projeto do Visual Studio voltou a listar todos os fontes, sem compilação testada. Regressão com 109 casos.
- **Descoberta:** o zip oficial baixado por `./get_data.sh` tem `newsh9.shp` com 38831 bytes; a cópia de referência em `/Users/vitor/Tyrian` tem 34888 bytes, idêntica ao seu `newsh^.shp`. Por isso um clone novo não conseguia rodar a regressão, e a CI nunca rodou `make regress`: só compila.
- **Decisão do usuário:** a referência dos testes passa a ser o zip oficial do `get_data.sh`, e a CI passa a rodar a regressão. Tarefa despachada na worktree `regress-ci`. Ela troca o manifesto, regenera só os baselines afetados (com prova de que a diferença vem do `newsh9.shp`) e cria os jobs de CI com cache dos dados.

### 2026-09-28 — Telas S2 entregues; primeira CI com regressão
- **S2 (`2b27f5e`, merge `38ab9f1`).** No Modern, jukebox, ship specs e créditos desenham numa superfície de 8 bits da largura do canvas (`modern_screen_begin()`), com as rotinas do próprio jogo. A projeção das estrelas do jukebox só lê o estado: não gasta RNG nem escreve estado.
  - Mapa de navegação: a primeira versão (carta estelar larga atrás da moldura centralizada) foi recusada na revisão. A tela pulava de lugar em relação às outras da loja e mostrava grade à direita do painel, sem sentido espacial. Ficou com o painel alargado do S1.
  - O que bloqueava o alargamento era a margem direita que o mapa pinta (x 314..319). O corte agora ignora elementos além da última coluna de corte possível (311), e um preenchimento de altura inteira na faixa de baixo acompanha a borda.
  - Na revisão também saíram: nave do ship specs deslocada duas vezes (cobria o texto); texto dos créditos centralizado na área preta, à esquerda da arte; estado do teste do mapa (paleta 18 e lista de fases).
  - O simulador de armas continua como no S1: alargar a janela exigiria refazer o layout da loja.
  - São 5 telas novas no `--regress-screen` e 12 casos; a regressão ficou com 121.
- **Primeira CI com regressão (`bdfc0e0`):** macOS verde. No Linux (x86_64 e arm64), só o caso `audio` falha, e todos os quadros e estados batem com os baselines gerados no Mac. No Windows, a trava de dados recusou tudo, provavelmente por CRLF no checkout. As correções estão com o agente da `regress-ci`: `.gitattributes` com LF e o áudio independente do `rand()` da libc.

### 2026-09-28 — Áudio: só o emulador Nuked OPL3
- Diagnóstico do áudio: saída mono; emulador OPL2 derivado do DOSBox de 2010; mixer com corte duro; efeitos de 8 bits a 11 kHz.
- Na CI, o agente tinha trocado o conversor sinc do SDL por interpolação linear só para o teste de áudio ficar portátil. Recusado na revisão, porque piora o som do jogador. Pedido um conversor polifásico sinc próprio, em ponto fixo e idêntico em todas as plataformas, com qualidade igual ou melhor que a do SDL e resposta de frequência medida.
- **Decisão do usuário:** das melhorias propostas (estéreo posicional, Nuked OPL3, limitador suave e trilha OGG fornecida pelo usuário), só o **Nuked OPL3** (emulação de referência do chip, LGPL-2.1+, compatível com a GPL-2+). As outras três não serão feitas.
- Ordem: o Nuked começa depois que o conversor sinc estiver no branch. Ele gera o som em 49716 Hz e precisa desse conversor, em modo contínuo, para chegar a 44,1 kHz.

### 2026-09-28 — Testes do usuário: bugs e decisões de escala
- O usuário testou o binário. Três bugs viraram tarefas, e duas decisões foram tomadas:
  - **Bug: o cenário e as nuvens "pulam" quando a nave se move.** Causa provável: o pan horizontal segue a nave. O x da linha de fundo dá a volta (`mapXOfs % 24`) enquanto o ponteiro do mapa avança um tile, e a interpolação desliza ~23 px para o lado errado a cada tile. Tarefa `interp-fix`, com uma checagem de suavidade na regressão.
  - **Bug: o HUD clássico pisca no início da fase.** Os primeiros quadros saem como quadro de menu. Mesma tarefa.
  - **Bug: "Fit 4:3" e "Fit 8:5" são iguais no Modern.** Unificados em "Fit"; a diferença passa para o Pixel Aspect, que agora vale também no Clássico. Tarefa `scaling`.
- **Decisão:** o pixel aspect correto para a arte é o Original (1,2, desenhada para CRT 4:3). No Modern, o usuário não deve conseguir escolher a pior opção. Pixel Aspect e Scaling Mode saem do menu Modern, e a escala passa a ser sempre "sharp bilinear": pré-escala inteira com nearest, e depois o ajuste fracionário final com linear. Isso deixa os pixels uniformes, sem linhas de 4 e 5 px misturadas que tremem no scroll, com aspecto exato. O Fit do Clássico usa o mesmo caminho.
- **Decisão:** os scalers de software (hq2x etc.) são removidos do jogo: "só queremos pixels perfeitos". A janela do Clássico passa a usar o dimensionamento por múltiplo inteiro.

### 2026-09-28 — Iluminação no branch; níveis redefinidos pelo usuário
- **Iluminação (`ceadcf3`, merge `6b3d7cc`).** Bloom + mapa de luz dinâmico em `src/modern_bloom.c`, só no playfield dos quadros de jogo do Modern. Na revisão foram corrigidos os halos quadrados (três passes de box, isofotas a ~9% do redondo), a ampliação em blocos (agora bilinear), os núcleos estourados (blend estilo screen) e a força excessiva. Custo < 0,5 ms. As opções viraram 281–284 no merge.
- **Decisão do usuário:** mesmo o "baixo" ficou forte demais. Serão três níveis, **Desligado / Baixo / Alto**: Alto = o "baixo" de hoje, Baixo = metade dele. Um só seletor "Lighting" no Setup → Graphics controla bloom e luz juntos. Padrão: Baixo. Tarefa encaixada na rodada 2 do `scaling`, que já mexe no menu.
- **Conversor de áudio (`1c16c81`, merge `34093b2`).** Sinc polifásico com janela Kaiser em inteiros, igual em qualquer plataforma. Plano até 4,98 kHz e rejeição > 90 dB (o do SDL deixava passar a primeira imagem a −6 dB). Só os hashes de sfx/mix mudaram. Na mesma resolução de conflito, o `.vcxproj` voltou a ter CRLF (o merge da iluminação tinha convertido para LF).
- CI no Windows: os quadros batem; os 4 casos `state-*` falham desde a primeira linha, provavelmente por largura de tipo no hash de estado (LLP64). Rodada 3 da `regress-ci`.

### 2026-09-28 — Regressão verde na CI nos três sistemas
- **`b4f42b6`: 123/123 em Linux (x86_64, arm64), macOS e Windows (x86_64, arm64).**
- Três causas de não-portabilidade corrigidas:
  1. CRLF no checkout do Windows: `.gitattributes` com `eol=lf` para `test/regress/**` e os scripts.
  2. Áudio: o resampler do SDL usa float/libm, e o ruído do OPL usava o `rand()` da libc. Agora há um sinc polifásico em inteiros e um Park–Miller local.
  3. Hash de estado: `Player.cash` é `unsigned long` e era hasheado com `sizeof`, que dá 8 bytes no LP64 e 4 no LLP64. Agora todos os escalares vão como little-endian de largura fixa, sem mudar os baselines.
- Todos os hashes de quadro bateram entre plataformas desde a primeira rodada: o desenho do jogo é portátil.
- As varreduras completas (`--replay-check`, `--interp-check`) ficam no workflow manual `regress-full.yml`.

### 2026-09-28 — Nuked OPL3, correções de tremido e HUD; pausa para desligar a máquina
- **Nuked-OPL3 (`570aa48`, merge `1536c6c`).** Upstream `765ec962` (LGPL-2.1+), em modo OPL2, gerado a 49716 Hz e convertido por um resampler polifásico contínuo. É integer-only, com o LFSR de ruído do próprio Nuked, e o Park–Miller saiu. Custo de ~3% do callback de áudio. Mudaram só as linhas music/mix do `audio.txt`. De quebra, foi corrigido um bug na decomposição de fase do resampler ao reduzir a taxa. CI verde nos três sistemas.
- **Bugs do teste do usuário corrigidos (`d9c9c21`, merge `a5aff13`).**
  - Tremido: a origem contínua do pan (`x − 24·bp`) é interpolada e depois reexpressa contra o mapa do tick atual. O novo `--regress-interp-smoothness` acusava 303 eventos na demo1-d2 e agora acusa 0.
  - HUD clássico piscando: a composição de gameplay fica mantida no intro, nos fades de morte e de fim de demo e na animação de fim de fase. `--regress-gameplay-check`: 0 quadros sem os painéis.
  - As opções novas ficaram em 287–289; 285/286 estão reservadas para o VFX. Os baselines Modern de gameplay mudaram só nos quadros de intro/fade.
- **Pausa (usuário vai desligar a máquina).** Dois trabalhos ficaram em andamento, cada um salvo em ref, com nota de passagem:
  - **VFX, rodada de correção 1.** Worktree `vfx`, commit `4830b8b` mais árvore em `refs/keep/vfx-wip`; nota em `.worker-reports/phase2-vfx-1-handoff.md`. Funcional e verde (fumaça com a translucidez do motor, níveis Off/Low/High, padrão Low). Falta atualizar o relatório (§9 com 125/125 e §7 com custo remedido) e mandar `worker_done`.
  - **Scaling, rodada 2.** Worktree `scaling`, commit `5062420` mais árvore em `refs/keep/scaling-wip`; nota em `.worker-reports/scaling-fit-handoff.md`. Implementado e verde: scalers removidos; sharp bilinear no Fit (Modern e Clássico); Modern com pixel aspect fixo 1,2 e sem Scaling Mode/Pixel Aspect no menu; iluminação Off/Low/High com seletor único e padrão Low. Falta: prova de pixels uniformes (o harness em `/tmp` pode se perder no reboot), capturas da iluminação, custo, relatório e reverificação depois de edições cosméticas.
- **Para retomar:** conferir se há sessão fantasma do opencode (§2.22); agentes novos nas duas worktrees, com a nota de passagem; merges na ordem scaling → VFX (os dois mexem no menu Graphics em `src/opentyr.c` e em `params.c`/`config.c`). Depois: luz por objeto a partir dos eventos de VFX e partículas de ambiente.

### 2026-09-28 — Retomada: escala e VFX no branch principal
- Nenhuma sessão fantasma do opencode depois do reboot. A CI do Windows de `a5aff13` também ficou verde.
- **Escala e níveis de luz (`159c519`, merge `ecf68e0`).**
  - Scalers de software removidos: o quadro é convertido em 1x e a GPU escala.
  - Fit usa sharp bilinear: pré-escala inteira nearest num render target em cache, depois um passe linear. As bordas dos pixels ficam numa grade uniforme (rms 0,04 px, contra 0,28 px do nearest).
  - Modern: sempre Fit com PAR 1,2; Scaling Mode e Pixel Aspect ficam escondidos no menu.
  - Luz: seletor único Off/Low/High, com padrão Low. High = o antigo low, Low = metade; `medium` vira alias de high.
  - Custo no renderer de software: +1,9 ms/quadro, que é o limite superior. Na GPU é um quad a mais. Janela real a ~119 fps.
  - No merge, os dois baselines `modern-light-*` mudaram só nas linhas de intro/fade da correção do HUD.
- **VFX parte 1 (`e88167b`, merge `fbd9d80`).**
  - Fumaça e anéis usam o blend/darken de nibble do motor, sem pontilhado.
  - Níveis Off/Low/High, com padrão Low; `medium` vira alias de high.
  - Custo com demo2 em Modern 16:9: +0,007 ms/quadro no high.
  - No merge: o menu Graphics ganhou "Lighting" e "Effects" (o item Effects também ficou clicável com o mouse, que faltava). Os `vfx-*` mudaram só nas linhas de intro/fade.
  - O `.filters` do Visual Studio tinha ficado com XML inválido na remoção dos scalers e foi corrigido. `vfx.c/h` entraram no projeto.
  - 128/128 na regressão.
- **A investigar:** a janela não pede `SDL_WINDOW_HIGH_PIXEL_DENSITY`. Numa tela Retina, o macOS amplia 2x um backbuffer em pontos, então o sharp bilinear roda na resolução lógica. Vale medir se a densidade alta deixa a imagem mais nítida.
- **Próximo:** teste do usuário com o lote todo (escala, luz, VFX). Depois: luz por objeto a partir dos eventos de VFX e partículas de ambiente.
