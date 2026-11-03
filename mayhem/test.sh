#!/usr/bin/env bash
#
# mayhem/test.sh — RUN gearmulator's own self-checking unit tests (built by mayhem/build.sh into build-tests/).
# Each program asserts known-answer values (adler32 sums, RC4 vectors, hand-assembled LZO1X streams, MIDI byte
# streams with their expected events) and prints a summary line; that line is parsed here, so a binary that
# was neutered to exit(0) prints nothing and fails.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
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

TESTDIR="$SRC/build-tests"
PASSED=0 FAILED=0

# run_checks <name>: programs that print "<F> of <N> checks failed"
run_checks() {
  local name="$1" bin="$TESTDIR/$1" out rc f n
  if [ ! -x "$bin" ]; then echo "missing test binary $bin (build.sh bug)" >&2; FAILED=$((FAILED + 1)); return; fi
  out="$("$bin" 2>&1)"; rc=$?
  printf '%s\n' "$out"
  if [[ "$out" =~ ([0-9]+)\ of\ ([0-9]+)\ checks\ failed ]]; then
    f="${BASH_REMATCH[1]}" n="${BASH_REMATCH[2]}"
    if [ "$n" -gt 0 ]; then
      PASSED=$((PASSED + n - f)); FAILED=$((FAILED + f))
    else
      echo "$name ran no checks" >&2; FAILED=$((FAILED + 1))
    fi
    [ "$rc" -eq 0 ] || { echo "$name exited $rc" >&2; FAILED=$((FAILED + 1)); }
  else
    echo "$name printed no summary line" >&2; FAILED=$((FAILED + 1))
  fi
}

run_checks adler32_test
run_checks lzo1x_test
run_checks rc4_test
run_checks parser_kat

# midiBufferParser_test prints "midiBufferParser: all passed" or "<N> failure(s)"
bin="$TESTDIR/midiBufferParser_test"
if [ ! -x "$bin" ]; then
  echo "missing test binary $bin (build.sh bug)" >&2; FAILED=$((FAILED + 1))
else
  out="$("$bin" 2>&1)"; rc=$?
  printf '%s\n' "$out"
  if [ "$rc" -eq 0 ] && grep -qx 'midiBufferParser: all passed' <<<"$out"; then
    PASSED=$((PASSED + 1))
  else
    echo "midiBufferParser_test failed (rc=$rc)" >&2; FAILED=$((FAILED + 1))
  fi
fi

emit_ctrf "gearmulator-selfcheck" "$PASSED" "$FAILED" 0
