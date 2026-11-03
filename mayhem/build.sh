#!/usr/bin/env bash
#
# mayhem/build.sh: build the fuzz harnesses and the upstream self-checking tests.
#
# Gearmulator's full build is JUCE/CMake and needs a dozen submodules. The fuzzed code (the baseLib and
# synthLib file/stream parsers) has no such dependency, so its sources are compiled directly: once with the
# sanitizers (fuzz + standalone binaries) and once with normal flags (the upstream test programs).
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

FW="$SRC/source/framework"
OUT="$SRC"
FUZZ_BUILD="$SRC/build-fuzz"
TEST_BUILD="$SRC/build-tests"

# Sources of the library the harnesses link. Only the pieces the parsers need, none of the audio/device code.
LIB_SOURCES=(
  $(ls "$FW"/baseLib/*.cpp | grep -v '_test\.cpp$')
  "$FW/synthLib/midiBufferParser.cpp"
  "$FW/synthLib/midiToSysex.cpp"
  "$FW/synthLib/sysexToMidi.cpp"
  "$FW/synthLib/midiTranslator.cpp"
  "$FW/synthLib/frontPanelSysex.cpp"
  "$FW/synthLib/vstpreset.cpp"
  "$FW/synthLib/wavReader.cpp"
  "$FW/synthLib/midi/rcpDecoder.cpp"
  "$FW/synthLib/midi/rcpTempo.cpp"
  "$FW/synthLib/midi/rcpTrack.cpp"
  "$FW/synthLib/midi/smfReader.cpp"
  "$FW/synthLib/midi/songReset.cpp"
)

# the mayhem shim stands in for the dsp56300 submodule's logger header
INCLUDES=(-I"$SRC/mayhem/shim" -I"$FW")
CXXSTD=(-std=c++17)

compile_lib_object() {
  local src="$1" rel
  rel="${src#"$FW"/}"
  # shellcheck disable=SC2086
  "$CXX" -std=c++17 -I"$SRC/mayhem/shim" -I"$FW" $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link \
         -c "$src" -o "$FUZZ_BUILD/obj/${rel//\//_}.o"
}
export -f compile_lib_object
export FW SRC FUZZ_BUILD

# 1) sanitized project objects + harnesses (fuzzer-no-link so the library is covered by libFuzzer too)
rm -rf "$FUZZ_BUILD" && mkdir -p "$FUZZ_BUILD/obj"
printf '%s\n' "${LIB_SOURCES[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'compile_lib_object "$1"' _ {}
LIB_OBJS=("$FUZZ_BUILD"/obj/*.o)

# LeakSanitizer is off for every target (leaks are not what this fleet fuzzes for)
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$FUZZ_BUILD/lsan_off.o"
# libFuzzer-less run-once driver; compiled as C so its LLVMFuzzerTestOneInput reference keeps C linkage
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_BUILD/standalone_main.o"

TARGETS=(fuzz_lzo1x fuzz_smf fuzz_rcp fuzz_vstpreset fuzz_wav fuzz_sysex fuzz_midistream fuzz_frontpanel)
for t in "${TARGETS[@]}"; do
  $CXX "${CXXSTD[@]}" "${INCLUDES[@]}" -I"$SRC/mayhem/harnesses" $SANITIZER_FLAGS $DEBUG_FLAGS \
       -fsanitize=fuzzer-no-link -c "$SRC/mayhem/harnesses/$t.cpp" -o "$FUZZ_BUILD/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$FUZZ_BUILD/$t.o" "${LIB_OBJS[@]}" \
       "$FUZZ_BUILD/lsan_off.o" -ldl -lpthread -o "$OUT/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$FUZZ_BUILD/$t.o" "$FUZZ_BUILD/standalone_main.o" "${LIB_OBJS[@]}" \
       "$FUZZ_BUILD/lsan_off.o" -ldl -lpthread -o "$OUT/$t-standalone"
done

# 2) upstream's self-checking test programs, NORMAL flags (no sanitizers, no -gdwarf-3), for mayhem/test.sh
rm -rf "$TEST_BUILD" && mkdir -p "$TEST_BUILD"
TESTS=(
  "baseLib/adler32_test.cpp"
  "baseLib/lzo1x_test.cpp"
  "baseLib/rc4_test.cpp"
  "synthLib/midiBufferParser_test.cpp"
)
BASE_SOURCES=($(ls "$FW"/baseLib/*.cpp | grep -v '_test\.cpp$'))
for t in "${TESTS[@]}"; do
  name="$(basename "${t%.cpp}")"
  extra=()
  case "$t" in
    synthLib/midiBufferParser_test.cpp) extra=("$FW/synthLib/midiBufferParser.cpp") ;;
  esac
  $CXX "${CXXSTD[@]}" "${INCLUDES[@]}" -I"$FW/$(dirname "$t")" -O1 $COVERAGE_FLAGS "$FW/$t" "${extra[@]}" \
       "${BASE_SOURCES[@]}" -ldl -lpthread $COVERAGE_FLAGS -o "$TEST_BUILD/$name"
done

# known-answer tests for the fuzzed parsers (mayhem/tests/parser_kat.cpp), same normal flags
$CXX "${CXXSTD[@]}" "${INCLUDES[@]}" -O1 $COVERAGE_FLAGS "$SRC/mayhem/tests/parser_kat.cpp" "${LIB_SOURCES[@]}" \
     -ldl -lpthread $COVERAGE_FLAGS -o "$TEST_BUILD/parser_kat"
