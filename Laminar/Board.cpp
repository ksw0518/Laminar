#include "Board.h"
#include "Const.h"
#include "Bit.h"
#include <string>
#include <cstring>
#include <iostream>

int getPieceFromChar(char pieceChar) {
    switch (pieceChar) {
    case 'P': return 0;
    case 'N': return 1;
    case 'B': return 2;
    case 'R': return 3;
    case 'Q': return 4;
    case 'K': return 5;
    case 'p': return 6;
    case 'n': return 7;
    case 'b': return 8;
    case 'r': return 9;
    case 'q': return 10;
    case 'k': return 11;
    default: return -1; // Default or error value
    }
}

char getCharFromPiece(int piece) {
    switch (piece) {
    case 0: return 'P';
    case 1: return 'N';
    case 2: return 'B';
    case 3: return 'R';
    case 4: return 'Q';
    case 5: return 'K';
    case 6: return 'p';
    case 7: return 'n';
    case 8: return 'b';
    case 9: return 'r';
    case 10: return 'q';
    case 11: return 'k';
    default: return -1; // Default or error value
    }
}
Board::Board()
    : side(0), enpassent((NO_SQ)), castle(0), halfmove(0), zobristKey(0) {
    std::memset(bitboards, 0, sizeof(bitboards));
    std::memset(occupancies, 0, sizeof(occupancies));
    std::memset(mailbox, 0, sizeof(mailbox));
    history.clear();
    history.reserve(256);
}
std::string CoordinatesToChessNotation(int square)
{
    int rawFile = square % 8;
    int rawRank = square == 0 ? 8 : 8 - square / 8;
    char File = (char)('a' + rawFile); // Convert column index to letter ('a' to 'h')
    int row = rawRank; // Row number (1 to 8)
    std::string str(1, File);
    return str + std::to_string(row);
}
int get_castle(uint8_t castle)
{
    int number = 0;

    // Set bits based on the presence of each castling right
    if ((castle & WhiteKingCastle) != 0) number |= 1 << 0; // Bit 0
    if ((castle & WhiteQueenCastle) != 0) number |= 1 << 1; // Bit 1
    if ((castle & BlackKingCastle) != 0) number |= 1 << 2; // Bit 2
    if ((castle & BlackQueenCastle) != 0) number |= 1 << 3; // Bit 3


    return number;
}
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

        if (fen[index] == 'K') board.castle |= WhiteKingCastle;
        if (fen[index] == 'Q') board.castle |= WhiteQueenCastle;
        if (fen[index] == 'k') board.castle |= BlackKingCastle;
        if (fen[index] == 'q') board.castle |= BlackQueenCastle;
        if (fen[index] == ' ') break;
        if (fen[index] == '-')
        {
            board.castle = 0;
            break;
        }

        index++;
    }
    index++;
    if (fen[index] == ' ') index++;
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
}
void PrintBoards(Board board)
{
    std::cout << ("\n");
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            if (file == 0)
            {
                std::cout << (" ") << (8 - rank) << (" ");
            }

            int piece = -1;

            for (int bb_piece = P; bb_piece <= k; bb_piece++)
            {
                if (Get_bit(board.bitboards[bb_piece], square))
                {
                    piece = bb_piece;
                }
            }
            std::cout << (" ") << ((piece == -1) ? '.' : getCharFromPiece(piece));
        }
        std::cout << ("\n");
    }

    std::cout << ("\n    a b c d e f g h");
    std::cout << ("\n    Side :     ") << ((board.side == 0 ? "w" : "b"));
    std::cout << ("\n    Enpassent :     ") << (board.enpassent != NO_SQ ? CoordinatesToChessNotation(board.enpassent) : "no");
    std::cout << ("\n    Castling :     ") << (((board.castle & WhiteKingCastle) != 0) ? 'K' : '-') << (((board.castle & WhiteQueenCastle) != 0) ? 'Q' : '-') << (((board.castle & BlackKingCastle) != 0) ? 'k' : '-') << (((board.castle & BlackQueenCastle) != 0) ? 'q' : '-');

    std::cout << "\n\n";
    std::cout << "    Castle_key:     " << get_castle(board.castle) << "\n";
    std::cout << ("\n");
}
void print_mailbox(int mailbox[])
{
    std::cout << ("\n MAILBOX \n");
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            if (file == 0)
            {
                std::cout << (" ") << (8 - rank) << " ";
            }
            //int piece = 0;
            if (mailbox[square] != NO_PIECE) //
            {

                std::cout << (" ") << getCharFromPiece(mailbox[square]);
            }
            else
            {
                std::cout << (" .");
            }
        }
        std::cout << ("\n");
    }

}
