#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

RUNS_ROOT="${SISTER_REFLEXA_RUNS_ROOT:-${ROOT}/docs/experiments/evidence/bootstrap/runs}"

mkdir -p "$RUNS_ROOT"

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"

# mktemp performs exclusive directory creation.
# The timestamp preserves human temporal readability while
# XXXXXX guarantees distinct run identities within the same second.
mktemp -d "${RUNS_ROOT}/${STAMP}-XXXXXX"
