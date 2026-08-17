#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRAXIS_HOME="${PRAXIS_HOME:-${HOME}/.local/share/praxis/current}"
if [[ ! -f "$PRAXIS_HOME/scripts/bootstrap_project_licensing.py" ]]; then
  echo "Praxis licensing bootstrap unavailable: $PRAXIS_HOME" >&2
  exit 2
fi
python3 "$PRAXIS_HOME/scripts/bootstrap_project_licensing.py" "$ROOT"
