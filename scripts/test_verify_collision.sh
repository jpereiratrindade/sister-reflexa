#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# T_ADV4 — collision-safe verification run identity
#
# Tests the production run-directory allocator directly.
# It intentionally does NOT invoke verify_mvp0.sh because doing so
# would recursively execute CTest, including this test itself.

set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "=== T_ADV4: collision-safe verification run identity ==="

TMP_ROOT="$(mktemp -d)"
TMP_BIN="$(mktemp -d)"

cleanup() {
    rm -rf "$TMP_ROOT" "$TMP_BIN"
}
trap cleanup EXIT

# Force both allocations to observe exactly the same wall-clock second.
cat > "$TMP_BIN/date" <<'MOCK'
#!/usr/bin/env bash
if [[ "${1:-}" == "-u" && "${2:-}" == "+%Y%m%dT%H%M%SZ" ]]; then
    printf '%s\n' "20990101T000000Z"
else
    exec /usr/bin/date "$@"
fi
MOCK

chmod +x "$TMP_BIN/date"

export PATH="$TMP_BIN:$PATH"
export SISTER_REFLEXA_RUNS_ROOT="$TMP_ROOT"

RUN1="$("$ROOT/scripts/allocate_verify_run_dir.sh")"
RUN2="$("$ROOT/scripts/allocate_verify_run_dir.sh")"

echo "run 1: $RUN1"
echo "run 2: $RUN2"

if [[ "$RUN1" == "$RUN2" ]]; then
    echo "FAIL: both allocations returned the same directory"
    exit 1
fi

if [[ ! -d "$RUN1" ]]; then
    echo "FAIL: first run directory does not exist"
    exit 1
fi

if [[ ! -d "$RUN2" ]]; then
    echo "FAIL: second run directory does not exist"
    exit 1
fi

BASE1="$(basename "$RUN1")"
BASE2="$(basename "$RUN2")"

case "$BASE1" in
    20990101T000000Z-*) ;;
    *)
        echo "FAIL: first directory lost timestamp identity: $BASE1"
        exit 1
        ;;
esac

case "$BASE2" in
    20990101T000000Z-*) ;;
    *)
        echo "FAIL: second directory lost timestamp identity: $BASE2"
        exit 1
        ;;
esac

# Demonstrate isolation: bytes written to run 1 survive activity in run 2.
printf '%s\n' "historical-run-one" > "$RUN1/sentinel.txt"

HASH_BEFORE="$(sha256sum "$RUN1/sentinel.txt" | awk '{print $1}')"

printf '%s\n' "independent-run-two" > "$RUN2/sentinel.txt"

HASH_AFTER="$(sha256sum "$RUN1/sentinel.txt" | awk '{print $1}')"

if [[ "$HASH_BEFORE" != "$HASH_AFTER" ]]; then
    echo "FAIL: later run changed earlier run bytes"
    exit 1
fi

echo "PASS: same-second allocations are distinct"
echo "PASS: earlier run remains byte-identical after later allocation"
