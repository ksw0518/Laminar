#pragma once
enum Square {
    a8 = 0, b8, c8, d8, e8, f8, g8, h8,
    a7 = 8, b7, c7, d7, e7, f7, g7, h7,
    a6 = 16, b6, c6, d6, e6, f6, g6, h6,
    a5 = 24, b5, c5, d5, e5, f5, g5, h5,
    a4 = 32, b4, c4, d4, e4, f4, g4, h4,
    a3 = 40, b3, c3, d3, e3, f3, g3, h3,
    a2 = 48, b2, c2, d2, e2, f2, g2, h2,
    a1 = 56, b1, c1, d1, e1, f1, g1, h1,

    NO_SQ = 64
};
constexpr int P = 0;
constexpr int N = 1;
constexpr int B = 2;
constexpr int R = 3;
constexpr int Q = 4;
constexpr int K = 5;
constexpr int p = 6;
constexpr int n = 7;
constexpr int b = 8;
constexpr int r = 9;
constexpr int q = 10;
constexpr int k = 11;
constexpr int NO_PIECE = 12;

constexpr uint8_t WhiteKingCastle = 0b0001;
constexpr uint8_t WhiteQueenCastle = 0b0010;
constexpr uint8_t BlackKingCastle = 0b0100;
constexpr uint8_t BlackQueenCastle = 0b1000;

constexpr int White = 0;
constexpr int Black = 1;
constexpr int Both = 2;

constexpr uint8_t promotionFlag = 0b1000;
constexpr uint8_t captureFlag = 0b0100;
constexpr uint8_t special1Flag = 0b0010;
constexpr uint8_t special0Flag = 0b0001;

constexpr uint8_t quiet_move = 0;
constexpr uint8_t double_pawn_push = special0Flag;
constexpr uint8_t king_castle = special1Flag;
constexpr uint8_t queen_castle = special0Flag | special1Flag;
constexpr uint8_t capture = captureFlag;
constexpr uint8_t ep_capture = captureFlag | special0Flag;
constexpr uint8_t knight_promo = promotionFlag;
constexpr uint8_t bishop_promo = promotionFlag | special0Flag;
constexpr uint8_t rook_promo = promotionFlag | special1Flag;
constexpr uint8_t queen_promo = promotionFlag | special1Flag | special0Flag;
constexpr uint8_t knight_promo_capture = knight_promo | capture;
constexpr uint8_t bishop_promo_capture = bishop_promo | capture;
constexpr uint8_t rook_promo_capture = rook_promo | capture;
constexpr uint8_t queen_promo_capture = queen_promo | capture;

constexpr int Side_value[] = { 0, 6 };
constexpr int Get_Whitepiece[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };


constexpr uint64_t WhiteKingCastleEmpty = (1ULL << f1) | (1ULL << g1);
constexpr uint64_t WhiteQueenCastleEmpty = (1ULL << d1) | (1ULL << c1) | (1ULL << b1);
constexpr uint64_t BlackKingCastleEmpty = (1ULL << f8) | (1ULL << g8);
constexpr uint64_t BlackQueenCastleEmpty = (1ULL << d8) | (1ULL << c8) | (1ULL << b8);
constexpr uint64_t WhiteQueenCastleAttack = (1ULL << d1) | (1ULL << c1);
constexpr uint64_t BlackQueenCastleAttack = (1ULL << d8) | (1ULL << c8);
inline int get_piece(int piece, int col)
{
    return Get_Whitepiece[piece] + Side_value[col];
}