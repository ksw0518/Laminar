#include "Accumulator.h"
#include "Bit.h"
#include "Const.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

Network EvalNetwork;

static inline const uint16_t Le = 1;
static inline const bool IS_LITTLE_ENDIAN = *reinterpret_cast<const char*>(&Le) == 1;
template <typename IntType>
inline IntType readLittleEndian(std::istream& stream)
{
    IntType result;

    if (IS_LITTLE_ENDIAN)
        stream.read(reinterpret_cast<char*>(&result), sizeof(IntType));
    else
    {
        std::uint8_t u[sizeof(IntType)];
        std::make_unsigned_t<IntType> v = 0;

        stream.read(reinterpret_cast<char*>(u), sizeof(IntType));
        for (size_t i = 0; i < sizeof(IntType); ++i)
            v = (v << 8) | u[sizeof(IntType) - i - 1];

        std::memcpy(&result, &v, sizeof(IntType));
    }

    return result;
}
void LoadNetwork(const std::string& filepath)
{
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream.is_open())
    {
        std::cerr << "Failed to open file: " << filepath << std::endl;
    }
    // Load weightsToHL
    for (size_t row = 0; row < INPUT_SIZE; ++row)
    {
        for (size_t col = 0; col < HL_SIZE; ++col)
        {
            EvalNetwork.accumulator_weights[row][col] = readLittleEndian<int16_t>(stream);
        }
    }
    // Load hiddenLayerBias
    for (size_t i = 0; i < HL_SIZE; ++i)
    {
        EvalNetwork.accumulator_biases[i] = readLittleEndian<int16_t>(stream);
    }
    // Load weightsToOut
    for (size_t i = 0; i < 2 * HL_SIZE; ++i)
    {
        EvalNetwork.output_weights[i] = readLittleEndian<int16_t>(stream);
    }
    // Load outputBias
    EvalNetwork.output_bias = readLittleEndian<int16_t>(stream);
}
int32_t SCReLU(int32_t value, int32_t min, int32_t max)
{
    const int32_t clamped = std::clamp(value, min, max);
    return clamped * clamped;
}
//SCReLU activation function
int32_t activation(int16_t value)
{
    return SCReLU(value, 0, QA);
}
int32_t autovec_screlu(Network const* network, Accumulator const* stm, Accumulator const* nstm)
{
    std::int32_t accumulator{};
    for (int i = 0; i < HL_SIZE; i++)
    {
        accumulator += (int32_t)activation(stm->values[i]) * network->output_weights[i];
        accumulator += (int32_t)activation(nstm->values[i]) * network->output_weights[i + HL_SIZE];
    }
    return accumulator;
}

int32_t forward(
    struct Network* const network,
    struct Accumulator* const stm_accumulator,
    struct Accumulator* const nstm_accumulator
)
{
    int32_t eval = autovec_screlu(network, stm_accumulator, nstm_accumulator);

    eval /= QA;
    eval += network->output_bias;

    eval *= SCALE;
    eval /= QA * QB;

    return eval;
}