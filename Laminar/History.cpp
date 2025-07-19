#include "History.h"
#include "Search.h"
#include "Tuneables.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
void UpdateHistoryEntry(int16_t& entry, int16_t bonus)
{
    int clampedBonus = std::clamp<int16_t>(bonus, -MAX_HISTORY, MAX_HISTORY);
    entry += clampedBonus - entry * abs(clampedBonus) / MAX_HISTORY;
}

void UpdateMainHist(ThreadData& data, bool stm, int from, int to, int16_t bonus)
{
    int16_t& historyEntry = data.histories.mainHist[stm][from][to];
    UpdateHistoryEntry(historyEntry, bonus);
}
void MalusMainHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus)
{
    bool stm = bonus_move.Piece <= 5 ? White : Black;
    for (int i = 0; i < searchedQuietMoves.count; ++i)
    {
        Move& searchedMove = searchedQuietMoves.moves[i];
        if (searchedMove != bonus_move)
        {
            UpdateMainHist(data, stm, searchedMove.From, searchedMove.To, -malus);
        }
    }
}