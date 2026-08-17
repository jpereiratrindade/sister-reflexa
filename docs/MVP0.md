<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MVP-0 — Contrato experimental

## Questao

Conseguimos materializar um sistema minimo em que uma pessoa registra estado anterior, evento, alegacoes, evidencias e avaliacao, preservando proveniencia temporal suficiente para reconstruir uma trajetoria sem reduzir reflexividade a uma equacao?

## Predicao

Um ledger append-only mais uma interface geral->especifico sera suficiente para demonstrar o primeiro circuito navegavel `evento -> alegacao -> evidencia -> avaliacao -> relacao/sucessor`, mas nao demonstrara validade do instrumento nem do futuro LM.

## Gate de suporte local

- projeto C++23 compila com warnings-as-errors;
- testes passam;
- servidor responde `/api/health`;
- interface web e servida localmente;
- registros persistem apos reinicio;
- nenhuma avaliacao agrega automaticamente T,E,Delta,K,A,R,P,F em um numero;
- correcoes sao novos registros, nao sobrescrita;
- estado exportavel inclui referencias de evidencia;
- contrato TRLM existe, mas modelo permanece `NOT_TRAINED`.

## Nao demonstrado

- validade externa das oito dimensoes;
- confiabilidade entre avaliadores;
- causalidade forte;
- reflexividade como propriedade psicologica;
- generalizacao do futuro TRLM;
- superioridade sobre metodos de avaliacao existentes.
