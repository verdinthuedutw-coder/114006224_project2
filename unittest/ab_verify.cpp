#include <iostream>
#include "state.hpp"
#include "policy/minimax.hpp"
#include "policy/alphabeta.hpp"
#include "policy/game_history.hpp"

static bool same_move(const Move& a, const Move& b){
    return a.first == b.first && a.second == b.second;
}

int main(){
    int fails = 0;
    for(int depth = 1; depth <= 4; depth++){
        State s;
        s.get_legal_actions();
        GameHistory h1, h2;
        SearchContext c1, c2;
        c1.params = MiniMax::default_params();
        c2.params = AlphaBeta::default_params();
        c2.params["UseQuiescence"] = "false";
        c2.params["UseQuiescence"] = "false";

        SearchResult mm = MiniMax::search(&s, depth, h1, c1);
        SearchResult ab = AlphaBeta::search(&s, depth, h2, c2);

        bool ok = (mm.score == ab.score) && same_move(mm.best_move, ab.best_move);
        std::cout << "depth=" << depth
                  << " mm_score=" << mm.score << " ab_score=" << ab.score
                  << " mm_nodes=" << mm.nodes << " ab_nodes=" << ab.nodes
                  << " " << (ok ? "PASS" : "FAIL") << "\n";
        if(!ok){
            fails++;
        }
    }
    return fails ? 1 : 0;
}
