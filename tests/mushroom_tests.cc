#include "test_support.h"
#include "puzzles/mushroom/solver.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <random>
#include <string>

namespace
{
    // Independent simulation: compare Manhattan distance instead of using the board's click operation.
    MushroomBoard simulate(const MushroomBoard& initial, const MushroomSolution& clicks)
    {
        MushroomBoard result = initial;
        for (int row = 0; row < initial.rows(); ++row)
        {
            for (int column = 0; column < initial.columns(); ++column)
            {
                if (!initial.hasCell({row, column}))
                {
                    continue;
                }

                bool on = initial.isOn({row, column});
                for (const auto click : clicks)
                {
                    if (std::abs(row - click.row) + std::abs(column - click.column) <= 1)
                    {
                        on = !on;
                    }
                }

                result.setCell({row, column}, on);
            }
        }

        return result;
    }

    void verifySolution(const MushroomBoard& board)
    {
        const auto original = board;
        const auto solution = solveMushroom(board);
        require(solution.has_value(), "A 6x6 board should have a solution.");
        require(board == original, "Solving modified the starting board.");
        require(simulate(board, *solution).isClear(), "Solution did not clear the board in an independent simulation.");

        std::array<bool, MushroomBoard::CellCount> seen{};
        for (const auto cell : *solution)
        {
            require(cell.row >= 0 && cell.row < 6 && cell.column >= 0 && cell.column < 6, "Invalid solution coordinate.");
            const auto index = cell.row * 6 + cell.column;
            require(!seen[index], "Solution clicked a cell twice.");
            seen[index] = true;
        }
    }

    void testClickRules()
    {
        for (int row = 0; row < 6; ++row)
        {
            for (int column = 0; column < 6; ++column)
            {
                MushroomBoard board;
                board.applyClick({row, column});
                require(board == simulate({}, {{row, column}}), "Click affected the wrong neighbors or wrapped an edge.");
                board.applyClick({row, column});
                require(board.isClear(), "Clicking twice did not cancel out.");
            }
        }

        for (const auto cell : std::array<MushroomCell, 4>{{{-1, 0}, {0, -1}, {6, 0}, {0, 6}}})
        {
            MushroomBoard board;
            bool rejected = false;
            try
            {
                board.applyClick(cell);
            }
            catch (const std::out_of_range&)
            {
                rejected = true;
            }
            require(rejected && board.isClear(), "Invalid cell was accepted or partially changed the board.");
        }
    }

    void testOriginalPuzzle()
    {
        const auto board = MushroomBoard::example();
        require(board.countOn() == 10, "The original puzzle must start with ten cells on.");
        const MushroomSolution expected = {
            {0, 0}, {0, 1}, {0, 4}, {0, 5}, {1, 2}, {1, 3}, {1, 4}, {2, 0}, {2, 1},
            {2, 3}, {2, 4}, {2, 5}, {3, 2}, {4, 0}, {4, 5}, {5, 0}, {5, 2}, {5, 4}
        };
        const auto solution = solveMushroom(board);
        require(solution && *solution == expected, "The original puzzle did not return the known 18-click solution.");
        verifySolution(board);
    }

    void testBoardCoverage()
    {
        const auto empty = solveMushroom({});
        require(empty && empty->empty(), "An empty board should need zero clicks.");

        MushroomBoard allOn;
        for (int row = 0; row < 6; ++row)
        {
            for (int column = 0; column < 6; ++column)
            {
                MushroomBoard single;
                single.setCell({row, column}, true);
                verifySolution(single);
                allOn.setCell({row, column}, true);
            }
        }
        verifySolution(allOn);

        std::mt19937 random(0x6A6A);
        for (int trial = 0; trial < 512; ++trial)
        {
            MushroomBoard arbitrary;
            MushroomSolution chosenClicks;
            for (int row = 0; row < 6; ++row)
            {
                for (int column = 0; column < 6; ++column)
                {
                    arbitrary.setCell({row, column}, (random() & 1U) != 0);
                    if ((random() & 1U) != 0)
                    {
                        chosenClicks.push_back({row, column});
                    }
                }
            }
            verifySolution(arbitrary);

            const auto generated = simulate({}, chosenClicks);
            const auto recovered = solveMushroom(generated);
            require(recovered && *recovered == chosenClicks, "Generated board did not recover its unique click pattern.");
        }
    }

    void testIrregularVariant(
        const MushroomVariant variant,
        const std::vector<MushroomCell>& missing,
        const int expectedImpossible)
    {
        const MushroomBoard empty(variant);
        const auto cells = empty.cells();
        require(empty.rows() == 3 && empty.columns() == 5 && cells.size() == 15 - missing.size(),
            "Incorrect irregular board shape.");
        for (const auto cell : missing)
        {
            require(!empty.hasCell(cell), "A missing position contains an object.");
            auto board = empty;
            bool rejected = false;
            try
            {
                board.applyClick(cell);
            }
            catch (const std::out_of_range&)
            {
                rejected = true;
            }
            require(rejected && board.isClear(), "Clicking a missing position changed the board.");
        }

        for (const auto cell : cells)
        {
            auto board = empty;
            board.applyClick(cell);
            require(board == simulate(empty, {cell}), "Irregular board toggled a missing object or wrong neighbor.");
        }

        // Independently enumerate every subset of clicks, then check every starting pattern against the oracle.
        const auto patternCount = 1U << cells.size();
        const auto unreachable = cells.size() + 1;
        std::vector<std::size_t> minimum(patternCount, unreachable);
        for (unsigned int mask = 0; mask < patternCount; ++mask)
        {
            MushroomSolution clicks;
            for (std::size_t index = 0; index < cells.size(); ++index)
            {
                if ((mask & (1U << index)) != 0)
                {
                    clicks.push_back(cells[index]);
                }
            }

            const auto board = simulate(empty, clicks);
            unsigned int pattern = 0;
            for (std::size_t index = 0; index < cells.size(); ++index)
            {
                if (board.isOn(cells[index]))
                {
                    pattern |= 1U << index;
                }
            }
            minimum[pattern] = std::min(minimum[pattern], clicks.size());
        }

        int impossible = 0;
        for (unsigned int pattern = 0; pattern < patternCount; ++pattern)
        {
            auto board = empty;
            for (std::size_t index = 0; index < cells.size(); ++index)
            {
                board.setCell(cells[index], (pattern & (1U << index)) != 0);
            }

            const auto solution = solveMushroom(board);
            require(solution.has_value() == (minimum[pattern] != unreachable),
                "Irregular board solvability disagrees with brute force.");
            if (solution)
            {
                require(solution->size() == minimum[pattern], "Irregular board solution is not minimal.");
                for (const auto cell : *solution)
                {
                    require(board.hasCell(cell), "Irregular board solution clicks a missing object.");
                }
                require(simulate(board, *solution).isClear(), "Irregular board solution did not clear all thorns.");
            }
            else
            {
                ++impossible;
            }
        }
        require(impossible == expectedImpossible, "Incorrect number of unreachable irregular-board states.");
    }
}

int main()
{
    return runTests([]
    {
        testClickRules();
        testOriginalPuzzle();
        testBoardCoverage();
        testIrregularVariant(MushroomVariant::Small, {{0, 0}, {0, 4}, {2, 0}, {2, 4}}, 1024);
        testIrregularVariant(MushroomVariant::MiddleGaps, {{1, 1}, {1, 3}}, 6144);
    });
}
