#include "puzzles/mushroom/solver.h"
#include "puzzles/tetris/solver.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string_view>

namespace
{
    void inventories(std::vector<TetrisInventory>& cases, TetrisInventory& counts, const std::size_t type, const int left)
    {
        if (type + 1 == counts.size())
        {
            counts[type] = left;
            cases.push_back(counts);
            return;
        }
        for (int count = 0; count <= left; ++count)
        {
            counts[type] = count;
            inventories(cases, counts, type + 1, left - count);
        }
    }

    std::vector<TetrisInventory> tetrisCases(const TetrisVariant variant)
    {
        if (variant == TetrisVariant::Rectangle)
        {
            std::vector<TetrisInventory> cases;
            TetrisInventory counts = {};
            inventories(cases, counts, 0, 8);
            return cases;
        }

        std::set<TetrisInventory> unique;
        for (std::size_t type = 0; type < static_cast<std::size_t>(TetrisPiece::Count); ++type)
        {
            TetrisInventory counts = {};
            counts[type] = 12;
            unique.insert(counts);
        }
        std::mt19937 random(21941);
        while (unique.size() < 512)
        {
            TetrisInventory counts = {};
            for (int piece = 0; piece < 12; ++piece)
            {
                ++counts[random() % counts.size()];
            }
            unique.insert(counts);
        }
        return {unique.begin(), unique.end()};
    }

    void benchmarkTetris(const int repetitions)
    {
        for (const auto variant : {TetrisVariant::Rectangle, TetrisVariant::Plus})
        {
            const auto cases = tetrisCases(variant);
            TetrisSearchOptions options;
            options.timeLimit = std::chrono::seconds(60);
            // Warm the placement tables before timing repeated puzzle solves.
            solveTetris(cases.front(), variant, options);
            for (int run = 0; run < repetitions; ++run)
            {
                std::array<int, 6> statuses = {};
                std::uint64_t nodes = 0;
                std::uint64_t checksum = 0;
                const auto start = std::chrono::steady_clock::now();
                for (const auto& counts : cases)
                {
                    const auto result = solveTetris(counts, variant, options);
                    ++statuses[static_cast<std::size_t>(result.status)];
                    nodes += result.visitedNodes;
                    for (const auto& placement : result.placements)
                    {
                        checksum = checksum * 31 + placement.mask + static_cast<std::uint64_t>(placement.piece);
                    }
                }
                const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
                std::cout << "{\"puzzle\":\"tetris\",\"variant\":" << static_cast<int>(variant)
                    << ",\"run\":" << run + 1 << ",\"cases\":" << cases.size() << ",\"ms\":" << elapsed.count()
                    << ",\"solved\":" << statuses[0] << ",\"impossible\":" << statuses[1]
                    << ",\"limited\":" << statuses[4] << ",\"nodes\":" << nodes << ",\"checksum\":" << checksum << "}\n";
                if (statuses[2] || statuses[3] || statuses[4] || statuses[5])
                {
                    std::exit(1);
                }
            }
        }
    }

    void benchmarkMushroom(const int repetitions)
    {
        std::mt19937 random(11743);
        for (const auto variant : {MushroomVariant::Standard, MushroomVariant::Small, MushroomVariant::MiddleGaps})
        {
            std::vector<MushroomBoard> cases;
            for (int index = 0; index < 4096; ++index)
            {
                MushroomBoard board(variant);
                for (const auto cell : board.cells())
                {
                    board.setCell(cell, (random() & 1U) != 0);
                }
                cases.push_back(board);
            }
            solveMushroom(cases.front());
            for (int run = 0; run < repetitions; ++run)
            {
                int solved = 0;
                std::uint64_t checksum = 0;
                const auto start = std::chrono::steady_clock::now();
                for (const auto& board : cases)
                {
                    if (const auto solution = solveMushroom(board))
                    {
                        ++solved;
                        for (const auto cell : *solution)
                        {
                            checksum = checksum * 31 + cell.row * 6 + cell.column;
                        }
                    }
                }
                const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
                std::cout << "{\"puzzle\":\"mushroom\",\"variant\":" << static_cast<int>(variant)
                    << ",\"run\":" << run + 1 << ",\"cases\":" << cases.size() << ",\"ms\":" << elapsed.count()
                    << ",\"solved\":" << solved << ",\"checksum\":" << checksum << "}\n";
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: solver_benchmark <tetris|mushroom> [repetitions]\n";
        return 2;
    }
    const int repetitions = argc == 3 ? std::atoi(argv[2]) : 5;
    if (repetitions < 1)
    {
        return 2;
    }
    std::cout << std::fixed << std::setprecision(3);
    const std::string_view puzzle = argv[1];
    if (puzzle == "tetris")
    {
        benchmarkTetris(repetitions);
    }
    else if (puzzle == "mushroom")
    {
        benchmarkMushroom(repetitions);
    }
    else
    {
        return 2;
    }
}
