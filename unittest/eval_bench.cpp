/* Micro-benchmark: State::evaluate() leaf cost (no search). */
#include <chrono>
#include <iostream>
#include <cstdlib>

#include "../src/games/minichess/state.hpp"
#include "../src/games/minichess/config.hpp"

static double bench_eval(State& state, int iterations, bool kp, bool mobility){
    state.get_legal_actions();
    volatile int sink = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iterations; i++){
        sink += state.evaluate(kp, mobility, nullptr);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    if(sink == 123456789){ std::cout << sink; }
    return us / iterations;
}

static State* midgame(){
    Board b{};
    for(int p = 0; p < 2; p++){
        for(int r = 0; r < BOARD_H; r++){
            for(int c = 0; c < BOARD_W; c++){
                b.board[p][r][c] = 0;
            }
        }
    }
    b.board[0][5][4] = 6;
    b.board[0][3][0] = 2;
    b.board[0][3][2] = 1;
    b.board[1][0][0] = 6;
    b.board[1][3][3] = 3;
    b.board[1][2][2] = 1;
    return new State(b, 0);
}

int main(){
    constexpr int ITERS = 200000;
    State init;
    State* mid = midgame();

    std::cout << "=== evaluate() micro-benchmark (" << ITERS << " calls) ===\n\n";

    std::cout << std::fixed;
    std::cout.precision(3);

    auto run = [&](const char* label, State& s){
        double kp_only = bench_eval(s, ITERS, true, false);
        double kp_mob  = bench_eval(s, ITERS, true, true);
        double mat_only = bench_eval(s, ITERS, false, false);
        std::cout << label << ":\n";
        std::cout << "  KP eval (mobility off): " << kp_only << " us/call\n";
        std::cout << "  KP eval + mobility:   " << kp_mob  << " us/call\n";
        std::cout << "  Material only:        " << mat_only << " us/call\n";
        if(kp_mob > 0.0){
            std::cout << "  Mobility overhead:    "
                      << (kp_mob / kp_only) << "x slower\n";
        }
        std::cout << "\n";
    };

    run("initial_white", init);
    run("midgame_white", *mid);

    delete mid;
    return 0;
}
