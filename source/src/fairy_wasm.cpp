/*
  fairy_wasm.cpp — synchronous search shim for Fairy-Stockfish (xiangqi),
  modeled on ffishjs.cpp but exposing a real search binding instead of
  movegen only.

  JS contract:
    - Before initEngine(): write the NNUE net into MEMFS at
      "/xiangqi-c07e94a5c7cb.nnue".
    - initEngine(): one-time engine init (idempotent). Aborts loudly
      (Eval::NNUE::verify() exits the process) if the net fails to load;
      on success the engine prints
      "info string NNUE evaluation using xiangqi-c07e94a5c7cb.nnue enabled"
      to stdout.
    - searchJson(fen, movetimeMs, maxDepth): synchronous search, returns
      a compact JSON string:
      {"move":<string|null>,"depth":int,"nodes":int,
       "scoreCp":int|null,"mate":int|null,"pv":"...","elapsedMs":int}
      "move"/"pv" are in Fairy UCI notation (rank digits 1..10).
      scoreCp / mate mirror exactly what UCI::value() would print for the
      root score ("cp X" / "mate N", N in moves, side-to-move POV).

  Requires the FAIRY_WASM_SYNC patch to thread.cpp (see
  ../fairy-wasm-sync.patch): with no -pthread, Thread::start_searching()
  runs the search synchronously on the calling thread.
*/

#include <emscripten.h>
#include <emscripten/bind.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bitboard.h"
#include "endgame.h"
#include "evaluate.h"
#include "misc.h"
#include "piece.h"
#include "position.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "thread.h"
#include "tt.h"
#include "tune.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

namespace {

bool g_initialized = false;

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

} // namespace

void initEngine() {
    if (g_initialized)
        return;

    // Mirror main.cpp + ffishjs.cpp::initialize_stockfish()
    pieceMap.init();
    variants.init();
    UCI::init(Options);
    Tune::init();
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();

    // Option assignment order matters: each assignment fires its on_change
    // handler (variant set -> PSQT::init + NNUE re-init attempt, EvalFile ->
    // NNUE load, Threads -> ThreadPool::set + Search::init, Hash -> TT resize).
    Options["UCI_Variant"] = std::string("xiangqi");
    Options["EvalFile"] = std::string("xiangqi-c07e94a5c7cb.nnue");
    Options["Threads"] = std::string("1");
    Options["Hash"] = std::string("64");

    Search::clear(); // After threads are up (as in main.cpp)

    // Prints "info string NNUE evaluation using ... enabled" on success;
    // prints ERROR lines and exit()s if the net was not loaded.
    Eval::NNUE::verify();
    if (!Eval::useNNUE)
        throw std::runtime_error("fairy_wasm: NNUE not in use after init");

    g_initialized = true;
}

std::string searchJson(const std::string& fen, int movetimeMs, int maxDepth) {
    if (!g_initialized)
        throw std::runtime_error("fairy_wasm: searchJson called before initEngine()");

    const Variant* variant = variants.find("xiangqi")->second;

    Position pos;
    StateListPtr states(new std::deque<StateInfo>(1));
    pos.set(variant, fen, false, &states->back(), Threads.main());

    Search::LimitsType limits;
    limits.startTime = now();
    if (movetimeMs > 0)
        limits.movetime = TimePoint(movetimeMs);
    if (maxDepth > 0)
        limits.depth = maxDepth;

    const auto t0 = std::chrono::steady_clock::now();
    Threads.start_thinking(pos, states, limits, false);
    Threads.main()->wait_for_search_finished();
    const auto t1 = std::chrono::steady_clock::now();
    const long long elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    Thread* mainThread = Threads.main();
    const uint64_t nodes = Threads.nodes_searched();
    const int depth = int(mainThread->completedDepth);

    std::ostringstream out;
    out << "{";

    if (mainThread->rootMoves.empty() || mainThread->rootMoves[0].pv.empty()) {
        out << "\"move\":null,\"depth\":" << depth
            << ",\"nodes\":" << nodes
            << ",\"scoreCp\":null,\"mate\":null,\"pv\":\"\""
            << ",\"elapsedMs\":" << elapsedMs << "}";
        return out.str();
    }

    const Search::RootMove& rm = mainThread->rootMoves[0];
    Position& rootPos = mainThread->rootPos;

    // Score: mirror UCI::value() output ("cp X" / "mate N") exactly.
    std::string scoreCp = "null", mate = "null";
    const Value v = rm.score;
    if (v > -VALUE_INFINITE && v < VALUE_INFINITE) {
        std::string sv = UCI::value(v); // "cp 123" or "mate -4"
        std::istringstream ss(sv);
        std::string kind, num;
        ss >> kind >> num;
        if (kind == "cp")
            scoreCp = num;
        else if (kind == "mate")
            mate = num;
    }

    // PV string: walk a scratch copy of the root position.
    std::string pvStr;
    {
        Position p;
        StateListPtr pst(new std::deque<StateInfo>(1));
        p.set(variant, rootPos.fen(), false, &pst->back(), Threads.main());
        for (Move m : rm.pv) {
            if (!pvStr.empty())
                pvStr += ' ';
            pvStr += UCI::move(p, m);
            pst->emplace_back();
            p.do_move(m, pst->back());
        }
    }

    out << "\"move\":\"" << json_escape(UCI::move(rootPos, rm.pv[0])) << "\""
        << ",\"depth\":" << depth
        << ",\"nodes\":" << nodes
        << ",\"scoreCp\":" << scoreCp
        << ",\"mate\":" << mate
        << ",\"pv\":\"" << json_escape(pvStr) << "\""
        << ",\"elapsedMs\":" << elapsedMs << "}";
    return out.str();
}

EMSCRIPTEN_BINDINGS(fairy_wasm) {
    emscripten::function("initEngine", &initEngine);
    emscripten::function("searchJson", &searchJson);
}
