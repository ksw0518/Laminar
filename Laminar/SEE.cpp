#include "SEE.h"
#include "Bit.h"
#include "Const.h"
#include "Movegen.h"
#include "Tuneables.h"
inline int getPiecePromoting(int type, int side)
{
    int return_piece = 0;
    if ((type == queen_promo) || (type == queen_promo_capture))
    {
        return_piece = Q;
    }
    else if (type == rook_promo || (type == rook_promo_capture))
    {
        return_piece = R;
    }
    else if (type == bishop_promo || (type == bishop_promo_capture))
    {
        return_piece = B;
    }
    else if (type == knight_promo || (type == knight_promo_capture))
    {
        return_piece = N;
    }
    return get_piece(return_piece, side);
}
inline int move_estimated_value(Board& board, Move move)
{
    // Start with the value of the piece on the target square
    int target_piece = board.mailbox[move.To] > 5 ? board.mailbox[move.To] - 6 : board.mailbox[move.To];

    int promoted_piece = getPiecePromoting(move.Type, White);
    int value = *SEEPieceValues[target_piece];

    // Factor in the new piece's value and remove our promoted pawn
    if ((move.Type & promotionFlag) != 0)
        value += *SEEPieceValues[promoted_piece] - *SEEPieceValues[P];

    // Target square is encoded as empty for enpass moves
    else if (move.Type == ep_capture)
        value = *SEEPieceValues[P];

    // We encode Castle moves as KxR, so the initial step is wrong
    else if (move.Type == king_castle || move.Type == queen_castle)
        value = 0;

    return value;
}

//SEE code from ethereal

//Does this move gain at least <threshold> material
int SEE(Board& pos, Move move, int threshold)
{
    int from, to, enpassant, promotion, colour, balance, nextVictim;
    uint64_t bishops, rooks, occupied, attackers, myAttackers;

    // Unpack move information
    from = move.From;
    to = move.To;
    enpassant = move.Type == ep_capture;
    promotion = (move.Type & promotionFlag) != 0;

    // Next victim is moved piece or promotion type
    nextVictim = promotion ? promotion : pos.mailbox[from];
    nextVictim = nextVictim > 5 ? nextVictim - 6 : nextVictim;

    // Balance is the value of the move minus threshold. Function
    // call takes care for Enpass, Promotion and Castling moves.
    balance = move_estimated_value(pos, move) - threshold;

    // Best case still fails to beat the threshold
    if (balance < 0)
        return 0;

    // Worst case is losing the moved piece
    balance -= *SEEPieceValues[nextVictim];

    // If the balance is positive even if losing the moved piece,
    // the exchange is guaranteed to beat the threshold.
    if (balance >= 0)
        return 1;

    // Grab sliders for updating revealed attackers
    bishops = pos.bitboards[b] | pos.bitboards[B] | pos.bitboards[q] | pos.bitboards[Q];
    rooks = pos.bitboards[r] | pos.bitboards[R] | pos.bitboards[q] | pos.bitboards[Q];

    // Let occupied suppose that the move was actually made
    occupied = pos.occupancies[Both];
    occupied = (occupied ^ (1ull << from)) | (1ull << to);
    if (enpassant)
    {
        int ep_square = (pos.side == White) ? move.To + 8 : move.To - 8;
        occupied ^= (1ull << ep_square);
    }

    // Get all pieces which attack the target square. And with occupied
    // so that we do not let the same piece attack twice
    attackers = all_attackers_to_square(pos, occupied, to) & occupied;

    // Now our opponents turn to recapture
    colour = pos.side ^ 1;

    while (1)
    {
        // If we have no more attackers left we lose
        myAttackers = attackers & pos.occupancies[colour];
        if (myAttackers == 0ull)
        {
            break;
        }

        // Find our weakest piece to attack with
        for (nextVictim = P; nextVictim <= Q; nextVictim++)
        {
            if (myAttackers & (pos.bitboards[nextVictim] | pos.bitboards[nextVictim + 6]))
            {
                break;
            }
        }

        // Remove this attacker from the occupied
        occupied ^= (1ull << get_ls1b(myAttackers & (pos.bitboards[nextVictim] | pos.bitboards[nextVictim + 6])));

        // A diagonal move may reveal bishop or queen attackers
        if (nextVictim == P || nextVictim == B || nextVictim == Q)
            attackers |= get_bishop_attacks(to, occupied) & bishops;

        // A vertical or horizontal move may reveal rook or queen attackers
        if (nextVictim == R || nextVictim == Q)
            attackers |= get_rook_attacks(to, occupied) & rooks;

        // Make sure we did not add any already used attacks
        attackers &= occupied;

        // Swap the turn
        colour = 1 - colour;

        // Negamax the balance and add the value of the next victim
        balance = -balance - 1 - *SEEPieceValues[nextVictim];

        // If the balance is non negative after giving away our piece then we win
        if (balance >= 0)
        {
            // As a slide speed up for move legality checking, if our last attacking
            // piece is a king, and our opponent still has attackers, then we've
            // lost as the move we followed would be illegal
            if (nextVictim == K && (attackers & pos.occupancies[colour]))
                colour = 1 - colour;

            break;
        }
    }

    // Side to move after the loop loses
    return pos.side != colour;
}