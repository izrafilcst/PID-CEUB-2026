# Relatório consolidado — LFR v2

**Data**: 2026-10-14 · **Autor**: analyst (ruflo `aval-wp3`, tarefa `task-1791917734734-dpv3kx`)
**Branch avaliada**: `worktree-agent-a869269b7b6fa8a75` @ `3aab625` = `feat/lfr-v2-retomada`
(`c0a70b8`, com WP1 e WP2 integrados) + os 10 commits do WP3 e os 14 da rodada 1 de correção
(atualizado depois da reavaliação da rodada 1; a versão anterior deste relatório avaliava
`67f185e`)
**Fontes**: `docs/avaliacao/2026-10-13-WP1.md`, `-WP2.md` e `-WP3.md`;
`docs/REVISAO_FIRMWARE.md`; `docs/DIVERGENCIAS.md`; spec
`docs/superpowers/specs/2026-10-13-lfr-v2-retomada-design.md` (§9 e §11).

## 1 Estado da branch

| Verificação | Resultado | Quem rodou |
|-------------|-----------|------------|
| `pio test -e native` | `271 test cases: 271 succeeded` | eu, em `67f185e` e de novo no merge da rodada 1 (`ec95321`) |
| `cd tools && python3 -m unittest discover -s tests` | `Ran 119 tests` / `OK` | eu, rodada 1 |
| `python3 tools/check_manuais.py` | `6 arquivo(s), 0 problema(s)` (com a regra 9: HTML em dia com o commit-fonte `f3c31ba`) | eu, rodada 1 |
| `cd docs/dashboard && python3 -m unittest test_lfr_sim` | `Ran 10 tests` / `OK` | eu |
| Manual HTML offline (Playwright, `file://`, rede bloqueada) | 0 requisições externas; abas, listas persistentes (17 + 9 + 9), tema escuro, 375 e 360 px OK | eu, rodada 1 |
| `pio run -e esp32dev` sem warnings de `src/`; os seis `bench_*`; CAD 92 testes; `build.py` exit 0 | OK | coordenador na P4 e avaliadores do WP1/WP2 (o WP3 não toca `src/`, `test/`, `examples/`, `platformio.ini` nem `hardware/cad/`: `git diff --stat c0a70b8..3aab625` vazio nesses caminhos) |

Nada foi testado em hardware real nem na pista (spec §9).

## 2 Vereditos

| Pacote | Veredito | Rodadas | Abertos |
|--------|----------|---------|---------|
| WP1 — Firmware | **APROVADO COM RESSALVAS** | 1 rodada de correção (`47f460d` → `178a2ca`); A-WP1-01 (HIGH) e os MED resolvidos, estes pelas emendas E-3 e E-4 | 0 CRIT · 0 HIGH · 0 MED · 4 LOW · 4 INFO, mais três confirmações em hardware |
| WP2 — Mecânica (build123d) | **APROVADO COM RESSALVAS** | 1 rodada (`8fda579` → `a616ecf`); 2 HIGH e 3 MED fechados | 0 CRIT · 0 HIGH · 0 MED · 11 LOW · 3 INFO (A-WP2-12 e -14 fechados no merge) |
| WP3 — Manuais e documentação | **APROVADO COM RESSALVAS** | 1 rodada (`67f185e` → `3aab625`); A-WP3-01 (CRIT), os 2 MED e 7 LOW resolvidos; bloqueio removido | 0 CRIT · 0 HIGH · 0 MED · 3 LOW · 7 INFO |

**Leitura de conjunto.** O firmware, o modelo mecânico e os manuais formam um carro coerente
no papel: os manuais batem com o firmware e com o CAD em quase tudo, e as decisões do usuário
(§11: E-1 `lock`, E-2 `stk` no `info`, E-3 parada no aperto, E-4 START bloqueado com
calibração de encoder aberta e erro 5 = ENC) estão no código, nos testes, no dashboard, no
simulador e nos manuais. A decisão de 14/10 na rodada de correção do WP3 (VCC da lógica do
TB6612 no 3V3, D17) também está em todos os documentos. Nenhum pacote tem CRIT, HIGH ou MED
aberto. Faltam, antes da pista, a montagem, os testes em hardware e a sintonia. E a v2 **não faz
volta válida na RoboCore** (F-34, regra 5.7), o que abre a próxima versão.

## 3 Achados abertos

### 3.1 WP3 — Manuais (detalhes em `2026-10-13-WP3.md`)

Resolvidos na rodada 1 e conferidos: A-WP3-01 (CRIT; VCC do TB6612 no 3V3, MP1584 ajustado sem
carga), A-WP3-02 e -03 (MED; encoders no 3V3, *bypass* de 3,3 V do QTR-8A), A-WP3-06 a -12 (LOW)
e as partes b e d da A-WP3-16.

| ID | Sev. | Resumo | Destino |
|----|------|--------|---------|
| A-WP3-04 | LOW | Transistor, resistor de base (e diodo) do buzzer "a definir" | dev, antes da montagem |
| A-WP3-05 | LOW | USB e pack ao mesmo tempo: depende de diodo USB/VIN na placa, não documentado (*a confirmar*) | montador + dev, na primeira energização |
| A-WP3-13 | LOW | Fichas 01/03/04/05/07 com notas da v1 (D16) | dev |
| A-WP3-14 | INFO | Intervalos de inspeção e perfil de PETG vindos do plano | montador + manutenção, na prática |
| A-WP3-15 | INFO | Lacunas menores do `CLAUDE.md` | dev |
| A-WP3-16 (a, c) | INFO | Rótulo "BLE" em tela estreita; tempo de montagem não medido | — |
| A-WP3-17 | INFO | Faixa de precisão do ADC a 8,4 V (2,69 V no GPIO 34) | operador, na aferição do `vbk` |
| A-WP3-18 | INFO | Carga do 3V3 da placa (agora com a lógica do TB6612 e os encoders) não demonstrada | montador + dev, na primeira energização |
| A-WP3-19 | INFO | Isolar a ponta do fio de saída do MP1584 enquanto ele estiver solto | wp3-docs, próxima edição |
| A-WP3-20 | INFO | A regra 9 do verificador depende do hash do rodapé (sem *squash*/*rebase*) | coordenador, no merge |

### 3.2 WP1 — Firmware (detalhes em `2026-10-13-WP1.md`)

| ID | Sev. | Resumo | Situação na branch |
|----|------|--------|--------------------|
| A-WP1-04 (a) | LOW | `defaults` volta `pprL`/`pprR` a 600; um `save` em seguida apaga a auto-calibração | Avisado nos manuais (OPERACAO §6, MANUTENCAO §4); código sem mudança |
| A-WP1-07 | LOW | `ack` de `start`/`stop`/`scal` confirma o recebimento, não o efeito | Avisado na OPERACAO §4 |
| A-WP1-09 | LOW | Regras ainda no `main.cpp` sem teste (sessão de calibração, contagem de atrasos, "bateria ok") | Aberto |
| A-WP1-10 | LOW | Compensação de tensão depois da mistura achata a diferença L/R com bateria baixa | Aberto |
| A-WP1-11 | INFO | `notify` não limita o tamanho (buffers de 192 B, limite de 180) | Aberto |
| A-WP1-12 | INFO | F-36: leitura dos 8 canais em ~192 µs de clock, não < 100 µs; comentário do `SensorArray.h` | `CLAUDE.md` corrigido; código e medição em aberto |
| A-WP1-13 | INFO | Resolução de RPM a 500 Hz (1 contagem = 50 RPM com PPR 600) | Avaliar na bancada |
| A-WP1-14 | INFO | Fase RED do TDD não verificável no histórico | Processo |
| — | — | Confirmar em hardware: `info` na inscrição (A-WP1-01), parada no aperto (E-3), ERRO(5) e START recusado (E-4) | Pendente de hardware |

### 3.3 WP2 — Mecânica (detalhes em `2026-10-13-WP2.md`)

| ID | Sev. | Resumo | Situação na branch |
|----|------|--------|--------------------|
| A-WP2-06 | LOW | `comp_furo` e `pcb` sem efeito no modelo; `celula_d` só informativa | MONTAGEM §2.2–2.3 avisa; CAD sem mudança |
| A-WP2-07 | LOW | Colar ⌀4 do N20 não modelado (cubo a 0,6 mm da face) | MONTAGEM §2.3 manda medir antes de imprimir |
| A-WP2-08 | LOW | Trava radial da roda com ~3,5 mm de rosca, sem porca cativa | MONTAGEM §3 passo 7 (pouco torque) |
| A-WP2-09 | LOW | Batente do motor com "chifres" de 0,05 mm | Aberto |
| A-WP2-10 | LOW | Bitola real ≈ 97,2–98,6 mm (D10 diz 95,8) | MONTAGEM §3 cita os dois valores |
| A-WP2-11 | LOW | Retenção axial do O-ring assimétrica | MANUTENCAO §3.6 passo 3; validar na pista |
| A-WP2-13 | LOW | STEP muda só no timestamp a cada build | Aberto |
| A-WP2-15 | LOW | Patim sem a rampa de 30° no bordo de ataque | Aberto |
| A-WP2-16 | LOW | Cupom não calibra M2; entalhes de 0,6 mm | MONTAGEM §2.2 manda manter os típicos |
| A-WP2-20 | LOW | O-ring fora de 38–44 mm sai com código 1, não 2 | MONTAGEM §2.3 explica |
| A-WP2-21 | LOW | Cotas estimadas do QTR-8A ~0,3 mm otimistas | MONTAGEM §2.3 manda medir as quatro |
| A-WP2-17 | INFO | Calços por passo | **Coberto** pela MONTAGEM §2.4 (12 + 6 para 6 mm) |
| A-WP2-18 | INFO | Aceleração estimada (13 m/s²) acima do limiar de empinar (6,6 m/s²) | OPERACAO §8 passo 6 (`acc`); validar na pista |
| A-WP2-19 | INFO | Carga nos patins cai para ~10 % com 30 g de fios atrás do eixo | MONTAGEM §8 (pesar e rodar `build.py`); D7 |

Fechados no merge, conferidos por mim: A-WP2-12 (`.gitignore:22` com
`!hardware/cad/out/*.stl`; `git check-ignore -v` casa a negação) e A-WP2-14
(`SENSOR_HEIGHT_MM 3` em `src/config.h:36`, D8 resolvido).

### 3.4 Revisão do firmware — itens fora do escopo da v2

| ID | Sev. | Resumo | Destino |
|----|------|--------|---------|
| F-34 | CRIT (regulamento) | Sem detecção de marcações nem parada automática depois da chegada: nenhuma volta é válida na RoboCore (regra 5.7) | **Próxima versão, primeira atividade** |
| F-13 | MED | `LapTimer` sem marcação: `lap`/`laps` sempre vazios | Próxima versão (depende de F-34) |
| F-32 | LOW | ISR do encoder não é segura com o cache desligado; PCNT | Próxima versão |
| F-35 (parte) | LOW | BLE sem pareamento; nome fixo `LFR-RACER-01` | Próxima versão |
| F-37, F-38 | INFO | `Logger` e `VelocityProfile` sem uso | Limpeza / próxima versão |

F-33 (modo competição) foi resolvido pela E-1.

### 3.5 Divergências abertas (`docs/DIVERGENCIAS.md`)

| ID | Resumo | Responsável |
|----|--------|-------------|
| D6 | Código do motor N20 a confirmar (a unidade é genérica) | montador + dev |
| D7 | Massas compradas estimadas; CG e carga nos patins dependem delas | montador (pesar) + dev (`specs.py`) |
| D11 | Seção da caixa do N20 (10 × 12 mm na Pololu, 13 na ficha 04) | montador (medir) + dev |
| D16 | Notas da v1 nas fichas 01, 03, 04, 05 e 07 | dev |

### 3.6 Spec

Erratas já apontadas, sem efeito no carro: §4.1 lista o GPIO 4 como *strapping* (não é) e §3
fala em 13 suítes (eram 12) — `docs/REVISAO_FIRMWARE.md`; §4.3 descreve `vcut` com "por 1 s"
também para armar (A-WP3-08; os manuais e o README já estão certos). O texto da §6 sobre o
ajuste do MP1584 ("antes de ligar o ESP32") ficou mais fraco que a prática adotada em D17
("com a saída sem carga, antes de ligá-la ao trilho de 5 V").

## 4 Próximas atividades (lista única, priorizada)

Critério: primeiro o que evita dano, depois o que precisa estar certo antes de gastar material
(compra e impressão), depois a ordem natural de montagem → bancada → pista, e por fim as
melhorias sem pressa. A próxima versão vem em bloco separado e abre com F-34, como decidido.

### 4.1 Versão 2 — até a pista

Rodada 1 do WP3: as antigas atividades 1 (A-WP3-01) e 2 (rodada de correção) foram feitas e
saíram da lista; o merge e a publicação do HTML continuam como próximo passo do coordenador.

| # | Atividade | Responsável | Origem | Quando |
|---|-----------|-------------|--------|--------|
| 1 | Merge do WP3 (`worktree-agent-a869269b7b6fa8a75` @ `3aab625`) e da avaliação (`aval-wp3`) com `git merge --no-ff`, para o hash do rodapé do HTML continuar no histórico (A-WP3-20); publicar o manual HTML (P7) e pôr o link no README | coordenador | Plano mestre P6–P7 | Agora: nada mais bloqueia |
| 2 | Definir o transistor, o resistor de base e o diodo do buzzer e pôr na BOM, no WIRING e na MONTAGEM (A-WP3-04); na mesma edição, a frase para isolar a ponta do fio de saída do MP1584 enquanto ele está solto (A-WP3-19) | dev (wp3-docs) | A-WP3-04, -19 | Antes da compra |
| 3 | Comprar a BOM e as reservas marcadas como compra extra na MANUTENCAO §7, registrar o preço real dos itens "a confirmar" e do QTR-8A importado, e **pesar cada componente comprado** (ESP32, N20, células, suporte, MP1584, TB6612, QTR-8A, perfboard) para o `specs.py`; regenerar fichas e `build.py` | montador + dev | Spec §9 (pesagem), D7, BOM | Ao receber as peças |
| 4 | Antes de imprimir: cupom → `[tolerancias]`; medir suporte 2×18650, células, O-ring, seção e comprimento do N20 (D11), colar do N20 (A-WP2-07), as quatro cotas do QTR-8A (A-WP2-21) e a chave → `[medidas]`; `build.py` com saída 0; só então imprimir | montador | MONTAGEM §2; A-WP2-07, -21; D11 | Antes de imprimir |
| 5 | Montar e fazer as checagens antes de energizar com a ordem corrigida; primeira energização: 3 bipes e LED a 1 Hz; anotar o `stk` das duas tasks (≥ ~1 KB); conferir se a placa tem diodo entre USB e VIN; medir a corrente no 3V3 com o QTR aceso e o BLE conectado | montador + dev | MONTAGEM §3–§6; A-WP1 atividade 5; A-WP3-05, -18 | Primeira energização |
| 6 | Seis testes de bancada no hardware: sentido dos motores (corrigir por `invL`/`invR`/`swap`), pull-ups de 18/19 e 36/39, encoders em 3,3 V, canais do QTR; medir o tempo do `readAll` | montador + manutenção | Spec §9 (hardware); F-36, A-WP1-12 | Logo depois de 6 |
| 7 | Confirmar em hardware o que só foi verificado por análise: `info` na inscrição (A-WP1-01), parada no aperto em ARMED/RUNNING (E-3), START recusado com `cal` aberto e ERRO(5) com um encoder desconectado em `mode` = 1 (E-4), `lock` recusando `stop`, `ovr` abaixo de 2000 µs correndo, frames inteiros no celular (MTU) | operador + dev | WP1 §6 | Na bancada, antes da pista |
| 8 | Calibrar os dois encoders (`cal`) e comparar com o PPR provisório de 600 (F-17); aferir `vbk` perto de 8,4 V e perto de 6,4 V | operador | OPERACAO §9–§10; A-WP3-17 | Antes da pista |
| 9 | Pesar o carro pronto, ajustar `fios_parafusos_massa`, rodar `build.py`: carga nos patins entre 10 e 20 % | montador | D7; A-WP2-19 | Antes da versão final das peças |
| 10 | Primeira volta e sintonia em pista no perfil 0, `mode` 0 (Ziegler–Nichols do `docs/TUNING.md`), registrando cada sessão no `docs/tuning_log.csv`; observar se a frente levanta na largada (`acc`), se o O-ring sai na curva e o desgaste dos patins; medir a autonomia | operador | Spec §9 (sintonia); A-WP2-11, -18 | Primeiras sessões de pista |
| 11 | Modo cascata na pista (`mode` 1, `kpv`/`kiv`), depois de 8 e 10 | operador + dev | OPERACAO §9 | Depois da sintonia em PWM direto |
| 12 | Ensaiar a tomada de tempo com `lock` = 1 e o checklist de box (regras 3.1.1 e 5.9); lembrar que a v2 não faz volta válida na RoboCore | operador | OPERACAO §12; F-33/E-1; F-34 | Antes de um evento |
| 13 | Fichas: corrigir as notas da v1 (D16, A-WP3-13), a seção do N20 (D11) e identificar o código do motor (D6) | dev + montador | DIVERGENCIAS | Quando houver as medidas |
| 14 | Firmware, LOW: `defaults` preservar o PPR (A-WP1-04 a); recusar `start`/`stop`/`scal` sem efeito (A-WP1-07); compensar a tensão antes da mistura (A-WP1-10); extrair a sessão de calibração e o monitor de atraso para classes testadas (A-WP1-09); limitar o `notify` (A-WP1-11); corrigir o comentário de tempo do `SensorArray.h` (A-WP1-12) | dev | WP1 | Próxima iteração do firmware |
| 15 | CAD, LOW: padrões do QTR em 2,7/1,3 mm ou janela maior (A-WP2-21); modelar o colar do N20 (A-WP2-07); toro no canal e bitola no relatório (A-WP2-10); O-ring inválido sair com 2 (A-WP2-20); `comp_furo`/`pcb` (A-WP2-06); batente (A-WP2-09); rampa de 30° (A-WP2-15); série M2 no cupom (A-WP2-16); timestamp do STEP (A-WP2-13); trava da roda (A-WP2-08) | dev (projetista) | WP2 | Antes de uma segunda impressão |
| 16 | `CLAUDE.md` e detalhes de leitura dos manuais (A-WP3-15, -16); confirmar os intervalos de inspeção e o tempo de montagem depois da primeira montagem (A-WP3-14) | dev + manutenção | WP3 | Sem pressa |

### 4.2 Próxima versão

| # | Atividade | Responsável | Origem |
|---|-----------|-------------|--------|
| N1 | **Parada automática depois da chegada (RoboCore 5.7)**: um sensor de cada lado, a ~50–90 mm do centro (as marcações ficam a 40 mm da linha, fora do alcance do QTR-8A, ±33 mm); canais de leitura (os 8 do MCP3008 estão ocupados); contagem das marcações da direita; parar sobre a linha dentro da área de 1 m e ficar 2 s parado. Exige spec nova, mudança na barra de sensores (CAD) e no firmware | usuário + dev + projetista CAD | F-34; spec §9 e §10 |
| N2 | Cronômetro de volta pelas marcações (`LapTimer`) | dev | F-13 (depende de N1) |
| N3 | Marcações de curvatura (lado esquerdo, regra 4.15) e integração do `VelocityProfile` | dev | F-38; spec §9 |
| N4 | Encoder pelo periférico PCNT; reavaliar a resolução de RPM | dev | F-32; A-WP1-13 |
| N5 | BLE com nome único (sufixo do MAC) e pareamento opcional | dev | F-35 |
| N6 | Pranchas dos componentes 11–13 e A1/A2 regeneradas para a v2 | dev | Spec §9; D13 |
| N7 | PCB dedicada no lugar do perfboard | dev | Spec §9 |
| N8 | Remover ou reaproveitar o `Logger` | dev | F-37 |

## 5 Observações sobre o processo

- As três avaliações acharam problemas que os testes do executor não pegavam, todos na
  fronteira com o mundo físico: o `info` que o simulador entregava e o BLE real não
  (A-WP1-01), cabeças de parafuso e conector fora do modelo (A-WP2-04, -05) e um trilho de 5 V
  que alimentava mais do que o manual dizia (A-WP3-01). Os três foram corrigidos em uma rodada.
  A primeira energização e a bancada (atividades 5 a 7) são onde o resto desse tipo de defeito
  vai aparecer; vale reservar tempo de desenvolvimento para elas.
- Os avisos dos manuais cobrem os LOW do firmware e do CAD que ficaram abertos (PPR no
  `defaults`, `ack` de estado, cotas MEDIR, colar do N20). Quando o código ou o modelo for
  corrigido, o aviso correspondente tem que sair do manual.
