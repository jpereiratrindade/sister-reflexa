<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MVP0-E001R — Prediction: Evidential Integrity Repair
## Recorded BEFORE implementation

recorded_at: 2026-08-18T01:10:00Z
baseline_commit: 2e32364 (HEAD, MVP0-E001 verdict=SUPPORTED)
prior_assessment: docs/experiments/evidence/MVP0-E001/assessment.md
author: Antigravity / AG-RFX-MVP0-001R

---

## Prior assessment status

The E001 verdict of SUPPORTED is treated as a historical assessment now
challenged by four new observations (OBS-1 through OBS-4). That assessment
is preserved unchanged. This experiment produces a revised assessment.

---

## New observations recorded before prediction

**OBS-1:** `scripts/verify_mvp0.sh` unconditionally overwrites
`docs/experiments/evidence/bootstrap/verification.txt`. A verification
executed after commit 2e32364 already mutated that file again. Every run
destroys the previous run's evidence.

**OBS-2:** `content_digest` (fnv1a64) is stored alongside the snapshot but
is NOT verified when the bundle is loaded or before assessment. An adversarial
test demonstrated: manually changing `exposure:0.1:1.0` to `exposure:0.9:1.0`
in the stored snapshot while leaving the digest unchanged caused
`assess_bundle` to accept the modified bundle and produce `exposure = 1.0`
instead of the expected value from the original frozen content.
Therefore: "digest presence" ≠ "integrity verification".

**OBS-3:** The frozen snapshot contains only `id | content | contributions`.
It does not freeze `claim_id`, `kind`, `valid_time`, `source_ref`, `digest`
(source digest), `registered_at`. The evidential provenance of each item
is not fully captured.

**OBS-4:** `Assessment` binds the evaluator by string name
(`deterministic-baseline-v0.1`) but does not cryptographically or
structurally bind the evaluator specification/implementation.
Two different implementations could share this string.

---

## Question

Can the following properties be simultaneously demonstrated in a minimal
C++23 implementation:

1. Bundle integrity is verified on load and before assessment.
2. A bundle whose digest does not match its stored content is rejected.
3. The frozen snapshot captures enough provenance to reconstruct the
   complete evidential identity of each item at freeze time.
4. The verify script produces immutable, run-specific evidence artifacts.
5. Assessment binds the bundle digest (not just bundle_id) and the
   evaluator specification identity.

---

## Prediction

> The five integrity properties above can be demonstrated by:
> (1) replacing fnv1a64 with SHA-256 as the bundle digest;
> (2) adding verify_digest() called in assess_bundle() and load;
> (3) expanding the snapshot serialization to include all evidence
>     provenance fields (id, claim_id, kind, valid_time, source_ref,
>     digest, content, registered_at, contributions);
> (4) modifying verify_mvp0.sh to write to a timestamped run-specific
>     subdirectory;
> (5) storing bundle_digest in the Assessment record alongside bundle_id;
> (6) adding a stable evaluator spec hash (SHA-256 of a canonical
>     evaluator specification string).
>
> The adversarial tests (mutated snapshot, unchanged digest) will
> demonstrate that assessment is rejected before vector calculation.

---

## Falsification conditions

### SUCCESS (per dimension)

**BUNDLE_TEMPORAL_PRESERVATION:**
  B1 frozen at t1 survives t2 mutation — ALREADY DEMONSTRATED in E001.
  Reasserts with stronger digest.

**EVIDENTIAL_INTEGRITY:**
  A bundle whose snapshot has been modified but whose digest was not updated
  is rejected by assess_bundle() with an explicit integrity error.
  Two adversarial tests PASS.

**VERIFICATION_EVIDENCE_IMMUTABILITY:**
  Running verify_mvp0.sh twice produces two separate, non-overwriting
  artifact directories. Both are retrievable after the second run.

**EVALUATOR_REPRODUCIBILITY:**
  The same bundle_id + same bundle_digest + same evaluator_spec_digest
  always produces the same vector.

**GLOBAL_TEMPORAL_CLOSURE:**
  All five properties hold simultaneously on a single clean run.

### REFUTATION (per dimension)

**EVIDENTIAL_INTEGRITY refuted if:**
  assess_bundle() accepts a tampered bundle in any code path.

**VERIFICATION_EVIDENCE_IMMUTABILITY refuted if:**
  The second verify run overwrites the first run's artifact.

**EVALUATOR_REPRODUCIBILITY refuted if:**
  Two assessments of the same bundle+evaluator produce different vectors.

### INCONCLUSIVE

  Any property that cannot be automatically tested and is only asserted
  by inspection.

---

## Pre-implementation expected surprises

**S-expected-1:** The existing `data/bundles.tsv` contains a live bundle
(bnd-000001) with an fnv1a64 digest. After switching to SHA-256, this
bundle will fail integrity verification. This is correct and expected —
the old bundle was created before integrity verification existed. It should
be left in place as historical evidence, not deleted.

**S-expected-2:** Expanding the snapshot format breaks the existing bundle
(bnd-000001). The adversarial test infrastructure must operate on fresh
bundles, not the legacy bundle.

**S-expected-3:** The evaluator spec hash requires defining a canonical
specification string. This string must be stable (stored in code as a
constant) and distinct from the evaluator name string.

---

## Evaluator specification identity mechanism

The smallest reasonable mechanism: define a `EVALUATOR_SPEC_v0_1` string
constant containing the canonical formula and parameter set. Store
`sha256(EVALUATOR_SPEC_v0_1)` (first 16 hex chars for readability) in the
Assessment record as `evaluator_spec_digest`. This does not prevent a
bad actor from changing the constant — but it makes the claim explicit
and testable: if the constant changes, the digest changes, making prior
assessments distinguishable from new ones.

---

## What this experiment does NOT change

- The 7 provisional process vector dimensions.
- The relationship between T,E,Δ,K,A,R,P,F and the 7 numeric dimensions
  (remains an open, unresolved question).
- The evaluator formula itself.
- TinyLM — not introduced.
- No history rewrite of commits 2894330, f465a15, 2e32364.

---

## Snapshot format (expanded, post-implementation)

Each evidence item will be serialized as a fixed-field line:
```
id|claim_id|kind|valid_time|source_ref|digest|content|registered_at|contributions
```
All fields percent-encoded. Field order is part of the canonical format.
A change to this format constitutes a new snapshot version.

---

status: PREDICTION_RECORDED
implementation_started: false
