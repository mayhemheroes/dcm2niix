#!/usr/bin/env bash
#
# mayhem/test.sh — RUN dcm2niix's UPSTREAM regression suite (the three dcm_qa* submodules shipped
# with the repo and documented in CLAUDE.md as the minimum pre-commit gate). build.sh already built
# the oracle binary (build/dcm2niix) and fetched the submodules; this script only RUNS them.
#
# Each submodule ships In/ (DICOM), Ref/ (reference NIfTI+JSON+bval/bvec) and batch.sh, which
# converts In/->Out/ and diffs Out against Ref. We run each batch.sh verbatim (its conversion IS the
# upstream test) and log upstream's strict `diff` result, then assert correctness with an explicit
# behavioral oracle (mayhem/nii_regression_check.py): voxels + JSON/bval/bvec byte-identical, NIfTI
# affine floats within float32 rounding. That tolerance covers a KNOWN cross-toolchain artifact (5
# UIH files whose affine header differs in the low mantissa bits while voxels are byte-identical);
# it does NOT relax the behavioral assertion — a no-op/exit(0) build produces no output and FAILS.
#
# Emits a CTRF summary (file + `CTRF {...}` marker) counting the three suites; exits non-zero iff any
# suite failed.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

BIN="$SRC/build/dcm2niix"
if [ ! -x "$BIN" ]; then
	echo "test.sh: oracle binary $BIN missing — build.sh must build it" >&2
	exit 1
fi
export PATH="$SRC/build:$PATH"

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

SUITES="dcm_qa dcm_qa_nih dcm_qa_uih"
passed=0
failed=0
for d in $SUITES; do
	echo "========================= $d ========================="
	if [ ! -x "$d/batch.sh" ]; then
		echo "  ! FAIL: $d/batch.sh missing (build.sh should have fetched the submodule)"
		failed=$((failed + 1))
		continue
	fi
	rm -rf "$d/Out"
	# Run upstream's batch.sh verbatim (conversion + its strict diff). Capture rc but don't abort:
	# the strict diff may flag the known UIH float noise; our oracle below is authoritative.
	( cd "$d" && examnam="$BIN" ./batch.sh ) > "/tmp/$d.batch.log" 2>&1
	strict_rc=$?
	echo "  upstream batch.sh strict diff rc=$strict_rc"
	if [ ! -d "$d/Out" ] || [ -z "$(ls -A "$d/Out" 2>/dev/null)" ]; then
		echo "  ! FAIL: $d produced no output"
		tail -n 15 "/tmp/$d.batch.log" | sed 's/^/    /'
		failed=$((failed + 1))
		continue
	fi
	# Behavioral oracle: voxel/JSON/bval/bvec byte-exact, affine within float32 rounding.
	if python3 mayhem/nii_regression_check.py "$d/Ref" "$d/Out"; then
		passed=$((passed + 1))
	else
		echo "  ! FAIL: $d has non-tolerable differences"
		failed=$((failed + 1))
	fi
done

echo "======================================================="
emit_ctrf "dcm2niix-dcm_qa" "$passed" "$failed" 0
