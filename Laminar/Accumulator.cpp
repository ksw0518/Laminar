#include "Accumulator.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"

void resetAccumulators(const Board& board, AccumulatorPair& accumulator)
{
    uint64_t whitePieces = board.occupancies[White];
    uint64_t blackPieces = board.occupancies[Black];

    memcpy(accumulator.white.values, EvalNetwork.accumulator_biases, sizeof(EvalNetwork.accumulator_biases));
    memcpy(accumulator.black.values, EvalNetwork.accumulator_biases, sizeof(EvalNetwork.accumulator_biases));

    while (whitePieces)
    {
        int sq = get_ls1b(whitePieces);

        uint16_t whiteInputFeature = calculateIndex(White, sq, get_piece(board.mailbox[sq], White), White, false);
        uint16_t blackInputFeature = calculateIndex(Black, sq, get_piece(board.mailbox[sq], White), White, false);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.white.values[i] += EvalNetwork.accumulator_weights[whiteInputFeature][i];
            accumulator.black.values[i] += EvalNetwork.accumulator_weights[blackInputFeature][i];
        }

        Pop_bit(whitePieces, sq);
    }
    while (blackPieces)
    {
        int sq = get_ls1b(blackPieces);

        uint16_t whiteInputFeature = calculateIndex(White, sq, get_piece(board.mailbox[sq], White), Black, false);
        uint16_t blackInputFeature = calculateIndex(Black, sq, get_piece(board.mailbox[sq], White), Black, false);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.white.values[i] += EvalNetwork.accumulator_weights[whiteInputFeature][i];
            accumulator.black.values[i] += EvalNetwork.accumulator_weights[blackInputFeature][i];
        }
        Pop_bit(blackPieces, sq);
    }
}
void resetWhiteAccumulator(const Board& board, AccumulatorPair& accumulator, bool flipFile)
{
    uint64_t whitePieces = board.occupancies[White];
    uint64_t blackPieces = board.occupancies[Black];

    memcpy(accumulator.white.values, EvalNetwork.accumulator_biases, sizeof(EvalNetwork.accumulator_biases));

    while (whitePieces)
    {
        int sq = get_ls1b(whitePieces);

        uint16_t whiteInputFeature = calculateIndex(White, sq, get_piece(board.mailbox[sq], White), White, flipFile);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.white.values[i] += EvalNetwork.accumulator_weights[whiteInputFeature][i];
        }

        Pop_bit(whitePieces, sq);
    }
    while (blackPieces)
    {
        int sq = get_ls1b(blackPieces);

        uint16_t whiteInputFeature = calculateIndex(White, sq, get_piece(board.mailbox[sq], White), Black, flipFile);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.white.values[i] += EvalNetwork.accumulator_weights[whiteInputFeature][i];
        }
        Pop_bit(blackPieces, sq);
    }
}
void resetBlackAccumulator(const Board& board, AccumulatorPair& accumulator, bool flipFile)
{
    uint64_t whitePieces = board.occupancies[White];
    uint64_t blackPieces = board.occupancies[Black];
    memcpy(accumulator.black.values, EvalNetwork.accumulator_biases, sizeof(EvalNetwork.accumulator_biases));

    while (whitePieces)
    {
        int sq = get_ls1b(whitePieces);
        uint16_t blackInputFeature = calculateIndex(Black, sq, get_piece(board.mailbox[sq], White), White, flipFile);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.black.values[i] += EvalNetwork.accumulator_weights[blackInputFeature][i];
        }
        Pop_bit(whitePieces, sq);
    }
    while (blackPieces)
    {
        int sq = get_ls1b(blackPieces);
        uint16_t blackInputFeature = calculateIndex(Black, sq, get_piece(board.mailbox[sq], White), Black, flipFile);
        for (size_t i = 0; i < HL_SIZE; i++)
        {
            accumulator.black.values[i] += EvalNetwork.accumulator_weights[blackInputFeature][i];
        }
        Pop_bit(blackPieces, sq);
    }
}
int flipHorizontal(int square)
{
    return square ^ 7;
}
int flipSquare(int square) //flip square so a1 = 0
{
    return square ^ 56;
}
int calculateIndex(int perspective, int square, int pieceType, int side, bool mirror)
{
    square ^= 56;
    if (perspective == 1)
    {
        square = flipSquare(square);
    }
    if (mirror)
    {
        square = flipHorizontal(square);
    }
    return 6 * 64 * (side != perspective) + 64 * pieceType + square;
}
void accumulatorAdd(struct Network* const network, struct Accumulator* accumulator, size_t index)
{
    for (int i = 0; i < HL_SIZE; i++)
        accumulator->values[i] += network->accumulator_weights[index][i];
}

void accumulatorSub(struct Network* const network, struct Accumulator* accumulator, size_t index)
{
    for (int i = 0; i < HL_SIZE; i++)
        accumulator->values[i] -= network->accumulator_weights[index][i];
}