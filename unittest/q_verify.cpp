#include <iostream>
#include "state.hpp"
#include "policy/alphabeta.hpp"
#include "policy/game_history.hpp"

static SearchResult run(State* s, int depth, bool qsearch){
    GameHistory h;
    SearchContext ctx;
    ctx.params = AlphaBeta::default_params();
    ctx.params["UseQuiescence"] = qsearch ? "true" : "false";
    return AlphaBeta::search(s, depth, h, ctx);
}

/* Black to move: Rc3 x Qc4 available. */
static State* black_can_take_queen(){
    Board b{};
    for(int pl = 0; pl < 2; pl++){
        for(int r = 0; r < BOARD_H; r++){
            for(int c = 0; c < BOARD_W; c++){
                b.board[pl][r][c] = 0;
            }
        }
    }
    b.board[0][5][4] = 6;
    b.board[0][2][2] = 5; /* Q c4 */
    b.board[1][0][0] = 6;
    b.board[1][3][2] = 2; /* R c3 */
    return new State(b, 1);
}

/* White to move: Qc4 attacked; quiet king move leaves Q en prise. */
static State* white_hangs_queen(){
    Board b{};
    for(int pl = 0; pl < 2; pl++){
        for(int r = 0; r < BOARD_H; r++){
            for(int c = 0; c < BOARD_W; c++){
                b.board[pl][r][c] = 0;
            }
        }
    }
    b.board[0][5][4] = 6;
    b.board[0][2][2] = 5;
    b.board[1][0][0] = 6;
    b.board[1][3][2] = 2;
    return new State(b, 0);
}

int main(){
    State start;
    start.get_legal_actions();
    SearchResult no_q = run(&start, 3, false);
    SearchResult yes_q = run(&start, 3, true);

    std::cout << "=== Node count (startpos depth=3) ===\n";
    std::cout << "  without qsearch: " << no_q.nodes << "\n";
    std::cout << "  with qsearch:    " << yes_q.nodes
              << " (+" << (yes_q.nodes - no_q.nodes) << ")\n\n";

    State* leaf = black_can_take_queen();
    leaf->get_legal_actions();
    SearchResult leaf_no = run(leaf, 0, false);
    SearchResult leaf_yes = run(leaf, 0, true);
    std::cout << "=== Leaf: Black Rxc3 (depth=0) ===\n";
    std::cout << "  static eval:  score=" << leaf_no.score << " nodes=" << leaf_no.nodes << "\n";
    std::cout << "  qsearch:      score=" << leaf_yes.score << " nodes=" << leaf_yes.nodes << "\n";
    bool leaf_ok = leaf_yes.score > leaf_no.score + 50;
    std::cout << "  horizon fix:  " << (leaf_ok ? "PASS" : "FAIL") << "\n\n";

    State* tac = white_hangs_queen();
    tac->get_legal_actions();
    SearchResult tac_no = run(tac, 2, false);
    SearchResult tac_yes = run(tac, 2, true);
    std::cout << "=== Root: W must save Q (depth=2) ===\n";
    std::cout << "  no qsearch: score=" << tac_no.score << " nodes=" << tac_no.nodes << "\n";
    std::cout << "  with qsearch: score=" << tac_yes.score << " nodes=" << tac_yes.nodes << "\n";

    delete leaf;
    delete tac;
    return leaf_ok ? 0 : 1;
}
