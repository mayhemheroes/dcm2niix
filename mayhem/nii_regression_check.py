#!/usr/bin/env python3
"""nii_regression_check.py — behavioral oracle for a dcm2niix regression submodule.

Compares a produced Out/ tree against the committed Ref/ tree of a dcm_qa* submodule, the same
pairing upstream's batch.sh checks with `diff -x '.*' -br Ref Out`. It asserts REAL behavior:

  * every Ref output must exist in Out (a no-op / exit(0) build produces nothing -> FAIL);
  * .json / .bval / .bvec / .txt and every non-NIfTI file: byte-identical (after dropping the two
    keys upstream itself ignores, ConversionSoftwareVersion and BidsGuess);
  * .nii: the VOXEL payload and every non-affine header byte must be byte-identical; the 19 affine
    floats (quatern_b/c/d, qoffset_x/y/z, srow_x/y/z) may differ only within float32 rounding
    (rel 1e-4 / abs 1e-3).

The affine tolerance exists because the NIfTI spatial transform is computed in float32 and its low
mantissa bits depend on the compiler's FP contraction/vectorization; the committed Ref .nii files
were generated on upstream's reference toolchain. Voxels + JSON/bval/bvec are still asserted
byte-exact, so this is NOT a weakened "did it run" check — a sabotaged converter fails loudly.

Exit 0 iff every Ref file matches (identical or affine-tolerant). Prints a per-suite summary.
"""
import json
import os
import struct
import sys

# NIfTI-1 header: the affine/orientation floats live at these byte offsets (little-endian float32).
QUAT_OFFSETS = [252, 256, 260, 264, 268, 272]          # quatern_b/c/d, qoffset_x/y/z
SROW_OFFSETS = [280 + 4 * i for i in range(12)]         # srow_x[4], srow_y[4], srow_z[4]
FLOAT_OFFSETS = QUAT_OFFSETS + SROW_OFFSETS
FLOAT_BYTES = set()
for _o in FLOAT_OFFSETS:
    FLOAT_BYTES.update(range(_o, _o + 4))

REL_TOL = 1e-4
ABS_TOL = 1e-3

IGNORE_JSON_KEYS = {"ConversionSoftwareVersion", "BidsGuess"}


def close(a, b):
    return abs(a - b) <= (ABS_TOL + REL_TOL * max(abs(a), abs(b)))


def cmp_nii(ref, out):
    """Return (ok, tolerated, detail). tolerated=True if it matched only via affine tolerance."""
    if len(ref) != len(out):
        return False, False, "size %d != %d" % (len(ref), len(out))
    if len(ref) < 352:
        return (ref == out), False, "short header"
    # Non-affine bytes (header + voxels) must be identical.
    tolerated = False
    for i in range(len(ref)):
        if ref[i] == out[i]:
            continue
        if i < 352 and i in FLOAT_BYTES:
            continue  # checked field-wise below
        return False, False, "byte %d differs (voxel/non-affine header)" % i
    # Affine floats: within float32 rounding.
    for off in FLOAT_OFFSETS:
        a = struct.unpack_from("<f", ref, off)[0]
        b = struct.unpack_from("<f", out, off)[0]
        if a != b:
            if not close(a, b):
                return False, False, "affine@%d %.9g vs %.9g out of tol" % (off, a, b)
            tolerated = True
    return True, tolerated, "affine-tolerant" if tolerated else "identical"


def load_json_norm(b):
    try:
        d = json.loads(b.decode("utf-8"))
    except Exception:
        return None
    for k in IGNORE_JSON_KEYS:
        d.pop(k, None)
    return json.dumps(d, sort_keys=True)


def cmp_other(ref, out):
    return ref == out


def main():
    refdir, outdir = sys.argv[1], sys.argv[2]
    identical = tolerated = failed = missing = 0
    tol_files, fail_files = [], []
    for root, _dirs, files in os.walk(refdir):
        for name in files:
            if name.startswith("."):
                continue
            rp = os.path.join(root, name)
            rel = os.path.relpath(rp, refdir)
            op = os.path.join(outdir, rel)
            if not os.path.exists(op):
                missing += 1
                fail_files.append(rel + " (missing in Out)")
                continue
            with open(rp, "rb") as f:
                rb = f.read()
            with open(op, "rb") as f:
                ob = f.read()
            if name.endswith(".nii"):
                ok, tol, detail = cmp_nii(rb, ob)
            elif name.endswith(".json"):
                nr, no = load_json_norm(rb), load_json_norm(ob)
                ok, tol, detail = (nr is not None and nr == no), False, "json"
            else:
                ok, tol, detail = cmp_other(rb, ob), False, "bytes"
            if not ok:
                failed += 1
                fail_files.append("%s (%s)" % (rel, detail))
            elif tol:
                tolerated += 1
                tol_files.append(rel)
            else:
                identical += 1
    total = identical + tolerated + failed + missing
    print("  files: %d  identical: %d  affine-tolerant: %d  failed: %d  missing: %d"
          % (total, identical, tolerated, failed, missing))
    for f in tol_files:
        print("    ~ tolerated (affine float32 rounding): %s" % f)
    for f in fail_files:
        print("    ! FAIL: %s" % f)
    return 1 if (failed or missing) else 0


if __name__ == "__main__":
    sys.exit(main())
