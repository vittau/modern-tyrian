# PROGRESS: contrato e implementação conservadora

Estado em 2026-10-07. Este documento descreve `src/modern_progress.[ch]`, os hooks de `tyrian2.c`/`mainint.c` e a composição de `modern_hud.c`/`modern.c`. A leitura de fontes não comprova execução: os resultados de validação ficam separados abaixo. O [inventário estrutural](progress-levels.md) contém contagens e sinais estáticos, sem certificar rotas completas.

## Decisão visual e significado

**PROGRESS** é uma barra horizontal contínua, preenchida da esquerda para a direita com paleta, rampa, bordas e brilho de **SHIELD**. Mede uma estimativa de avanço de roteiro; não mede tempo restante, inimigos mortos ou dano ao boss. Durante boss reconhecido, o percentual congela. Quando o encontro termina, a continuação pode retomar se a estimativa ainda estiver disponível. As barras de HP de boss existentes permanecem.

**Uma saída natural antecipada ou alternativa pode terminar com o percentual atual, abaixo de 100%.** Sair da fase, avançar campanha ou desaparecer a barra do boss não prova cumprimento do objetivo principal. O preenchimento até 100% exige a condição positiva descrita abaixo, sem causa negativa anterior. Não há identificação universal de boss final nem promoção direta pela sua morte.

## Modelo efetivamente implementado

O observador possui somente estado próprio de apresentação. Os hooks recebem decisões já tomadas pelo jogo; não sorteiam, não chamam `JE_searchFor` e não escrevem arrays, flags, cursores ou RNG do gameplay. A interpretação dos eventos passa por `gameEventCase`, respeitando a tabela da variante. Em Classic, os hooks de observação ficam inertes.

| Parte | Comportamento atual | Limite |
|---|---|---|
| Denominador | `modern_progress_begin` varre em memória os eventos carregados e escolhe o maior tempo positivo de ação semântica 11/36; empates escolhem o último índice encontrado. | É uma estimativa de marco principal, sem análise de alcançabilidade. Não é o último evento serializado, o sentinela nem duração garantida. |
| Disponibilidade | Sem marco, ou com ação 76/retorno 65535 nas ações 54/70/71, inicia `percent=-1`. O HUD omite o indicador. | Rotas de retorno não possuem perfil de chamada/retorno certificado. Não fabricar zero ou um denominador pelo maior endereço. |
| Avanço | Usa incrementos observados de `curLoc` no trecho atual e orçamento remanescente até 99%; percentual inteiro monotônico por tentativa. | A barra não promete precisão de distância física nem tempo restante. |
| Salto para frente | Não credita o endereço pulado; conserva percentual e redistribui apenas o orçamento restante na continuação até o marco. | Se atingir/ultrapassar o marco por salto, invalida a estimativa. |
| Recuo/loop | Congela abaixo da fronteira já observada; retoma ao ultrapassá-la. | Repetição não soma crédito. Não modela um grafo completo nem número de iterações. |
| Retorno/reposição | Retorno invalida suporte; reposição das ações 38/75 também congela a estimativa existente. | As buscas de índice dessas ações não são equivalentes a `JE_eventJump`. |
| Espera | Pausa por boss, ausência de jogadores vivos, `returnActive`, scroll parado sem `forceEvents`, `readyToEndLevel`, warp/finalização ou término já registrado. | Movimento do relógio durante espera é descartado, sem crédito retroativo ao liberar. |

O boss reconhecido é um inimigo ativo com armadura positiva e link correspondente a uma das duas barras existentes, nos primeiros 100 slots consultados. Configurar a barra sem inimigo correspondente não basta para pausar. Isso não reconhece bosses sem barra, gerações de links, fases de transformação ou grupos finais obrigatórios; remoção/despawn não equivale a derrota final. As esperas não dependem da fila VFX.

## Saídas e precedência

`modern_progress_event` registra execução do índice escolhido como marco. `modern_progress_end(true)` solicita resolução de saída natural: só considera objetivo principal se esse marco foi executado e `curLoc >= endpoint`. `modern_progress_resolve_end` consulta o estado atual de sobrevivência, sem depender de `allPlayersGone` defasado; um sobrevivente basta em 2P. Se a condição principal faltar, `modern_progress_observer_finish(false)` encerra a estimativa no percentual atual (`ended`/`early_ends`), sem forçar 100%. Esse fechamento fica latched até reset: um marco/callback posterior durante warp não promove a tentativa encerrada.

Abandono, skip forçado, término/interrupção de demo, término sintético de flow e timeout não autorizado bloqueiam promoção posterior. Morte definitiva na resolução natural também bloqueia; durante morte temporária/respawn, a ausência de jogadores vivos apenas pausa e descarta avanço do relógio, sem cancelar ou iniciar nova tentativa. O limite de frames do harness apenas termina a sondagem, sem ser sucesso da fase. Uma conclusão principal já confirmada mantém o latch de 100% até o próximo reset, inclusive se houver saída posterior.

A política de timer contém uma exceção autoral para E4/L5 no modo comum, compartilhada pelas variantes, permitindo resolver a rota de sobrevivência pela condição principal normal. Timed Battle e demais expirações ficam conservadoramente bloqueados. Essa exceção não certifica todos os timers, modos ou caminhos de E4; sua evidência de execução deve ser identificada na validação, não deduzida do comentário no fonte.

`endLevel`, `reallyEndLevel`, `mainLevel`, animação de resultado e sobrevivência isolados não constituem enumeração de sucesso. Eventos 11/36 observados antecipadamente também não bastam; caso 36 espera a condição real de inimigos/explosões. A implementação usa hooks nos pontos reais de decisão, sem modificar esses resultados do jogo.

## Reset e apresentação

Depois de `JE_loadMap` e `modern_level_reset`, `modern_progress_begin` reinicia tentativa e contadores. Pausa, redesenho, respawn e início de warp não resetam o observador. A resolução pendente também ocorre antes de seguir para o próximo nível.

O desenho consulta o snapshot e não avança a observação. `modern_build_frame` redesenha PROGRESS após a interpolação das barras existentes e antes de compor painéis/faixa de mensagem, incluindo frames mantidos de warp/fade. A barra nova não tem registro interpolado: 100% confirmado aparece na próxima apresentação existente, sem acrescentar ticks. Fades continuam afetando a composição.

- **1P/arcade:** painel esquerdo, rótulo em y=142 e barra de cinco pixels em y=150, largura máxima 144. Em painel estreito o percentual fica ao lado da barra na segunda linha. Preserva sidekick e região de boss a partir de y=158.
- **2P normal:** lado direito da linha de título na faixa sob o playfield, barra de 36 pixels; o nome da fase é truncado à largura restante. A linha inferior de mensagens permanece disponível.
- **Sem painéis/Modern 4:3:** indicador intencionalmente omitido para não cobrir informação existente. Classic também não recebe indicador.

Estas posições descrevem a implementação; assertions e leitura de coordenadas não substituem revisão visual de layouts, overlays, resize e fades.

## Cobertura e validação

`--regress-progress-check` (420) verifica limites, monotonicidade, pausa e latch no snapshot e emite `Progress coverage` agregada, incluindo amostras de espera, retomadas de boss/loop, avanço pós-boss, saídas antecipadas e percentual final. Contadores só demonstram os caminhos efetivamente tomados; zero não certifica ausência do mecanismo no mapa. `--regress-observer-off` (421) permite comparar observador OFF/ON. A faixa 420–429 está reservada em AGENTS.md.

`tools/check_progress.sh` separa três tipos de evidência:

1. **Fixtures escalares autorais** (`tools/progress_fixture.c`): saltos, retorno, loops, pausa, retomada, latch, indisponibilidade e causas negativas. Exercitam o modelo, não spawn, morte de boss ou completude dos hooks reais. Nomes de causas na fixture não provam execução dessas causas no jogo.
2. **Sondagens de runtime representativas:** 300 frames iniciais em E1/L6, E1/L4, E1/L16 e E4/L18; também E5/L1 em 2000. Comparam estado/RNG OFF/ON nos dois modos e framebuffer Classic. Exigem eventos e avanço quando aplicável; disponibilidade negativa é esperada nos casos sem denominador. Uma sondagem inicial não cobre o percurso completo nem garante que o recuo/retorno foi tomado.
3. **Caminhos específicos:** script 1:3 com input/movimento, autofire e solicitação de pausa; fixture acelerada E1/L1 via `--regress-boss`, com amostras de boss ativo e congelamento. A aceleração não prova chegada natural ao encontro, boss intermediário vencido ou derrota do boss final.

### Resultados confirmados pelo coordenador

Os probes abaixo usam o **release final**, sem `--regress-boss` ou aceleração. Os resultados substituem as medidas debug anteriores nesses caminhos; o cap é limite da sondagem, não causa de vitória.

| Probe | 2.1 | 2000 | Evidência e alcance |
|---|---|---|---|
| E1/L6, cap 5000 | final 52%, `early_ends=1` | final 52%, `early_ends=1` | Saída natural antecipada preserva valor; nenhum bloqueio ou sucesso. |
| E1/L4 | final 100%, 40 amostras de sucesso | final 100%, 40 amostras de sucesso | 99 avanços em cada variante; objetivo principal resolvido e latch observado. |
| E4/L7, 2P/fire, VFX OFF, cap 15000 | 5542 amostras de espera boss, 3 retomadas, 5 avanços pós-boss; final 45% | 5684 amostras de espera boss, 3 retomadas, 3 avanços pós-boss; final 43% | Retomada real de avanço nas duas variantes; ambas com saída antecipada, sem bloqueio/sucesso. Não certifica identidade de boss final. |

A fixture escalar final passou **271 assertions**, incluindo fechamento antecipado que ignora promoção posterior, também com ASan/UBSan. Sanitizers cobrem essa fixture, não o motor completo. Builds debug e release, auditoria GCC16 dos oito arquivos C tocados (incluindo a fixture) e guard de ausência de dados 2000 foram confirmados. A revisão visual utilizou capturas reais somente de 2.1 em 16:9, 16:10 e 2P; não certifica todas as combinações de overlay/resize/fade.

A exceção de timer E4/L5 foi sondada no debug anterior aos ajustes finais de fechamento antecipado e pausa por morte: com fire e cap 9000, ambas as variantes tiveram 1950 ticks de espera boss e uma retomada; 2.1 confirmou 100% com 37 amostras de sucesso, enquanto 2000 terminou antecipadamente a 50%, sem bloqueio. Essa evidência tem revisão e escopo distintos dos probes release acima; não certifica todos os timers ou caminhos de sobrevivência.

As referências de imagem Modern mudam intencionalmente pelos novos pixels de PROGRESS: 20 arquivos de hashes de frame em 2.1 e 12 em 2000, mais duas entradas de CRC de frame de pausa na matriz mista de cada variante. Os quatro checks derivados divergentes na rodada inicial 2.1 correspondiam aos mesmos frames Modern; suas saídas foram comparadas byte a byte aos baselines Modern correspondentes. Nenhuma referência Classic ou de estado foi alterada. Isso registra o motivo explícito da mudança 2.1. Os reruns completos abaixo validaram as referências finais.

O coordenador confirmou saída zero das suítes release finais: **`make regress`: 172 casos; `make regress-2000`: 153 casos**, usando a instalação 2000 já verificada. Os guards existentes e `check_progress.sh` passaram, incluindo igualdade OFF/ON de estado/RNG, framebuffer Classic, input/pausa real e boss acelerado. A auditoria GCC16 foi repetida nos oito arquivos C da revisão final; verificações de portabilidade, XML dos projetos Visual Studio, CRLF do `.vcxproj`, whitespace e ausência de dados 2000 também passaram. Essas verificações não constituem certificação de todos os percursos, modos ou encontros. Esta tarefa documental não executou testes nem alterou baselines; os PASS são resultados recebidos do coordenador, separados da leitura de fontes. Tyrian 2000 permanece restrito a hashes/metadados e cobertura agregada, sem exportar strings, sprites, mapas, streams ou screenshots.

## Evolução futura: grafos de rota

Um desenho futuro pode analisar trechos, transições, pesos e encontros em memória, com identidade por variante/episódio/nível físico/modo e políticas explícitas para branches, loops finitos, retorno, substituições e bosses sem barra. Deverá observar decisões executadas, sem repetir predicados com efeitos colaterais ou RNG, e preservar o avanço já mostrado.

**Esse grafo e perfis certificados por caminho não estão implementados.** O inventário estático não os certifica; hashes identificam entradas, não provam execução. Não há promessa de estimativa útil em todos os níveis ou precisão universal. Ampliação de cobertura exige percursos reais com marcos/categorias de saída observáveis, dois links e transformações de boss, 2P com sobrevivente, modos/dificuldades, timeout, skip, abandono/rede, demos e persistência no warp/fade. Uma saída alternativa abaixo de 100% continua parte do contrato aceito.
