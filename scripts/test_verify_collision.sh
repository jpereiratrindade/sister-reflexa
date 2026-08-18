#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# T_ADV4: collision-safe verification runs test

set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "=== T_ADV4: collision-safe verification runs ==="

# We need to simulate two runs initiated during the SAME second.
# We will create a fake 'date' command that always returns the same string.
TMP_BIN=$(mktemp -d)
cat << 'MOCK' > "$TMP_BIN/date"
#!/usr/bin/env bash
if [ "$1" == "-u" ] && [ "$2" == "+%Y%m%dT%H%M%SZ" ]; then
    echo "20990101T000000Z"
else
    /usr/bin/date "$@"
fi
MOCK
chmod +x "$TMP_BIN/date"

# Export path so our fake date is used
export PATH="$TMP_BIN:$PATH"

# Clean any existing 2099 runs
rm -rf docs/experiments/evidence/bootstrap/runs/2099*

# Run twice sequentially (date is mocked to the exact same second)
./scripts/verify_mvp0.sh > /dev/null 2>&1
./scripts/verify_mvp0.sh > /dev/null 2>&1

rm -rf "$TMP_BIN"

# Get the two directories created for our fake date
LATEST_RUNS=$(ls -1d docs/experiments/evidence/bootstrap/runs/20990101T000000Z* 2>/dev/null || true)
RUN_COUNT=$(echo "$LATEST_RUNS" | grep -v -e '^$' | wc -l)

if [ "$RUN_COUNT" -ne 2 ]; then
    echo "FAIL: expected 2 runs, found $RUN_COUNT"
    exit 1
fi

DIR1=$(echo "$LATEST_RUNS" | head -n 1)
DIR2=$(echo "$LATEST_RUNS" | tail -n 1)

if [ "$DIR1" == "$DIR2" ]; then
    echo "FAIL: collision detected! Both processes wrote to $DIR1"
    exit 1
fi

if [ ! -f "$DIR1/metadata.txt" ] || [ ! -f "$DIR2/metadata.txt" ]; then
    echo "FAIL: one or both directories are missing metadata.txt"
    exit 1
fi

echo "PASS: distinct directories created:"
echo "  - $DIR1"
echo "  - $DIR2"
exit 0
