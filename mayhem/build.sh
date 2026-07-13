#!/usr/bin/env bash
#
# mayhem/build.sh — build dcm2niix's fuzz harness(es) AND its upstream regression suite.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. The base image
# (ghcr.io/mayhemheroes/base) exports the build contract (CC/CXX/LIB_FUZZING_ENGINE/
# SANITIZER_FLAGS/DEBUG_FLAGS/STANDALONE_FUZZ_MAIN/SRC). Everything here is ADDITIVE — no
# upstream file is edited; the fuzz binary compiles the same console/ sources the batch
# converter does and calls readDICOMv() (the core DICOM parser) via mayhem/fuzz_dcm2niix.cpp.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# 0) Ensure the upstream regression submodules (dcm_qa / dcm_qa_nih / dcm_qa_uih) are present.
#    They are gitlinked in upstream; a clean CI checkout copies only the empty gitlink dirs, so
#    fetch them at IMAGE-BUILD time (network available) and bake them in. On the air-gapped
#    build.sh RE-RUN the content already exists, so this is a no-op (idempotent + offline-safe).
if [ ! -e dcm_qa/batch.sh ] || [ ! -e dcm_qa_nih/batch.sh ] || [ ! -e dcm_qa_uih/batch.sh ]; then
	git submodule update --init --recursive
fi

# ---------------------------------------------------------------------------------------------
# 1) TEST/ORACLE build — dcm2niix CLI with NORMAL flags (no sanitizer). This is the binary that
#    mayhem/test.sh runs the three regression suites against. Build from console/ (not the
#    SuperBuild) so nothing is fetched from the network. System OpenJPEG + bundled CharLS give
#    full JPEG2000 + JPEG-LS decode so the reference conversions reproduce.
# ---------------------------------------------------------------------------------------------
cmake -S console -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
	-DUSE_OPENJPEG=System -DUSE_JPEGLS=ON -DZLIB_IMPLEMENTATION=Miniz \
	-DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build -j"$MAYHEM_JOBS"
test -x build/dcm2niix

# ---------------------------------------------------------------------------------------------
# 2) FUZZ build — the SAME console/ parser sources the batch converter links, compiled with
#    $SANITIZER_FLAGS + $DEBUG_FLAGS so the fuzzed *library* (not just the harness) is
#    instrumented and carries DWARF<4 symbols. libFuzzer provides main().
# ---------------------------------------------------------------------------------------------
OPJ_CFLAGS="$(pkg-config --cflags libopenjp2)"
OPJ_LIBS="$(pkg-config --libs libopenjp2)"

# The batch converter's translation units (console/CMakeLists.txt DCM2NIIBATCH_SRCS) minus the
# CLI entrypoint, plus the harness. CharLS provides JPEG-LS; OpenJPEG (system) provides JPEG2000.
PARSER_SRCS=(
	console/nii_dicom.cpp
	console/jpg_0XC3.cpp
	console/ujpeg.cpp
	console/nifti1_io_core.cpp
	console/nii_foreign.cpp
	console/nii_ortho.cpp
	console/nii_dicom_batch.cpp
	console/cJSON.cpp
	console/base64.cpp
	console/charls/interface.cpp
	console/charls/jpegls.cpp
	console/charls/jpegmarkersegment.cpp
	console/charls/jpegstreamreader.cpp
	console/charls/jpegstreamwriter.cpp
)
FEATURE_FLAGS="-DmyEnableJPEGLS -DmyEnableJNIFTI"
CXXSTD="-std=c++14"

# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $CXXSTD $FEATURE_FLAGS \
	-Iconsole -Iconsole/charls $OPJ_CFLAGS \
	mayhem/fuzz_dcm2niix.cpp "${PARSER_SRCS[@]}" $OPJ_LIBS \
	-o /mayhem/dcm2niibatch

# ---------------------------------------------------------------------------------------------
# 3) STANDALONE reproducer — same harness linked against LLVM's run-once driver instead of the
#    fuzzing engine (one input file, natural crash, no libFuzzer runtime). Compile the C driver
#    as a C object first so its LLVMFuzzerTestOneInput ref keeps C linkage.
# ---------------------------------------------------------------------------------------------
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $CXXSTD $FEATURE_FLAGS \
	-Iconsole -Iconsole/charls $OPJ_CFLAGS \
	mayhem/fuzz_dcm2niix.cpp "${PARSER_SRCS[@]}" /tmp/standalone_main.o $OPJ_LIBS \
	-o /mayhem/dcm2niibatch-standalone

echo "build.sh: OK — /mayhem/dcm2niibatch (+ -standalone), build/bin/dcm2niix"
