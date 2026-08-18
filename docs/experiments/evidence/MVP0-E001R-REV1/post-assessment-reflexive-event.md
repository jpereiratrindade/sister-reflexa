# MVP0-E001R-REV1 — Post-Assessment Reflexive Event

## Historical context

The assessment committed at `14f55ad` concluded `SUPPORTED`.

That historical assessment is preserved unchanged.

A subsequent independent audit discovered that the original
`T_ADV4_verify_collision` test invoked `verify_mvp0.sh`.

`verify_mvp0.sh` invoked `build.sh`, which invoked CTest, which invoked
`T_ADV4_verify_collision` again.

The test therefore recursively exercised the complete verification
system while attempting to test verification-run isolation.

## Observation

The defective test produced 114 untracked verification-run directories.

The contamination episode was recorded before cleanup in:

`recursive-run-contamination.txt`

The generated directories themselves were not promoted as valid
scientific verification runs.

## Interpretation

The collision-safety mechanism itself was not necessarily refuted.

However, the evidence originally used to support that property was
contaminated by the test harness.

Therefore the original assessment remains historically valid as the
conclusion reached at that time, but its evidential basis for this
specific property required subsequent qualification.

## Corrective intervention

Verification-run directory allocation was isolated into:

`scripts/allocate_verify_run_dir.sh`

Both production verification and `T_ADV4_verify_collision` now exercise
the same allocator.

The adversarial collision test operates entirely inside temporary
directories and does not invoke `verify_mvp0.sh`, CTest, or the real
evidence directory.

## Post-correction observations

Clean C++23 build:

`PASS`

CTest:

`12/12 PASS`

Verification evidence run count before standalone CTest:

`3`

Verification evidence run count after standalone CTest:

`3`

Result:

`CTest created no verification evidence runs.`

A single subsequent execution of `verify_mvp0.sh` changed the run count:

`3 -> 4`

Result:

`exactly one verification run created.`

Clean verification run:

`20260818T020556Z-I5YvNF`

Verification output SHA-256:

`3f9d2c833e0dbe6e727723d05e47010322d801aecc4a19521c7ccafaf7dd7589`

## Epistemic status

The corrected evidence supports:

- collision-safe allocation of verification-run directories;
- isolation of the collision test from production evidence;
- absence of verification-run creation as a CTest side effect;
- exactly one evidence run for one explicit verification execution.

The earlier contaminated evidence is not erased.

The stronger property has been re-demonstrated with a corrected test
harness.

This corrective verification was performed after discovery of the
problem and is therefore **not presented as a prospectively predicted
experiment**.

It is a reflexive revision of the evidential basis of the earlier
assessment.

## Result

`PROPERTY_REDEMONSTRATED: SUPPORTED`

`ORIGINAL_REV1_ASSESSMENT: PRESERVED`

`ORIGINAL_T_ADV4_EVIDENCE: QUALIFIED_AS_CONTAMINATED`

`TEMPORAL_HISTORY_REWRITTEN: NO`
