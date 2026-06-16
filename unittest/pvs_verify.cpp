#include <iostream>
#include "state.hpp"
#include "policy/alphabeta.hpp"
#include "policy/pvs.hpp"
#include "policy/game_history.hpp"

static bool same_move(const Move& a, const Move& b){
    return a.first == b.first && a.second == b.second;
}

int main(){
    int fails = 0;
    std::cout << "depth  ab_nodes  pvs_nodes  ratio   status\n";
    for(int depth = 1; depth <= 4; depth++){
        State s;
        s.get_legal_actions();
        GameHistory h1, h2;
        SearchContext c1, c2;
        c1.params = AlphaBeta::default_params();
        c2.params = PVS::default_params();

        SearchResult ab = AlphaBeta::search(&s, depth, h1, c1);
        SearchResult pv = PVS::search(&s, depth, h2, c2);
        bool ok = (ab.score == pv.score) && same_move(ab.best_move, pv.best_move);
        double ratio = ab.nodes ? (100.0 * pv.nodes / ab.nodes) : 100.0;
        std::cout << depth << "      " << ab.nodes << "      " << pv.nodes
                  << "      " << ratio << "%   " << (ok ? "PASS" : "FAIL") << "\n";
        if(!ok) fails++;
    }
    return fails ? 1 : 0;
}
