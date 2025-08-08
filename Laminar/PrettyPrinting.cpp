#include "PrettyPrinting.h"
#include "Search.h"
#include "Transpositions.h"
#include "cmath"
#include <iomanip>
#include <iostream>
void setRGB(int r, int g, int b)
{
    std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
}
double roundN(double value, int n)
{
    double factor = std::pow(10, n);
    return std::round(value * factor) / factor;
}
// Clamp helper
double clamp(double x, double min, double max)
{
    return std::max(min, std::min(x, max));
}

int lerp(int a, int b, double t)
{
    return static_cast<int>(a + (b - a) * t);
}
void desaturate(int& r, int& g, int& b, double amount)
{
    int gray = (r + g + b) / 2;
    r = lerp(r, gray, amount);
    g = lerp(g, gray, amount);
    b = lerp(b, gray, amount);
}

void setScoreColor(double score, double maxScore)
{
    score = clamp(score, -maxScore, maxScore);
    double t = score / maxScore;

    int r, g, b;
    if (t < 0)
    {
        //red to gray
        t = std::abs(t);
        r = lerp(128, 255, t);
        g = lerp(128, 0, t);
        b = lerp(128, 0, t);
    }
    else
    {
        //green to gray
        r = lerp(128, 0, t);
        g = lerp(128, 255, t);
        b = lerp(128, 0, t);
    }

    desaturate(r, g, b, 0.5);

    std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
}

void resetColor()
{
    std::cout << "\033[0m";
}
void printPretty(int score, int64_t elapsedMS, float nps, ThreadData& data)
{
    Move Bestmove = data.pvTable[0][0];
    std::cout << color::white;
    std::cout << std::right;
    std::cout << color::bright_blue << std::setw(3) << data.currDepth;
    std::cout << std::left;
    std::cout << color::white << " / " << color::bright_green << std::setw(5) << data.selDepth;
    std::cout << color::white;
    if (std::abs(score) >= MATESCORE - MAXPLY)
    {
        int mate_ply = 49000 - std::abs(score);
        int mate_fullmove = (int)std::ceil(static_cast<double>(mate_ply) / 2);
        if (score < 0)
        {
            mate_fullmove *= -1;
        }

        std::cout << "mate in " << std::setw(3) << mate_fullmove;
    }
    else
    {
        double pawnScale = (double)score / 100;
        double print = roundN(pawnScale, 3);
        std::cout << (print > 0 ? "+" : "-");
        setScoreColor(score, 500);
        std::cout << std::setw(10) << std::abs(print) << color::white;
    }
    int hashfull = get_hashfull();
    std::cout << color::bright_blue << std::right << std::setw(5) << static_cast<int>(std::round(elapsedMS))
              << color::white << " ms    ";
    std::cout << color::bright_blue << std::right << std::setw(8) << data.searchNodeCount << color::white
              << " nodes       ";
    std::cout << color::bright_blue << std::right << std::setw(8) << static_cast<int>(std::round(nps)) << color::white
              << " nodes/sec      ";
    std::cout << color::bright_blue << std::right << std::setw(5) << (1000 - hashfull) / 10 << color::white
              << " % TT     ";
    /*std::cout << " time " << () << " nodes " << data.searchNodeCount << " nps "
              << static_cast<int>(std::round(nps)) << " hashfull " << hashfull << " pv " << std::flush;*/
    std::cout << "\n";
    std::cout << "PV: ";
    for (int count = 0; count < data.pvLengths[0]; count++)
    {
        int brightness = 255 - (count * (255 - 128) / 16);
        if (brightness < 128)
            brightness = 128;
        std::cout << "\033[38;2;" << brightness << ";" << brightness << ";" << brightness << "m";

        printMove(data.pvTable[0][count]);
        std::cout << " ";
        if (count > 15)
            break;
    }
    std::cout << "\n\n" << std::flush;
    ;
}
