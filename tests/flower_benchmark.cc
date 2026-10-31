#include "flower_fixtures.h"
#include "puzzles/flower/solver.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv)
{
    const int milliseconds = argc > 1 ? std::atoi(argv[1]) : 10000;
    const int repetitions = argc > 2 ? std::atoi(argv[2]) : 1;
    const int transform = argc > 3 ? std::atoi(argv[3]) : 0;
    if (milliseconds < 1 || repetitions < 1 || transform < 0 || transform > 7)
    {
        return 2;
    }

    FlowerSearchOptions options;
    options.timeLimit = std::chrono::milliseconds(milliseconds);
    std::uint64_t previews = 0;
    if (argc > 4 && std::atoi(argv[4]) != 0)
    {
        options.progress = [&](const FlowerSearchPreview&)
        {
            ++previews;
        };
    }
    const auto board = reportedFlowerPuzzle(transform);
    for (int repetition = 0; repetition < repetitions; ++repetition)
    {
        previews = 0;
        const auto start = std::chrono::steady_clock::now();
        const auto result = solveFlower(board, options);
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::fixed << std::setprecision(3)
                  << "{\"transform\":" << transform << ",\"run\":" << repetition + 1
                  << ",\"status\":" << static_cast<int>(result.status) << ",\"ms\":" << elapsed
                  << ",\"nodes\":" << result.visitedNodes << ",\"previews\":" << previews << ",\"paths\":[";
        for (std::size_t color = 0; color < result.paths.size(); ++color)
        {
            if (color != 0)
            {
                std::cout << ',';
            }
            std::cout << '[';
            for (std::size_t index = 0; index < result.paths[color].size(); ++index)
            {
                if (index != 0)
                {
                    std::cout << ',';
                }
                const auto cell = result.paths[color][index];
                std::cout << '[' << cell.row + 1 << ',' << cell.column + 1 << ']';
            }
            std::cout << ']';
        }
        std::cout << "]}\n" << std::flush;
    }
}
