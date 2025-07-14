#pragma once
#include <cstdint>

#include <vector>
#include <string>
class Board
{
public:
    uint64_t bitboards[12];
    uint64_t occupancies[3];
    int mailbox[64];
    bool side;
    uint8_t enpassent;
    uint8_t castle;
    uint8_t halfmove;
    uint64_t zobristKey;
    std::vector<uint64_t> history;

    int lastIrreversiblePly = 0;
    Board();
};
void parse_fen(std::string fen, Board& board);
void PrintBoards(Board board);
void print_mailbox(int mailbox[]);
std::string CoordinatesToChessNotation(int square);