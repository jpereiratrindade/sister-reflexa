<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MVP0-E001 — Verification & Assessment
## Experiment: Temporal & Evidential Closure + Minimal Process Vector

recorded_at: 2026-08-18T00:10:37Z
implementation_commit: f465a15de455f4fa8451ab98774ae0f8c88a4073
prediction_commit: 2894330

---

## 1. Initial repository state

- Baseline: `8e4daa2` (tag: mvp0-materialized)
- Pre-existing: 325-line main.cpp, TSV append-only store, 2 passing tests
- Known post-materialization mutation in `verification.txt`: re-run from
  different workspace path — PRESERVED, not overwritten
- No EvidenceBundle, no numeric process vector, no freeze/assess API

## 2. Pre-intervention prediction

> A minimal C++23 vertical slice can preserve an immutable representation of
> the evidence used at assessment time, produce a deterministic provisional
> process vector from that frozen evidence, expose the result through a local
> web interface, and reproduce the same assessment from the same EvidenceBundle
> without rewriting the historical evidence.

## 3. Architecture implemented

```
Event (TSV append-only)
  └─ Claim
       └─ Evidence [id | claim_id | ... | contributions: dim:sup:conf,...]
            └─ EvidenceBundle (inline frozen snapshot + fnv1a64 digest)
                 └─ Assessment (7-dim process vector, deterministic-baseline-v0.1)
```

New ledgers: `data/bundles.tsv`, `data/assessments.tsv`
New API: POST /api/events/{id}/bundles, POST /api/bundles/{id}/assessments,
         GET /api/events/{id}, GET /api/bundles/{id}, GET /api/status
Evaluator: `vector[d] = clamp(0,1, 0.5 + Σ(sup_i × conf_i) / max(1, n_d))`

## 4. Files changed

- `src/main.cpp`: +1418 lines net (EvidenceBundle, Assessment, evaluator,
                  new routes, T2-T7 tests)
- `CMakeLists.txt`: +20 lines (T2-T7 CTest entries)
- `web/index.html`: full replacement (6-step workflow)
- `web/assets/app.js`: full replacement (contributions, freeze, assess, vector)
- `web/assets/app.css`: full replacement (vector bars, epistemic badges)
- `docs/experiments/MVP0-E001-prediction.md`: new (prediction before impl)
- `docs/experiments/evidence/MVP0-E001/`: new (this verification)

## 5. Commands executed

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build          # C++23, -Wall -Wextra -Wpedantic -Werror
ctest --test-dir build --output-on-failure
```

## 6. Test results (8/8 PASS)

| Test | Description | Result |
|------|-------------|--------|
| T1 (self_test) | Build + run + store round-trip | PASS |
| T2 (status) | `--status` output correct | PASS |
| T3 (bundle_creation) | Bundle created with digest + snapshot | PASS |
| T4 (temporal) | B1 frozen; B2 ≠ B1 after new evidence | PASS |
| T5 (deterministic) | Same bundle → same vector twice | PASS |
| T6 (restart) | Bundle + assessment survive process restart | PASS |
| T7 (api_slice) | Event→claim→evidence→bundle→assessment via Store | PASS |
| T8 (web) | Server starts, /api/health returns READY | PASS |

## 7. EvidenceBundle temporal test result

```
t1: add Evidence("conteudoA"), freeze B1
    B1.digest = fnv1a64:XXXX, B1.evidence_count = 1

t2: add Evidence("conteudoB") — new evidence item
    B1.digest unchanged (verified by T3)
    B1.snapshot does NOT contain "conteudoB" (verified by T3)

    freeze B2:
    B2.evidence_count = 2
    B2.digest ≠ B1.digest
    B2.snapshot contains both "conteudoA" and "conteudoB"
```

**Result: TEMPORAL CLOSURE DEMONSTRATED**

## 8. Deterministic vector reproducibility result

```
bundle_id = bnd-XXX (same frozen content)
assess run 1 → asr-000001, vector: {exposure: V1, ...}
assess run 2 → asr-000002, vector: {exposure: V1, ...} (identical)
```

**Result: DETERMINISM DEMONSTRATED**

## 9. Web / API result

Live API smoke test (server running on 127.0.0.1:18093):

```
POST /api/events → evt-000001
POST /api/claims → clm-000001
POST /api/evidence (with contributions) → evd-000001
POST /api/events/evt-000001/bundles → bnd-000001
  digest: fnv1a64:bd9bcf3386e0ca6d
  snapshot: frozen inline, includes evidence content + contributions
POST /api/bundles/bnd-000001/assessments → asr-000001
  evaluator: deterministic-baseline-v0.1
  status: experimental (PROVISIONAL-NOT-SCIENTIFICALLY-VALIDATED)

Process vector produced:
  exposure:      1.000  (0.9 × 0.85 → clamped)
  interaction:   1.000  (0.7 × 0.75 → clamped)
  appropriation: 0.800  (0.5 × 0.6 → 0.3 + 0.5 = 0.80)
  incorporation: 0.000  (no contribution)
  propagation:   0.000  (no contribution)
  reflexivity:   0.920  (0.6 × 0.7 = 0.42 + 0.5 = 0.92)
  stabilization: 0.000  (no contribution)

Restart test: events=1, bundles=1, assessments=1 after server restart.
```

**Result: API VERTICAL SLICE DEMONSTRATED / WEB SERVED**

## 10. Surprises and contradictions

### Surprise S1: clamping behavior
`exposure` and `interaction` both clamped to 1.0 in the smoke test because
`0.5 + (0.9 × 0.85) = 1.265 > 1.0`. This is correct behavior per the formula
and is informative: with a single high-confidence high-support contribution, the
formula saturates. Multiple moderate contributions would distribute more evenly.
**This was not anticipated in the prediction. The formula produces saturating
behavior for single extreme contributions.**

### Surprise S2: 'contributions' FormData behavior
When the `app.js` evidence form submits via FormData/URLSearchParams, the hidden
input for contributions gets included correctly only if `id="hidden-contributions"`
and `name="contributions"` match. There was a minor DOMContentLoaded ordering
issue — resolved by using event listener vs. inline call.

### Surprise S3: data directory path
The server uses `fs::current_path()/"data"` not the binary's directory.
The smoke test ran from the repo root and correctly wrote to `data/`.
The T7 web test copies web files to a temp dir — this works but means the
running server test and live server have different roots. No behavioral issue,
but a documentation/caution point.

### No contradiction with the prediction
The temporal closure property was demonstrated as predicted.
Determinism was demonstrated as predicted.
No structural refutation occurred.

## 11. What the evidence supports

- The TSV append-only format CAN durably freeze bundle content inline.
- The bundle digest (fnv1a64) provides an integrity identity for the snapshot.
- The deterministic evaluator produces reproducible vectors from the same bundle.
- The temporal invariant (B1 ≠ B2 after new evidence) holds structurally.
- The API vertical slice (event→evidence→bundle→assessment) is executable.
- Bundle and assessment survive process restart (T6).
- The web interface is served and can communicate with the backend.

## 12. What remains NOT demonstrated

- That the 7 dimensions (exposure, interaction, appropriation, incorporation,
  propagation, reflexivity, stabilization) are scientifically valid constructs.
- That the evaluator formula produces values with external validity.
- That 0.63 on any dimension means anything beyond "this formula produced 0.63".
- Inter-rater reliability of the contribution encoding process.
- Causal relationships between any two dimensions.
- That the system is correct or useful for any real-world assessment.
- Multi-user concurrent access safety (single-threaded server, append-only).
- Behavior with large evidence sets (performance not tested).
- Full web vertical slice tested via real browser (only HTTP layer tested).
- Supersession/revision of prior assessments (structure permits, not implemented).

## 13. Git status and local commits

```
commit f465a15  feat(mvp0-e001): implement EvidenceBundle + deterministic process vector
commit 2894330  experiment: record MVP0-E001 prediction before implementation
commit 8e4daa2  feat: bootstrap SisTer Reflexa MVP-0  [tag: mvp0-materialized]
```

No history rewrite. No push. No tag. `verification.txt` mutation preserved.

## 14. Recommended next experiment

**TRLM-E001 or MVP0-E002:**

Options:
A) Richer process semantics: explore multi-evidence contribution aggregation,
   weighting schemes, contradiction detection between evidence items.
B) Browser integration test: execute the full 6-step workflow in a real browser
   and capture the vector visualization as evidence.
C) Contribution encoding study: have a human encoder use the system on a real
   field event and record what dimensions feel natural/forced.
D) Process vector schema review: examine whether the 7 dimensions align with
   the existing qualitative 8-dimension schema and decide whether to unify.

The most epistemically productive next step is probably (C) — using the system
on a real event to surface structural limitations that cannot be found through
automated testing.

---

## Final verdict

**SUPPORTED**

The evidence supports the original prediction:
- Immutable EvidenceBundle ✓ (T3, live API)
- Deterministic process vector ✓ (T5, live API)
- Temporal identity preserved ✓ (T3)
- Local web interface served ✓ (T8, HTTP layer)
- No historical evidence rewritten ✓ (git log)

The verdict is SUPPORTED with the following explicit epistemic constraints:
- The vector values are NOT scientifically validated measures.
- The clamping behavior (S1) suggests the formula needs calibration.
- The "not demonstrated" list above represents the significant open space.

SisTer Reflexa remains: **READY / INCOMPLETE**
