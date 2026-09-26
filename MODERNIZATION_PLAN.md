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
8. **Display e input**: fullscreen desktop, janela, scalers e os modos Center/Integer/8:5/4:3 já existem, assim como o remapeamento de teclado (`src/config.c:297`). O joystick usa a API legada `SDL_Joystick`, não `SDL_GameController`.

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
| Parallax e ambiente | Parcial | P (ambiente) / G (camadas) | O parallax já existe. Camadas novas exigem arte nova, o que contradiz a seção "Scope Philosophy". Poeira, névoa e partículas ambiente são baratas. |
| Widescreen com playfield fixo | Alta | M | Recortar os 264×184 e redesenhar fora deles o HUD que hoje fica dentro. As laterais podem mostrar, escurecida, a arte original que existe além da borda. Menus, loja e cutscenes (320×200) ficam centralizados com moldura. |
| HUD expandido | Alta | M | Todo o estado está em globais (`player[]`, armas, escudo, armadura), então é só um desenho novo lendo esses dados. Os modos 2P, arcade e rede multiplicam os layouts. |
| Controle, remapeamento e autofire | Alta | P/M | Migrar para a API de gamepad do SDL dá mapeamento padrão e hot-plug. O Tyrian já atira segurando o botão, então turbo é trivial. |
| Acessibilidade | Alta | P/M | Reduzir flashes = interceptar `fade_white` e os filtros de fase. Cores alternativas de projéteis dependem do tag buffer. Reduzir partículas e shake vale só para os efeitos novos. |
| Modos Classic e Modern | Alta | — | O Classic é o caminho atual, mantido intacto. |

## 4. Arquitetura

```text
Tick de lógica original (~35 Hz, intocado)
   │  desenha no framebuffer 8-bit 320×200 (como hoje)
   │  + tag buffer 8-bit paralelo (categoria de cada pixel)
   │  + snapshot só-leitura do estado + fila de eventos
   ▼
Camada moderna (GPU, na taxa do display)
   ├── paleta aplicada no shader (efeitos 8-bit preservados)
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

## 5. Riscos

| Risco | Impacto | Mitigação |
|---|---|---|
| Quebra de determinismo (demos e netplay) | Alto e silencioso | Teste de regressão por demos em cada mudança; RNG separado para os efeitos. |
| Trepidação a 35 Hz em telas de 60 Hz | É o que mais "parece antigo" | VRR primeiro; a interpolação fica para a Fase 3. |
| Mistura de resoluções (efeito em 4K sobre pixel de 320×200) | Perder a identidade visual | Prototipar cedo com opção de efeitos presos à grade; comparar lado a lado. |
| Divergência do upstream OpenTyrian | Correções do upstream difíceis de trazer | Concentrar as mudanças em módulos novos; tocar o mínimo em `tyrian2.c` e `mainint.c`. |
| Contradições na proposta: "High-resolution sprites" e camadas de parallax extras exigem arte nova | Escopo | Decidir explicitamente (ver decisões em aberto). |

## 6. Fases

### Fase 0 — Fundação (visualmente não muda nada)
- [ ] Build local funcionando e documentado (macOS)
- [ ] Teste de regressão: rodar as demos sem janela, com hash do framebuffer 8-bit e do estado por tick; baseline gravado
- [ ] Backend GPU com a paleta aplicada no shader; saída idêntica ao scaler atual
- [ ] Modos Classic/Modern como configuração (Modern = Classic por enquanto)

### Fase 1 — Ganhos visíveis e baratos
- [ ] Correção de PAR (4:3) com integer scaling por eixo
- [ ] Widescreen: playfield 264×184 recortado, HUD original ao lado, laterais com a arte além da borda escurecida
- [ ] HUD que fica dentro do playfield (dinheiro, vidas, superbombs) movido para fora no modo Modern
- [ ] Gamepad via API moderna do SDL, com hot-plug e remapeamento
- [ ] Bloom simples pela máscara de brilho da paleta

### Fase 2 — O "Modern"
- [ ] Snapshot e fila de eventos por tick
- [ ] Tag buffer nas funções `blit_sprite*`
- [ ] Luzes dinâmicas, com cor derivada da matiz da paleta
- [ ] VFX: faíscas, destroços, fumaça, shockwave, trilhas, muzzle flash e impactos
- [ ] Partículas ambiente (poeira, névoa, energia)
- [ ] HUD expandido (painéis esquerdo e direito; layouts 1P, 2P, arcade e rede)
- [ ] Acessibilidade: menos flashes, menos partículas, cores alternativas de projéteis, intensidade dos efeitos ajustável

### Fase 3 — Opcional e cara
- [ ] Separar as camadas de fundo (bg1, bg2, bg3, inimigos) em buffers próprios, preservando a matemática dos blends
- [ ] Interpolação para renderizar a 60/120/144 Hz

## 7. Decisões

### Em aberto
- **Estética dos efeitos:** partículas e luzes em alta resolução ou presas à grade de 320×200? (Resolver com protótipo na Fase 2.)
- **Arte nova:** aceitar alguma (camadas de parallax extras, sprites em alta resolução) ou manter 100% da arte original?

### Tomadas
- **2026-09-26 — Fase 0 aprovada; backend = SDL3.** A migração SDL2 → SDL3 (API de GPU/renderer com shaders e API nova de gamepad) foi a recomendação aceita. SDL3 3.4.16 já está instalado via Homebrew.
- **2026-09-26 — Ordem da Fase 0:** o teste de regressão por demos vem antes da migração para SDL3, porque é a rede de segurança dela.

## 8. Processo

- **Branch:** todo o trabalho acontece na branch `modernization` (remote `origin` = `github.com/vittau/modern-tyrian`).
- **Implementação:** feita por agentes OpenCode (DeepSeek V4.1 Flash) orquestrados pelo Orca, no checkout principal, uma tarefa por vez quando as tarefas mexem nos mesmos arquivos.
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
