#pragma once
#include <cstdint>
constexpr int16_t MAX_HISTORY = 16384;

constexpr int MAINHIST_BONUS_BASE = 140;
constexpr int MAINHIST_BONUS_MULT = 420;
constexpr int MAINHIST_BONUS_MAX = 2400;

constexpr int MAINHIST_MALUS_BASE = 140;
constexpr int MAINHIST_MALUS_MULT = 420;
constexpr int MAINHIST_MALUS_MAX = 2400;

constexpr int RFP_MULTIPLIER = 80;
constexpr int RFP_BASE = 0;

constexpr int RFP_MAX_DEPTH = 4;

constexpr int ASP_WINDOW_INITIAL = 30;
constexpr int ASP_WINDOW_MAX = 300;

constexpr int LMR_DIVISOR = 236;
constexpr int LMR_OFFSET = 77;

constexpr int MIN_LMR_DEPTH = 3;