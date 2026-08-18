#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# E001R (OBS-1 repair): each run writes to a unique timestamped subdirectory.
# Never overwrites a previous run's evidence artifact.
# Usage: ./scripts/verify_mvp0.sh [--bootstrap]
#   --bootstrap: additionally write to the legacy bootstrap/ path (for initial setup only)
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Generate a unique run ID from UTC timestamp
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="docs/experiments/evidence/bootstrap/runs/${RUN_ID}"
mkdir -p "${RUN_DIR}"

{
  echo "=== SisTer Reflexa MVP-0 verification ==="
  echo "run_id: ${RUN_ID}"
  echo "UTC: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "root: ${ROOT}"
  echo
  ./scripts/build.sh
  echo
  ./build/sister-reflexa --status
  echo
  ./build/sister-reflexa --status | grep -q 'score_policy=NO_AGGREGATE_SCORE'
  grep -q 'TinyReflexiveLM' docs/model/TINY_REFLEXIVE_LM.md
  grep -q 'append-only' docs/MVP0.md
  echo "MVP-0: PASS"
} 2>&1 | tee "${RUN_DIR}/verification.txt"

# Write run metadata
echo "run_id=${RUN_ID}" > "${RUN_DIR}/metadata.txt"
echo "timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${RUN_DIR}/metadata.txt"
echo "root=${ROOT}" >> "${RUN_DIR}/metadata.txt"

# Hash the verification output for integrity evidence
sha256sum "${RUN_DIR}/verification.txt" > "${RUN_DIR}/verification.txt.sha256"

echo "Run evidence written to: ${RUN_DIR}/"
echo "SHA-256: $(cat "${RUN_DIR}/verification.txt.sha256")"
