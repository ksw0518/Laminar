#include "Bench.h"
#include "Bit.h"
#include "Board.h"
#include "Evaluation.h"
#include "Movegen.h"
#include "Search.h"
#include "Threading.h"
#include "Transpositions.h"
#include "Tuneables.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

const std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const std::string KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
Board mainBoard;
std::vector<std::string> position_commands = {"position", "startpos", "fen", "moves"};
std::vector<std::string> go_commands = {"go", "movetime", "wtime", "btime", "winc", "binc", "movestogo"};
std::vector<std::string> option_commands = {"setoption", "name", "value"};
int threadCount = 1;

std::string trim(const std::string& str)
{
    const std::string whitespace = " \t\n\r\f\v";
    const auto start = str.find_first_not_of(whitespace);
    const auto end = str.find_last_not_of(whitespace);

    if (start == std::string::npos || end == std::string::npos)
    {
        return "";
    }

    return str.substr(start, end - start + 1);
}
std::string TryGetLabelledValue(
    const std::string& text,
    const std::string& label,
    const std::vector<std::string>& allLabels,
    const std::string& defaultValue = ""
)
{
    // Trim leading and trailing whitespace
    std::string trimmedText = trim(text);

    // Find the position of the label in the trimmed text
    size_t labelPos = trimmedText.find(label);
    if (labelPos != std::string::npos)
    {
        // Determine the start position of the value
        size_t valueStart = labelPos + label.length();
        size_t valueEnd = trimmedText.length();

        // Iterate through allLabels to find the next label position
        for (const std::string& otherID : allLabels)
        {
            if (otherID != label)
            {
                size_t otherIDPos = trimmedText.find(otherID, valueStart);
                if (otherIDPos != std::string::npos && otherIDPos < valueEnd)
                {
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
int64_t TryGetLabelledValueInt(
    const std::string& text,
    const std::string& label,
    const std::vector<std::string>& allLabels,
    int64_t defaultValue = -1
)
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
    catch (const std::invalid_argument&)
    {
        // If conversion fails, return the default value
        return defaultValue;
    }
}
static void InitAll(ThreadData& data)
{
    InitializeLeaper();
    init_sliders_attacks(1);
    init_sliders_attacks(0);
    init_tables();
    init_random_keys();
    InitializeLMRTable();
    InitializeSearch(data);
    InitNNUE();
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
    AccumulatorPair last_accumulator = board.accumulator;
    for (int i = 0; i < move_list.count; ++i)
    {
        Move& move = move_list.moves[i];
        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];

        uint64_t last_zobrist = board.zobristKey;
        refresh_if_cross(move, board);
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

        board.zobristKey = last_zobrist;
        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;
        board.accumulator = last_accumulator;
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
int64_t CalculateHardLimit(int64_t time, int64_t incre)
{
    return time / 2;
}
int64_t CalculateSoftLimit(int64_t time, int64_t incre)
{
    return 0.6
         * (static_cast<float>(time) / static_cast<float>(20)
            + static_cast<float>(incre) * static_cast<float>(3) / static_cast<float>(4));
}
void PlayMoves(std::string& moves_string, Board& board)
{
    if (moves_string != "") // move is not empty
    {
        std::vector<std::string> moves_seperated = splitStringBySpace(moves_string);
        MoveList moveList;

        for (size_t i = 0; i < moves_seperated.size(); i++)
        {
            std::string From = std::string(1, moves_seperated[i][0]) + std::string(1, moves_seperated[i][1]);
            std::string To = std::string(1, moves_seperated[i][2]) + std::string(1, moves_seperated[i][3]);
            std::string promo = "";

            if (moves_seperated[i].size() > 4)
            {
                promo = std::string(1, (moves_seperated[i][4]));
            }
            Move move_to_play;
            move_to_play.From = GetSquare(From);
            move_to_play.To = GetSquare(To);

            moveList.clear();
            GeneratePseudoLegalMoves(moveList, board);

            for (size_t j = 0; j < moveList.count; j++)
            {
                if ((move_to_play.From == moveList.moves[j].From)
                    && (move_to_play.To == moveList.moves[j].To)) //found same move
                {
                    move_to_play = moveList.moves[j];
                    refresh_if_cross(move_to_play, board);
                    if ((moveList.moves[j].Type & knight_promo) != 0) // promo
                    {
                        if (promo == "q")
                        {
                            if ((moveList.moves[j].Type == queen_promo)
                                || (moveList.moves[j].Type == queen_promo_capture))
                            {
                                MakeMove(board, moveList.moves[j]);
                                break;
                            }
                        }
                        else if (promo == "r")
                        {
                            if ((moveList.moves[j].Type == rook_promo)
                                || (moveList.moves[j].Type == rook_promo_capture))
                            {
                                MakeMove(board, moveList.moves[j]);
                                break;
                            }
                        }
                        else if (promo == "b")
                        {
                            if ((moveList.moves[j].Type == bishop_promo)
                                || (moveList.moves[j].Type == bishop_promo_capture))
                            {
                                MakeMove(board, moveList.moves[j]);
                                break;
                            }
                        }
                        else if (promo == "n")
                        {
                            if ((moveList.moves[j].Type == knight_promo)
                                || (moveList.moves[j].Type == knight_promo_capture))
                            {
                                MakeMove(board, moveList.moves[j]);
                                break;
                            }
                        }
                    }
                    else
                    {
                        MakeMove(board, moveList.moves[j]);
                        break;
                    }
                }
            }
        }
    }
}
void ProcessUCI(std::string input, ThreadData& data, ThreadData* data_heap)
{
    std::vector<std::string> Commands = splitStringBySpace(input);
    std::string mainCommand = Commands[0];
    if (mainCommand == "uci")
    {
        std::cout << "id name Laminar"
                  << "\n";
        ;
        std::cout << "id author ksw0518"
                  << "\n";
        ;
        std::cout << "\n";
        std::cout << "option name Threads type spin default 1 min 1 max 1024\n";
        std::cout << "option name Hash type spin default 12 min 1 max 4096\n";
        std::cout << "uciok"
                  << "\n";

        for (int i = 0; i < AllTuneablesCount; i++)
        {
            std::cout << "option name " << AllTuneables[i]->name;
            std::cout << " type spin";
            std::cout << " default " << AllTuneables[i]->value;
            std::cout << " min " << AllTuneables[i]->minValue;
            std::cout << " max " << AllTuneables[i]->maxValue;
            std::cout << "\n";
        }
        IsUCI = true;
    }
    else if (mainCommand == "spsa")
    {
        for (int i = 0; i < AllTuneablesCount; i++)
        {
            std::cout << AllTuneables[i]->name;
            std::cout << ", int";
            std::cout << ", " << AllTuneables[i]->value;
            std::cout << ", " << AllTuneables[i]->minValue;
            std::cout << ", " << AllTuneables[i]->maxValue;
            std::cout << ", " << AllTuneables[i]->step;
            std::cout << ", "
                      << "0.002";
            std::cout << "\n";
        }
    }
    else if (mainCommand == "ucinewgame")
    {
        ClearTT();
        InitAll(data);
        persistentThreadData.clear();
        persistentThreadData.reserve(threadCount);
        for (int i = 0; i < threadCount; i++)
        {
            persistentThreadData.push_back(std::make_unique<ThreadData>());
            InitializeSearch(*persistentThreadData[i]);
        }
        allThreadDataPtrs.clear();
        allThreadDataPtrs.reserve(threadCount);
        for (int i = 0; i < threadCount; i++)
        {
            allThreadDataPtrs.push_back(persistentThreadData[i].get());
        }
    }
    else if (mainCommand == "isready")
    {
        std::cout << "readyok"
                  << "\n";
    }
    else if (mainCommand == "setoption")
    {
        std::string option = TryGetLabelledValue(input, "name", option_commands);
        int value = TryGetLabelledValueInt(input, "value", option_commands);

        if (option == "Hash")
        {
            Initialize_TT(value);
        }
        if (option == "Threads")
        {
            threadCount = value;

            if ((int)persistentThreadData.size() != threadCount)
            {
                persistentThreadData.clear();
                persistentThreadData.reserve(threadCount);
                for (int i = 0; i < threadCount; i++)
                {
                    persistentThreadData.push_back(std::make_unique<ThreadData>());
                    InitializeSearch(*persistentThreadData[i]);
                }
                allThreadDataPtrs.clear();
                allThreadDataPtrs.reserve(threadCount);
                for (int i = 0; i < threadCount; i++)
                {
                    allThreadDataPtrs.push_back(persistentThreadData[i].get());
                }
            }
        }
        else
        {
            for (int i = 0; i < AllTuneablesCount; i++)
            {
                if (option == AllTuneables[i]->name)
                {
                    AllTuneables[i]->value = value;
                }
            }
        }
    }
    else if (mainCommand == "quit")
    {
        delete data_heap;
        exit(0);
    }
    if (mainCommand == "go")
    {
        SearchLimitations searchLimits = SearchLimitations();
        int depth = MAXPLY;
        /*  if (Commands[1] == "gigachad")
        {
           
        }*/
        if (Commands.size() == 1 || Commands[1] == "infinite")
        {
            //IterativeDeepening(mainBoard, MAXPLY, searchLimits, data);
        }
        else if (Commands[1] == "depth")
        {
            depth = std::stoi(Commands[2]);
            // IterativeDeepening(mainBoard, depth, searchLimits, data);
        }
        else if (Commands[1] == "movetime")
        {
            int64_t movetime = std::stoll(Commands[2]);
            searchLimits.HardTimeLimit = movetime;
            //IterativeDeepening(mainBoard, MAXPLY, searchLimits, data);
        }
        else if (Commands[1] == "wtime" || Commands[1] == "btime")
        {
            int depth = (int)TryGetLabelledValueInt(input, "depth", go_commands);
            int64_t wtime = TryGetLabelledValueInt(input, "wtime", go_commands);
            int64_t btime = TryGetLabelledValueInt(input, "btime", go_commands);
            int64_t winc = TryGetLabelledValueInt(input, "winc", go_commands);
            int64_t binc = TryGetLabelledValueInt(input, "binc", go_commands);
            int64_t nodes = TryGetLabelledValueInt(input, "nodes", go_commands);

            int64_t hard_bound;
            int64_t soft_bound;

            int64_t time = mainBoard.side == White ? wtime : btime;
            int64_t incre = mainBoard.side == White ? winc : binc;

            hard_bound = CalculateHardLimit(time, incre);
            soft_bound = CalculateSoftLimit(time, incre);

            searchLimits.HardTimeLimit = hard_bound;

            searchLimits.SoftTimeLimit = soft_bound;
            searchLimits.HardNodeLimit = nodes;

            //IterativeDeepening(mainBoard, MAXPLY, searchLimits, data);
        }
        else if (Commands[1] == "nodes")
        {
            searchLimits.HardNodeLimit = TryGetLabelledValueInt(input, "nodes", go_commands);
        }
        RunSearchInMultipleThreads(mainBoard, depth, searchLimits, threadCount);

        if (Commands[1] == "perft")
        {
            int perftDepth = stoi(Commands[2]);
            auto start = std::chrono::high_resolution_clock::now();

            uint64_t nodes = Perft(mainBoard, perftDepth, perftDepth);
            auto end = std::chrono::high_resolution_clock::now();

            int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            float second = (float)(elapsedMS) / 1000;

            uint64_t nps = nodes / second + 1;

            std::cout << "nodes: " << (nodes) << " nps:" << std::fixed << std::setprecision(0) << nps;
            std::cout << "\n";
        }
    }
    else if (mainCommand == "position")
    {
        if (Commands[1] == "startpos")
        {
            if (Commands.size() == 2)
            {
                parse_fen(STARTPOS, mainBoard);
            }
            else
            {
                parse_fen(STARTPOS, mainBoard);

                std::string moves_in_string = TryGetLabelledValue(input, "moves", position_commands);
                PlayMoves(moves_in_string, mainBoard);
            }
        }
        else if (Commands[1] == "fen")
        {
            std::string fen = TryGetLabelledValue(input, "fen", position_commands);
            std::string moves = TryGetLabelledValue(input, "moves", position_commands);

            if (moves == "")
            {
                parse_fen(fen, mainBoard);
            }
            else
            {
                parse_fen(fen, mainBoard);
                std::string moves_in_string = TryGetLabelledValue(input, "moves", position_commands);
                PlayMoves(moves_in_string, mainBoard);
            }
        }
    }
    else if (mainCommand == "bench")
    {
        bench();
    }
    else if (mainCommand == "show")
    {
        PrintBoards(mainBoard);
        print_mailbox(mainBoard.mailbox);
    }
    else if (mainCommand == "moves")
    {
        std::string moves_in_string = TryGetLabelledValue(input, "moves", position_commands);
        PlayMoves(moves_in_string, mainBoard);
    }
    else if (mainCommand == "eval")
    {
        int eval = Evaluate(mainBoard);

        std::cout << ("evaluation: ") << eval << "cp ";
        if (mainBoard.side == White)
        {
            std::cout << ("(White's perspective)\n");
        }
        else
        {
            std::cout << ("(Black's perspective)\n");

            std::cout << ("White's perspective: ") << -eval << "cp \n";
        }
    }
}
int main(int argc, char* argv[])
{
    ThreadData* heapAllocated = new ThreadData(); // Allocate safely on heap
    ThreadData& data = *heapAllocated;

    InitAll(data);
    parse_fen(STARTPOS, mainBoard);
    Initialize_TT(32); //set initial TT size as 32mb
    threadCount = 1;
    persistentThreadData.clear();
    persistentThreadData.reserve(threadCount);
    for (int i = 0; i < threadCount; i++)
    {
        persistentThreadData.push_back(std::make_unique<ThreadData>());
        InitializeSearch(*persistentThreadData[i]);
    }
    allThreadDataPtrs.clear();
    allThreadDataPtrs.reserve(threadCount);
    for (int i = 0; i < threadCount; i++)
    {
        allThreadDataPtrs.push_back(persistentThreadData[i].get());
    }
    if (argc > 1)
    {
        IsUCI = true;
        ProcessUCI(argv[1], data, heapAllocated);
        delete heapAllocated;
        return 0;
    }
    IsUCI = false;
    while (true)
    {
        std::string input;

        std::getline(std::cin, input);
        if (input != "")
        {
            ProcessUCI(input, data, heapAllocated);
        }
    }
    return 0;
}