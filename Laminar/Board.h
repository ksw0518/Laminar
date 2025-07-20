#pragma once
#include <cstdint>

#include <string>
#include <vector>
int getPieceFromChar(char pieceChar);

char getCharFromPiece(int piece);
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
void PrintBoards(Board board);
void print_mailbox(int mailbox[]);
int get_castle(uint8_t castle);
std::string CoordinatesToChessNotation(int square);