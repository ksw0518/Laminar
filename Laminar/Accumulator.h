#pragma once
#include <cstddef>
#include <cstdint>

const int INPUT_SIZE = 768;
const int HL_SIZE = 1024;
const int SCALE = 400;
const int QA = 255;
const int QB = 64;

struct Network
{
    alignas(64) int16_t accumulator_weights[INPUT_SIZE][HL_SIZE];
    alignas(64) int16_t accumulator_biases[HL_SIZE];
    alignas(64) int16_t output_weights[2 * HL_SIZE];
    alignas(64) int16_t output_bias;
};

struct Accumulator
{
    alignas(64) int16_t values[HL_SIZE];
};
struct AccumulatorPair
{
    Accumulator white{};
    Accumulator black{};
};
extern Network EvalNetwork;

class Board;
int flipHorizontal(int square);
int flipSquare(int square);
int calculateIndex(int perspective, int square, int pieceType, int side, bool mirror);
void accumulatorAdd(struct Network* const network, struct Accumulator* accumulator, size_t index);
void accumulatorSub(struct Network* const network, struct Accumulator* accumulator, size_t index);
void resetAccumulators(const Board& board, AccumulatorPair& accumulator);
void resetWhiteAccumulator(const Board& board, AccumulatorPair& accumulator, bool flipFile);
void resetBlackAccumulator(const Board& board, AccumulatorPair& accumulator, bool flipFile);