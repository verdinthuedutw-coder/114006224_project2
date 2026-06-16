#include <algorithm>
#include <vector>
#include "state.hpp"
#include "config.hpp"
#include "alphabeta.hpp"

namespace {

constexpr int MAX_PLY = 64;
constexpr int SQ_N = BOARD_H * BOARD_W;

Move killers[MAX_PLY][2];
int history_heur[7][SQ_N];

void clear_search_heuristics(){
    for(int p = 0; p < MAX_PLY; p++){
        killers[p][0] = Move();
        killers[p][1] = Move();
    }
    for(int t = 0; t < 7; t++){
        for(int s = 0; s < SQ_N; s++){
            history_heur[t][s] = 0;
        }
    }
}

static int sq_index(int r, int c){
    return r * BOARD_W + c;
}

static bool is_promotion(const State* state, const Move& m){
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece != 1){
        return false;
    }
    int tr = (int)m.second.first;
    return tr == 0 || tr == BOARD_H - 1;
}

static bool is_capture(const State* state, const Move& m){
    int opp = 1 - state->player;
    return state->piece_at(opp, (int)m.second.first, (int)m.second.second) != 0;
}

static int mvv_lva_score(const State* state, const Move& m){
    int attacker = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    int victim = state->piece_at(1 - state->player, (int)m.second.first, (int)m.second.second);
    if(victim == 0){
        return 0;
    }
    return PIECE_VALUES[victim] * 512 - PIECE_VALUES[attacker];
}

static int promotion_order_score(const State* state, const Move& m){
    if(!is_promotion(state, m)){
        return 0;
    }
    int tr = (int)m.second.first;
    int to_back_rank = (tr == 0 || tr == BOARD_H - 1) ? 512 : 0;
    return 9500000 + to_back_rank + mvv_lva_score(state, m);
}

static int move_order_score(
    const State* state,
    const Move& m,
    int ply,
    const Move& pv_move
){
    if(m == pv_move){
        return 10000000;
    }
    int promo = promotion_order_score(state, m);
    if(promo > 0){
        return promo;
    }
    int cap = mvv_lva_score(state, m);
    if(cap > 0){
        return 8500000 + cap;
    }
    if(m == killers[ply][0]){
        return 7500000;
    }
    if(m == killers[ply][1]){
        return 7400000;
    }
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece >= 1 && piece <= 6){
        return history_heur[piece][sq_index((int)m.second.first, (int)m.second.second)];
    }
    return 0;
}

static void order_moves(
    const State* state,
    std::vector<Move>& moves,
    int ply,
    const Move& pv_move
){
    std::stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return move_order_score(state, a, ply, pv_move) > move_order_score(state, b, ply, pv_move);
    });
}

static void store_killer(int ply, const Move& m){
    if(killers[ply][0] == m){
        return;
    }
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = m;
}

static void bump_history(const State* state, const Move& m, int depth){
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece >= 1 && piece <= 6){
        int idx = sq_index((int)m.second.first, (int)m.second.second);
        history_heur[piece][idx] += depth * depth + depth;
        if(history_heur[piece][idx] > 100000){
            history_heur[piece][idx] = 100000;
        }
    }
}

static bool is_tactical(const State* state, const Move& m){
    return is_capture(state, m) || is_promotion(state, m);
}

static void collect_tactical(const State* state, std::vector<Move>& out){
    out.clear();
    for(const Move& m : state->legal_actions){
        if(is_tactical(state, m)){
            out.push_back(m);
        }
    }
}

int quiescence(
    State* state,
    int alpha,
    int beta,
    int ply,
    int qdepth,
    SearchContext& ctx,
    const ABParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }
    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, nullptr);
    if(stand_pat >= beta){
        return beta;
    }
    if(stand_pat > alpha){
        alpha = stand_pat;
    }
    if(qdepth >= p.quiescence_depth){
        return alpha;
    }

    std::vector<Move> tactical;
    collect_tactical(state, tactical);
    if(tactical.empty()){
        return alpha;
    }
    order_moves(state, tactical, ply, Move());

    for(const Move& action : tactical){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        if(next->legal_actions.empty() && next->game_state == UNKNOWN){
            next->get_legal_actions();
        }
        int raw = quiescence(next, -beta, -alpha, ply + 1, qdepth + 1, ctx, p);
        int score = same ? raw : -raw;
        delete next;

        if(score >= beta){
            return beta;
        }
        if(score > alpha){
            alpha = score;
        }
    }
    return alpha;
}

int ab_search(
    State* state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const ABParams& p,
    const Move& pv_move,
    Move& best_out
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    if(depth <= 0){
        int score = p.use_quiescence
            ? quiescence(state, alpha, beta, ply, 0, ctx, p)
            : state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
        history.pop(state->hash());
        best_out = Move();
        return score;
    }

    auto moves = state->legal_actions;
    order_moves(state, moves, ply, pv_move);

    OrderStats* st = ctx.order_stats;
    if(st && !moves.empty()){
        st->nodes++;
    }

    int best_score = M_MAX;
    Move best_move;
    Move child_pv;
    bool has_move = false;
    bool first = true;

    for(const Move& action : moves){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        Move child_best;
        Move next_pv = (action == pv_move) ? child_pv : Move();
        int raw = ab_search(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p, next_pv, child_best);
        int score = same ? raw : -raw;
        delete next;

        if(score > best_score){
            best_score = score;
            best_move = action;
            child_pv = child_best;
            has_move = true;
        }
        if(score > alpha){
            alpha = score;
            if(!is_capture(state, action) && !is_promotion(state, action)){
                store_killer(ply, action);
                bump_history(state, action, depth);
            }
        }
        if(alpha >= beta){
            if(st && first){
                st->ab_first_fail_high++;
                st->first_move_is_best++;
            }
            break;
        }
        first = false;
    }

    history.pop(state->hash());
    if(st && !moves.empty() && !first && has_move && best_move == moves[0]){
        st->first_move_is_best++;
    }
    if(!has_move){
        best_out = Move();
        return best_score;
    }
    best_out = best_move;
    return best_score;
}

} // namespace

SearchResult AlphaBeta::search(
    State* state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    clear_search_heuristics();
    ABParams p = ABParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()){
        state->get_legal_actions();
    }

    if(depth <= 0){
        result.score = p.use_quiescence
            ? quiescence(state, M_MAX, P_MAX, 0, 0, ctx, p)
            : state->evaluate(p.use_kp_eval, p.use_eval_mobility, nullptr);
        result.nodes = ctx.nodes;
        result.seldepth = ctx.seldepth;
        if(!state->legal_actions.empty()){
            result.best_move = state->legal_actions[0];
            result.pv = {result.best_move};
        }
        return result;
    }

    int best_score = M_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    int alpha = M_MAX;
    int beta = P_MAX;
    Move pv_move;

    if(!state->legal_actions.empty()){
        result.best_move = state->legal_actions[0];
    }

    auto root_moves = state->legal_actions;
    order_moves(state, root_moves, 0, Move());

    for(const Move& action : root_moves){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        Move child_best;
        int raw = ab_search(next, depth - 1, -beta, -alpha, history, 1, ctx, p, pv_move, child_best);
        int score = same ? raw : -raw;
        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
            pv_move = action;
            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update(
                    {result.best_move, best_score, depth, move_index + 1, total_moves}
                );
            }
        }
        if(score > alpha){
            alpha = score;
        }
        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    if(!state->legal_actions.empty()){
        result.pv = {result.best_move};
    }
    return result;
}

ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
        {"UseQuiescence", "true"},
        {"QuiescenceDepth", "10"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
        {"UseQuiescence", ParamDef::CHECK, "true"},
        {"QuiescenceDepth", ParamDef::SPIN, "10", 2, 10},
    };
}
