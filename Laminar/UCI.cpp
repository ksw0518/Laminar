#include "Movegen.h"
#include "Board.h"
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip> 

const std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const std::string KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
Board mainBoard;
std::vector<std::string> position_commands = { "position", "startpos", "fen", "moves" };

std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    const auto start = str.find_first_not_of(whitespace);
    const auto end = str.find_last_not_of(whitespace);

    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }

    return str.substr(start, end - start + 1);
}
std::string TryGetLabelledValue(const std::string& text, const std::string& label, const std::vector<std::string>& allLabels, const std::string& defaultValue = "") {

    // Trim leading and trailing whitespace
    std::string trimmedText = trim(text);

    // Find the position of the label in the trimmed text
    size_t labelPos = trimmedText.find(label);
    if (labelPos != std::string::npos) {
        // Determine the start position of the value
        size_t valueStart = labelPos + label.length();
        size_t valueEnd = trimmedText.length();

        // Iterate through allLabels to find the next label position
        for (const std::string& otherID : allLabels) {
            if (otherID != label) {
                size_t otherIDPos = trimmedText.find(otherID, valueStart);
                if (otherIDPos != std::string::npos && otherIDPos < valueEnd) {
                    valueEnd = otherIDPos;
                }
            }
        }

        // Extract the value and trim leading/trailing whitespace
        std::string value = trimmedText.substr(valueStart, valueEnd - valueStart);
        return trim(value);
    }

    return defaultValue;
}
int64_t TryGetLabelledValueInt(const std::string& text, const std::string& label, const std::vector<std::string>& allLabels, int64_t defaultValue = 0)
{
    // Helper function TryGetLabelledValue should be implemented as shown earlier
    std::string valueString = TryGetLabelledValue(text, label, allLabels, std::to_string(defaultValue));

    // Extract the first part of the valueString up to the first space
    std::istringstream iss(valueString);
    std::string firstPart;
    iss >> firstPart;

    // Try converting the extracted string to an integer
    try
    {
        return std::stoll(firstPart);
    }
    catch (const std::invalid_argument& e)
    {
        // If conversion fails, return the default value
        return defaultValue;
    }
}
static void InitAll()
{
    InitializeLeaper();
    init_sliders_attacks(1);
    init_sliders_attacks(0);
}
uint64_t Perft(Board& board, int depth, int perftDepth)
{

    if (depth == 0)
    {
        return 1ULL;
    }

    MoveList move_list;


    uint64_t nodes = 0;

    GeneratePseudoLegalMoves(move_list, board);
    for (int i = 0; i < move_list.count; ++i)
    {
        Move& move = move_list.moves[i];
        int lastEp = board.enpassent;
        uint64_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];

        uint64_t last_zobrist = board.zobristKey;

        MakeMove(board, move);

        if (isLegal(move, board))
        {
            uint64_t nodes_added = Perft(board, depth - 1, perftDepth);
            nodes += nodes_added;
            if (depth == perftDepth)
            {
                printMove(move);
                std::cout << ":";
                std::cout << nodes_added;
                std::cout << "\n";
            }
        }
    

        UnmakeMove(board, move, captured_piece);

        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;

    }
    return nodes;
}
std::vector<std::string> splitStringBySpace(const std::string& str)
{
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;

    while ((end = str.find(' ', start)) != std::string::npos)
    {
        if (end != start)
        { // Ignore multiple consecutive spaces
            tokens.push_back(str.substr(start, end - start));
        }
        start = end + 1;
    }

    // Push the last token if it's not empty
    if (start < str.size())
    {
        tokens.push_back(str.substr(start));
    }

    return tokens;
}

void ProcessUCI(std::string input)
{
    std::vector<std::string> Commands = splitStringBySpace(input);
    std::string mainCommand = Commands[0];
    if (mainCommand == "go")
    {
        if (Commands[1] == "perft")
        {
            int perftDepth = stoi(Commands[2]);
            auto start = std::chrono::high_resolution_clock::now();

            uint64_t nodes = Perft(mainBoard, perftDepth, perftDepth);
            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsedMS = end - start;

            float second = elapsedMS.count() / 1000;

            double nps = nodes / second;

            std::cout << "nodes: "<<(nodes)<<" nps:"<<std::fixed << std::setprecision(0) <<nps;
            std::cout << "\n";
        }
    }
    else if (mainCommand == "position")
    {
        std::string fen = TryGetLabelledValue(input, "fen", position_commands);
        parse_fen(fen, mainBoard);
        PrintBoards(mainBoard);
    }
}
int main()
{
    InitAll();
    parse_fen(STARTPOS, mainBoard);
    PrintBoards(mainBoard);
    MoveList moves;
    GeneratePseudoLegalMoves(moves, mainBoard);
    for (int i = 0; i < moves.count; i++)
    {
        printMove(moves.moves[i]);
        std::cout << "\n";
    }
    while (true)
    {
        std::string input;

        std::getline(std::cin, input);
        if (input != "")
        {
            ProcessUCI(input);
        }
    }
    //MakeMove(mainBoard, moves.moves[0]);
    //PrintBoards(mainBoard);
    return 0;
}