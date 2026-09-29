# Modern Tyrian — Plano de integração do Tyrian 2000

Plano e diário da integração do **Tyrian 2000** ao Modern Tyrian, mantendo o **Tyrian 2.1 Freeware** exatamente como está. Nasceu do briefing do usuário de 2026-09-29. O plano geral de modernização continua em `MODERNIZATION_PLAN.md`, e este documento cobre só a trilha do 2000.

**Estado:** Fase 1 (pesquisa) concluída em 2026-09-29, com os documentos em `docs/t2000/` aguardando a aprovação do usuário, necessária antes da Fase 3. Fase 2 concluída (2a: variante, provider e `--variant=`; 2b: saves por variante e migração). A Fase 3 espera a aprovação dos documentos.

---

## 1. Objetivo

Uma única aplicação, uma única engine, duas variantes históricas do mesmo jogo:

```text
Modern Tyrian
├── Tyrian 2.1 Freeware  (dados embutidos no pacote, como hoje)
└── Tyrian 2000          (dados obtidos pelo usuário, nunca distribuídos pelo projeto)
```

No começo, um launcher 16:9 deixa o usuário escolher a variante:
- o **2.1** continua funcionando como hoje;
- o **2000** usa a funcionalidade do **OpenTyrian2000** portada para a nossa base, com o renderer moderno, e seus dados são baixados ou apontados pelo próprio usuário.

**Filosofia (do briefing):** preservar o Tyrian original e modernizar só a forma como ele roda (resolução, widescreen, iluminação, partículas, efeitos, áudio, interface, controles, qualidade de vida). O 2.1 e o 2000 são duas versões históricas do mesmo jogo, não dois jogos.

---

## 2. Princípios

1. **Uma aplicação só.** Nada de dois executáveis ou dois projetos.
2. **Engine separada do conteúdo.**
   - **Compartilhado:** renderer, áudio, input, timing, UI, efeitos, saves e game loop.
   - **Isolado atrás de abstrações:** o que for específico de uma variante.
   - **Sem `if (isTyrian2000)` espalhado pela engine.** Diferença estrutural ganha uma abstração; diferença pontual ganha uma tabela ou hook por variante.
3. **Portar a funcionalidade, não a estrutura.** Nada de merge cego do OpenTyrian2000: cada mudança dele é classificada (§4) e reimplementada sobre a nossa arquitetura.
4. **O 2.1 não regride.** Os 164 casos da regressão (baselines do 2.1) são a trava. Toda mudança estrutural roda a suíte e testa as duas variantes.
5. **A diferença histórica é preservada.** Se o 2000 se comporta diferente do 2.1, cada versão mantém o seu comportamento correto. Nada de "corrigir" um lado.
6. **Os dados do 2000 nunca entram no projeto** (§9).

---

## 3. O que já sabemos (pesquisa inicial, 2026-09-29)

### 3.1 O fork de referência
- **Repositório:** `KScl/opentyrian2000` (GPL-2.0, fork do `opentyrian/opentyrian`).
- **Candidatos a pino:**

  | Ref | Commit | Data |
  |---|---|---|
  | tag `v2000.20250408` | `573ccd6` | abril de 2025 |
  | `master` | `aad5aca` | 2026-02-22 |

  O `master` traz um commit próprio a mais: `dfe1050`, "Charging sidekicks do not auto-fire in Tyrian 2000". A decisão está em §12.
- **Tamanho da divergência:** o `master` do fork está 44 commits à frente do upstream (a maioria merges) e 19 atrás. O diff é de ~50 arquivos e ~1.300 linhas. É uma camada fina sobre o OpenTyrian, não uma reescrita.
- **Commits próprios do fork que definem o 2000:**
  - Modo **Timed Battle**: cronômetros e bônus de tempo idênticos ao T2K, e nenhum save ao iniciar a batalha.
  - **Eventos de fase do T2K** (`e376530`, `62f58dc`, `10a5752`), incluindo o evento 83, que duplica o 4, e a substituição de inimigos.
  - **Leitura de todas as strings** dos dados do 2000 (`73eab84`), com `helptext.c` e `episodes.c` estendidos.
  - **Dados embutidos no código atualizados** (`e1b86fc`): os "twiddles", ou combos de manche.
  - **Diálogo de confirmação ao salvar** e melhorias de menu.
  - **Upgrade da arma traseira** quando não existe "None" (`3896d6c`).
  - **High score do Hazudra Fodder**, **placar mais claro** sob o smoothie de visão obscurecida, e o **rastro do Flying Punch**.
  - **Sidekicks com carga** que não disparam sozinhos no 2000 (`dfe1050`).
  - **Modo Natal** (`--xmas` / `--no-xmas`).
  - **Correções gerais** que não são exclusivas do 2000 (config corrompido quando os dados falham, mouse inválido sem config, acesso fora dos limites no menu de upgrades). Candidatas a correção comum.
- **Arquivos que o fork toca, com a sobreposição com o nosso trabalho:**

  | Arquivo | Linhas no fork (+/−) | Sobreposição com o Modern |
  |---|---|---|
  | `src/tyrian2.c` | +170/−46 | alta |
  | `src/mainint.c` | +187/−110 | alta |
  | `src/game_menu.c` | +226/−58 | alta (menus alargados, remap, HUD) |
  | `src/menus.c` | +187 | alta |
  | `src/config.c` / `.h` | +167/−72 | alta (Deck, detalhe Pentium, analógico) |
  | `src/episodes.c` / `.h` | — | — |
  | `src/helptext.c` / `.h` | — | — |
  | `src/lvlmast.c` / `.h` | — | — |
  | `src/sndmast.c` / `.h` | — | — |
  | `src/varz.c` / `.h` | — | — |
  | `src/keyboard.c` | — | — |
  | `src/shots.c` | — | — |
  | `src/sprite.c` | — | — |
  | `src/opentyr.c` | — | — |
  | `src/params.c` | — | — |

  Os arquivos de maior sobreposição são justamente os que o Modern mais alterou, e é neles que um merge cego quebraria.
- **Rede:** o fork não tem arena, só `--net` pela linha de comando, e a rede do 2000 nunca foi testada.

### 3.2 Os dados do Tyrian 2000
- **Fonte:** `https://www.camanis.net/tyrian/tyrian2000.zip`, o site do autor (Jason Emery), a mesma fonte que o README do OpenTyrian2000 indica.
- **Arquivo medido em 2026-09-29:**

  | Campo | Valor |
  |---|---|
  | `EXPECTED_SIZE` | **5.051.363** bytes |
  | `EXPECTED_SHA256` | `348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667` |
  | Conteúdo | 100 arquivos, 12,3 MB descompactados, sob `tyrian2000/` |

  Tamanho e hash foram medidos com um download feito para o scratchpad, nunca para o repositório.
- **Conteúdo:**
  - `tyrian1.lvl` … `tyrian5.lvl` e `levels2–5.dat`;
  - `cubetxt1–5.dat`;
  - `tyrian.hdt`, `tyrian.snd`, `music.mus`, `tyrian.pic`, `palette.dat`;
  - `newsh*.shp`, `shapes*.dat`, `estsc.shp`;
  - `demo.1–5` (as cinco idênticas byte a byte às do 2.1);
  - executáveis DOS (`tyrian2.exe`, `setup.exe`, `shipedit.exe`) e ícones.
- **Licença (ponto de atenção):** o zip **não traz texto de licença**. O `readme.txt` é o release note de 1999 ("Copyright (c) 1999, Eclipse Software / Stealth Productions, All Rights Reserved"). O status de freeware vem do anúncio da Camanis. Por isso o projeto **não redistribui** esses dados em nenhuma forma (§9). Diferente do 2.1, cujo pacote freeware traz `license.doc`, que permite a redistribuição e que já embutimos.

### 3.3 O nosso código hoje (pontos de contato)
- **Episódios:** `src/episodes.h` já tem `EPISODE_MAX 5` e `EPISODE_AVAILABLE 4`, então a estrutura do episódio 5 existe em parte, herdada do upstream.
- **Dados:**
  - a pasta vem de `--data` (`src/params.c`) ou do padrão `./data`;
  - `src/file.c` testa `tyrian1.lvl` para validar a pasta;
  - `get_data.sh` baixa o zip do 2.1.
- **Config e saves** (`src/config.c`):
  - arquivos `tyrian.cfg`, `tyrian.sav` e `opentyrian.cfg`, no diretório do usuário ou no modo portátil (`opentyrian.cfg` ao lado do executável);
  - no Steam Deck, config e log vão para `~/.config/opentyrian/`.
- **Regressão:**
  - `tools/regress.sh` recusa uma pasta de dados que não bata com `test/regress/data-manifest.txt`, o manifesto do 2.1;
  - todos os baselines valem só para os dados do 2.1;
  - em modo de regressão, `userFilesDisable()` bloqueia arquivos do usuário.
- **Renderer moderno:** o pipeline de composição na CPU (`src/modern.c`, `modern_hud.c`, `modern_bloom.c`, `interp.c`, `vfx_*`) e o tag buffer (`src/drawlist.c`). Tudo isso é independente da variante, desde que os dados usem o mesmo formato. Os pontos frágeis são:
  - as heurísticas pic-1 e pic-2 (colunas de alargamento, créditos da pic 2 em linhas fixas);
  - o HUD construído sobre `miscText[...]`;
  - as tabelas de classe e cor do tag buffer.

---

## 4. Classificação das mudanças do fork (entregável da Fase 1)

Cada mudança do OpenTyrian2000 (diff `opentyrian/opentyrian@<base>...KScl/opentyrian2000@<pino>`) entra numa destas classes, com arquivo, linhas e destino:

| Classe | Destino no Modern Tyrian |
|---|---|
| Específica do 2000 (conteúdo e regras) | Módulo ou tabela da variante 2000 (§5) |
| Correção geral (vale para os dois) | Correção comum, com caso de regressão no 2.1 |
| Renderer | Adaptar ao pipeline moderno; nunca voltar ao caminho antigo |
| Input | Reimplementar sobre o input do SDL3 e o analógico |
| Áudio | Reimplementar sobre Nuked-OPL3 e loudness |
| Gameplay | Hook da variante, preservando o comportamento de cada versão (§2.5) |
| Efeito colateral da arquitetura do fork | Descartar, documentando o motivo |

A classificação vira `docs/t2000/fork-diff.md`, que é o documento técnico da integração.

---

## 5. Arquitetura alvo

### 5.1 `GameVariant`
Enum explícito, decidido **antes** da inicialização completa (antes de carregar config, dados e episódios):

```c
typedef enum { VARIANT_TYRIAN21, VARIANT_TYRIAN2000 } GameVariant;
```

Uma estrutura por variante reúne o que muda:
- nome exibido e sufixo de logs;
- lista de arquivos obrigatórios e o validador;
- episódios disponíveis (4 ou 5);
- namespace de saves e config;
- tabelas de dados embutidos (twiddles, textos);
- hooks de regra (eventos de fase do T2K, Timed Battle, sidekicks com carga, rear "None").

Nada de strings de versão soltas. O resto da engine consulta essa estrutura ou os hooks dela.

### 5.2 `GameDataProvider`
Camada que localiza e valida os dados, **separada** do downloader e do carregamento de assets:

```text
GameDataProvider
├── Tyrian21DataProvider    (pasta ./data do pacote ou --data)
└── Tyrian2000DataProvider  (pasta do usuário, instalada ou apontada)
```

Cada provider sabe:
- onde procurar os dados;
- quais arquivos são obrigatórios;
- como validar a instalação (tamanho e hash por arquivo, num manifesto no estilo do `data-manifest.txt`);
- como reconhecer uma instalação existente (GOG, pasta manual, zip).

O 2.1 passa a usar o provider explicitamente, sem mudar de comportamento.

### 5.3 Namespaces de usuário
- **Pastas separadas por variante:**
  ```text
  <user dir>/
  ├── tyrian21/   (tyrian.sav, tyrian.cfg, opentyrian.cfg?)
  └── tyrian2000/
  ```
- **Migração:** o 2.1 **migra** os arquivos atuais da raiz para `tyrian21/` na primeira execução, sem apagar os originais, e com prova por teste de que saves antigos continuam carregando.
- **Formato de save:** se os formatos diferirem, cada variante ganha seu parser. Nunca se sobrescreve um save da outra variante.
- **Configurações compartilhadas:** as de apresentação (Modern/Classic, escala, controle) podem ser comuns. A decisão está em §12.

### 5.4 Launcher
- **Formato:** tela 16:9 desenhada no pipeline moderno, em arte procedural (§7 do plano geral: nada desenhado à mão), com dois painéis:
  - **esquerda, TYRIAN 2.1 FREEWARE:** Episodes 1–4 e a experiência original;
  - **direita, TYRIAN 2000:** Episodes 1–5, naves e armas novas, Timed Battle e conteúdo expandido.
- **Controles:** `←/→` escolhe a variante, `Enter/A` confirma, `Esc/B` sai. Funciona com teclado e gamepad (Steam Deck incluso).
- **Estado dos dados:** o launcher mostra se os dados do 2000 estão instalados. Quando faltam, o botão vira "INSTALL", e ele nunca presume o 2000 instalado.
- **Sempre abre, dentro do binário:** o launcher é a primeira tela do próprio executável, não um programa separado, e abre em todo início, inclusive no Deck. A última escolha só define o painel que começa selecionado. `--variant=2.1|2000` pula o launcher apenas para regressão e automação (§12.5).
- **Correção do briefing:** o briefing cita "Arena Multiplayer" no 2.1, mas o Modern Tyrian não tem arena. A rede é só `--net` pela linha de comando, herdada do OpenTyrian. O texto do launcher lista só o que existe.

### 5.5 Instalador dos dados do 2000
- **Fluxo:** download direto da máquina do usuário para a fonte definida (§3.2), sem servidor intermediário, proxy ou mirror do projeto.
  ```text
  Download → resposta HTTP → tamanho → SHA-256 → extração → validação dos arquivos → instalação
  ```
- **Constantes versionadas no código:** `DOWNLOAD_URL`, `EXPECTED_FILENAME`, `EXPECTED_SIZE`, `EXPECTED_SHA256` e o manifesto dos arquivos extraídos.
- **Nada falha pela metade:** o download vai para um arquivo temporário, a extração para uma pasta temporária, e só um `rename` atômico instala. Um download interrompido pode ser repetido.
- **Falha de validação:** o instalador mostra "The downloaded Tyrian 2000 data could not be verified. Please try again or install the data manually." e não instala nada.
- **Instalação manual:** o usuário pode escolher uma pasta, o `tyrian2000.zip` ou uma instalação existente (GOG, Steam ou compatibilidade). O caminho não é fixo: há sugestões por sistema e sempre um fallback de seleção manual, e tudo passa pela mesma validação.
- **Implementação:**
  - HTTP(S) com a pilha do próprio sistema: WinHTTP no Windows, NSURLSession ou `curl` no macOS, `libcurl` via dlopen no Linux/SteamOS. A escolha é decidida na Fase 5, sem dependência estática nova se houver alternativa.
  - Extração de zip mínima: só método stored/deflate, com `miniz` ou equivalente de licença compatível.
  - O downloader fica independente da engine: é um módulo sem acesso ao estado do jogo.

### 5.6 Identificação da variante
- **Onde aparece:** "TYRIAN 2.1" ou "TYRIAN 2000" discreto no menu principal, no pause e no About.
- **Log de inicialização:**
  ```text
  Modern Tyrian v0.x.y
  Game Variant: Tyrian 2000
  Data Path: …
  Data Validation: OK            (ou FAILED, com Missing: … / Invalid: …)
  ```

---

## 6. Fases

As mesmas regras do plano geral valem aqui:
- cada tarefa vai para um worker novo (Codex `gpt-6-sol`, ou Sonnet 5.5 quando a cota acabar), num worktree próprio a partir de `modernization`;
- o Claude revisa, faz commit, merge e push;
- todo merge roda `make regress`;
- este arquivo é o diário da trilha 2000.

### Fase 1 — Pesquisa (sem código de jogo)
- [x] Fixar o pino do OpenTyrian2000 (§12) e a base do upstream contra a qual o diff é lido.
- [x] Classificar cada mudança do fork (§4) → `docs/t2000/fork-diff.md`.
- [x] Mapear os formatos de dados do 2000 que diferem do 2.1: `.lvl` do episódio 5, strings em `tyrian.hdt`, `levels5.dat`, `cubetxt5.dat`, eventos de fase novos e save.
- [x] Documentar as diferenças de gameplay confirmadas no jogo original e no fork (armas, naves, sidekicks, eventos, Timed Battle, scoring).
- [x] Levantar todos os pontos de integração no nosso código, arquivo por arquivo, com o risco para os recursos Modern (§3.3).
- [x] Plano de testes do 2000: como gerar baselines e demos sem commitar dados (§8).

**Resultado:** documento técnico aprovado pelo usuário antes da Fase 3.

### Fase 2 — `GameVariant` e provider (só no 2.1)
- [x] `GameVariant` e a estrutura por variante (§5.1), com o 2.1 como única implementação.
- [x] `GameDataProvider` (§5.2). O 2.1 carrega os dados por ele.
- [x] Namespaces de usuário com migração transparente do save atual (§5.3), provada por teste. As configs ficam compartilhadas na raiz.
- [x] Log de variante e validação (§5.6).
- [x] `--variant=` (parse antes de tudo, em `src/bootstrap.c`). O hook do launcher fica para a Fase 6.
- [x] **Trava:** os 164 casos passam sem mudar nenhum baseline.

**Resultado:** a arquitetura suporta duas versões, e o 2.1 fica byte a byte igual.

### Fase 3 — Núcleo do OpenTyrian2000
- [ ] Parser e dados do 2000 (strings, episódio 5, tabelas embutidas), atrás da variante.
- [ ] Eventos de fase do T2K e substituição de inimigos, como hooks.
- [ ] Regras específicas: sidekicks com carga, rear "None", twiddles do 2000.
- [ ] Resolver os conflitos com os menus e HUD do Modern (`game_menu.c`, `menus.c`, `mainint.c`, `tyrian2.c`).
- [ ] Compila e roda com os dados do 2000 apontados à mão, e o Episódio 1 inicia.
- [ ] O 2.1 segue verde na regressão.

**Resultado:** o Tyrian 2000 inicia.

### Fase 4 — Tyrian 2000 completo
- [ ] Episódios 1 a 5, jogáveis do início ao fim pela interface normal. O episódio 5 não ganha implementação paralela.
- [ ] Naves, armas, inimigos, bosses, música, sons, loja, menus, textos e cutscenes, progressão, scoring e saves.
- [ ] Timed Battle e modo Natal.
- [ ] Sessões de teste reproduzíveis por item (demos e scripts de regressão, §8).

**Resultado:** o Tyrian 2000 é jogável do começo ao fim.

### Fase 5 — Instalador dos dados
- [ ] Detector de instalação existente (pastas sugeridas e seleção manual).
- [ ] Downloader com URL, tamanho e SHA-256 versionados (§3.2), temporário e rename atômico.
- [ ] Extração e validação por manifesto.
- [ ] Instalação manual por pasta, zip ou GOG.
- [ ] Mensagens de erro do briefing.
- [ ] Testes:
  - [ ] instalação limpa;
  - [ ] instalação existente;
  - [ ] download interrompido;
  - [ ] zip corrompido rejeitado;
  - [ ] dados errados (os do 2.1 apontados como 2000) rejeitados.

**Resultado:** o usuário instala o 2000 sem conhecimento técnico.

### Fase 6 — Launcher
- [ ] Tela 16:9 (§5.4) no pipeline moderno, com teclado, gamepad e navegação.
- [ ] Estado dos dados, "Install" quando necessário e o painel da última escolha pré-selecionado.
- [ ] Steam Deck: sempre começa no launcher, como nos outros sistemas.

**Resultado:** a experiência de início é simples.

### Fase 7 — Renderer moderno no 2000
- [ ] Widescreen, escala, iluminação, partículas, HUD e efeitos validados no 2000, e o movimento suave também.
- [ ] Heurísticas pic-1 e pic-2 e créditos conferidos nas telas do 2000.
- [ ] Tag buffer e luz colorida cobrindo os sprites novos (naves e armas do 2000).
- [ ] Comparação visual com o 2000 original no Classic.

**Resultado:** as duas versões ganham a modernização visual.

### Fase 8 — Regressão final
- [ ] Matriz completa (§7) nas duas variantes.
- [ ] O 2.1 não regrediu.
- [ ] O 2000 não depende por acidente dos dados do 2.1.
- [ ] Saves separados.
- [ ] Nenhum dado do 2000 no Git (verificação automática na CI, §9).
- [ ] Instalador: limpo, interrompido, corrompido, dados errados.

---

## 7. Matriz de testes

| Área | Tyrian 2.1 | Tyrian 2000 |
|---|:---:|:---:|
| Launcher | ✓ | ✓ |
| Detecção de dados | ✓ | ✓ |
| Instalação de dados | N/A | ✓ |
| Menu principal | ✓ | ✓ |
| Episódio 1 | ✓ | ✓ |
| Episódio 2 | ✓ | ✓ |
| Episódio 3 | ✓ | ✓ |
| Episódio 4 | ✓ | ✓ |
| Episódio 5 | N/A | ✓ |
| Timed Battle | N/A | ✓ |
| Naves | ✓ | ✓ |
| Armas | ✓ | ✓ |
| Inimigos | ✓ | ✓ |
| Bosses | ✓ | ✓ |
| Áudio | ✓ | ✓ |
| Save/Load | ✓ | ✓ |
| Widescreen | ✓ | ✓ |
| Renderer moderno | ✓ | ✓ |
| Pause | ✓ | ✓ |
| Controle | ✓ | ✓ |

---

## 8. Testes e regressão do 2000

- **O 2.1 continua como está:** manifesto e baselines atuais, 164 casos, `make regress`.
- **O 2000 ganha uma suíte separada** (`make regress-2000`):
  - manifesto próprio, com tamanho e CRC de cada arquivo do 2000;
  - baselines próprios em `test/regress-2000/`.

  Os baselines são hashes de quadro e estado, não dados do jogo. Nenhum trecho dos arquivos originais é commitado.
- **Origem dos dados nos testes:**
  - **local:** a suíte usa a pasta instalada pelo instalador ou `TYRIAN2000_DATA`;
  - **CI:** baixa da mesma URL oficial a cada execução (5 MB) e confere o tamanho e o SHA-256, **sem cache do Actions**. A Fase 1 mostrou que o cache de um repositório público pode ser restaurado por PRs de forks, então não é privado. Os dados ficam só na pasta temporária do job, fora do checkout e dos pacotes. A escolha está em §12.
- **Demos:** as `demo.1–5` do 2000 são idênticas às do 2.1 e só jogam o episódio 1. Servem de base, mas os casos do episódio 5, do Timed Battle e dos eventos novos vêm de scripts de regressão e fixtures do próprio código (`docs/t2000/integration.md`).

---

## 9. O que nunca entra no projeto

- **Fora do repositório e dos pacotes:** o `tyrian2000.zip`, qualquer arquivo dele (`*.lvl`, `*.dat`, `*.shp`, `*.snd`, `*.mus`, `*.pic`, `*.hdt`, demos) e qualquer sprite, música ou mapa extraído.
- **Fora de toda hospedagem do projeto:** GitHub Releases, LFS, site pessoal, CDN e mirrors.
- **Checagem na CI:** a CI falha se algum arquivo com o hash de um arquivo do 2000 aparecer no repositório ou num pacote de release. Os hashes ficam no manifesto da Fase 5.
- **Código do fork:** é GPL-2.0, compatível com o nosso. Todo trecho portado guarda a atribuição e o histórico (commit de origem citado na mensagem).

---

## 10. Regras para os agentes (valem em todas as tarefas desta trilha)

**Não fazer:**
- reescrever o OpenTyrian2000 inteiro sem necessidade;
- copiar arquivos do fork em bloco;
- misturar regras do 2.1 e do 2000;
- mudar comportamento histórico por preferência;
- adicionar dados do 2000 ao Git;
- presumir que os dados têm a licença do código;
- remover qualquer coisa do 2.1 para acomodar o 2000;
- espalhar `if` de variante pela engine.

**Fazer:**
- abstrações compartilhadas;
- diferenças isoladas e documentadas;
- mudanças pequenas e incrementais;
- rodar a regressão do 2.1 (e a do 2000, quando existir) após cada mudança estrutural;
- preservar o renderer moderno e a compatibilidade existente;
- validar os dados antes de carregar;
- manter o downloader fora da engine;
- commits pequenos com mensagem clara.

**Continuam valendo as regras de sempre:** os workers não fazem commit, push nem troca de branch e não editam os arquivos de plano; nada de caminhos absolutos do usuário no código; a auditoria GCC-16 e a regressão rodam antes de entregar.

---

## 11. Riscos

| Risco | Mitigação |
|---|---|
| Conflito do fork com os arquivos que o Modern mais alterou (menus, HUD, config) | Portar por classe (§4), nunca por merge. Menus e HUD do 2000 são refeitos sobre o código Modern |
| Heurísticas do renderer (pic-1, pic-2, créditos, `miscText`) presas ao 2.1 | Fase 7 dedicada. Por tela, dados por variante em vez de constantes |
| Saves atuais dos usuários perdidos na migração para namespaces | Copiar sem apagar, com teste de carga dos saves antigos e log da migração |
| A fonte oficial (camanis.net) sair do ar ou mudar o arquivo | Hash fixo recusa arquivo diferente, e a instalação manual e a detecção do GOG seguem funcionando |
| Licença dos dados do 2000 sem texto explícito | Nunca redistribuir. O download é feito pelo usuário, da fonte do autor |
| O fork segue o upstream com merges, e a nossa base é o upstream de `5a9d8da` | Fixar o pino e ler só o diff próprio do fork. Correções do upstream entram pelo caminho normal |
| O Timed Battle e os eventos do T2K mexem no game loop compartilhado | Hooks da variante com regressão do 2.1 obrigatória. O loop comum não muda de assinatura sem necessidade |

---

## 12. Decisões do usuário (2026-09-29)

1. **Pino do fork:** `KScl/opentyrian2000@master`, commit `aad5aca` (2026-02-22).
2. **Fonte do download:** `camanis.net/tyrian/tyrian2000.zip`, com o tamanho e o SHA-256 de §3.2. Instalação manual e detecção do GOG como alternativas.
3. **CI do 2000:** a CI baixa os dados da fonte oficial, confere o hash e roda `make regress-2000`. Sem cache do Actions, que num repositório público não é privado (ajuste da Fase 1).
4. **Configurações:** apresentação e controle compartilhados entre as variantes; saves, high scores e progresso separados por variante.
5. **Launcher:** **sempre abre no launcher**, inclusive no Steam Deck, sem pular para a última variante. O launcher é **parte do próprio binário**, a primeira tela do jogo desenhada no pipeline moderno, e não um programa separado. A última escolha só define qual painel começa selecionado. `--variant=` fica para regressão e automação.
6. **Branch:** `modernization`, fase a fase, com o 2.1 sempre verde.

---

## 13. Diário

### 2026-09-29 — Mapeamento
- **Briefing do usuário registrado:** Tyrian 2000 integrado a uma única aplicação, launcher 16:9, instalador que baixa direto da fonte oficial com SHA-256, instalação manual e GOG, saves separados, renderer moderno nas duas variantes e nenhum dado do 2000 distribuído.
- **Pesquisa inicial:**
  - o fork `KScl/opentyrian2000` é uma camada fina (~1.300 linhas em ~50 arquivos) sobre o OpenTyrian;
  - o `tyrian2000.zip` oficial tem 5.051.363 bytes, SHA-256 `348bc76e…1667` e 100 arquivos, sem licença explícita;
  - o nosso `EPISODE_MAX` já vale 5.
- **Correção do briefing:** o Modern Tyrian não tem arena multiplayer, então o launcher não pode anunciar uma.
- **Próximo:** as decisões de §12 e depois a Fase 1 (classificação do diff do fork → `docs/t2000/fork-diff.md`), com um worker novo.
- **Decisões do usuário (§12):**
  - fork no `master`;
  - download da Camanis com o hash fixo;
  - dados do 2000 baixados na CI;
  - apresentação e controle compartilhados, saves e progresso separados;
  - **sempre abrir no launcher, embutido no binário**;
  - trabalho no `modernization`.
- **Fase 1 despachada.**
- **Fase 1 pausada a pedido do usuário**, que retoma amanhã.
  - A tentativa com o Sonnet foi parada antes de qualquer edição (`task_1a7951363cc7`, dispatch `ctx_1d9092e90fd1`).
  - Especificação: `.worker-reports/specs/spec-t2000-phase1.md`. Worktree `t2000p1` limpo.
  - Para retomar: `worker-start --task task_1a7951363cc7 --retry-of ctx_1d9092e90fd1`, ou, com o Codex, `task-create` com a mesma especificação e `dispatch --inject`.

### 2026-09-29 — Fase 1 concluída
- **Worker:** Codex `gpt-6-sol` high, retomado a pedido do usuário. Levou ~22 min. Documentos revisados e trazidos para `docs/t2000/`:
  - `fork-diff.md`: diff próprio do fork contra o merge-base `967c12e` (50 arquivos, +1.292/−476 linhas), 187 linhas de classificação e a evidência de cada diferença de gameplay;
  - `data-formats.md`: formatos 2.1 × 2000, arquivo a arquivo, e quais carregadores nossos quebrariam;
  - `integration.md`: riscos por arquivo, a API C99 da Fase 2 (`game_variant.h`, `game_data.h`, `bootstrap.h`, `user_paths.h`) e a suíte `regress-2000`.
- **Achados principais:**
  - o formato dos arquivos é o mesmo, e mudam as contagens: um segundo banco de armas e inimigos, mais strings, 13 bancos de shapes, 31 efeitos sonoros (as vozes mudam de ID), 14 imagens e 24 paletas;
  - o save do 2000 tem 4.722 bytes, os mesmos 2.502 do 2.1 mais placares sem criptografia. O nosso carregador aceitaria o prefixo e truncaria o arquivo ao salvar, então namespaces separados são obrigatórios;
  - o evento 68 é explosão aleatória no 2.1 e substituição de inimigo no 2000. Tem que ficar atrás da variante;
  - os carregadores de strings, itens, shapes, imagens, sons e créditos quebrariam com os dados do 2000; hoje o guarda em `src/opentyr.c` recusa esses dados antes de tudo;
  - as cores e classes do tag buffer já saem da análise de cada sprite, então não precisamos de uma tabela de cores por ID.
- **Correções ao plano:**
  - cinco demos, não quatro, todas iguais às do 2.1;
  - sem cache do Actions na CI (acima);
  - a "correção" do fork no menu de upgrade não se aplica ao nosso código, que já protege o índice;
  - no fim de cada bloco de itens sobram 77 bytes que o fork não lê. Fica em aberto, sem inventar ID;
  - o fork marca os eventos novos e o rastro do Flying Punch como aproximações, então a fidelidade ao DOS precisa de comparação manual.
- **Validação:** build GCC-16 C99 com `-Werror` e os 164 casos passando, sem mudança de baseline. Nenhum dado do 2000 entrou no worktree ou nos documentos, só nomes, tamanhos e checksums.

### 2026-09-29 — Fase 2a: variante, provider e `--variant=` (`4c7ec1c`)
- **Worker:** Codex `gpt-6-sol` high, ~11 min.
- **`src/game_variant.c`:** descritores imutáveis do 2.1 e do 2000, com nome, rótulo de log, namespace de save, episódios e demos. Selecionar o 2000 devolve "indisponível".
- **`src/game_data.c`:** o `GameDataProvider` agora faz a busca dos dados, com a mesma ordem de antes (`--data`, pasta do executável/bundle, `TYRIAN_DIR`, cwd). Também faz a recusa do 1.x e do 2000, com as mesmas mensagens, e só abre arquivos para leitura, sem caminhos absolutos nem `..`. `dataFileOpen` passa por ele.
- **`src/bootstrap.c`:** lê `--variant=` e `--data` antes de SDL, config e saves.
  - `--variant=2000` sai com "Tyrian 2000 is not available yet."; valor desconhecido ou conflitante dá erro.
  - Os arquivos do usuário são desativados para regress/selftest logo no início.
  - O `JE_paramCheck` aceita as duas opções sem reaplicar.
- **Log:** uma linha no startup com variante, raiz dos dados e status da validação.
- **Testes:** `tools/check_variant_bootstrap.sh`, chamado pelo `make regress`, confere:
  - os erros de `--variant`;
  - hashes idênticos com e sem `--variant=2.1`;
  - as formas curtas e abreviadas de `--data`;
  - a recusa com um cabeçalho de 13 bancos gerado pelo script, sem dado original;
  - a ausência de fallback entre pastas;
  - que nenhum arquivo de usuário é criado.
- **Trava:** os 164 casos passam sem mudar baseline (81 s no tree integrado).
- **Desvios do `integration.md`, aceitos:**
  - o descritor só tem os campos com uso atual;
  - `gameVariantSelect` devolve um status em vez de `bool`;
  - um `--data` repetido continua valendo a última ocorrência, como antes;
  - a reordenação do startup para o launcher fica para a 2b e a Fase 6.

### 2026-09-29 — Fase 2b: saves por variante e migração (`dec73ff`)
- **Worker:** Codex `gpt-6-sol` high, ~15 min, com uma pergunta respondida: publicar a cópia com `link` + `unlink` no POSIX e `rename` no Windows e em FAT/exFAT.
- **Layout:**
  - na raiz do usuário ficam o que é compartilhado, `opentyrian.cfg`, `tyrian.cfg` (28 bytes: detalhe, gamma, teclas, joystick, volumes), `newsh$.shp` e o log;
  - em `tyrian21/` ficam `tyrian.sav` e as demos gravadas (`demorec.N`);
  - `tyrian2000/` está reservado.
- **Desvio consciente do `integration.md`:** o `tyrian.cfg` também fica compartilhado, porque só guarda apresentação e controles (decisão 4 de §12).
- **API:** `userFileOpenKind`/`userFileExistsKind`, com um tipo compartilhado, save ou demo, em `src/file.c`. `userFileOpen` segue apontando para a raiz.
- **Migração** (só 2.1, antes de `loadSaves`, nunca em regress/selftest):
  - copia o `tyrian.sav` da raiz somente se tiver exatamente 2.502 bytes;
  - grava num temporário exclusivo e publica sem sobrescrever;
  - nunca mexe no original nem num destino existente;
  - de qualquer outro tamanho, pula com aviso;
  - se a cópia falhar, a sessão lê o save da raiz só para leitura e não grava save nenhum, então nunca aparece um save em branco escondendo o antigo.
- **Testes:** `tools/check_user_paths.sh`, dentro do `make regress`, em sandbox com as opções de regress `--regress-user-root`/`--regress-user-files`. Cobre:
  - migração byte a byte;
  - repetição;
  - destino existente;
  - tamanhos errados;
  - falhas e nova tentativa;
  - round trip do save;
  - configs na raiz;
  - demos;
  - raízes portable e XDG;
  - isolamento de regress e selftest.
- **Trava:** os 164 casos passam sem mudar baseline (61 s no tree integrado).
- **Para quem atualiza da v0.2.x:** a primeira abertura copia o save. Um binário antigo continua usando o save da raiz, e o progresso passa a divergir entre os dois.

- **Correção no Windows (`d313c43`):** o `stat` abaixo de um arquivo devolve `ENOENT` no Windows e `ENOTDIR` no POSIX. Com isso, um arquivo chamado `tyrian21` não ligava o modo só-leitura. Agora a migração checa explicitamente a raiz e a pasta do namespace, e o `check_user_paths.sh` diz o passo que falhou. Worker: Claude Sonnet 5.5 high (`worker-start` normal), a pedido do usuário para esta sessão. CI verde nos três sistemas; o Linux arm64 precisou de uma nova execução porque o runner travou 45 min na compilação.
