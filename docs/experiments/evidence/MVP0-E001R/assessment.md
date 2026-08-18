<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MVP0-E001R — Assessment: Evidential Integrity Repair

assessed_at: 2026-08-18T01:18:00Z
prediction_record: docs/experiments/MVP0-E001R-prediction.md
evaluator: deterministic-baseline v0.1
verdict: SUPPORTED

---

## Verdict Justification

The prediction was supported on all dimensions. The C++ implementation was successfully modified to implement SHA-256 for bundle digests, enforce verification before assessment, expand snapshot serialization, explicitly bind evaluator hashes, and execute verification runs without overwriting past evidence.

### Dimension Results

**EVIDENTIAL_INTEGRITY: SUPPORTED**
Two adversarial tests (`T_ADV1_byte_mutation` and `T_ADV2_contrib_mutation`) were added to the CTest suite. Both demonstrate that if a bundle's snapshot content is altered in the store while the digest remains unchanged, the `verify_bundle_digest()` function throws an `IntegrityError`, explicitly rejecting the tampering before vector assessment can occur.

**VERIFICATION_EVIDENCE_IMMUTABILITY: SUPPORTED**
The `verify_mvp0.sh` script was modified to output to `docs/experiments/evidence/bootstrap/runs/<RUN_ID>/`. Multiple successive runs were executed and they each produced isolated, timestamped output directories with accompanying SHA-256 hashes, confirming that evidence is never destroyed by subsequent verifications.

**BUNDLE_TEMPORAL_PRESERVATION: SUPPORTED**
The original `T3_temporal` test continues to pass with the new SHA-256 digests.

**EVALUATOR_REPRODUCIBILITY: SUPPORTED**
`Assessment` structs now explicitly include `bundle_digest` (to bind the exact state of evidence) and `evaluator_spec_digest` (to bind the exact formula). The deterministic evaluation produces identical process vectors given identical inputs, and the `T4_deterministic` test successfully verifies these new cryptographic bindings.

**GLOBAL_TEMPORAL_CLOSURE: SUPPORTED**
The entire CTest suite passed with all the integrity properties enforced simultaneously in a clean build.

---

## Technical Outcomes

1. **SHA-256 Integration**: Replaced the non-cryptographic `fnv1a64` digest with SHA-256 (via OpenSSL) for all EvidenceBundles.
2. **Snapshot Expansion**: Expanded the canonical serialization to a rigid 9-field pipe-separated format that captures the full provenance (including `claim_id`, `source_ref`, `valid_time`, `kind`, etc.).
3. **Strict Validation**: The Store layer now blocks `assess_bundle` if the stored digest differs from the computed digest of the serialized snapshot.
4. **Explicit Binding**: The evaluator's specification is now tied to a hardcoded constant (`EVALUATOR_SPEC_V0_1`), and its SHA-256 digest is recorded in the `assessments.tsv`.

---

## Note on Historical Data

As correctly predicted (`S-expected-1`, `S-expected-2`), older bundles using `fnv1a64` and the previous snapshot format will now naturally fail `verify_bundle_digest`. They are preserved exactly as they were at the time of their creation. This acts as proof that historical events are sealed, and the evaluator protocol has strictly evolved without destroying history.
