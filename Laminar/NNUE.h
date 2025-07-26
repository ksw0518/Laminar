#pragma once
#include "Accumulator.h"
#include <string>
void LoadNetwork(const std::string& filepath);
int32_t forward(
    struct Network* const network,
    struct Accumulator* const stm_accumulator,
    struct Accumulator* const nstm_accumulator
);