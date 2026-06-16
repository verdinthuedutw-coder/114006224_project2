/* Move-ordering statistics for AlphaBeta and PVS. */
#include <cstdlib>
#include <iostream>
#include <vector>

#include "../src/games/minichess/state.hpp"
#include "../src/policy/alphabeta.hpp"
#include "../src/policy/pvs.hpp"
#include "../src/policy/game_history.hpp"

static State* play_random(int n_moves, unsigned seed){
    std::srand(seed);
    State* state = new State();
    state->get_legal_actions();
    for(int i = 0; i < n_moves; i++){
        if(state->game_state == WIN || state->game_state == DRAW){
            break;
        }
        if(state->legal_actions.empty()){
            break;
        }
        int idx = std::rand() % (int)state->legal_actions.size();
        State* next = state->next_state(state->legal_actions[idx]);
        delete state;
        state = next;
        state->get_legal_actions();
    }
    return state;
}

static OrderStats run_algo(
    const char* name,
    SearchResult (*search_fn)(State*, int, GameHistory&, SearchContext&),
    ParamMap params,
    State* state,
    int depth
){
    GameHistory history;
    SearchContext ctx;
    ctx.params = params;
    OrderStats stats;
    ctx.order_stats = &stats;
    search_fn(state, depth, history, ctx);
    std::cout << name << " depth=" << depth
              << " nodes=" << stats.nodes
              << " first_best=" << (100.0 * stats.first_move_is_best / (stats.nodes ? stats.nodes : 1)) << "%"
              << " ab_first_fail_high=" << (100.0 * stats.ab_first_fail_high / (stats.nodes ? stats.nodes : 1)) << "%"
              << " pvs_research=" << (100.0 * stats.pvs_researches / (stats.pvs_non_first_moves ? stats.pvs_non_first_moves : 1)) << "%"
              << "\n";
    return stats;
}

static OrderStats add(const OrderStats& a, const OrderStats& b){
    OrderStats s;
    s.nodes = a.nodes + b.nodes;
    s.first_move_is_best = a.first_move_is_best + b.first_move_is_best;
    s.ab_first_fail_high = a.ab_first_fail_high + b.ab_first_fail_high;
    s.pvs_non_first_moves = a.pvs_non_first_moves + b.pvs_non_first_moves;
    s.pvs_researches = a.pvs_researches + b.pvs_researches;
    return s;
}

static void print_summary(const char* label, const OrderStats& s){
    std::cout << label
              << " first_best=" << (100.0 * s.first_move_is_best / (s.nodes ? s.nodes : 1)) << "%"
              << " ab_first_fail_high=" << (100.0 * s.ab_first_fail_high / (s.nodes ? s.nodes : 1)) << "%"
              << " pvs_research=" << (100.0 * s.pvs_researches / (s.pvs_non_first_moves ? s.pvs_non_first_moves : 1)) << "%"
              << "\n";
}

int main(){
    constexpr int DEPTH = 6;
    std::vector<State*> positions;
    positions.push_back(new State());
    positions.back()->get_legal_actions();
    for(unsigned seed : {1u, 7u, 42u, 99u}){
        positions.push_back(play_random(8, seed));
    }

    auto ab_params = AlphaBeta::default_params();
    ab_params["UseQuiescence"] = "false";
    auto pvs_params = PVS::default_params();
    pvs_params["UseQuiescence"] = "false";

    OrderStats ab_total{}, pvs_total{};
    for(State* pos : positions){
        State copy(*pos);
        copy.get_legal_actions();
        ab_total = add(ab_total, run_algo("AlphaBeta", AlphaBeta::search, ab_params, &copy, DEPTH));
        copy = State(*pos);
        copy.get_legal_actions();
        pvs_total = add(pvs_total, run_algo("PVS", PVS::search, pvs_params, &copy, DEPTH));
    }

    std::cout << "TOTAL\n";
    print_summary("AlphaBeta", ab_total);
    print_summary("PVS", pvs_total);

    for(State* pos : positions){
        delete pos;
    }
    return 0;
}
