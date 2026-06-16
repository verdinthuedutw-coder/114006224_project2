#include <algorithm>
#include <vector>
#include "state.hpp"
#include "config.hpp"
#include "pvs.hpp"

namespace {

constexpr int MAX_PLY = 64;
constexpr int MAX_QDEPTH = 10;
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

static int sq_index(int r, int c){ return r * BOARD_W + c; }

static bool is_promotion(const State* state, const Move& m){
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece != 1) return false;
    int tr = (int)m.second.first;
    return tr == 0 || tr == BOARD_H - 1;
}

static bool is_capture(const State* state, const Move& m){
    return state->piece_at(1 - state->player, (int)m.second.first, (int)m.second.second) != 0;
}

static int mvv_lva_score(const State* state, const Move& m){
    int attacker = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    int victim = state->piece_at(1 - state->player, (int)m.second.first, (int)m.second.second);
    if(victim == 0) return 0;
    return PIECE_VALUES[victim] * 64 - PIECE_VALUES[attacker];
}

static int move_order_score(const State* state, const Move& m, int ply, const Move& pv_move){
    if(m == pv_move) return 10000000;
    if(is_promotion(state, m)) return 9000000 + mvv_lva_score(state, m);
    int cap = mvv_lva_score(state, m);
    if(cap > 0) return 8000000 + cap;
    if(m == killers[ply][0]) return 7000000;
    if(m == killers[ply][1]) return 6000000;
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece >= 1 && piece <= 6){
        return history_heur[piece][sq_index((int)m.second.first, (int)m.second.second)];
    }
    return 0;
}

static void order_moves(const State* state, std::vector<Move>& moves, int ply, const Move& pv_move){
    std::stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return move_order_score(state, a, ply, pv_move) > move_order_score(state, b, ply, pv_move);
    });
}

static void store_killer(int ply, const Move& m){
    if(killers[ply][0] == m) return;
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = m;
}

static void bump_history(const State* state, const Move& m, int depth){
    int piece = state->piece_at(state->player, (int)m.first.first, (int)m.first.second);
    if(piece >= 1 && piece <= 6){
        history_heur[piece][sq_index((int)m.second.first, (int)m.second.second)] += depth * depth;
    }
}

static bool is_tactical(const State* state, const Move& m){
    return is_capture(state, m) || is_promotion(state, m);
}

static void collect_tactical(const State* state, std::vector<Move>& out){
    out.clear();
    for(const Move& m : state->legal_actions){
        if(is_tactical(state, m)) out.push_back(m);
    }
}

int quiescence(State* state, int alpha, int beta, int ply, int qdepth, SearchContext& ctx, const PVSParams& p){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;
    if(state->legal_actions.empty() && state->game_state == UNKNOWN) state->get_legal_actions();
    if(state->game_state == WIN) return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, nullptr);
    if(stand_pat >= beta) return beta;
    if(stand_pat > alpha) alpha = stand_pat;
    if(qdepth >= MAX_QDEPTH) return alpha;

    std::vector<Move> tactical;
    collect_tactical(state, tactical);
    if(tactical.empty()) return alpha;
    order_moves(state, tactical, ply, Move());

    for(const Move& action : tactical){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        if(next->legal_actions.empty() && next->game_state == UNKNOWN) next->get_legal_actions();
        int raw = quiescence(next, -beta, -alpha, ply + 1, qdepth + 1, ctx, p);
        int score = same ? raw : -raw;
        delete next;
        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }
    return alpha;
}

int pvs_search(
    State* state, int depth, int alpha, int beta,
    GameHistory& history, int ply, SearchContext& ctx,
    const PVSParams& p, const Move& pv_move
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;
    if(state->legal_actions.empty() && state->game_state == UNKNOWN) state->get_legal_actions();
    if(state->game_state == WIN) return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    int rep_score;
    if(state->check_repetition(history, rep_score)) return rep_score;
    history.push(state->hash());

    if(depth <= 0){
        int score = p.use_quiescence
            ? quiescence(state, alpha, beta, ply, 0, ctx, p)
            : state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
        history.pop(state->hash());
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
    bool first = true;

    for(size_t i = 0; i < moves.size(); i++){
        const Move& action = moves[i];
        State* next = state->next_state(action);
        int score;
        if(i == 0){
            score = -pvs_search(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p, Move());
        }else{
            if(st){
                st->pvs_non_first_moves++;
            }
            score = -pvs_search(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p, Move());
            if(score > alpha && score < beta){
                if(st){
                    st->pvs_researches++;
                }
                score = -pvs_search(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p, Move());
            }
        }
        delete next;

        if(score > best_score){
            best_score = score;
            best_move = action;
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
                st->first_move_is_best++;
            }
            break;
        }
        first = false;
    }

    if(st && !moves.empty() && !first && best_move == moves[0]){
        st->first_move_is_best++;
    }

    history.pop(state->hash());
    return best_score;
}

} // namespace

SearchResult PVS::search(State* state, int depth, GameHistory& history, SearchContext& ctx){
    ctx.reset();
    clear_search_heuristics();
    PVSParams p = PVSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()) state->get_legal_actions();

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

    int best_score = M_MAX, alpha = M_MAX, beta = P_MAX;
    int move_index = 0, total_moves = (int)state->legal_actions.size();
    Move pv_move;
    if(!state->legal_actions.empty()) result.best_move = state->legal_actions[0];

    auto root_moves = state->legal_actions;
    order_moves(state, root_moves, 0, Move());
    OrderStats* st = ctx.order_stats;

    for(size_t i = 0; i < root_moves.size(); i++){
        const Move& action = root_moves[i];
        State* next = state->next_state(action);
        int score;
        if(i == 0){
            score = -pvs_search(next, depth - 1, -beta, -alpha, history, 1, ctx, p, pv_move);
        }else{
            if(st){
                st->pvs_non_first_moves++;
            }
            score = -pvs_search(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p, pv_move);
            if(score > alpha && score < beta){
                if(st){
                    st->pvs_researches++;
                }
                score = -pvs_search(next, depth - 1, -beta, -alpha, history, 1, ctx, p, pv_move);
            }
        }
        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
            pv_move = action;
            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }
        if(score > alpha) alpha = score;
        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    if(!state->legal_actions.empty()) result.pv = {result.best_move};
    return result;
}

ParamMap PVS::default_params(){
    return {{"UseKPEval","true"},{"UseEvalMobility","true"},{"ReportPartial","true"},{"UseQuiescence","true"}};
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
        {"UseQuiescence", ParamDef::CHECK, "true"},
    };
}
