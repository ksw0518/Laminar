#pragma once
#include <cstdint>
constexpr int MAX_HISTORY = 16384;
constexpr int MAX_CONTHIST = 16384;

constexpr int MAINHIST_BONUS_BASE = 140;
constexpr int MAINHIST_BONUS_MULT = 420;
constexpr int MAINHIST_BONUS_MAX = 2400;

constexpr int MAINHIST_MALUS_BASE = 140;
constexpr int MAINHIST_MALUS_MULT = 420;
constexpr int MAINHIST_MALUS_MAX = 2400;

constexpr int CONTHIST_BONUS_BASE = 140;
constexpr int CONTHIST_BONUS_MULT = 420;
constexpr int CONTHIST_BONUS_MAX = 2400;

constexpr int CONTHIST_MALUS_BASE = 140;
constexpr int CONTHIST_MALUS_MULT = 420;
constexpr int CONTHIST_MALUS_MAX = 2400;

constexpr int RFP_MULTIPLIER = 80;
constexpr int RFP_BASE = 0;

constexpr int RFP_MAX_DEPTH = 4;

constexpr int ASP_WINDOW_INITIAL = 30;
constexpr int ASP_WINDOW_MAX = 300;

constexpr int LMR_DIVISOR = 236;
constexpr int LMR_OFFSET = 100;

constexpr int MIN_LMR_DEPTH = 3;
constexpr int SEEPieceValues[] = {98, 280, 295, 479, 1064, 0, 0};

constexpr int QS_SEE_MARGIN = 0;

constexpr int PVS_QUIET_BASE = 0;
constexpr int PVS_QUIET_MULTIPLIER = 60;

constexpr int PVS_NOISY_BASE = -10;
constexpr int PVS_NOISY_MULTIPLIER = 20;

constexpr int PAWN_CORRHIST_MULTIPLIER = 178;
constexpr int NONPAWN_CORRHIST_MULTIPLIER = 183;

constexpr int LMP_BASE = 300;
constexpr int LMP_MULTIPLIER = 100;

constexpr int NMP_EVAL_DIVISER = 400;
constexpr int MAX_NMP_EVAL_R = 3;