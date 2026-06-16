#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

struct ABParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;
    bool use_quiescence = true;
    int quiescence_depth = 10;

    static ABParams from_map(const ParamMap& m){
        ABParams p;
        p.use_kp_eval       = param_bool(m, "UseKPEval", true);
        p.use_eval_mobility = param_bool(m, "UseEvalMobility", true);
        p.report_partial    = param_bool(m, "ReportPartial", true);
        p.use_quiescence    = param_bool(m, "UseQuiescence", true);
        p.quiescence_depth  = param_int(m, "QuiescenceDepth", 10);
        return p;
    }
};

class AlphaBeta{
public:
    static SearchResult search(
        State *state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );

    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};
