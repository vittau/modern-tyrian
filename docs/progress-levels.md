# Inventário estrutural para o indicador PROGRESS

Investigação em 2026-10-07, somente leitura, sem builds, execução do jogo ou atualização de baselines. Escopo: scripts de episódios e eventos dos níveis físicos, com interpretação pelos loaders deste checkout. Decisão atual: **PROGRESS** horizontal no estilo de SHIELD, congelado durante boss reconhecido e repetição de trecho. Saída natural antecipada/alternativa pode terminar no percentual atual abaixo de 100%; 100% exige condição positiva do objetivo principal. As barras existentes de HP de boss permanecem.

Foram analisados os dados 2.1 em `data/` e os dados 2000 já presentes na localização de instalação do usuário no macOS. `TYRIAN2000_DATA` não estava definido. Nenhum download, instalação, cópia de dados ou exportação de eventos, textos, mapas ou imagens foi feito. O parser e sua saída exclusivamente de metadados ficaram em scratch externo ao repositório. As contagens abaixo pertencem à investigação estática original; a revisão documental posterior as mantém e distingue a implementação conservadora de sua cobertura de execução.

## O que está sendo contado

**Nível físico** é o registro selecionado por `lvlFileNum`: o cabeçalho de `tyrianN.lvl` contém pares de offsets e uma entrada final, portanto `(quantidade_de_offsets - 1) / 2` registros. A entrada final é EOF em E1–E3 e início do bloco de itens em E4/E5; o bloco de itens não é nível. Referência: `src/lvllib.c`, `analyzeLevel`; `src/tyrian2.c`, `JE_loadMap`; [data-formats.md](t2000/data-formats.md).

**Seção lógica** é cada registro iniciado por `*` no arquivo `levelsN.dat`. Há cenas, menus, escolhas e finais além de comandos de carga `]L`. O loader usa o campo numérico a partir do byte 25 de `]L` para selecionar o nível físico, o campo a partir do byte 9 para a próxima seção e, quando este é zero, `mainLevel + 1`. Seções e cargas não são uma lista de fases alcançáveis: apenas inventário sintático. Uma mesma fase física pode servir a várias rotas e modos.

`--regress-level=E:L` seleciona nível físico no cenário; `--regress-script=E:L` passa pelo script e sua seção lógica. Não reutilizar automaticamente os IDs físicos desta tabela como IDs de seção. Os cinco demos apenas cobrem E1, níveis físicos 9, 12, 13, 7 e 5, em ambas as variantes; não constituem cobertura deste inventário.

## Cobertura estática medida

| Variante/episódio | Offsets | Níveis físicos | Eventos serializados | Seções lógicas | Comandos de carga | Físicos válidos referenciados | Cargas fora do intervalo |
|---|---:|---:|---:|---:|---:|---:|---:|
| 2.1/E1 | 37 | 18 | 12270 | 42 | 18 | 18 | 0 |
| 2.1/E2 | 25 | 12 | 10210 | 24 | 12 | 12 | 0 |
| 2.1/E3 | 25 | 12 | 11326 | 25 | 12 | 12 | 0 |
| 2.1/E4 | 41 | 20 | 19532 | 51 | 20 | 20 | 0 |
| 2000/E1 | 37 | 18 | 12324 | 48 | 20 | 18 | 1 |
| 2000/E2 | 25 | 12 | 10205 | 24 | 12 | 12 | 0 |
| 2000/E3 | 25 | 12 | 11376 | 25 | 12 | 12 | 0 |
| 2000/E4 | 41 | 20 | 19586 | 51 | 20 | 20 | 0 |
| 2000/E5 | 17 | 8 | 6186 | 20 | 12 | 8 | 0 |

**2.1:** 62 níveis físicos, 53,338 eventos, 142 seções e 62 comandos de carga. Categorias exclusivas: sequencial=16, condicional=6, chefe=23, recuo=16, retorno=1.

**2000:** 70 níveis físicos, 59,677 eventos, 168 seções e 76 comandos de carga. Categorias exclusivas: sequencial=18, condicional=5, chefe=27, recuo=19, retorno=1.

Todos os 62 físicos de 2.1 e os 70 de 2000 têm ao menos uma referência sintática válida. Isto não demonstra que cada um seja alcançável em qualquer modo ou dificuldade. Em 2000/E1, duas cargas referenciam L5; há ainda uma carga na seção 44 que solicita ID físico 20, fora dos 18 registros desse contêiner. É uma exceção estrutural a investigar pelo caminho real do intérprete, sem concluir que a carga seja executada ou que exista um vigésimo nível em E1. Em 2000/E5, L1 tem três referências; L3 e L4 têm duas cada. Não usar quantidade de cargas como quantidade de fases.

## Categorias e leitura da tabela

Categorias exclusivas, por prioridade: **retorno** (evento 76 ou salto com destino especial 65535); **recuo** (salto explícito para tempo menor ou igual ao tempo serializado do evento, incluindo configuração de timer ativo); **chefe** (evento 79 com algum link não zero); **condicional** (evento 61/63/66/70/71/75/80 ou timer); **sequencial** (nenhum dos sinais anteriores). “Sequencial” significa ausência destes sinais, não prova de execução linear. Um evento 57 arma um salto posterior na morte de inimigo; compará-lo com seu tempo de configuração é somente um sinal conservador. Eventos 38 e `JE_eventJump` têm buscas de índice diferentes; a classificação não simula essas buscas.

A coluna **11/36** conta os dois marcadores de fim. **79** conta todos os comandos de barra de chefe, inclusive desativação; a categoria chefe exige link não zero. Sinais adicionais sobrepõem categorias: **C** condição; **T** timer 67, ou 84 em 2000; **P** parada de mapa 4, ou 83 em 2000; **R** retorno; **B** recuo. Quantidades são metadados; não são fluxos de eventos ou tempos extraídos.

### Variante 2.1

| Físico | Eventos | 11/36 | 79 | Categoria | Sinais |
|---|---:|---:|---:|---|---|
| E1/L1 | 456 | 1/1 | 1 | chefe | P |
| E1/L2 | 731 | 1/0 | 1 | chefe | P |
| E1/L3 | 323 | 2/0 | 0 | condicional | C P |
| E1/L4 | 240 | 0/1 | 0 | sequencial | — |
| E1/L5 | 545 | 1/2 | 1 | chefe | P |
| E1/L6 | 440 | 3/0 | 0 | sequencial | — |
| E1/L7 | 517 | 2/0 | 0 | sequencial | — |
| E1/L8 | 584 | 2/1 | 1 | chefe | P |
| E1/L9 | 1009 | 1/1 | 1 | chefe | C |
| E1/L10 | 248 | 1/1 | 0 | sequencial | — |
| E1/L11 | 1214 | 2/0 | 0 | condicional | C |
| E1/L12 | 832 | 2/1 | 1 | chefe | P |
| E1/L13 | 380 | 1/0 | 0 | sequencial | — |
| E1/L14 | 838 | 2/0 | 0 | sequencial | — |
| E1/L15 | 1246 | 1/1 | 1 | chefe | C |
| E1/L16 | 833 | 0/1 | 2 | recuo | C B |
| E1/L17 | 1000 | 1/0 | 2 | recuo | C B |
| E1/L18 | 834 | 1/0 | 0 | recuo | C B |
| E2/L1 | 1752 | 1/1 | 1 | recuo | C B |
| E2/L2 | 828 | 1/0 | 1 | chefe | C P |
| E2/L3 | 178 | 2/1 | 0 | sequencial | — |
| E2/L4 | 421 | 2/1 | 0 | condicional | C P |
| E2/L5 | 1304 | 2/0 | 0 | sequencial | — |
| E2/L6 | 1299 | 1/2 | 1 | recuo | B |
| E2/L7 | 610 | 1/0 | 2 | recuo | B |
| E2/L8 | 946 | 2/0 | 0 | condicional | C |
| E2/L9 | 1256 | 2/2 | 0 | recuo | C B |
| E2/L10 | 812 | 1/1 | 1 | chefe | — |
| E2/L11 | 552 | 1/1 | 1 | chefe | — |
| E2/L12 | 252 | 1/0 | 0 | sequencial | — |
| E3/L1 | 1205 | 1/0 | 1 | chefe | C P |
| E3/L2 | 390 | 1/1 | 0 | sequencial | — |
| E3/L3 | 1021 | 2/0 | 0 | sequencial | — |
| E3/L4 | 903 | 1/1 | 1 | chefe | — |
| E3/L5 | 786 | 2/0 | 0 | sequencial | — |
| E3/L6 | 2259 | 1/1 | 1 | recuo | B |
| E3/L7 | 672 | 1/1 | 0 | sequencial | — |
| E3/L8 | 1182 | 1/1 | 1 | chefe | — |
| E3/L9 | 818 | 2/0 | 0 | condicional | C |
| E3/L10 | 724 | 2/1 | 1 | chefe | P |
| E3/L11 | 656 | 1/2 | 1 | chefe | P |
| E3/L12 | 710 | 2/1 | 0 | sequencial | — |
| E4/L1 | 1109 | 2/0 | 1 | chefe | C P |
| E4/L2 | 970 | 2/0 | 0 | sequencial | — |
| E4/L3 | 1129 | 2/1 | 1 | chefe | C |
| E4/L4 | 904 | 2/0 | 2 | chefe | C P |
| E4/L5 | 675 | 3/1 | 2 | chefe | T P |
| E4/L6 | 640 | 1/1 | 0 | condicional | C |
| E4/L7 | 1338 | 1/1 | 3 | chefe | C T P |
| E4/L8 | 750 | 1/1 | 1 | recuo | C B |
| E4/L9 | 1444 | 1/1 | 2 | chefe | C P |
| E4/L10 | 552 | 2/0 | 0 | sequencial | — |
| E4/L11 | 909 | 2/1 | 2 | chefe | C T |
| E4/L12 | 1243 | 2/0 | 1 | chefe | C |
| E4/L13 | 1006 | 1/1 | 2 | recuo | C B |
| E4/L14 | 1022 | 2/1 | 5 | recuo | C B |
| E4/L15 | 1969 | 1/0 | 3 | recuo | C B |
| E4/L16 | 666 | 1/1 | 5 | recuo | C B |
| E4/L17 | 473 | 1/0 | 0 | recuo | C B |
| E4/L18 | 595 | 0/0 | 0 | retorno | C R B |
| E4/L19 | 1288 | 1/0 | 1 | recuo | C B |
| E4/L20 | 850 | 2/0 | 2 | recuo | C T B |
### Variante 2000

| Físico | Eventos | 11/36 | 79 | Categoria | Sinais |
|---|---:|---:|---:|---|---|
| E1/L1 | 481 | 1/1 | 2 | chefe | P |
| E1/L2 | 731 | 1/0 | 1 | chefe | P |
| E1/L3 | 323 | 2/0 | 0 | condicional | C P |
| E1/L4 | 240 | 0/1 | 0 | sequencial | — |
| E1/L5 | 552 | 3/2 | 1 | chefe | T P |
| E1/L6 | 441 | 3/0 | 0 | sequencial | — |
| E1/L7 | 517 | 2/0 | 0 | sequencial | — |
| E1/L8 | 602 | 2/1 | 1 | chefe | P |
| E1/L9 | 1010 | 1/2 | 1 | chefe | C |
| E1/L10 | 248 | 1/1 | 0 | sequencial | — |
| E1/L11 | 1214 | 2/0 | 0 | sequencial | — |
| E1/L12 | 832 | 2/1 | 1 | chefe | P |
| E1/L13 | 379 | 1/0 | 0 | sequencial | — |
| E1/L14 | 838 | 2/0 | 0 | sequencial | — |
| E1/L15 | 1246 | 1/1 | 1 | chefe | C |
| E1/L16 | 833 | 0/1 | 2 | recuo | C B |
| E1/L17 | 1003 | 1/1 | 3 | recuo | C B |
| E1/L18 | 834 | 1/0 | 0 | recuo | C B |
| E2/L1 | 1752 | 1/1 | 1 | recuo | C B |
| E2/L2 | 829 | 1/0 | 2 | chefe | C P |
| E2/L3 | 178 | 2/1 | 0 | sequencial | — |
| E2/L4 | 421 | 2/1 | 0 | condicional | C P |
| E2/L5 | 1304 | 2/0 | 0 | sequencial | — |
| E2/L6 | 1299 | 1/2 | 1 | recuo | B |
| E2/L7 | 610 | 1/0 | 2 | recuo | B |
| E2/L8 | 946 | 2/0 | 0 | condicional | C |
| E2/L9 | 1256 | 2/2 | 0 | recuo | C B |
| E2/L10 | 806 | 1/1 | 1 | chefe | — |
| E2/L11 | 552 | 1/1 | 1 | chefe | — |
| E2/L12 | 252 | 1/0 | 0 | sequencial | — |
| E3/L1 | 1236 | 1/0 | 1 | chefe | C P |
| E3/L2 | 390 | 1/1 | 0 | sequencial | — |
| E3/L3 | 1025 | 2/0 | 0 | sequencial | — |
| E3/L4 | 902 | 1/1 | 1 | chefe | — |
| E3/L5 | 786 | 2/0 | 0 | sequencial | — |
| E3/L6 | 2259 | 1/1 | 1 | recuo | B |
| E3/L7 | 672 | 1/1 | 0 | sequencial | — |
| E3/L8 | 1197 | 1/1 | 1 | chefe | — |
| E3/L9 | 818 | 2/0 | 0 | condicional | C |
| E3/L10 | 725 | 2/1 | 2 | chefe | P |
| E3/L11 | 656 | 1/2 | 1 | chefe | P |
| E3/L12 | 710 | 2/1 | 0 | sequencial | — |
| E4/L1 | 1109 | 2/0 | 1 | chefe | C P |
| E4/L2 | 970 | 2/0 | 0 | sequencial | — |
| E4/L3 | 1188 | 2/1 | 1 | chefe | C |
| E4/L4 | 904 | 2/0 | 2 | chefe | C P |
| E4/L5 | 676 | 3/2 | 2 | chefe | T P |
| E4/L6 | 641 | 2/1 | 0 | condicional | C |
| E4/L7 | 1329 | 1/1 | 3 | chefe | C T P |
| E4/L8 | 749 | 1/1 | 1 | recuo | C B |
| E4/L9 | 1447 | 1/1 | 2 | chefe | C P |
| E4/L10 | 552 | 2/0 | 0 | sequencial | — |
| E4/L11 | 909 | 2/1 | 2 | chefe | C T |
| E4/L12 | 1243 | 2/0 | 1 | chefe | C |
| E4/L13 | 1006 | 1/1 | 2 | recuo | C B |
| E4/L14 | 1022 | 2/1 | 5 | recuo | C B |
| E4/L15 | 1969 | 1/0 | 3 | recuo | C B |
| E4/L16 | 666 | 1/1 | 5 | recuo | C B |
| E4/L17 | 473 | 1/0 | 0 | recuo | C B |
| E4/L18 | 595 | 0/0 | 0 | retorno | C R B |
| E4/L19 | 1288 | 1/0 | 1 | recuo | C B |
| E4/L20 | 850 | 2/0 | 2 | recuo | C T B |
| E5/L1 | 197 | 0/0 | 0 | sequencial | — |
| E5/L2 | 311 | 1/0 | 2 | chefe | — |
| E5/L3 | 1000 | 2/0 | 1 | chefe | C T |
| E5/L4 | 983 | 2/1 | 3 | chefe | T P |
| E5/L5 | 514 | 1/0 | 3 | recuo | B |
| E5/L6 | 1442 | 1/1 | 1 | chefe | — |
| E5/L7 | 894 | 1/1 | 1 | recuo | C B |
| E5/L8 | 845 | 1/1 | 1 | recuo | B |

## Fim, chefe final e chefe intermediário

Evento **11** inicia saída ou força `reallyEndLevel`, dependendo do parâmetro e de jogadores ausentes. Evento **36** arma `readyToEndLevel`: a saída aguarda `enemyOnScreen == 0` e ausência de explosões pendentes. Há também saída associada à parada de fundo e `waitToEndLevel`. Por isso nem último evento, nem maior tempo serializado, nem último marcador 11/36 representa sozinho uma duração garantida ou sucesso.

Em ambas as variantes, E1/L4 e L16 têm apenas 36; E4/L18 não tem 11 nem 36. Em 2000, E5/L1 também não tem nenhum desses marcadores. Estes dois últimos registros são exceções prioritárias: pode haver retorno, rota externa ou finalização por estado, e não se deve fabricar um denominador pelo máximo tempo. Os demais físicos têm ao menos um dos dois marcadores; isto é cobertura de presença.

Evento **79 não distingue chefe final de intermediário**: apenas associa até dois links à barra. `draw_boss_bar` consulta inimigos ativos, limpa links sem armadura válida e pode consolidar as barras; evento 70 depende da existência de inimigos por link, não de uma classificação “boss”. Um chefe pode não ter barra 79. Parada de fundo, inimigos comuns restantes e explosões também podem prolongar a fase. Nenhum percentual deve derivar da armadura.

Há comandos 79 ativos em 36/62 físicos de 2.1 e 43/70 de 2000. Na ordem serializada, existe marcador de fim depois do último 79 ativo em 35 e 42 deles, respectivamente; a exceção em ambas é E1/L16. São **candidatos estruturais a trecho final**, não 35/42 chefes finais confirmados: salto pode desviar, repetir ou antecipar saída, e spawn posterior pode pertencer ao mesmo chefe. Não classificamos spawn depois de 79 como prova de chefe intermediário. Confirmar a distinção exige observar “barra ativa → barra encerrada → retomada de percurso” versus “barra ativa → conclusão”, com a rota real. Quantidade de 79 tampouco equivale a quantidade de chefes.

## Saltos, loops e condições

`curLoc` acompanha a velocidade de fundo e pode avançar com `forceEvents` mesmo quando o fundo está parado; não é tempo de parede nem distância irreversível. Evento 38 reatribui `curLoc`/índice; 54 chama `JE_eventJump`; 57 arma salto de morte de inimigo; 70 salta se grupos de inimigos desapareceram; 71 depende da posição do mapa. `JE_eventJump` guarda `returnLoc = curLoc + 1` e trata destino 65535 como retorno. Evento 76 arma retorno quando os inimigos saírem. E4/L18 apresenta retorno e recuo nas duas variantes. Um indicador que use diretamente `curLoc / último_tempo` pode regredir, atingir um destino remoto indevidamente ou avançar dentro de um loop.

Recuos candidatos: E1/L16–18; E2/L1, L6, L7, L9; E3/L6; E4/L8, L13–20, nas duas variantes. Acrescentam-se E5/L5, L7, L8 em 2000. A comparação estática usa tempo do próprio comando, não o instante dinâmico de execução. Não prova que o loop foi tomado nem identifica sozinho todos os ciclos: saltos encadeados, destino de retorno e mutações dos eventos dependem de estado.

Eventos 61 e 60 relacionam flags e morte de inimigos; 63 depende de 2P ou arcade de 1P; 80 depende de 2P; 66 usa `initialDifficulty <= limiar`, apesar do comentário em inglês sugerir outra comparação. 75 escolhe link aleatório ou pula eventos quando não encontra candidato, inclusive ajustando `curLoc`. Não executar RNG para analisar progresso. Timer 67 salta ao expirar; 84 faz isso somente em Timed Battle. 83 em 2000 reutiliza parada de mapa, 85 só atua em Timed Battle; não aplicar a semântica 2.1 a eventos reatribuídos de 2000 (`gameEventCase`, `src/game_rules.c`).

No script lógico, `]G` oferece escolhas, `]J` salta, `]2` depende de 2P/arcade, `]w` depende da nave, `]t` do timer, `]l` de morte; `]H` e `]h` dependem de dificuldade. `]T` seleciona seção da batalha e `]q` encerra a batalha apenas nesse modo; `]Q` encerra episódio. Esses comandos não são endpoints temporais do nível físico.

| Variante/E | Comandos de rota presentes (contagens sintáticas) |
|---|---|
| 2.1/E1 | `]J`=8, `]2`=1, `]G`=17, `]H`=1, `]h`=1, `]Q`=1 |
| 2.1/E2 | `]G`=11, `]Q`=1 |
| 2.1/E3 | `]G`=12, `]H`=1, `]Q`=1 |
| 2.1/E4 | `]J`=8, `]2`=2, `]w`=1, `]t`=2, `]l`=1, `]G`=22, `]Q`=1 |
| 2000/E1 | `]J`=8, `]2`=1, `]G`=17, `]H`=1, `]h`=1, `]T`=1, `]q`=1, `]Q`=1 |
| 2000/E2 | `]G`=11, `]Q`=1 |
| 2000/E3 | `]G`=12, `]H`=1, `]Q`=1 |
| 2000/E4 | `]J`=8, `]2`=3, `]w`=1, `]t`=2, `]l`=1, `]G`=22, `]Q`=1 |
| 2000/E5 | `]J`=1, `]G`=5, `]T`=1, `]q`=1, `]Q`=1 |

## Relação com a implementação atual

O [contrato de runtime](progress-runtime.md) descreve o observador conservador efetivamente implementado. Ele escolhe em memória o marco positivo 11/36 mais distante, sem provar alcançabilidade, e acompanha avanço local com orçamento até 99%. Não utiliza estas categorias como perfis certificados nem implementa um grafo por fase. Saltos para frente não creditam endereços pulados; recuos congelam abaixo da fronteira anterior. Retorno e reposições 38/75 invalidam suporte; ausência de denominador inicia indisponibilidade, omitida no HUD. Assim E4/L18 e 2000/E5/L1 são casos de fallback, não percursos completos resolvidos.

Boss pausa apenas quando há inimigo ativo com armadura positiva ligado às barras existentes. A implementação não distingue universalmente boss intermediário/final, não mede dano e não preenche PROGRESS pelo desaparecimento da barra. O objetivo principal exige execução do marco escolhido, posição suficiente e saída natural resolvida com sobrevivente, sem bloqueio negativo. **Saída antecipada ou alternativa preserva o percentual atual abaixo de 100% até reset, sem promoção por marco posterior no warp.** `reallyEndLevel` ou avanço de campanha isolados continuam insuficientes.

Grafos com pesos por trecho, perfis de chamada/retorno, identidade de encontros e certificação por variante/modo são evolução futura. Os 62/70 níveis inventariados demonstram cobertura estática de registros e referências, não avanço útil em todos os mapas, alcance de todos os ramos ou vitória sobre bosses.

## Cobertura de execução: escopo separado

O guard `tools/check_progress.sh` contém fixtures escalares e sondagens iniciais OFF/ON em E1/L6, E1/L4, E1/L16 e E4/L18, mais E5/L1 em 2000. As sondagens de 300 frames exigem eventos/avanço quando aplicável; não certificam recuos/retornos apenas por selecionar um candidato estrutural. O script 1:3 exercita input/movimento, autofire e pausa. O boss E1/L1 é uma fixture acelerada, não um percurso natural nem derrota final. Fixtures negativas usam o latch comum do modelo, sem provar todos os hooks de gameplay.

O coordenador confirmou probes release finais nas duas variantes: E1/L6 termina a 52% com saída antecipada, E1/L4 resolve objetivo principal a 100% e E4/L7 em 2P/fire, VFX OFF, mostra três retomadas de boss com avanço posterior (5/3) e termina a 45%/43%. Detalhes e limites estão em [progress-runtime.md](progress-runtime.md). O coordenador confirmou as suítes release finais: **172 casos 2.1 e 153 casos 2000, com os guards existentes e de PROGRESS passando**. Isso valida a matriz executada, sem transformar o inventário em certificação por caminho. Nenhum PASS é inferido deste inventário ou de leitura de fonte. A matriz abaixo trata de ampliação de cobertura, sem exigir certificação universal para entregar o fallback conservador.

## Ampliação de cobertura ainda necessária

1. Ampliar os probes de E1/L6 (saída antecipada) e E1/L4 (objetivo principal) para causas simultâneas, inimigos/explosões pendentes e persistência da apresentação em diferentes overlays/fades.
2. E1/L16 e L17, E2/L1 e E3/L6: registrar metadados de saltos tomados, pausas e retomadas; verificar que a barra não cresce durante repetição nem regride ao voltar. E1/L16 é prioridade por 79 ativo sem marcador posterior na ordem serializada.
3. E4/L18 em ambas as variantes, e E5/L1 em 2000: resolver finalização/retorno real e selecionar denominador antes de considerá-los cobertos. Conferir a carga fora do intervalo na seção lógica 44 de 2000/E1 pelo intérprete e pelas rotas de batalha.
4. Além da retomada com avanço já observada em E4/L7, distinguir identidade/etapas de chefe final: verificar ausência de barra, dois links, desativação, transformações e explosões pendentes. A presença de 79 não satisfaz essa ampliação.
5. E4/L5, L7, L11 e L20: comparar timer expirado/não expirado; E4/L20 inclui recuo. Em 2000, acrescentar E1/L5 e E5/L3–4 no modo comum e em Timed Battle, além de E5/L5, L7–8 para loops.
6. Exercitar 1P Full Game, arcade e 2P nas rotas `]2`, eventos 63/80; dificuldade normal/hard e ambos os lados de cada limiar 66 presente; escolha `]G`, nave `]w`, morte `]l` e timer `]t`. O mínimo representativo não substitui a cobertura por caminho.
7. Morte, desistência, restart/backup, bônus e conclusão de batalha: nunca promover fracasso a 100%. Pausa de menu e transição de nível devem preservar/resetar apenas o estado visual apropriado.

Usar `--regress-level` para sondagem física e `--regress-script`/`flow` para o caminho real de seção/menu; logs de cobertura devem demonstrar a execução efetiva dos marcadores e transições. Esta tarefa documental não executou esses ensaios; resultados recebidos do coordenador estão identificados separadamente. Toda evidência de 2000 deve permanecer em hashes/metadados e cobertura agregada; não gerar screenshots nem despejos de eventos. O inventário não autoriza alterações de baseline; as invariantes de Classic continuam obrigatórias.

## Reprodução em somente leitura

O procedimento não exige build: Python 3, `struct`, `collections`, `hashlib` e `pathlib`. Abrir arquivos em modo binário de leitura; nunca exportar os buffers. Para 2000, usar somente `TYRIAN2000_DATA` existente ou instalação já presente do usuário. Falta de instalação significa cobertura indisponível, nunca autorização de download.

Algoritmo exato do inventário: ler u16 do cabeçalho e os u32 offsets; verificar quantidade ímpar e offsets ordenados; para cada offset de índice par anterior ao final, pular 8 bytes, ler u16 da quantidade de inimigos, pular seus IDs u16, ler u16 da quantidade de eventos e decodificar em memória essa quantidade de registros com `struct.unpack_from('<HBhhbbbB', buffer, posição)`, passo 11. Confirmar que o fim dos eventos não ultrapassa o próximo offset. Contar 11, 36 e 79; classificar pelos predicados acima. Timer conta pela presença do tipo; recuo de timer exige parâmetro de ativação igual a 1 e destino menor ou igual ao tempo do comando. O conjunto de saltos comparados para recuo é 38/54/57/70/71; o conjunto de destinos especiais usado para retorno é 54/70/71, acrescido da presença de 76. Destinos s16 de salto são reinterpretados como u16 (`valor & 65535`). Chefe ativo significa primeiro ou segundo parâmetro de 79 não zero. Candidato final significa índice do último 79 ativo anterior ao índice do último 11/36, sem análise de alcançabilidade.

Para scripts: ler tamanho u8 seguido desse número de bytes; decifrar de trás para frente com XOR da chave do loader `{204,129,63,255,71,19,25,62,1,99}`, repetida a cada dez bytes, e do byte cifrado anterior (exceto índice zero). Contar registros `*` e comandos `]`; somente para `]L`, converter o campo a partir do índice 25 como `atoi` (primeiro inteiro com espaços/sinal opcionais). Agrupar referências por ID físico e registrar contagens, nunca texto. Esta varredura sintática não executa consumo contextual de linhas por `]I`, `]W`, `]Q` ou `]h`; portanto as contagens de comandos devem ser confrontadas com interpretação real, especialmente a exceção de E1. Para reproduzir as categorias, usar conjuntos de eventos exatamente declarados nesta seção e na legenda.

SHA-256 dos contêineres e scripts analisados (identificação de entrada, sem conteúdo):

| Variante/E | `tyrianN.lvl` | `levelsN.dat` |
|---|---|---|
| 2.1/E1 | `b41e532214fa8cea36e37fed6512b357c6ac9c8c3267899ceb4a258fcb1bf0b9` | `e82164c96abb994450d416a3ab0e5fe6479fd4db831cd0f203a60dfe4aa78dcb` |
| 2.1/E2 | `3e18b124cd4ff5f766e5bf2e833eb79d3118010a253a30dc4c0ae6f483ae50e9` | `9aa94b70aae2b24b16c295ed86e392ac16873306012bdd89e0cf995d605178ea` |
| 2.1/E3 | `0995c7e2279995485576e02b21ac8599b4924c35af5afffdede69c01994b9937` | `0d935cd7bb3c84451d9f57dbcd31a8d515205b18fb050070a416e9b7411dfbd3` |
| 2.1/E4 | `181fde04387ba47e6b49a7f27010ac92fe00566a5f9fef075d74e4c4b2ebef75` | `891b9ecbdd223df291077fd95626f37ac03b527ca6fd50f76198067a5edebdb4` |
| 2000/E1 | `54cbb00b57b84aa992a70cfcb170cb1edeb59507873723630d60080ba8351352` | `2c2f5b8509ac44442592580c329f2b3c48a4bf972df7d049a9f174cc2c81c427` |
| 2000/E2 | `930e7c4a5452c6c79406d8f7126fdaace9994635d9f4bcdfb645d693a22575b6` | `3ec8a84e6f83500792b8e1415d91176bd5431046d4d9f0910a58ee0715b84533` |
| 2000/E3 | `6a5b94a7d3e4f5a228bc0f702cfe035c8f9f03b20a3f02867d25e95b4249ce86` | `0d935cd7bb3c84451d9f57dbcd31a8d515205b18fb050070a416e9b7411dfbd3` |
| 2000/E4 | `6589b898094887925e7717c8663c4b8b5e4b76f3c89a7898ed9f4691c1f2cc16` | `5c12be86d167f7ad3ffb6821ab19da6edf8a2a252ef03b7044f959ee16871482` |
| 2000/E5 | `275b5aaf55caecee8dc95e26f7cf118a9ee265f095ce61945b9ece56a4523323` | `788710b2321fc58b8fbe43da960be8bb8370ef5595f63c62e5ed3a17e008797f` |

Confiança: alta para contagens/formato, moderada para sinais estruturais de risco, não estabelecida para alcançabilidade, número real de chefes, duração e sucesso. Fontes locais de semântica: `src/tyrian2.c` (`JE_loadMap`, `JE_eventSystem`, `JE_eventJump`, `draw_boss_bar`, `start_level`), `src/helptext.c` (decifração), `src/game_rules.c`, `src/regress.c`, `tools/scan_smoothies.py`; nenhuma alteração nesses arquivos.
