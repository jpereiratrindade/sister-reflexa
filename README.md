<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# SisTer Reflexa

Infraestrutura experimental para reconstruir e avaliar reflexividade evidenciada e trajetorias de evolucao sociotecnica sob integridade temporal.

## MVP-0

O primeiro MVP e deliberadamente menor que a teoria. Ele permite:

1. registrar um evento e seu contexto anterior;
2. registrar alegacoes ligadas ao evento;
3. anexar evidencias com tempo valido, tempo de registro, fonte e digest opcional;
4. ligar entidades por relacoes de contribuicao, sucessao ou contestacao;
5. registrar snapshots de avaliacao humana nas dimensoes T, E, Delta, K, A, R, P e F;
6. visualizar a trajetoria pela interface web;
7. exportar o estado pela API local;
8. acumular um corpus versionado para o futuro TinyReflexiveLM.

**Nao existe IRE calculado no MVP-0.** O vetor precede qualquer projecao escalar.

## Execucao

```bash
./scripts/verify_mvp0.sh
./bin/sister-reflexa serve
```

Abra: `http://127.0.0.1:8092`

## Principios

- destino aberto, passado preservado, transicao explicavel;
- append-only no MVP: corrigir significa acrescentar um sucessor, nao sobrescrever silently;
- tempo valido != tempo de registro != tempo de avaliacao != tempo de sucessao;
- a interface avalia trajetorias/eventos, nao pessoas;
- ausencia de evidencia pode permanecer `NA`;
- avaliacao humana e fonte de treinamento futura, nao ground truth metafisica;
- o modelo futuro deve aprender uma avaliacao estruturada a partir de bundles de evidencia, nao imitar uma equacao agregadora.
