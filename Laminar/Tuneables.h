#pragma once
#include <cstdint>
#include <string>

struct Tuneable
{
    std::string name;
    int value;
    int minValue;
    int maxValue;
    int step;
    Tuneable(std::string n, int v, int minv, int maxv, int s) :
            name(n), value(v), minValue(minv), maxValue(maxv), step(s)
    {
    }
    operator int() const
    {
        return value;
    }
};

extern Tuneable MAINHIST_BONUS_BASE;
extern Tuneable MAINHIST_BONUS_MULT;
extern Tuneable MAINHIST_BONUS_MAX;
extern Tuneable MAINHIST_MALUS_MULT;
extern Tuneable MAINHIST_MALUS_BASE;
extern Tuneable MAINHIST_MALUS_MAX;

extern Tuneable SE_MAINHIST_BONUS_BASE;
extern Tuneable SE_MAINHIST_BONUS_MULT;
extern Tuneable SE_MAINHIST_BONUS_MAX;

extern Tuneable CAPTHIST_BONUS_BASE;
extern Tuneable CAPTHIST_BONUS_MULT;
extern Tuneable CAPTHIST_BONUS_MAX;
extern Tuneable CAPTHIST_MALUS_BASE;
extern Tuneable CAPTHIST_MALUS_MULT;
extern Tuneable CAPTHIST_MALUS_MAX;

extern Tuneable SE_CAPTHIST_BONUS_BASE;
extern Tuneable SE_CAPTHIST_BONUS_MULT;
extern Tuneable SE_CAPTHIST_BONUS_MAX;

extern Tuneable CONTHIST_BONUS_BASE;
extern Tuneable CONTHIST_BONUS_MULT;
extern Tuneable CONTHIST_BONUS_MAX;
extern Tuneable CONTHIST_MALUS_BASE;
extern Tuneable CONTHIST_MALUS_MULT;
extern Tuneable CONTHIST_MALUS_MAX;

extern Tuneable RFP_MULTIPLIER;
extern Tuneable RFP_BASE;
extern Tuneable RFP_IMPROVING_SUB;

extern Tuneable RAZORING_MULTIPLIER;
extern Tuneable RAZORING_BASE;

extern Tuneable ASP_WINDOW_INITIAL;
extern Tuneable LMR_DIVISOR;
extern Tuneable LMR_OFFSET;

extern Tuneable SEE_PAWN_VAL;
extern Tuneable SEE_KNIGHT_VAL;
extern Tuneable SEE_BISHOP_VAL;
extern Tuneable SEE_ROOK_VAL;
extern Tuneable SEE_QUEEN_VAL;

extern Tuneable QS_SEE_MARGIN;
extern Tuneable PVS_QUIET_BASE;
extern Tuneable PVS_QUIET_MULT;
extern Tuneable PVS_NOISY_BASE;
extern Tuneable PVS_NOISY_MULT;
extern Tuneable PVS_SEE_HISTORY_DIV;

extern Tuneable PAWN_CORRHIST_MULTIPLIER;
extern Tuneable NONPAWN_CORRHIST_MULTIPLIER;
extern Tuneable MINOR_CORRHIST_MULTIPLIER;

extern Tuneable LMP_BASE;
extern Tuneable LMP_MULTIPLIER;

extern Tuneable DEXT_MARGIN;

extern Tuneable HISTORY_PRUNING_MULTIPLIER;
extern Tuneable HISTORY_PRUNING_BASE;

extern Tuneable PV_LMR_ADD;
extern Tuneable CUTNODE_LMR_ADD;
extern Tuneable TTPV_LMR_SUB;
extern Tuneable IMPROVING_LMR_SUB;
extern Tuneable CORRPLEXITY_LMR_SUB;
extern Tuneable KILLER_LMR_SUB;
extern Tuneable EVALPLEXITY_LMR_SUB;

extern Tuneable DODEEPER_MULTIPLIER;

extern Tuneable HIST_LMR_DIV;
extern Tuneable CORRPLEXITY_LMR_THRESHOLD;
extern Tuneable EVALPLEXITY_LMR_THRESHOLD;
extern Tuneable EVALPLEXITY_LMR_SCALE;

extern Tuneable NMP_BETA_OFFSET;
extern Tuneable NMP_EVAL_DIVISOR;

extern Tuneable SCALING_KNIGHT_VAL;
extern Tuneable SCALING_BISHOP_VAL;
extern Tuneable SCALING_ROOK_VAL;
extern Tuneable SCALING_QUEEN_VAL;
extern Tuneable SCALING_BASE;

extern Tuneable QS_SEE_ORDERING;
extern Tuneable PVS_SEE_ORDERING;

extern Tuneable CAPT_LMR_DIVISOR;

extern Tuneable CAPT_LMR_OFFSET;

constexpr int ASP_WINDOW_MAX = 300;
constexpr int MAX_HISTORY = 16384;
constexpr int MAX_CONTHIST = 16384;
constexpr int RFP_MAX_DEPTH = 6;
constexpr int MAX_NMP_EVAL_R = 3;
constexpr int MIN_LMR_DEPTH = 3;

extern Tuneable* AllTuneables[];

extern Tuneable* SEEPieceValues[];
extern int AllTuneablesCount;