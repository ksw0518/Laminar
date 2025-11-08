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
std::int32_t autovectorised_screlu(Network const* network, Accumulator const* stm, Accumulator const* nstm)
{
    std::int32_t accumulator{};
    for (int i = 0; i < HL_SIZE; i++)
    {
        accumulator += (int32_t)activation(stm->values[i]) * network->output_weights[i];
        accumulator += (int32_t)activation(nstm->values[i]) * network->output_weights[i + HL_SIZE];
    }
    return accumulator;
}

#if defined(__x86_64__) || defined(__amd64__) || (defined(_WIN64) && (defined(_M_X64) || defined(_M_AMD64)))
    #define is_x86 1
    #if defined(__AVX512F__) || defined(__AVX2__) || defined(__SSE__)
        #define has_simd 1
    #else
        #define has_simd 0
    #endif
#else
// TODO: arm
    #define is_x86 0
    #define has_simd 0
#endif

#if is_x86 && has_simd
    #undef __AVX512F__
    #include <immintrin.h>
#endif

std::int32_t vectorised_screlu(Network const* network, Accumulator const* stm, Accumulator const* nstm)
{
#if is_x86 && has_simd
    #if defined(__AVX512F__)
    using native_vector = __m512i;
        #define set1_epi16 _mm512_set1_epi16
        #define load_epi16 _mm512_load_si512
        #define min_epi16 _mm512_min_epi16
        #define max_epi16 _mm512_max_epi16
        #define madd_epi16 _mm512_madd_epi16
        #define mullo_epi16 _mm512_mullo_epi16
        #define add_epi32 _mm512_add_epi32
        #define reduce_epi32 _mm512_reduce_add_epi32
    #elif defined(__AVX2__)
    using native_vector = __m256i;
        #define set1_epi16 _mm256_set1_epi16
        #define load_epi16 [](auto ptr) { return _mm256_load_si256(reinterpret_cast<native_vector const*>(ptr)); }
        #define min_epi16 _mm256_min_epi16
        #define max_epi16 _mm256_max_epi16
        #define madd_epi16 _mm256_madd_epi16
        #define mullo_epi16 _mm256_mullo_epi16
        #define add_epi32 _mm256_add_epi32
        // based on output from zig 0.14.0 for @reduce(.Add, @as(@Vector(8, i32), x)
        #define reduce_epi32 \
            [](native_vector vec) \
            { \
                __m128i xmm1 = _mm256_extracti128_si256(vec, 1); \
                __m128i xmm0 = _mm256_castsi256_si128(vec); \
                xmm0 = _mm_add_epi32(xmm0, xmm1); \
                xmm1 = _mm_shuffle_epi32(xmm0, 238); \
                xmm0 = _mm_add_epi32(xmm0, xmm1); \
                xmm1 = _mm_shuffle_epi32(xmm0, 85); \
                xmm0 = _mm_add_epi32(xmm0, xmm1); \
                return _mm_cvtsi128_si32(xmm0); \
            }
    #elif defined(__SSE__)
    using native_vector = __m128i;
        #define set1_epi16 _mm_set1_epi16
        #define load_epi16 [](auto ptr) { return _mm_load_si128(reinterpret_cast<native_vector const*>(ptr)); }
        #define min_epi16 _mm_min_epi16
        #define max_epi16 _mm_max_epi16
        #define madd_epi16 _mm_madd_epi16
        #define mullo_epi16 _mm_mullo_epi16
        #define add_epi32 _mm_add_epi32
        // based on output from zig 0.14.0 for @reduce(.Add, @as(@Vector(4, i32), x)
        #define reduce_epi32 \
            [](native_vector vec) \
            { \
                __m128i xmm1 = _mm_shuffle_epi32(vec, 238); \
                vec = _mm_add_epi32(vec, xmm1); \
                xmm1 = _mm_shuffle_epi32(vec, 85); \
                vec = _mm_add_epi32(vec, xmm1); \
                return _mm_cvtsi128_si32(vec); \
            }
    #endif
    constexpr auto VECTOR_SIZE = sizeof(native_vector) / sizeof(std::int16_t);
    static_assert(
        HL_SIZE % VECTOR_SIZE == 0,
        "HL_SIZE must be divisible by the native register size for this vectorization implementation to work"
    );
    const native_vector VEC_QA = set1_epi16(QA);
    const native_vector VEC_ZERO = set1_epi16(0);

    constexpr auto UNROLL = 2;

    native_vector accumulator[UNROLL] = {};
    int i = 0;

    const auto one_iteration = [&](auto idx, native_vector& acc)
    {
        // load accumulator values
        const native_vector stm_accum_values = load_epi16(&stm->values[idx]);
        const native_vector nstm_accum_values = load_epi16(&nstm->values[idx]);

        // load network weights
        const native_vector stm_weights = load_epi16(&network->output_weights[idx]);
        const native_vector nstm_weights = load_epi16(&network->output_weights[idx + HL_SIZE]);

        // clamp the values to [0, QA]
        const native_vector stm_clamped = min_epi16(VEC_QA, max_epi16(stm_accum_values, VEC_ZERO));
        const native_vector nstm_clamped = min_epi16(VEC_QA, max_epi16(nstm_accum_values, VEC_ZERO));

        // apply lizard screlu
        const native_vector stm_screlud = madd_epi16(stm_clamped, mullo_epi16(stm_clamped, stm_weights));
        const native_vector nstm_screlud = madd_epi16(nstm_clamped, mullo_epi16(nstm_clamped, nstm_weights));

        acc = add_epi32(acc, stm_screlud);
        acc = add_epi32(acc, nstm_screlud);
    };

    if constexpr (HL_SIZE >= UNROLL * VECTOR_SIZE)
    {
        for (; i < HL_SIZE; i += UNROLL * VECTOR_SIZE)
        {
            for (int j = 0; j < UNROLL; ++j)
            {
                one_iteration(i + j * VECTOR_SIZE, accumulator[j]);
            }
        }
    }
    for (; i < HL_SIZE; i += VECTOR_SIZE)
    {
        one_iteration(i, accumulator[0]);
    }
    if constexpr (HL_SIZE >= UNROLL * VECTOR_SIZE)
    {
        for (int j = 1; j < UNROLL; ++j)
        {
            accumulator[0] = add_epi32(accumulator[0], accumulator[j]);
        }
    }
    return reduce_epi32(accumulator[0]);
#else
    return autovectorised_screlu(network, stm, nstm);
#endif
}

int32_t forward(
    struct Network* const network,
    struct Accumulator* const stm_accumulator,
    struct Accumulator* const nstm_accumulator
)
{
    int32_t eval = vectorised_screlu(network, stm_accumulator, nstm_accumulator);

    eval /= QA;
    eval += network->output_bias;

    eval *= SCALE;
    eval /= QA * QB;

    return eval;
}