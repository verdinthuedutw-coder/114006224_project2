#pragma once
#include "alphabeta.hpp"

class Submission{
public:
    static SearchResult search(State* s, int d, GameHistory& h, SearchContext& c){
        return AlphaBeta::search(s, d, h, c);
    }
    static ParamMap default_params(){
        auto p = AlphaBeta::default_params();
        p["QuiescenceDepth"] = "10";
        return p;
    }
    static std::vector<ParamDef> param_defs(){ return AlphaBeta::param_defs(); }
};
