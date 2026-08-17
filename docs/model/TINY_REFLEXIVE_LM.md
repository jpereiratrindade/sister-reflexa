<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# TinyReflexiveLM (TRLM) — modelo candidato 2

## Genealogia

`TinyLogicLM -> TinyReflexiveLM` e uma sucessao experimental, nao uma alegacao de transferencia de capacidade.

Do TinyLogicLM preservamos:

- C++23, CPU-only, implementacao pequena e inspecionavel;
- linguagem/modelo causal com logits sobre vocabulario completo;
- backward explicito e verificacao por diferencas finitas;
- gates antes de claims fortes;
- curriculum e observacao de estabilidade;
- separacao entre atingir desempenho e permanecer estavel;
- learning-rate decay como intervencao local demonstrada no caso anterior, nao como dogma do novo modelo.

## Mudanca de tarefa

TinyLogicLM aprendia uma tarefa simbolica Y/N. O TRLM deve aprender a produzir uma **avaliacao estruturada** a partir de um bundle de eventos, alegacoes, evidencias, tempos e relacoes.

Nao havera equacao de IRE embutida como target.

### Input candidato

O primeiro experimento deve usar representacao simbolica derivada dos registros, por exemplo:

```text
<BOS>
<EVT_FIELD_DAY>
<BASELINE_PRESENT>
<CLAIM_TRANSFORMATION>
<EVID_DOC><EVID_BEFORE><SOURCE_OK>
<EVID_REPORT><EVID_AFTER><SOURCE_OK>
<ALT_CAUSE_PRESENT>
<REACH_GROUP>
<PERSIST_UNKNOWN>
<NEXT_CYCLE_CHANGED>
<EVALUATE>
```

Isso permite estudar a capacidade avaliativa antes de misturar o problema com compreensao irrestrita de linguagem natural.

### Target candidato autoregressivo

```text
<VERDICT_PARTIAL>
<T_HIGH><E_HIGH><D_HIGH><K_MED><A_HIGH><R_MED><P_NA><F_HIGH>
<GAP_PERSISTENCE><GAP_ALTERNATIVE_CAUSES>
<EOS>
```

O numero escalar nao e target. O modelo aprende um perfil e suas lacunas.

## Classes de saida

Verdict provisoria, contestavel:

- `INSUFFICIENT`
- `EMERGENT`
- `PARTIAL`
- `STRONG`
- `REFLEXIVE`

Dimensoes:

- `NA`, `LOW`, `MED`, `HIGH` para T, E, Delta, K, A, R, P, F.

Esses rotulos sao **rubrica candidata**. Devem ser alterados se dados reais mostrarem que nao discriminam o fenomeno.

## Dataset real

O MVP-0 funciona como instrumento de producao do dataset. Cada exemplo de treinamento deve preservar:

```text
event_id
snapshot_time
input_entity_ids[]
evidence_refs[]
human_evaluation_id
rubric_version
verdict
T,E,Delta,K,A,R,P,F
uncertainty
adjudication_status
```

Um exemplo so entra no conjunto de referencia depois de revisao/adjudicacao; discordancia entre avaliadores deve ser preservada, nao apagada.

## Experimento TRLM-E001 — aprendizagem sintetica

Pergunta: a cadeia de aprendizagem de TinyLogicLM consegue ser generalizada para sequencias de avaliacao estruturada sem hardcode da rubrica?

Candidato inicial, ainda nao autorizado como arquitetura final:

```text
vocab ~= 48
max_context = 64
d_model = 8
1 cabeca de atencao
FFN 8 -> 16 -> 8
LM head 8 -> vocab
~1.7k parametros, dependendo do vocabulario final
```

O primeiro alvo e **aprender o conjunto sintetico e reproduzir uma sequencia de avaliacao**, nao avaliar eventos humanos reais.

## Experimento TRLM-E002 — estabilidade

Repetir o contraste aprendido no TinyLogicLM:

- regime de treino suficiente para aquisicao;
- observar eventual forgetting;
- somente se observado, testar decay temporalmente localizado;
- nao importar automaticamente `0.05 -> 0.005` como receita.

## Experimento TRLM-E003 — generalizacao composicional

Separar combinacoes de evidencias/relacoes do treino e testar se o modelo generaliza para bundles estruturalmente novos.

## Experimento TRLM-E004 — avaliacao humana real

Somente depois de dataset adjudicado suficiente:

- treino em snapshots humanos;
- split por evento/contexto, nao por linhas quase duplicadas;
- comparar com avaliadores humanos e baseline mais simples;
- medir calibracao e discordancia;
- preservar explicacao por referencias/slots de evidencias;
- resultado possivel: `SUPPORTED`, `REFUTED` ou `INCONCLUSIVE`.

## Regra central

O TRLM nao deve aprender a maximizar um numero chamado reflexividade. Deve aprender, se a evidencia permitir, a **reconstruir um julgamento estruturado e contestavel** sobre uma trajetoria.
