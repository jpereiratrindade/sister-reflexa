#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p docs/experiments/evidence/bootstrap
{
  echo "=== SisTer Reflexa MVP-0 verification ==="
  echo "UTC: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  ./scripts/build.sh
  echo
  ./build/sister-reflexa --status
  echo
  ./build/sister-reflexa --status | grep -q 'score_policy=NO_AGGREGATE_SCORE'
  grep -q 'TinyReflexiveLM' docs/model/TINY_REFLEXIVE_LM.md
  grep -q 'append-only' docs/MVP0.md
  echo "MVP-0: PASS"
} 2>&1 | tee docs/experiments/evidence/bootstrap/verification.txt
