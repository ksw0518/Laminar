#include "Movegen.h"
#include "Accumulator.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include <cstring>
#include <iostream>
uint64_t bishop_masks[64] = {};
uint64_t rook_masks[64] = {};
uint64_t bishop_attacks[64][512] = {};
uint64_t rook_attacks[64][4096] = {};
uint64_t pawn_attacks[2][64] = {};
uint64_t knight_attacks[64] = {};
uint64_t king_attacks[64] = {};

uint64_t piece_keys[12][64];
uint64_t enpassant_keys[64];
uint64_t castle_keys[16];
uint64_t side_key;
uint32_t random_state;

void parse_fen(std::string fen, Board& board)
{
    for (int i = 0; i < 64; i++)
    {
        board.mailbox[i] = NO_PIECE;
    }
    //board.mailbox
    for (int i = 0; i < 12; i++)
    {
        board.bitboards[i] = 0;
    }
    for (int i = 0; i < 3; i++)
    {
        board.occupancies[i] = 0;
    }
    board.side = 0;
    board.enpassent = NO_SQ;
    int square = 0;
    size_t index = 0;
    for (size_t i = 0; i < fen.length(); i++)
    {
        char text = fen[i];
        if (text == ' ')
        {
            index = i + 1;
            break;
        }
        if (text == '/')
        {
            continue;
        }
        if (std::isdigit(text))
        {
            square += text - '0';
        }
        if (std::isalpha(text))
        {
            int piece = getPieceFromChar(text);
            board.mailbox[square] = piece;
            Set_bit(board.bitboards[piece], square);
            square++;
        }
    }
    if (fen[index] == 'w')
        board.side = White;
    else
        board.side = Black;

    index += 2;

    board.castle = 0;
    for (int i = 0; i < 4; i++)
    {
        if (fen[index] == 'K')
            board.castle |= WhiteKingCastle;
        if (fen[index] == 'Q')
            board.castle |= WhiteQueenCastle;
        if (fen[index] == 'k')
            board.castle |= BlackKingCastle;
        if (fen[index] == 'q')
            board.castle |= BlackQueenCastle;
        if (fen[index] == ' ')
            break;
        if (fen[index] == '-')
        {
            board.castle = 0;
            break;
        }

        index++;
    }
    index++;
    if (fen[index] == ' ')
        index++;
    if (fen[index] != '-')
    {
        int file = fen[index] - 'a';
        int rank = 8 - (fen[index + 1] - '0');

        board.enpassent = rank * 8 + file;
    }
    else
    {
        board.enpassent = NO_SQ;
    }

    if (index + 2 >= fen.length())
    {
        board.halfmove = 0;
    }
    else
    {
        std::string halfmoves_str = "";
        index += 2;
        halfmoves_str += fen[index];
        if (fen[index + 1] != ' ')
        {
            halfmoves_str += fen[index + 1];
        }

        int halfmoves = std::stoi(halfmoves_str);
        board.halfmove = halfmoves;
    }
    for (int piece = P; piece <= K; piece++)
    {
        board.occupancies[White] |= board.bitboards[piece];
    }
    for (int piece = p; piece <= k; piece++)
    {
        board.occupancies[Black] |= board.bitboards[piece];
    }
    board.occupancies[Both] |= board.occupancies[Black];
    board.occupancies[Both] |= board.occupancies[White];
    board.zobristKey = generate_hash_key(board);
    board.pawnKey = generate_pawn_key(board);

    resetAccumulators(board, board.accumulator);

    int whiteKingFile = getFile(get_ls1b(board.bitboards[K]));
    if (whiteKingFile >= 4) //king is on right now, have to flip
    {
        resetWhiteAccumulator(board, board.accumulator, true);
    }
    if (whiteKingFile <= 3) //king is on left now, have to flip
    {
        resetWhiteAccumulator(board, board.accumulator, false);
    }

    int blackKingFile = getFile(get_ls1b(board.bitboards[k]));
    if (blackKingFile >= 4) //king is on right now, have to flip
    {
        resetBlackAccumulator(board, board.accumulator, true);
    }
    if (blackKingFile <= 3) //king is on left now, have to flip
    {
        resetBlackAccumulator(board, board.accumulator, false);
    }
}
uint32_t get_random_U32_number()
{
    // get current state
    uint32_t number = random_state;

    // XOR shift algorithm
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    // update random number state
    random_state = number;

    // return random number
    return number;
}

// generate 64-bit pseudo legal numbers
uint64_t get_random_U64_number()
{
    // define 4 random numbers
    uint64_t n1, n2, n3, n4;

    // init random numbers slicing 16 bits from MS1B side
    n1 = (uint64_t)(get_random_U32_number()) & 0xFFFF;
    n2 = (uint64_t)(get_random_U32_number()) & 0xFFFF;
    n3 = (uint64_t)(get_random_U32_number()) & 0xFFFF;
    n4 = (uint64_t)(get_random_U32_number()) & 0xFFFF;

    // return random number
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}
void init_random_keys()
{
    random_state = 1804289383;

    for (int piece = P; piece <= k; piece++)
    {
        for (int square = 0; square < 64; square++)
        {
            piece_keys[piece][square] = get_random_U64_number();
        }
    }

    for (int square = 0; square < 64; square++)
    {
        enpassant_keys[square] = get_random_U64_number();
    }

    for (int castle = 0; castle < 16; castle++)
    {
        castle_keys[castle] = get_random_U64_number();
    }

    side_key = get_random_U64_number();
}
uint64_t generate_pawn_key(Board& board)
{
    uint64_t final_key = 0ULL;
    uint64_t bitboard;

    bitboard = board.bitboards[P];

    while (bitboard)
    {
        int square = get_ls1b(bitboard);

        final_key ^= piece_keys[P][square];
        Pop_bit(bitboard, square);
    }

    bitboard = board.bitboards[p];

    while (bitboard)
    {
        int square = get_ls1b(bitboard);

        final_key ^= piece_keys[p][square];
        Pop_bit(bitboard, square);
    }
    return final_key;
}

uint64_t generate_hash_key(Board& board)
{
    uint64_t final_key = 0ULL;

    uint64_t bitboard;

    for (int piece = P; piece <= k; piece++)
    {
        bitboard = board.bitboards[piece];

        while (bitboard)
        {
            int square = get_ls1b(bitboard);

            final_key ^= piece_keys[piece][square];
            Pop_bit(bitboard, square);
        }
    }

    if (board.enpassent != NO_SQ)
    {
        final_key ^= enpassant_keys[board.enpassent];
    }
    final_key ^= castle_keys[get_castle(board.castle)];

    if (board.side == Black)
    {
        final_key ^= side_key;
    }
    return final_key;
}
// Move constructor
Move::Move(unsigned char from, unsigned char to, unsigned char type, unsigned char piece) :
        From(from), To(to), Type(type), Piece(piece)
{
}

Move::Move() :
        From(0), To(0), Type(0), Piece(0)
{
}
int GetSquare(std::string squareName)
{
    char file = squareName[0];
    char rank = squareName[1];

    int fileIndex = -1;
    switch (file)
    {
        case 'a':
            fileIndex = 0;
            break;
        case 'b':
            fileIndex = 1;
            break;
        case 'c':
            fileIndex = 2;
            break;
        case 'd':
            fileIndex = 3;
            break;
        case 'e':
            fileIndex = 4;
            break;
        case 'f':
            fileIndex = 5;
            break;
        case 'g':
            fileIndex = 6;
            break;
        case 'h':
            fileIndex = 7;
            break;
    }

    int rankIndex = -1;
    switch (rank)
    {
        case '1':
            rankIndex = 7;
            break;
        case '2':
            rankIndex = 6;
            break;
        case '3':
            rankIndex = 5;
            break;
        case '4':
            rankIndex = 4;
            break;
        case '5':
            rankIndex = 3;
            break;
        case '6':
            rankIndex = 2;
            break;
        case '7':
            rankIndex = 1;
            break;
        case '8':
            rankIndex = 0;
            break;
    }

    return rankIndex * 8 + fileIndex;
}

uint64_t MaskBishopAttack(int square)
{
    uint64_t attacks = 0UL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
    }
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
    }
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
    }
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
    }
    return attacks;
}

uint64_t MaskRookAttack(int square)
{
    uint64_t attacks = 0UL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 6; r++)
    {
        attacks |= (1ULL << (r * 8 + tf));
    }
    for (r = tr - 1; r >= 1; r--)
    {
        attacks |= (1ULL << (r * 8 + tf));
    }
    for (f = tf + 1; f <= 6; f++)
    {
        attacks |= (1ULL << (tr * 8 + f));
    }
    for (f = tf - 1; f >= 1; f--)
    {
        attacks |= (1ULL << (tr * 8 + f));
    }
    return attacks;
}
static uint64_t CalculateRookAttack(int square, uint64_t block)
{
    uint64_t attacks = 0UL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 7; r++)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf) & block) != 0)
        {
            break;
        }
    }
    for (r = tr - 1; r >= 0; r--)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf) & block) != 0)
        {
            break;
        }
    }
    for (f = tf + 1; f <= 7; f++)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f) & block) != 0)
        {
            break;
        }
    }
    for (f = tf - 1; f >= 0; f--)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f) & block) != 0)
        {
            break;
        }
    }
    return attacks;
}

static uint64_t CalculateBishopAttack(int square, uint64_t block)
{
    uint64_t attacks = 0UL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f) & block) != 0)
        {
            break;
        }
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f) & block) != 0)
        {
            break;
        }
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f) & block) != 0)
        {
            break;
        }
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f) & block) != 0)
        {
            break;
        }
    }
    return attacks;
}
void init_sliders_attacks(int bishop)
{
    for (int square = 0; square < 64; square++)
    {
        bishop_masks[square] = MaskBishopAttack(square);
        rook_masks[square] = MaskRookAttack(square);

        uint64_t attack_mask = bishop != 0 ? bishop_masks[square] : rook_masks[square];
        int relevant_bits_count = count_bits(attack_mask);
        int occupancy_indicies = (1 << relevant_bits_count);

        for (int index = 0; index < occupancy_indicies; index++)
        {
            if (bishop != 0)
            {
                uint64_t occupancy = set_occupancy(index, relevant_bits_count, attack_mask);

                int magic_index =
                    (int)((occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]));

                if (magic_index < 512)
                {
                    bishop_attacks[square][magic_index] = CalculateBishopAttack(square, occupancy);
                }
            }
            else
            {
                uint64_t occupancy = set_occupancy(index, relevant_bits_count, attack_mask);

                int magic_index = (int)((occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]));

                if (magic_index < 4096)
                {
                    rook_attacks[square][magic_index] = CalculateRookAttack(square, occupancy);
                }
            }
        }
    }
}
uint64_t CalculatePawnAttack(int square, int side)
{
    uint64_t attacks = 0UL;
    uint64_t bitboard = 0UL;
    Set_bit(bitboard, square);
    //PrintBitboard(bitboard);
    if (side == White) //white
    {
        if (((bitboard >> 7) & NotAFile) != 0)
            attacks |= (bitboard >> 7);
        if (((bitboard >> 9) & NotHFile) != 0)
            attacks |= (bitboard >> 9);
    }
    else //black
    {
        if (((bitboard << 7) & NotHFile) != 0)
            attacks |= (bitboard << 7);
        if (((bitboard << 9) & NotAFile) != 0)
            attacks |= (bitboard << 9);
    }

    return attacks;
}

uint64_t CalculateKnightAttack(int square)
{
    uint64_t attacks = 0UL;
    uint64_t bitboard = 0UL;
    Set_bit(bitboard, square);
    //PrintBitboard(bitboard);
    //17 15 10 6

    if (((bitboard >> 17) & NotHFile) != 0)
        attacks |= (bitboard >> 17);
    if (((bitboard >> 15) & NotAFile) != 0)
        attacks |= (bitboard >> 15);
    if (((bitboard >> 10) & NotHGFile) != 0)
        attacks |= (bitboard >> 10);
    if (((bitboard >> 6) & NotABFile) != 0)
        attacks |= (bitboard >> 6);

    if (((bitboard << 17) & NotAFile) != 0)
        attacks |= (bitboard << 17);
    if (((bitboard << 15) & NotHFile) != 0)
        attacks |= (bitboard << 15);
    if (((bitboard << 10) & NotABFile) != 0)
        attacks |= (bitboard << 10);
    if (((bitboard << 6) & NotHGFile) != 0)
        attacks |= (bitboard << 6);
    return attacks;
}
uint64_t CalculateKingAttack(int square)
{
    uint64_t attacks = 0UL;
    uint64_t bitboard = 0UL;
    Set_bit(bitboard, square);
    //PrintBitboard(bitboard);
    //17 15 10 6

    if (((bitboard >> 8)) != 0)
        attacks |= (bitboard >> 8);
    if (((bitboard >> 9) & NotHFile) != 0)
        attacks |= (bitboard >> 9);
    if (((bitboard >> 7) & NotAFile) != 0)
        attacks |= (bitboard >> 7);
    if (((bitboard >> 1) & NotHFile) != 0)
        attacks |= (bitboard >> 1);

    if (((bitboard << 8)) != 0)
        attacks |= (bitboard << 8);
    if (((bitboard << 9) & NotAFile) != 0)
        attacks |= (bitboard << 9);
    if (((bitboard << 7) & NotHFile) != 0)
        attacks |= (bitboard << 7);
    if (((bitboard << 1) & NotAFile) != 0)
        attacks |= (bitboard << 1);
    return attacks;
}
void InitializeLeaper()
{
    for (int i = 0; i < 64; i++)
    {
        pawn_attacks[0][i] = CalculatePawnAttack(i, 0);
        pawn_attacks[1][i] = CalculatePawnAttack(i, 1);

        knight_attacks[i] = CalculateKnightAttack(i);
        king_attacks[i] = CalculateKingAttack(i);
    }
}
uint64_t get_bishop_attacks(int square, uint64_t occupancy)
{
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];
    //std::cout << square << "\n";
    return bishop_attacks[square][occupancy];
}
uint64_t get_rook_attacks(int square, uint64_t occupancy)
{
    //std::cout << "asdt";
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];

    return rook_attacks[square][occupancy];
}
uint64_t get_queen_attacks(int square, uint64_t occupancy)
{
    //Console.WriteLine(square);
    uint64_t queen_attacks;
    uint64_t bishop_occupancies = occupancy;
    uint64_t rook_occupancies = occupancy;

    rook_occupancies &= rook_masks[square];
    rook_occupancies *= rook_magic_numbers[square];
    rook_occupancies >>= 64 - rook_relevant_bits[square];
    queen_attacks = rook_attacks[square][rook_occupancies];

    bishop_occupancies &= bishop_masks[square];
    bishop_occupancies *= bishop_magic_numbers[square];
    bishop_occupancies >>= 64 - bishop_relevant_bits[square];
    queen_attacks |= bishop_attacks[square][bishop_occupancies];

    return queen_attacks;
}
void printMove(Move move)
{
    std::cout
        << (CoordinatesToChessNotation(static_cast<uint8_t>(move.From))
            + CoordinatesToChessNotation(static_cast<uint8_t>(move.To)));
    if (move.Type == queen_promo || move.Type == queen_promo_capture)
        std::cout << ("q");
    if (move.Type == rook_promo || move.Type == rook_promo_capture)
        std::cout << ("r");
    if (move.Type == bishop_promo || move.Type == bishop_promo_capture)
        std::cout << ("b");
    if (move.Type == knight_promo || move.Type == knight_promo_capture)
        std::cout << ("n");

    //std::cout << ("")
}
void GeneratePawnMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t pawnBB, pawn_capture_mask, pawn_capture, pawnOnePush, pawnTwoPush;
    uint64_t forward, doublePushSquare, promotionSquare, enpassent;
    if (side == White)
    {
        pawnBB = board.bitboards[P];
        promotionSquare = 0b0000000000000000000000000000000000000000000000001111111100000000;
        doublePushSquare = 0b0000000011111111000000000000000000000000000000000000000000000000;
    }
    else
    {
        pawnBB = board.bitboards[p];
        promotionSquare = 0b0000000011111111000000000000000000000000000000000000000000000000;
        doublePushSquare = 0b0000000000000000000000000000000000000000000000001111111100000000;
    }
    if (pawnBB != 0)
    {
        while (true)
        {
            int From = get_ls1b(pawnBB);

            uint64_t currPawnBB = 1ULL << From;
            uint64_t twoForward;

            if (side == White)
            {
                forward = currPawnBB >> 8;
                twoForward = currPawnBB >> 16;
            }
            else
            {
                forward = currPawnBB << 8;
                twoForward = currPawnBB << 16;
            }

            if ((currPawnBB & promotionSquare) != 0)
            {
                // =======promo======= //
                uint64_t pawnPromo = forward & ~board.occupancies[Both];
                if (pawnPromo != 0)
                {
                    while (true)
                    {
                        int To = get_ls1b(pawnPromo);
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            knight_promo,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            bishop_promo,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            rook_promo,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            queen_promo,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        Pop_bit(pawnPromo, To);
                        if (pawnPromo == 0ULL)
                            break;
                    }
                }

                // =======promo_capture======= //
                pawn_capture_mask = pawn_attacks[board.side][From];
                pawn_capture = (pawn_capture_mask & board.occupancies[1 - side]);

                if (pawn_capture != 0)
                {
                    while (true)
                    {
                        int To = get_ls1b(pawn_capture);
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            knight_promo_capture,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            bishop_promo_capture,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            rook_promo_capture,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            queen_promo_capture,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        Pop_bit(pawn_capture, To);
                        if (pawn_capture == 0ULL)
                            break;
                    }
                }
            }
            else
            {
                // =======pawn one square push======= //
                pawnOnePush = (forward & ~board.occupancies[Both]);

                bool isPossible = pawnOnePush != 0;

                if (pawnOnePush != 0)
                {
                    while (true)
                    {
                        int To = get_ls1b(pawnOnePush);
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            quiet_move,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        Pop_bit(pawnOnePush, To);
                        if (pawnOnePush == 0ULL)
                            break;
                    }
                }

                // =======pawn two square push======= //

                pawnTwoPush = 0;
                if (isPossible)
                {
                    if ((doublePushSquare & currPawnBB) != 0) //pawn on second rank
                    {
                        pawnTwoPush = (twoForward & ~board.occupancies[Both]);
                    }
                }
                if (pawnTwoPush != 0)
                {
                    while (true)
                    {
                        int To = get_ls1b(pawnTwoPush);
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            double_pawn_push,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        Pop_bit(pawnTwoPush, To);
                        if (pawnTwoPush == 0ULL)
                            break;
                    }
                }
                // =======pawn capture======= //
                pawn_capture_mask = pawn_attacks[board.side][From];
                pawn_capture = pawn_capture_mask & board.occupancies[1 - side];

                if (pawn_capture != 0)
                {
                    while (true)
                    {
                        int To = get_ls1b(pawn_capture);
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            capture,
                            static_cast<uint8_t>(get_piece(p, side))
                        ));
                        Pop_bit(pawn_capture, To);
                        if (pawn_capture == 0ULL)
                            break;
                    }
                }

                // =======pawn enpassent capture======= //
                if (board.enpassent != NO_SQ) // enpassent possible
                {
                    enpassent = (pawn_capture_mask & (1ULL << board.enpassent));

                    if (enpassent != 0)
                    {
                        while (true)
                        {
                            int To = get_ls1b(enpassent);
                            MoveList.add(Move(
                                static_cast<uint8_t>(From),
                                static_cast<uint8_t>(To),
                                ep_capture,
                                static_cast<uint8_t>(get_piece(p, side))
                            ));
                            Pop_bit(enpassent, To);
                            if (enpassent == 0ULL)
                                break;
                        }
                    }
                }
            }
            Pop_bit(pawnBB, From);
            if (pawnBB == 0ULL)
                break;
        }
    }
}
void GenerateKnightMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t KnightBB;
    if (side == White)
    {
        KnightBB = board.bitboards[N];
    }
    else
    {
        KnightBB = board.bitboards[n];
    }
    if (KnightBB != 0)
    {
        while (true)
        {
            int From = get_ls1b(KnightBB);
            uint64_t KnightMove = (knight_attacks[From] & ~board.occupancies[side]);
            if (KnightMove != 0)
            {
                while (true)
                {
                    int To = get_ls1b(KnightMove);
                    if ((board.occupancies[1 - side] & (1ULL << To)) != 0)
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            capture,
                            static_cast<uint8_t>(get_piece(n, side))
                        ));
                    }
                    else
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            quiet_move,
                            static_cast<uint8_t>(get_piece(n, side))
                        ));
                    }
                    Pop_bit(KnightMove, To);
                    if (KnightMove == 0)
                        break;
                }
            }
            Pop_bit(KnightBB, From);
            if (KnightBB == 0)
                break;
        }
    }
}
void GenerateBishopMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t BishopBB;
    if (side == White)
    {
        BishopBB = board.bitboards[B];
    }
    else
    {
        BishopBB = board.bitboards[b];
    }
    if (BishopBB != 0)
    {
        while (true)
        {
            int From = get_ls1b(BishopBB);
            uint64_t BishopMove = (get_bishop_attacks(From, board.occupancies[Both]) & ~board.occupancies[side]);
            if (BishopMove != 0)
            {
                while (true)
                {
                    int To = get_ls1b(BishopMove);
                    if ((board.occupancies[1 - side] & (1ULL << To)) != 0)
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            capture,
                            static_cast<uint8_t>(get_piece(b, side))
                        ));
                    }
                    else
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            quiet_move,
                            static_cast<uint8_t>(get_piece(b, side))
                        ));
                    }
                    Pop_bit(BishopMove, To);
                    if (BishopMove == 0)
                        break;
                }
            }
            Pop_bit(BishopBB, From);
            if (BishopBB == 0)
                break;
        }
    }
}
void GenerateRookMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t RookBB;
    if (side == White)
    {
        RookBB = board.bitboards[R];
    }
    else
    {
        RookBB = board.bitboards[r];
    }
    //PrintBitboard(RookBB);
    if (RookBB != 0)
    {
        while (true)
        {
            int From = get_ls1b(RookBB);
            uint64_t RookMove =
                (get_rook_attacks(static_cast<uint8_t>(From), board.occupancies[Both]) & ~board.occupancies[side]);
            if (RookMove != 0)
            {
                while (true)
                {
                    int To = get_ls1b(RookMove);
                    if ((board.occupancies[1 - side] & (1ULL << To)) != 0)
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            capture,
                            static_cast<uint8_t>(get_piece(r, side))
                        ));
                    }
                    else
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            quiet_move,
                            static_cast<uint8_t>(get_piece(r, side))
                        ));
                    }
                    Pop_bit(RookMove, To);
                    if (RookMove == 0)
                        break;
                }
            }
            Pop_bit(RookBB, From);
            if (RookBB == 0)
                break;
        }
    }
}
void GenerateQueenMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t QueenBB;
    if (side == White)
    {
        QueenBB = board.bitboards[Q];
    }
    else
    {
        QueenBB = board.bitboards[q];
    }
    if (QueenBB != 0)
    {
        while (true)
        {
            int From = get_ls1b(QueenBB);
            uint64_t QueenMove = (get_queen_attacks(From, board.occupancies[Both]) & ~board.occupancies[side]);
            if (QueenMove != 0)
            {
                while (true)
                {
                    int To = get_ls1b(QueenMove);
                    if ((board.occupancies[1 - side] & (1ULL << To)) != 0)
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            capture,
                            static_cast<uint8_t>(get_piece(q, side))
                        ));
                    }
                    else
                    {
                        MoveList.add(Move(
                            static_cast<uint8_t>(From),
                            static_cast<uint8_t>(To),
                            quiet_move,
                            static_cast<uint8_t>(get_piece(q, side))
                        ));
                    }
                    Pop_bit(QueenMove, To);
                    if (QueenMove == 0)
                        break;
                }
            }
            Pop_bit(QueenBB, From);
            if (QueenBB == 0)
                break;
        }
    }
}
void GenerateKingMoves(MoveList& MoveList, Board& board)
{
    int side = board.side;
    uint64_t KingBB;
    if (side == White)
    {
        KingBB = board.bitboards[K];
    }
    else
    {
        KingBB = board.bitboards[k];
    }
    while (true)
    {
        int From = get_ls1b(KingBB);
        uint64_t KingMove = king_attacks[From] & ~board.occupancies[side];
        if (KingMove != 0)
        {
            while (true)
            {
                int To = get_ls1b(KingMove);
                if ((board.occupancies[1 - side] & (1ULL << To)) != 0)
                {
                    MoveList.add(Move(
                        static_cast<uint8_t>(From),
                        static_cast<uint8_t>(To),
                        capture,
                        static_cast<uint8_t>(get_piece(k, side))
                    ));
                }
                else
                {
                    MoveList.add(Move(
                        static_cast<uint8_t>(From),
                        static_cast<uint8_t>(To),
                        quiet_move,
                        static_cast<uint8_t>(get_piece(k, side))
                    ));
                }
                Pop_bit(KingMove, To);
                if (KingMove == 0)
                    break;
            }
        }

        Pop_bit(KingBB, From);
        if (KingBB == 0)
            break;
    }
    if (side == White)
    {
        if ((board.castle & WhiteKingCastle) != 0) // kingside castling
        {
            if ((board.occupancies[Both] & WhiteKingCastleEmpty) == 0)
            {
                MoveList.add(Move(
                    static_cast<uint8_t>(e1),
                    static_cast<uint8_t>(g1),
                    king_castle,
                    static_cast<uint8_t>(get_piece(k, side))
                ));
            }
        }
        if ((board.castle & WhiteQueenCastle) != 0)
        {
            if ((board.occupancies[Both] & WhiteQueenCastleEmpty) == 0)
            {
                MoveList.add(Move(
                    static_cast<uint8_t>(e1),
                    static_cast<uint8_t>(c1),
                    queen_castle,
                    static_cast<uint8_t>(get_piece(k, side))
                ));
            }
        }
    }
    else
    {
        if ((board.castle & BlackKingCastle) != 0) // kingside castling
        {
            if ((board.occupancies[Both] & BlackKingCastleEmpty) == 0)
            {
                MoveList.add(Move(
                    static_cast<uint8_t>(e8),
                    static_cast<uint8_t>(g8),
                    king_castle,
                    static_cast<uint8_t>(get_piece(k, side))
                ));
            }
        }

        if ((board.castle & BlackQueenCastle) != 0)
        {
            if ((board.occupancies[Both] & BlackQueenCastleEmpty) == 0)
            {
                MoveList.add(Move(
                    static_cast<uint8_t>(e8),
                    static_cast<uint8_t>(c8),
                    queen_castle,
                    static_cast<uint8_t>(get_piece(k, side))
                ));
            }
        }
    }
}
bool is_move_irreversible(Move& move)
{
    if (((move.Type & captureFlag) != 0) || move.Piece == p || move.Piece == P)
    {
        return true;
    }
    return false;
}
void XORZobrist(uint64_t& zobrist, uint64_t key)
{
    zobrist ^= key;
}
void XORPieceZobrist(int piece, int square, Board& board, bool flipWhite, bool flipBlack, bool AddingPiece)
{
    XORZobrist(board.zobristKey, piece_keys[piece][square]);
    if (get_piece(piece, White) == P)
    {
        XORZobrist(board.pawnKey, piece_keys[piece][square]);
    }
    bool side = piece <= 5 ? White : Black;
    Accumulator& accumulator = ((side == White) ? board.accumulator.white : board.accumulator.black);

    if (AddingPiece) //adding piece
    {
        /* std::cout << "adding"
                  << "\n"
                  << "flipwhite:" << flipWhite << "\n"
                  << "flipblack: " << flipBlack << "\n"
                  << "piece :" << getCharFromPiece(get_piece(piece, White)) << "\nside" << side << "\nsquare"
                  << CoordinatesToChessNotation(square) << "\n\n";*/

        accumulatorAdd(
            &EvalNetwork,
            &board.accumulator.white,
            calculateIndex(White, square, get_piece(piece, White), side, flipWhite)
        );
        accumulatorAdd(
            &EvalNetwork,
            &board.accumulator.black,
            calculateIndex(Black, square, get_piece(piece, White), side, flipBlack)
        );
    }
    else
    {
        /*std::cout << "removing"
                  << "\n"
                  << "flipwhite:" << flipWhite << "\n"
                  << "flipblack: " << flipBlack << "\n"
                  << "piece :" << getCharFromPiece(get_piece(piece, White)) << "\nside" << side << "\nsquare"
                  << CoordinatesToChessNotation(square) << "\n\n";*/
        accumulatorSub(
            &EvalNetwork,
            &board.accumulator.white,
            calculateIndex(White, square, get_piece(piece, White), side, flipWhite)
        );
        accumulatorSub(
            &EvalNetwork,
            &board.accumulator.black,
            calculateIndex(Black, square, get_piece(piece, White), side, flipBlack)
        );
    }
}
int GetPromotingPiece(Move& move)
{
    int type = move.Type;
    int side = move.Piece >= 6;
    if (type == queen_promo || type == queen_promo_capture)
    {
        return get_piece(Q, side);
    }
    else if (type == rook_promo || type == rook_promo_capture)
    {
        return get_piece(R, side);
    }
    else if (type == knight_promo || type == knight_promo_capture)
    {
        return get_piece(N, side);
    }
    else if (type == bishop_promo || type == bishop_promo_capture)
    {
        return get_piece(B, side);
    }
    else
    {
        return NO_PIECE;
    }
}
void UpdateZobrist(
    Board& board,
    Move& move,
    bool flipWhite,
    bool flipBlack
) //have to call before doing anything to board
{
    bool isEP = (move.Type == ep_capture);
    bool isCapture = (move.Type & captureFlag) != 0;
    bool isKingCastle = (move.Type == king_castle);
    bool isQueenCastle = (move.Type == queen_castle);
    bool isPromo = (move.Type & promotionFlag) != 0;
    bool isDoublePush = (move.Type == double_pawn_push);

    int castle_change = board.castle;
    if (get_piece(move.Piece, White) == K) //if king moved
    {
        if (board.side == White)
        {
            board.zobristKey ^= castle_keys[get_castle(castle_change)];
            castle_change &= ~WhiteKingCastle;
            castle_change &= ~WhiteQueenCastle;
            board.zobristKey ^= castle_keys[get_castle(castle_change)];
        }
        else
        {
            board.zobristKey ^= castle_keys[get_castle(castle_change)];
            castle_change &= ~BlackKingCastle;
            castle_change &= ~BlackQueenCastle;
            board.zobristKey ^= castle_keys[get_castle(castle_change)];
        }
    }
    else if (get_piece(move.Piece, White) == R) //if rook moved
    {
        if (board.side == White)
        {
            if ((board.castle & WhiteQueenCastle) != 0 && move.From == a1) // no q castle
            {
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
                castle_change &= ~WhiteQueenCastle;
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
            }
            //DO FROM HERE FUCKER
            else if ((board.castle & WhiteKingCastle) != 0 && move.From == h1) // no k castle
            {
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
                castle_change &= ~WhiteKingCastle;
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
            }
        }
        else
        {
            if ((board.castle & BlackQueenCastle) != 0 && move.From == a8) // no q castle
            {
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
                castle_change &= ~BlackQueenCastle;
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
            }
            else if ((board.castle & BlackKingCastle) != 0 && move.From == h8) // no k castle
            {
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
                castle_change &= ~BlackKingCastle;
                board.zobristKey ^= castle_keys[get_castle(castle_change)];
            }
        }
    }
    if (board.enpassent != NO_SQ)
    {
        XORZobrist(board.zobristKey, enpassant_keys[board.enpassent]);
    }
    XORZobrist(board.zobristKey, side_key); //flip side

    XORPieceZobrist(move.Piece, move.From, board, flipWhite, flipBlack,
                    false);                                                  //remove piece in from square
    XORPieceZobrist(move.Piece, move.To, board, flipWhite, flipBlack, true); //add piece in to square
    if (isDoublePush)
    {
        if (board.side == White)
        {
            XORZobrist(board.zobristKey, enpassant_keys[move.To + 8]);
        }
        else
        {
            XORZobrist(board.zobristKey, enpassant_keys[move.To - 8]);
        }
    }
    if (isCapture)
    {
        if (board.mailbox[move.To] == get_piece(r, 1 - board.side))
        {
            if (getFile(move.To) == 0) // a file rook captured; delete queen castle
            {
                if (board.side == White) // have to delete black queen castle
                {
                    if (getRank(move.To) == 7)
                    {
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                        castle_change &= ~BlackQueenCastle;
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                    }
                }
                else
                {
                    if (getRank(move.To) == 0)
                    {
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                        castle_change &= ~WhiteQueenCastle;
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                    }
                }
            }
            else if (getFile(move.To) == 7) // h file rook captured; delete king castle
            {
                if (board.side == White) // have to delete black king castle
                {
                    if (getRank(move.To) == 7)
                    {
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                        castle_change &= ~BlackKingCastle;
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                    }
                }
                else
                {
                    if (getRank(move.To) == 0)
                    {
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                        castle_change &= ~WhiteKingCastle;
                        board.zobristKey ^= castle_keys[get_castle(castle_change)];
                    }
                }
            }
        }
        int capture_square;
        if (isEP)
        {
            if (board.side == White)
            {
                capture_square = move.To + 8;
            }
            else
            {
                capture_square = move.To - 8;
            }
        }
        else
        {
            capture_square = move.To;
        }
        int captured_piece = board.mailbox[capture_square];
        XORPieceZobrist(
            captured_piece,
            capture_square,
            board,
            flipWhite,
            flipBlack,
            false
        ); //remove captured piece in to square
    }

    if (isKingCastle)
    {
        int rookSquare;
        if (board.side == White)
        {
            rookSquare = h1;
        }
        else
        {
            rookSquare = h8;
        }

        XORPieceZobrist(
            board.mailbox[rookSquare],
            rookSquare,
            board,
            flipWhite,
            flipBlack,
            false
        ); //remove castling rook
        XORPieceZobrist(
            board.mailbox[rookSquare],
            rookSquare - 2,
            board,
            flipWhite,
            flipBlack,
            true
        ); //add castling rook
    }
    else if (isQueenCastle)
    {
        int rookSquare;
        if (board.side == White)
        {
            rookSquare = a1;
        }
        else
        {
            rookSquare = a8;
        }

        XORPieceZobrist(
            board.mailbox[rookSquare],
            rookSquare,
            board,
            flipWhite,
            flipBlack,
            false
        ); //remove castling rook
        XORPieceZobrist(
            board.mailbox[rookSquare],
            rookSquare + 3,
            board,
            flipWhite,
            flipBlack,
            true
        ); //add castling rook
    }
    if (isPromo)
    {
        XORPieceZobrist(move.Piece, move.To, board, flipWhite, flipBlack,
                        false); //remove pawn in to square

        int promoPiece = GetPromotingPiece(move);
        XORPieceZobrist(promoPiece, move.To, board, flipWhite, flipBlack,
                        true); //add promoting piece in to square
    }
}
void MakeNullMove(Board& board)
{
    if (board.enpassent != NO_SQ)
    {
        board.zobristKey ^= enpassant_keys[board.enpassent];
        board.enpassent = NO_SQ;
    }
    board.side = 1 - board.side;
    board.zobristKey ^= side_key;
}
void UnmakeNullmove(Board& board)
{
    board.side = 1 - board.side;
    board.zobristKey ^= side_key;
}
void MakeMove(Board& board, Move move)
{
    bool flipWhite = false;
    bool flipBlack = false;

    int whiteKingFile = getFile(get_ls1b(board.bitboards[K]));
    int blackKingFile = getFile(get_ls1b(board.bitboards[k]));

    int stmKingFile, nstmKingFile;

    if (board.side == White)
    {
        stmKingFile = whiteKingFile;
        nstmKingFile = blackKingFile;
    }
    else
    {
        stmKingFile = blackKingFile;
        nstmKingFile = whiteKingFile;
    }

    if (stmKingFile >= 4) //king is on the right, need to flip
    {
        if (board.side == White)
        {
            flipWhite = true;
        }
        else
        {
            flipBlack = true;
        }
    }
    else
    {
        if (board.side == White)
        {
            flipWhite = false;
        }
        else
        {
            flipBlack = false;
        }
    }

    if (nstmKingFile >= 4)
    {
        if (board.side == White)
        {
            flipBlack = true;
        }
        else
        {
            flipWhite = true;
        }
    }
    else
    {
        if (board.side == White)
        {
            flipBlack = false;
        }
        else
        {
            flipWhite = false;
        }
    }

    if (get_piece(move.Piece, White) == K)
    {
        if (getFile(move.From) <= 3) //king was left before
        {
            if (getFile(move.To) >= 4) //king moved to right
            {
                //fully refresh the stm accumulator, and change that to start mirroring
                if (board.side == White)
                {
                    flipWhite = true;
                }
                else
                {
                    flipBlack = true;
                }
            }
        }
        else //king was right before
        {
            if (getFile(move.To) <= 3) //king moved to left
            {
                //fully refresh the stm accumulator, and change that to stop mirroring
                if (board.side == White)
                {
                    flipWhite = false;
                }
                else
                {
                    flipBlack = false;
                }
            }
        }
    }
    UpdateZobrist(board, move, flipWhite, flipBlack);
    //uint64_t lzobrist = board.zobristKey;

    if (board.enpassent != NO_SQ)
    {
        board.enpassent = NO_SQ;
    }

    int side = board.side;

    if (get_piece(move.Piece, White) == K)
    {
        if (side == White)
        {
            board.castle &= ~WhiteKingCastle;
            board.castle &= ~WhiteQueenCastle;
        }
        else
        {
            board.castle &= ~BlackKingCastle;
            board.castle &= ~BlackQueenCastle;
        }
    }
    else if (get_piece(move.Piece, White) == R)
    {
        if (side == White)
        {
            if ((board.castle & WhiteQueenCastle) != 0 && move.From == a1)
            {
                board.castle &= ~WhiteQueenCastle;
            }
            else if ((board.castle & WhiteKingCastle) != 0 && move.From == h1)
            {
                board.castle &= ~WhiteKingCastle;
            }
        }
        else
        {
            if ((board.castle & BlackQueenCastle) != 0 && move.From == a8)
            {
                board.castle &= ~BlackQueenCastle;
            }
            else if ((board.castle & BlackKingCastle) != 0 && move.From == h8)
            {
                board.castle &= ~BlackKingCastle;
            }
        }
    }
    board.halfmove++;
    switch (move.Type)
    {
        case double_pawn_push:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = move.Piece;
            if (side == White)
            {
                board.enpassent = move.To + 8;
            }
            else
            {
                board.enpassent = move.To - 8;
            }
            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case quiet_move:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = move.Piece;

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case capture:
        {
            if (board.mailbox[move.To] == get_piece(r, 1 - side))
            {
                if (getFile(move.To) == 0)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackQueenCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteQueenCastle);
                        }
                    }
                }
                else if (getFile(move.To) == 7)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackKingCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteKingCastle);
                        }
                    }
                }
            }
            int captured_piece = board.mailbox[move.To];
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = move.Piece;

            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case king_castle:
        {
            int rookSquare;
            if (side == White)
            {
                rookSquare = h1;
            }
            else
            {
                rookSquare = h8;
            }

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.bitboards[get_piece(r, side)] &= ~(1ULL << rookSquare);
            board.bitboards[get_piece(r, side)] |= (1ULL << (rookSquare - 2));

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << rookSquare);
            board.occupancies[side] |= (1ULL << (rookSquare - 2));

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << rookSquare);
            board.occupancies[Both] |= (1ULL << (rookSquare - 2));

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = move.Piece;

            int rook = board.mailbox[rookSquare];

            board.mailbox[rookSquare] = NO_PIECE;

            board.mailbox[rookSquare - 2] = get_piece(r, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case queen_castle:
        {
            int rookSquare;
            if (side == White)
            {
                rookSquare = a1;
            }
            else
            {
                rookSquare = a8;
            }

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.bitboards[get_piece(r, side)] &= ~(1ULL << rookSquare);
            board.bitboards[get_piece(r, side)] |= (1ULL << (rookSquare + 3));

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << rookSquare);
            board.occupancies[side] |= (1ULL << (rookSquare + 3));

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << rookSquare);
            board.occupancies[Both] |= (1ULL << (rookSquare + 3));

            board.mailbox[move.From] = NO_PIECE;
            board.mailbox[move.To] = move.Piece;

            int rook = board.mailbox[rookSquare];

            board.mailbox[rookSquare] = NO_PIECE;

            board.mailbox[rookSquare + 3] = get_piece(r, side);

            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case queen_promo:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(q, side)] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(q, side);

            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case rook_promo:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(r, side)] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(r, side);

            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case bishop_promo:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(b, side)] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(b, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }

            break;
        }
        case knight_promo:
        {
            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(n, side)] |= (1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(n, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case queen_promo_capture:
        {
            if (board.mailbox[move.To] == get_piece(r, 1 - side))
            {
                if (getFile(move.To) == 0)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackQueenCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteQueenCastle);
                        }
                    }
                }
                else if (getFile(move.To) == 7)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackKingCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteKingCastle);
                        }
                    }
                }
            }
            int captured_piece = board.mailbox[move.To];

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(q, side)] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(q, side);

            board.side = 1 - board.side;
            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case rook_promo_capture:
        {
            if (board.mailbox[move.To] == get_piece(r, 1 - side))
            {
                if (getFile(move.To) == 0)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackQueenCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteQueenCastle);
                        }
                    }
                }
                else if (getFile(move.To) == 7)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackKingCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteKingCastle);
                        }
                    }
                }
            }
            int captured_piece = board.mailbox[move.To];

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(r, side)] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(r, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case bishop_promo_capture:
        {
            if (board.mailbox[move.To] == get_piece(r, 1 - side))
            {
                if (getFile(move.To) == 0)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackQueenCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteQueenCastle);
                        }
                    }
                }
                else if (getFile(move.To) == 7)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackKingCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteKingCastle);
                        }
                    }
                }
            }
            int captured_piece = board.mailbox[move.To];

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(b, side)] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(b, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case knight_promo_capture:
        {
            if (board.mailbox[move.To] == get_piece(r, 1 - side))
            {
                if (getFile(move.To) == 0)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackQueenCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteQueenCastle);
                        }
                    }
                }
                else if (getFile(move.To) == 7)
                {
                    if (side == White)
                    {
                        if (getRank(move.To) == 7)
                        {
                            board.castle &= ~(BlackKingCastle);
                        }
                    }
                    else
                    {
                        if (getRank(move.To) == 0)
                        {
                            board.castle &= ~(WhiteKingCastle);
                        }
                    }
                }
            }
            int captured_piece = board.mailbox[move.To];

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[get_piece(n, side)] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << move.To);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << move.To);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = get_piece(n, side);

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
        case ep_capture:
        {
            int capture_square;
            if (side == White)
            {
                capture_square = move.To + 8;
            }
            else
            {
                capture_square = move.To - 8;
            }

            int captured_piece = board.mailbox[capture_square];

            board.bitboards[move.Piece] &= ~(1ULL << move.From);
            board.bitboards[move.Piece] |= (1ULL << move.To);

            board.bitboards[captured_piece] &= ~(1ULL << capture_square);

            board.occupancies[side] &= ~(1ULL << move.From);
            board.occupancies[side] |= (1ULL << move.To);

            board.occupancies[1 - side] &= ~(1ULL << capture_square);

            board.occupancies[Both] &= ~(1ULL << move.From);
            board.occupancies[Both] &= ~(1ULL << capture_square);
            board.occupancies[Both] |= (1ULL << move.To);

            board.mailbox[move.From] = NO_PIECE;

            board.mailbox[move.To] = move.Piece;

            board.mailbox[capture_square] = NO_PIECE;

            board.side = 1 - board.side;

            if (is_move_irreversible(move))
            {
                board.halfmove = 0;
                board.lastIrreversiblePly = board.history.size();
            }
            break;
        }
    }
}

void UnmakeMove(Board& board, Move move, int captured_piece)
{
    int side = 1 - board.side;
    // change castling flag

    if (move.Type == quiet_move || move.Type == double_pawn_push)
    {
        //Console.WriteLine("q");
        //Console.WriteLine(CoordinatesToChessNotation(move.From) + "," + CoordinatesToChessNotation(move.To));
        //update piece bitboard
        board.bitboards[move.Piece] &= ~(1ULL << move.To);
        board.bitboards[move.Piece] |= (1ULL << move.From);

        //update moved piece occupancy
        board.occupancies[side] &= ~(1ULL << move.To);
        board.occupancies[side] |= (1ULL << move.From);

        //update both occupancy
        board.occupancies[Both] &= ~(1ULL << move.To);
        board.occupancies[Both] |= (1ULL << move.From);
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == capture)
    {
        //update piece bitboard
        //int captured_piece = board.mailbox[move.To];
        board.bitboards[move.Piece] &= ~(1ULL << move.To);
        board.bitboards[move.Piece] |= (1ULL << move.From);

        board.bitboards[captured_piece] |= (1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] &= ~(1ULL << move.To);
        board.occupancies[side] |= (1ULL << move.From);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << move.To);

        //update both occupancy
        //board.occupancies[Side.Both] &= ~(1UL << move.To);
        board.occupancies[Both] |= (1ULL << move.From);
        //update mailbox
        board.mailbox[move.To] = captured_piece;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == king_castle)
    {
        //update castling right & find rook square

        int rookSquare;
        if (side == White)
        {
            rookSquare = h1;
            //board.castle &= ~WhiteKingCastle;
        }
        else
        {
            rookSquare = h8;
            //board.castle &= ~BlackKingCastle;
        }

        board.bitboards[move.Piece] &= ~(1ULL << move.To);
        board.bitboards[move.Piece] |= (1ULL << move.From);

        board.bitboards[get_piece(r, side)] &= ~(1ULL << (rookSquare - 2));
        board.bitboards[get_piece(r, side)] |= (1ULL << (rookSquare));

        //update moved piece occupancy
        board.occupancies[side] &= ~(1ULL << move.To);
        board.occupancies[side] |= (1ULL << move.From);

        board.occupancies[side] &= ~(1ULL << (rookSquare - 2));
        board.occupancies[side] |= (1ULL << (rookSquare));

        //update both occupancy
        board.occupancies[Both] &= ~(1ULL << move.To);
        board.occupancies[Both] |= (1ULL << move.From);

        board.occupancies[Both] &= ~(1ULL << (rookSquare - 2));
        board.occupancies[Both] |= (1ULL << (rookSquare));
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;

        board.mailbox[rookSquare - 2] = NO_PIECE;
        board.mailbox[rookSquare] = get_piece(r, side);
    }
    else if (move.Type == queen_castle)
    {
        //update castling right & find rook square

        int rookSquare;
        if (side == White)
        {
            rookSquare = a1;
            //board.castle &= ~WhiteKingCastle;
        }
        else
        {
            rookSquare = a8;
            //board.castle &= ~BlackKingCastle;
        }
        //Console.WriteLine(CoordinatesToChessNotation(rookSquare));

        board.bitboards[move.Piece] &= ~(1ULL << move.To);
        board.bitboards[move.Piece] |= (1ULL << move.From);

        board.bitboards[get_piece(r, side)] &= ~(1ULL << (rookSquare + 3));
        board.bitboards[get_piece(r, side)] |= (1ULL << (rookSquare));

        //update moved piece occupancy
        board.occupancies[side] &= ~(1ULL << move.To);
        board.occupancies[side] |= (1ULL << move.From);

        board.occupancies[side] &= ~(1ULL << (rookSquare + 3));
        board.occupancies[side] |= (1ULL << (rookSquare));

        //update both occupancy
        board.occupancies[Both] &= ~(1ULL << move.To);
        board.occupancies[Both] |= (1ULL << move.From);

        board.occupancies[Both] &= ~(1ULL << (rookSquare + 3));
        board.occupancies[Both] |= (1ULL << (rookSquare));
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;

        board.mailbox[rookSquare + 3] = NO_PIECE;
        board.mailbox[rookSquare] = get_piece(r, side);
    }
    else if (move.Type == queen_promo)
    {
        //update piece bitboard
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(q, side)] &= ~(1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update both occupancy
        board.occupancies[Both] |= (1ULL << move.From);
        board.occupancies[Both] &= ~(1ULL << move.To);
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == rook_promo)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(r, side)] &= ~(1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update both occupancy
        board.occupancies[Both] |= (1ULL << move.From);
        board.occupancies[Both] &= ~(1ULL << move.To);
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == bishop_promo)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(b, side)] &= ~(1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update both occupancy
        board.occupancies[Both] |= (1ULL << move.From);
        board.occupancies[Both] &= ~(1ULL << move.To);
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == knight_promo)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(n, side)] &= ~(1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update both occupancy
        board.occupancies[Both] |= (1ULL << move.From);
        board.occupancies[Both] &= ~(1ULL << move.To);
        //update mailbox
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == queen_promo_capture)
    {
        //int captured_piece = board.mailbox[move.To];
        //update piece bitboard

        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(q, side)] &= ~(1ULL << move.To);

        board.bitboards[captured_piece] |= (1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << move.To);

        //update both occupancy
        //board.occupancies[Side.Both] &= ~(1UL << move.From);
        board.occupancies[Both] |= (1ULL << move.From);

        //update mailbox
        board.mailbox[move.To] = captured_piece;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == rook_promo_capture)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(r, side)] &= ~(1ULL << move.To);

        board.bitboards[captured_piece] |= (1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << move.To);

        //update both occupancy
        //board.occupancies[Side.Both] &= ~(1UL << move.From);
        board.occupancies[Both] |= (1ULL << move.From);

        //update mailbox
        board.mailbox[move.To] = captured_piece;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == bishop_promo_capture)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(b, side)] &= ~(1ULL << move.To);

        board.bitboards[captured_piece] |= (1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << move.To);

        //update both occupancy
        //board.occupancies[Side.Both] &= ~(1UL << move.From);
        board.occupancies[Both] |= (1ULL << move.From);

        //update mailbox
        board.mailbox[move.To] = captured_piece;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == knight_promo_capture)
    {
        board.bitboards[move.Piece] |= (1ULL << move.From);
        board.bitboards[get_piece(n, side)] &= ~(1ULL << move.To);

        board.bitboards[captured_piece] |= (1ULL << move.To);

        //update moved piece occupancy
        board.occupancies[side] |= (1ULL << move.From);
        board.occupancies[side] &= ~(1ULL << move.To);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << move.To);

        //update both occupancy
        //board.occupancies[Side.Both] &= ~(1UL << move.From);
        board.occupancies[Both] |= (1ULL << move.From);

        //update mailbox
        board.mailbox[move.To] = captured_piece;
        board.mailbox[move.From] = move.Piece;
    }
    else if (move.Type == ep_capture)
    {
        int capture_square;
        int captured_pawn = get_piece(p, 1 - side);
        if (side == White)
        {
            capture_square = move.To + 8;
        }
        else
        {
            capture_square = move.To - 8;
        }

        //int captured_piece = board.mailbox[capture_square];
        //update piece bitboard
        board.bitboards[move.Piece] &= ~(1ULL << move.To);
        board.bitboards[move.Piece] |= (1ULL << move.From);

        board.bitboards[captured_pawn] |= (1ULL << capture_square);

        //update moved piece occupancy
        board.occupancies[side] &= ~(1ULL << move.To);
        board.occupancies[side] |= (1ULL << move.From);

        //update captured piece occupancy
        board.occupancies[1 - side] |= (1ULL << capture_square);

        //update both occupancy
        board.occupancies[Both] &= ~(1ULL << move.To);
        board.occupancies[Both] |= (1ULL << capture_square);
        board.occupancies[Both] |= (1ULL << move.From);

        //update mailbox
        board.mailbox[move.From] = move.Piece;
        board.mailbox[move.To] = NO_PIECE;
        board.mailbox[capture_square] = captured_pawn;
    }

    board.side = 1 - board.side;
}

void GeneratePseudoLegalMoves(MoveList& MoveList, Board& board)
{
    MoveList.clear();

    GenerateQueenMoves(MoveList, board);
    GenerateRookMoves(MoveList, board);
    GenerateKnightMoves(MoveList, board);
    GenerateBishopMoves(MoveList, board);

    GeneratePawnMoves(MoveList, board);
    GenerateKingMoves(MoveList, board);
}
bool IsSquareAttacked(int square, int side, const Board& board, uint64_t occupancy)
{
    const int bishop = (side == White) ? B : b;
    const int rook = (side == White) ? R : r;
    const int queen = (side == White) ? Q : q;
    const int knight = (side == White) ? N : n;
    const int king = (side == White) ? K : k;
    const int pawn = (side == White) ? P : p;

    if (get_bishop_attacks(square, occupancy) & (board.bitboards[bishop] | board.bitboards[queen]))
        return true;

    if (get_rook_attacks(square, occupancy) & (board.bitboards[rook] | board.bitboards[queen]))
        return true;

    if (knight_attacks[square] & board.bitboards[knight])
        return true;

    if (pawn_attacks[1 - side][square] & board.bitboards[pawn])
        return true;

    if (king_attacks[square] & board.bitboards[king])
        return true;

    return false;
}
uint64_t GetAttackedSquares(int side, Board& board, uint64_t occupancy)
{
    uint64_t attack_map = 0ULL;

    const int piece_indices[6] = {
        (side == White ? K : k),
        (side == White ? N : n),
        (side == White ? B : b),
        (side == White ? R : r),
        (side == White ? Q : q),
        (side == White ? P : p),
    };

    uint64_t kingBB = board.bitboards[piece_indices[0]];
    uint64_t knightBB = board.bitboards[piece_indices[1]];
    uint64_t bishopBB = board.bitboards[piece_indices[2]];
    uint64_t rookBB = board.bitboards[piece_indices[3]];
    uint64_t queenBB = board.bitboards[piece_indices[4]];
    uint64_t pawnBB = board.bitboards[piece_indices[5]];

    // King
    if (kingBB)
        attack_map |= king_attacks[get_ls1b(kingBB)];

    // Knights
    while (knightBB)
    {
        int sq = get_ls1b(knightBB);
        attack_map |= knight_attacks[sq];
        Pop_bit(knightBB, sq);
    }

    // Bishops
    while (bishopBB)
    {
        int sq = get_ls1b(bishopBB);
        attack_map |= get_bishop_attacks(sq, occupancy);
        Pop_bit(bishopBB, sq);
    }

    // Rooks
    while (rookBB)
    {
        int sq = get_ls1b(rookBB);
        attack_map |= get_rook_attacks(sq, occupancy);
        Pop_bit(rookBB, sq);
    }

    // Queens
    while (queenBB)
    {
        int sq = get_ls1b(queenBB);
        attack_map |= get_queen_attacks(sq, occupancy);
        Pop_bit(queenBB, sq);
    }

    // Pawns
    while (pawnBB)
    {
        int sq = get_ls1b(pawnBB);
        attack_map |= pawn_attacks[side][sq];
        Pop_bit(pawnBB, sq);
    }

    return attack_map;
}

bool isLegal(Move& move, Board& board)
{
    uint64_t kingSquare = (board.bitboards[get_piece(K, 1 - board.side)]);

    if (move.Type != king_castle && move.Type != queen_castle)
    {
        if (!IsSquareAttacked(get_ls1b(kingSquare), board.side, board, board.occupancies[Both]))
        {
            //std::cout << (1);
            return true;
        }
        return false;
    }
    else
    {
        uint64_t attacked_squares = GetAttackedSquares(board.side, board, board.occupancies[Both]);
        if (move.Type == king_castle)
        {
            uint64_t NoAttackArea;
            if (1 - board.side == White)
            {
                NoAttackArea = WhiteKingCastleEmpty | (1ULL << e1);
            }
            else
            {
                NoAttackArea = BlackKingCastleEmpty | (1ULL << e8);
            }
            if ((attacked_squares & kingSquare) == 0 && (attacked_squares & NoAttackArea) == 0)
            {
                return true;
            }
            return false;
        }
        else // queen castle
        {
            uint64_t NoAttackArea;
            if (1 - board.side == White)
            {
                NoAttackArea = WhiteQueenCastleAttack | (1ULL << e1);
            }
            else
            {
                NoAttackArea = BlackQueenCastleAttack | (1ULL << e8);
            }
            if ((attacked_squares & kingSquare) == 0 && (attacked_squares & NoAttackArea) == 0)
            {
                return true;
            }
            return false;
        }

        return false;
    }
}
bool is_in_check(Board& board)
{
    if (IsSquareAttacked(
            get_ls1b(board.side == White ? board.bitboards[K] : board.bitboards[k]),
            1 - board.side,
            board,
            board.occupancies[Both]
        ))
    {
        return true;
    }
    return false;
}
uint64_t all_attackers_to_square(Board& board, uint64_t occupied, int sq)
{
    // When performing a static exchange evaluation we need to find all
    // attacks to a given square, but we also are given an updated occupied
    // bitboard, which will likely not match the actual board, as pieces are
    // removed during the iterations in the static exchange evaluation

    return (pawn_attacks[White][sq] & board.bitboards[p]) | (pawn_attacks[Black][sq] & board.bitboards[P])
         | (knight_attacks[sq] & (board.bitboards[n] | board.bitboards[N]))
         | (get_bishop_attacks(sq, occupied)
            & ((board.bitboards[b] | board.bitboards[B]) | (board.bitboards[q] | board.bitboards[Q])))
         | (get_rook_attacks(sq, occupied)
            & ((board.bitboards[r] | board.bitboards[R]) | (board.bitboards[q] | board.bitboards[Q])))
         | (king_attacks[sq] & (board.bitboards[k] | board.bitboards[K]));

    return 0ULL;
}
bool IsOnlyKingPawn(Board& board)
{
    return (board.occupancies[Both]
            & ~(board.bitboards[P] | board.bitboards[p] | board.bitboards[K] | board.bitboards[k]))
        == 0ULL;
}
std::string getCastlingRights(const Board& board)
{
    std::string rights = "";
    if (board.castle & WhiteKingCastle)
        rights += "K"; // White kingside castling
    if (board.castle & WhiteQueenCastle)
        rights += "Q"; // White queenside castling
    if (board.castle & BlackKingCastle)
        rights += "k"; // Black kingside castling
    if (board.castle & BlackQueenCastle)
        rights += "q"; // Black queenside castling
    return (rights.empty()) ? "-" : rights;
}
std::string boardToFEN(const Board& board)
{
    // Step 1: Construct the piece placement part (ranks 8 to 1)
    std::string piecePlacement = "";
    for (int rank = 7; rank >= 0; --rank)
    { // Fix: Iterate from 7 down to 0
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file)
        {
            int square = (7 - rank) * 8 + file;
            int piece = board.mailbox[square];

            if (piece == NO_PIECE)
            {
                ++emptyCount; // Empty square
            }
            else
            {
                if (emptyCount > 0)
                {
                    piecePlacement += std::to_string(emptyCount); // Insert the number of empty squares
                    emptyCount = 0;
                }
                piecePlacement += getCharFromPiece(piece); // Add the piece (e.g., 'p', 'R')
            }
        }

        if (emptyCount > 0)
        {
            piecePlacement += std::to_string(emptyCount); // End of rank with empty squares
        }

        if (rank > 0)
        { // Fix: Only add '/' if it's not the last rank (rank 1)
            piecePlacement += "/";
        }
    }

    // Step 2: Construct the other parts of FEN
    std::string sideToMove = (board.side == 0) ? "w" : "b"; // White or Black to move
    std::string castlingRights = getCastlingRights(board);  // Castling rights
    std::string enPassant =
        (board.enpassent == NO_SQ) ? "-" : CoordinatesToChessNotation(board.enpassent); // En passant square
    std::string halfmove = std::to_string(board.halfmove);                              // Halfmove clock
    std::string fullmove = std::to_string(board.history.size() / 2 + 1);                // Fullmove number

    // Step 3: Combine all parts into the final FEN string
    std::string fen =
        piecePlacement + " " + sideToMove + " " + castlingRights + " " + enPassant + " " + halfmove + " " + fullmove;

    return fen;
}
