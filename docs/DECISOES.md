# Log de Decisões de Arquitetura — LFR

Registro cronológico das decisões estratégicas do projeto (o "porquê" por trás do roadmap).
Cada entrada é datada e tem um status. Entradas novas vão no topo.

---

## 2026-07-04 — Congelar roadmap S3/RL/EDF e ir para a pista (Council)

**Status:** Aceito
**Método:** Análise via LLM Council (5 lentes independentes: Contrarian, First Principles,
Expansionist, Outsider, Executor + revisão cruzada anônima).
**Decisão do dono do projeto:** concordou com o veredito.

### Decisão

1. **Congelar** o port ESP32-S3, o RL residual (Modo Offroad) e o EDF até existir um
   **baseline medido na pista real**.
2. **Manter** a cascata (PID de posição externo + PID de RPM interno) como está — está pronta,
   testada e habilita frenagem controlada na entrada da curva + o Modo Corsa. **Não** removê-la,
   mas também **não** assumir que ajuda antes de medir (decidir com A/B na pista: cascata ON vs.
   PID→PWM direto).
3. **Levar o `main` para uma pista física** e produzir o primeiro conjunto de tempos de volta +
   caracterização de planta.

### Contexto

O firmware `main` está estável e coberto por 82 testes nativos, **mas o robô nunca rodou numa
pista real** — não há tuning real nem tempos medidos. O roadmap (`feat/esp32s3-port`) empilhava
port S3, mapeamento em PSRAM, RL sim-to-real e downforce por EDF sobre uma baseline empírica
inexistente.

### Racional (síntese do Council)

**Consenso (alta confiança):**
- O problema nº 1 é que o robô não tocou a pista. TDD valida lógica, não física.
- Sim-to-real sem planta medida = calibrar contra "robô fantasma" (garbage-in).
- **Modo Corsa** (odometria → feedforward) é a maior alavanca de ROI para menor tempo e **não usa
  IA**; é o que campeões brasileiros de fato fazem em pista repetida.
- A disciplina de TDD virou, na prática, fuga do hardware.

**Divergências e como foram resolvidas:**
- *Cascata de RPM ajuda ou atrapalha o seguimento reativo?* → decidir empiricamente na pista
  (A/B). Encoders são consenso como "ouro" para odometria/mapeamento.
- *EDF* → downforce escala com v² e é desprezível a ~300 g e 2–4 m/s (e consome 10–40 A da 2S);
  só reavaliar se um dado real provar perda por aderência em alta velocidade — e, se for, usar
  sucção com impeller contra o solo, não duct aéreo.
- *Simulador/RL* → valioso como ativo de plataforma/portfólio, mas fora de ordem; não gera
  velocidade enquanto não houver o "real" do sim-to-real.

**Pontos cegos que a revisão cruzada pegou:**
- Ganha-se **terminando**, não com a volta teórica mais rápida. São 3 tentativas; sair da pista
  numa curva é DNF. Faltava tratar modo de falha: recuperação de perda de linha, **detecção de
  cruzamentos** (exigida pelas regras), linha de largada/parada, marcadores laterais para frear
  antes da curva. **Confiabilidade > pico.**
- A cascata/encoders habilitam **frenagem a uma velocidade-alvo** na entrada da curva —
  potencialmente decisivo — e sustentam o Modo Corsa; removê-la mataria o item de maior ROI.
- Faltava **métrica quantitativa de "vencer"** (tempo-alvo vs. campeões) e critério de *kill/gate*
  por item do roadmap.
- Faltava **caracterizar a planta na bancada** (resposta ao degrau, latência, atrito) — pré-
  requisito para um simulador honesto.

### Prioridades resultantes (ordem de ROI para menor tempo)

1. **Confiabilidade** — terminar a volta, recuperar linha, tratar cruzamentos (decide DNF).
2. **Tuning PD real a alta velocidade** + caracterização da planta.
3. **Modo Corsa** — mapeamento + frenagem/feedforward por posição (usa os encoders).

Só reavaliar RL, EDF e o port S3 quando um **tempo medido** provar que são o gargalo.

### Primeiro passo concreto

Montar o hardware no chassis atual (**ESP32 DevKit, não o S3**) e rodar os sketches de bring-up
que já existem em `examples/` — `sensor_raw` → `motor_smoke` → `encoder_smoke` →
`differential_hw` → `velocity_calibration` — até cronometrar a **primeira volta** com o `LapTimer`
numa pista oval improvisada (~2×1 m). Esse "marco zero" (tempo + comportamento em curva) é o dado
que destrava todas as outras decisões. Registrar tudo em [`TUNING.md`](TUNING.md).

### Consequências

- A branch `feat/esp32s3-port` fica **pausada** (não abandonada) — o trabalho de simulador
  (S1 concluída, S2a em andamento) é preservado como ativo, mas sai do caminho crítico.
- O foco de curto prazo migra de "escrever código" para "coletar dados de pista".
- Próxima revisão desta decisão: quando houver ≥1 sessão de pista com tempos medidos.
