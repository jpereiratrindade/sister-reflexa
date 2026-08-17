<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Foundation — MVP-0

## Unidade operacional

```text
contexto anterior
  -> evento
  -> alegacao
  -> evidencia
  -> interpretacao/avaliacao
  -> decisao/transformacao
  -> relacao/sucessor
  -> novo ciclo
```

O armazenamento do MVP usa ledgers TSV append-only para manter a implementacao pequena, auditavel e independente de banco externo. Isso e uma decisao de MVP, nao uma tese de arquitetura definitiva.

## Entidades

- `event`: perturbacao/interacao identificavel;
- `claim`: alegacao sobre mudanca, alcance, persistencia ou relacao;
- `evidence`: suporte recuperavel associado a uma alegacao;
- `relation`: aresta entre entidades, por exemplo `contributes_to`, `successor_of`, `contests`, `supports`;
- `evaluation`: snapshot humano versionado, nunca atualizacao in-place.

## Tempos preservados

- `valid_time`: quando o fato/estado alegadamente ocorreu;
- `registered_at`: quando entrou no sistema;
- `evaluated_at`: representado pelo `registered_at` do snapshot de avaliacao;
- `succession_time`: pode ser registrado em `relation.note`/valid time nesta primeira fatia e deve ganhar campo proprio em milestone posterior.

## Runtime

O servidor e local-only por default (`127.0.0.1`) e usa POSIX sockets diretamente. Nao ha autenticacao no MVP-0; portanto, nao expor na rede.
