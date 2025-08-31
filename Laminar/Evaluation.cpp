

#include "Evaluation.h"
#include "Accumulator.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include "Movegen.h"
#include "NNUE.h"
#include <iomanip>
#include <iostream>

inline int getSide(int piece)
{
    return (piece > 5) ? Black : White;
};

int16_t total_mat(const Board& board)
{
    int m = (count_bits(board.bitboards[P]) + count_bits(board.bitboards[p])) * 100
          + (count_bits(board.bitboards[B]) + count_bits(board.bitboards[b])) * 300
          + (count_bits(board.bitboards[N]) + count_bits(board.bitboards[n])) * 300
          + (count_bits(board.bitboards[R]) + count_bits(board.bitboards[r])) * 500
          + (count_bits(board.bitboards[Q]) + count_bits(board.bitboards[q])) * 900;

    return m;
}
int ScaleEval(int score, Board& board)
{
    float multiplier = ((float)750 + (float)total_mat(board) / 25) / 1024;
    return score * multiplier;
}
int Evaluate(Board& board)
{
    int NN_score;
    if (board.side == White)
        NN_score = forward(&EvalNetwork, &board.accumulator.white, &board.accumulator.black);
    else
        NN_score = forward(&EvalNetwork, &board.accumulator.black, &board.accumulator.white);

    return NN_score;
    //int mg[2];
    //int eg[2];

    //mg[White] = 0;
    //mg[Black] = 0;
    //eg[White] = 0;
    //eg[Black] = 0;

    //int gamePhase = 0;

    //int evalSide = board.side;

    //for (int sq = 0; sq < 64; ++sq)
    //{
    //    int pc = board.mailbox[sq];
    //    if (pc != NO_PIECE)
    //    {
    //        int col = getSide(pc);
    //        mg[col] += mg_table[pc][sq];
    //        eg[col] += eg_table[pc][sq];
    //        gamePhase += gamephaseInc[pc];
    //    }
    //}
    //int mgScore = mg[evalSide] + mg[1 - evalSide];
    //int egScore = eg[evalSide] + eg[1 - evalSide];
    //int mgPhase = gamePhase;
    //if (mgPhase > 24)
    //    mgPhase = 24; /* in case of early promotion */
    //int egPhase = 24 - mgPhase;

    //int Whiteeval = (mgScore * mgPhase + egScore * egPhase) / 24;
    //return Whiteeval * side_multiply[evalSide];
}