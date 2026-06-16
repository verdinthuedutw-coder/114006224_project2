/* Benchmark minimax nodes/sec at fixed depth (eval cost dominates leaves). */
#include <chrono>
#include <iostream>

#include "../src/games/minichess/state.hpp"
#include "../src/policy/minimax.hpp"
#include "../src/policy/game_history.hpp"

int main(){
    State state;
    state.get_legal_actions();
    GameHistory history;
    SearchContext ctx;
    ctx.params = MiniMax::default_params();

    constexpr int DEPTH = 5;
    auto t0 = std::chrono::high_resolution_clock::now();
    SearchResult r = MiniMax::search(&state, DEPTH, history, ctx);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "depth=" << DEPTH
              << " nodes=" << r.nodes
              << " score=" << r.score
              << " time_ms=" << ms
              << " nps=" << (ms > 0 ? (r.nodes * 1000.0 / ms) : 0)
              << " best=" << r.best_move.first.first << "," << r.best_move.first.second
              << "->" << r.best_move.second.first << "," << r.best_move.second.second
              << "\n";
    return 0;
}
