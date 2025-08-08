#pragma once

#include "Search.h"

namespace color
{
    inline constexpr const char* reset = "\033[0m";
    inline constexpr const char* black = "\033[30m";
    inline constexpr const char* red = "\033[31m";
    inline constexpr const char* green = "\033[32m";
    inline constexpr const char* yellow = "\033[33m";
    inline constexpr const char* blue = "\033[34m";
    inline constexpr const char* magenta = "\033[35m";
    inline constexpr const char* cyan = "\033[36m";
    inline constexpr const char* white = "\033[37m";

    inline constexpr const char* bright_red = "\033[91m";
    inline constexpr const char* bright_green = "\033[92m";
    inline constexpr const char* bright_yellow = "\033[93m";
    inline constexpr const char* bright_blue = "\033[94m";
    inline constexpr const char* gray = "\033[90m";

} // namespace color

void printPretty(int score, int64_t elapsedMS, float nps, ThreadData& data);
