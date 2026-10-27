#!/bin/bash
# Build Fairy-Stockfish (xiangqi) as a single-threaded WASM module with the
# (staging-snapshot copy: source lives in ./source/src — see README.md)
# synchronous search shim (src/fairy_wasm.cpp).
#
# Recipe adapted from src/Makefile_js with ffishjs.cpp replaced by
# fairy_wasm.cpp, plus -DFAIRY_WASM_SYNC (see fairy-wasm-sync.patch) and
# MODULARIZE output for Node/web embedding.
#
# Deviations from the ffish.js Makefile_js era flags (emscripten 4.x):
#   -s TOTAL_MEMORY=32MB / WASM_MEM_MAX  ->  -s INITIAL_MEMORY / MAXIMUM_MEMORY
#   output name fairy-wasm.js (MODULARIZE factory "FairyWasm")
#   -DNO_THREADS DROPPED: in this tree it guards only the do_move() node
#   counter (position.cpp:1552) and one prefetch — ffish did not need node
#   counts, this shim's API does. Threading itself is handled by
#   -DFAIRY_WASM_SYNC (fairy-wasm-sync.patch), not by NO_THREADS.
#   -s STACK_SIZE=16777216 added: Emscripten's default WASM stack overflows
#   in deep search recursion (native search threads get 8 MB); symptom was
#   a wasm "table index is out of bounds" trap around depth 4.
#   -msimd128 added: lets the NNUE use wasm SIMD instead of scalar code.
#   Verified bit-identical search results (same nodes/PV/scores) at
#   ~1.6x the NPS (87-108k vs 50-66k) under Node.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/source/src"
OUT="$HERE/dist"
EMSDK="${EMSDK:-$HOME/workspace/tools/emsdk-4}"
export EM_CONFIG="$EMSDK/.emscripten"
EMXX="$EMSDK/upstream/emscripten/em++"

mkdir -p "$OUT"

# Makefile_js SRCS, ffishjs.cpp -> fairy_wasm.cpp
SRCS=(fairy_wasm.cpp benchmark.cpp bitbase.cpp bitboard.cpp endgame.cpp evaluate.cpp
material.cpp misc.cpp movegen.cpp movepick.cpp pawns.cpp position.cpp psqt.cpp
search.cpp thread.cpp timeman.cpp tt.cpp uci.cpp ucioption.cpp tune.cpp syzygy/tbprobe.cpp
nnue/evaluate_nnue.cpp nnue/features/half_ka_v2.cpp
partner.cpp parser.cpp piece.cpp variant.cpp xboard.cpp
nnue/features/half_ka_v2_variants.cpp)

CXXFLAGS=(--bind -DNNUE_EMBEDDING_OFF -DFAIRY_WASM_SYNC
-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -std=c++17 -O3 -DNDEBUG -msimd128)

LDFLAGS=(-s MODULARIZE=1 -s EXPORT_NAME=FairyWasm -s ENVIRONMENT=web,worker,node
-s "EXPORTED_RUNTIME_METHODS=['FS']" -s ALLOW_MEMORY_GROWTH=1
-s INITIAL_MEMORY=134217728 -s MAXIMUM_MEMORY=1073741824 -s ASSERTIONS=0 -s SAFE_HEAP=0
-s STACK_SIZE=16777216)

cd "$SRC"
"$EMXX" "${CXXFLAGS[@]}" "${SRCS[@]}" "${LDFLAGS[@]}" -o "$OUT/fairy-wasm.js"
echo "built: $OUT/fairy-wasm.js + fairy-wasm.wasm"
ls -la "$OUT"
