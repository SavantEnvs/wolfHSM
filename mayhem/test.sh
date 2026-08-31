#!/usr/bin/env bash
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER=/mayhem/wh_test
[ -x "$RUNNER" ] || { echo "MISSING: $RUNNER (build.sh should have produced it)" >&2; emit_ctrf wolfhsm 0 1; exit 1; }

# Run the test suite; capture output for behavioral check
output=$("$RUNNER" 2>&1) || rc=$?
rc=${rc:-0}

# Behavioral check: a no-op binary (exit 0) won't print "Enter unit tests"
if ! printf '%s' "$output" | grep -q "Enter unit tests"; then
    printf '%s\n' "$output"
    echo "FAIL: expected 'Enter unit tests' in output (behavioral oracle)" >&2
    emit_ctrf wolfhsm 0 1
    exit 1
fi

printf '%s\n' "$output"

if [ "$rc" -ne 0 ]; then
    emit_ctrf wolfhsm 0 1
    exit 1
fi

emit_ctrf wolfhsm 1 0
