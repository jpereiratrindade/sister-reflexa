<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MVP0-E001 — Temporal & Evidential Closure + Minimal Process Vector
## Prediction (written before implementation)

recorded_at: 2026-08-18T00:00:00Z
baseline_commit: 8e4daa2 (tag: mvp0-materialized)
author: Antigravity / AG-RFX-MVP0-001

---

## Question

Can a minimal C++23 vertical slice preserve an immutable representation of the
evidence used at assessment time, produce a deterministic provisional process
vector from that frozen evidence, expose the result through a local web
interface, and reproduce the same assessment from the same EvidenceBundle
without rewriting the historical evidence?

---

## Prediction

> A minimal C++23 vertical slice can preserve an immutable representation of
> the evidence used at assessment time (EvidenceBundle), produce a deterministic
> provisional process vector from that frozen evidence using a transparent
> baseline evaluator, expose the full workflow through a local web interface,
> and reproduce the same assessment from the same EvidenceBundle after process
> restart, without rewriting or silently mutating historical evidence.

---

## Falsification conditions

### SUCCESS

All of the following must hold:

1. EvidenceBundle created at t1 with frozen evidence content is identical after
   process restart (structural + digest).
2. Altering/adding evidence after bundle creation at t2 does NOT change the
   frozen bundle B1 created at t1.
3. Two separate calls to assess the same bundle produce identical numeric
   process vectors.
4. The API vertical slice event→evidence→bundle→assessment executes end-to-end
   via HTTP.
5. The web interface renders the vector with PROVISIONAL labels and shows the
   epistemic status "READY / INCOMPLETE".
6. No assessment overwrites or silently changes a prior bundle or assessment.

### REFUTATION

Any of:

- The TSV append-only persistence cannot durably freeze bundle content without
  dependence on mutable live evidence (i.e., the snapshot is not stored inline).
- Same evaluator+bundle produces different numeric vectors across independent
  runs.
- A frozen bundle's content changes when underlying evidence items are mutated
  or new evidence is added.

### INCONCLUSIVE

- Build passes, tests pass, but the temporal preservation property cannot be
  demonstrated automatically (only manually).
- Web interface is served but the full vertical slice cannot be completed
  through the UI.
- The vector is produced but the provenance (which bundle) cannot be
  mechanically verified.

---

## Pre-implementation expectation of surprises

1. The evidence→dimension contribution mapping requires a new field in
   evidence.tsv (9th column). Old records lack it. The parser must tolerate
   both sizes. This is expected to work but is a compatibility surface to watch.

2. The TSV append-only model stores the bundle snapshot inline (frozen content
   encoded as a single percent-encoded field). This keeps the design minimal but
   makes large evidence sets verbose. Acceptable for MVP.

3. The `--serve` root path is `fs::current_path()` not the binary path.
   Tests that invoke the server must set the correct working directory.
   This may require a small test harness adjustment.

---

## What this experiment does NOT demonstrate even if successful

- Validity of the seven process vector dimensions as scientific constructs.
- Inter-rater reliability of the baseline evaluator.
- That 0.63 on any dimension means "63% of X occurred".
- Generalization of the future TinyReflexiveLM.
- That the system is ready for multi-user or production use.
- That the existing qualitative dimensions (T,E,Δ,K,A,R,P,F) and the new
  numeric vector (exposure, interaction, appropriation, incorporation,
  propagation, reflexivity, stabilization) are equivalent or comparable.

---

## Domain model summary (expected after implementation)

```
Event
  └─ Evidence (with optional dimension contributions)
       └─ EvidenceBundle (frozen at time t: immutable snapshot + digest)
            └─ Assessment (process vector from deterministic-baseline-v0.1)
```

The existing Claim/Relation/Evaluation ledgers are preserved unchanged.
The new bundle+assessment ledgers are additive.

---

## Evaluator specification (pre-implementation)

Name: deterministic-baseline v0.1
Dimensions: exposure, interaction, appropriation, incorporation,
            propagation, reflexivity, stabilization
Input per evidence item: dimension, support ∈ [-1.0, +1.0], confidence ∈ [0.0, 1.0]
Aggregation: vector[d] = clamp(0.0, 1.0, 0.5 + Σ(support_i * confidence_i) / max(1, n_d))
Where n_d = count of contributions for dimension d.
If n_d = 0: vector[d] = 0.0 (no information).
Status: PROVISIONAL / EXPERIMENTAL / NOT SCIENTIFICALLY VALIDATED.

This formula is not claimed to measure any real-world property.
It is a mechanical baseline to exercise the architecture.

---

## Temporal test design (critical)

```
t1: create Evidence E1 content="conteudo A"
    freeze bundle B1 (snapshot contains "conteudo A")

t2: add Evidence E2 content="conteudo B" (same event)

verify:
    B1.snapshot still contains "conteudo A" only
    B1.digest unchanged

    freeze bundle B2 (snapshot contains "conteudo A" + "conteudo B")
    B2.digest != B1.digest
    B2.evidence_count = B1.evidence_count + 1
```

This is the structural analog of the post-materialization mutation already
observed in docs/experiments/evidence/bootstrap/verification.txt (where the
content changed after the initial commit without the commit being amended).

---
status: PREDICTION_RECORDED
implementation_started: false
