# CotuongEngine — Fairy-Stockfish Corresponding Source

## Purpose

This snapshot is the corresponding source for the Fairy-Stockfish
engine binary served to the Xiangqi game (PacClone) at
cotuong.synerworlds.com, provided in compliance with the GNU General
Public License v3 (GPLv3). It mirrors the pattern used for the
Stockfish mirror (CoVuaEngine) and the Pikafish mirror
(CotuongEngine, for the engine code).

## Play it live

This engine powers the computer opponent in **Xiangqi (Cờ Tướng) at
SynerWorlds** — play against it free in your browser, no downloads:

- **Xiangqi:** https://cotuong.synerworlds.com
- **All SynerWorlds games:** https://synerworlds.com

## What ships in the game

- `fairy-wasm.js` (80,404 bytes) + `fairy-wasm.wasm` (857,781 bytes,
  SHA-256 eca7f67752b297685401ac2a3e495068…), built from the source in
  `source/` as described below. The binary is NOT stored in this
  snapshot; it is served from the public Supabase Storage bucket
  `fairy-engine`, path `v1/fairy-wasm.wasm`, alongside the JS glue and
  the net:
  https://zhtmoacveycpdpuilgte.supabase.co/storage/v1/object/public/fairy-engine/v1/
- The NNUE network `xiangqi-c07e94a5c7cb.nnue` (11,261,932 bytes,
  SHA-256 c07e94a5c7cbeae443ed79a8fa412875d833a7f8e04333815e39729c59d52e11),
  the official xiangqi net from the Fairy-Stockfish project
  (https://github.com/fairy-stockfish/Fairy-Stockfish-NNUE), trained
  by the Pikafish developers and released under **CC0** — the Pikafish
  team's licence page states verbatim that the weights they train for
  the xiangqi variant of Fairy-Stockfish are CC0 and "not constrained"
  by the Pikafish weights licence. No permission is required for any
  use of the net, commercial or otherwise.

## Provenance

- Engine: Fairy-Stockfish, upstream
  https://github.com/fairy-stockfish/Fairy-Stockfish
- Exact upstream commit: `9f778da667f6e07dae1e85d3e2ea204fc6dee94d`
  (master, 2026-09-23; self-identifies as "Fairy-Stockfish 271026 LB").
- The `source/` tree is that commit's tree with two local changes,
  both fully contained in `fairy-wasm-sync.patch`:
  1. `source/src/fairy_wasm.cpp` (NEW) — a small Embind shim exposing
     `initEngine()` and a synchronous `searchJson(fen, movetimeMs,
     maxDepth)`. It replaces `ffishjs.cpp` in the build (ffish.js is a
     movegen library and exposes no search). The shim drives the
     engine through the same calls UCI makes internally
     (`Threads.start_thinking` + `wait_for_search_finished`).
  2. `fairy-wasm-sync.patch` — guarded by `#ifdef FAIRY_WASM_SYNC`:
     makes `Thread::start_searching()` run the search synchronously
     on the calling thread (a no-`-pthread` WASM build cannot spawn
     `std::thread`), and makes `TranspositionTable::clear()` memset
     inline instead of spawning helper threads. Behaviour at
     Threads=1 is identical to upstream.
- Toolchain: Emscripten 4.0.15 (emsdk), `-O3`, wasm SIMD128 enabled.
  The build is bit-faithful: on test positions the WASM build returns
  identical best moves, scores and node counts to the native build of
  the same commit. Measured under Node: ~85–105k NPS (depth-16
  probes), ~58k NPS in match play at 1 s/move; in-browser ~36k NPS.
  Build + bench trail: `~/workspace/fairy-wasm/` (build.log,
  bench.log, match-wasm.ts, match-ours-results.json).

## Rebuild

```sh
./build-wasm.sh   # needs emsdk 4.0.15 at ~/workspace/tools/emsdk-4
                  # (or set EMSDK=/path/to/emsdk)
```

Outputs `dist/fairy-wasm.js` + `dist/fairy-wasm.wasm`. The net is not
built; download it from the upstream NNUE repo above and verify the
SHA-256.

## License

- Engine: GPLv3 — full text in `source/Copying.txt` (also served with
  the engine assets as `v1/Copying.txt`).
- Embedded/served NNUE network: CC0 (see above).

## Note

Local staging snapshot prepared 2026-10-27. To be pushed to a public
repo by the project owner, mirroring CoVuaEngine/CotuongEngine.
