#include "test_support.h"
#include "puzzles/flower/solver.h"
#include "puzzles/flower/constraint_solver.h"
#include "flower_fixtures.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <random>
#include <thread>

namespace
{
    void verifyPaths(const FlowerBoard& board, const FlowerSolveResult& result)
    {
        require(result.status == FlowerSolveStatus::Solved, "Expected a solved Flower puzzle.");
        std::vector<bool> used(static_cast<std::size_t>(board.rows() * board.columns()));
        const auto flowers = board.flowers();
        for (std::size_t color = 0; color < flowers.size(); ++color)
        {
            const auto& path = result.paths[color];
            if (flowers[color].empty())
            {
                require(path.empty(), "Unused color has a path.");
                continue;
            }

            require(path.size() >= 2, "Path does not contain both flowers.");
            require((path.front() == flowers[color][0] && path.back() == flowers[color][1]) ||
                (path.front() == flowers[color][1] && path.back() == flowers[color][0]), "Wrong path endpoints.");
            for (std::size_t index = 0; index < path.size(); ++index)
            {
                const auto cell = path[index];
                require(cell.row >= 0 && cell.row < board.rows() && cell.column >= 0 && cell.column < board.columns(),
                    "Path leaves the grid.");
                const int slot = cell.row * board.columns() + cell.column;
                require(!used[slot], "Paths cross, share a cell, or revisit themselves.");
                used[slot] = true;
                require(board.colorAt(cell) == 0 || board.colorAt(cell) == static_cast<int>(color) + 1,
                    "Path passes through another color's flower.");
                if (index > 0)
                {
                    require(std::abs(cell.row - path[index - 1].row) + std::abs(cell.column - path[index - 1].column) == 1,
                        "Path includes a diagonal or nonadjacent step.");
                }
            }
        }
    }

    // Independent oracle: enumerate complete paths for one pair at a time, without the solver's MRV/BFS pruning.
    bool oracle(const FlowerBoard& board)
    {
        const auto flowers = board.flowers();
        std::vector<bool> used(static_cast<std::size_t>(board.rows() * board.columns()));
        std::function<bool(std::size_t)> connectPair;
        std::function<bool(std::size_t, FlowerCell)> walk;
        walk = [&](const std::size_t color, const FlowerCell head)
        {
            if (head == flowers[color][1])
            {
                return connectPair(color + 1);
            }
            for (int row = 0; row < board.rows(); ++row)
            {
                for (int column = 0; column < board.columns(); ++column)
                {
                    const FlowerCell next = {row, column};
                    const int slot = row * board.columns() + column;
                    if (std::abs(row - head.row) + std::abs(column - head.column) != 1 || used[slot])
                    {
                        continue;
                    }
                    if (board.colorAt(next) != 0 && next != flowers[color][1])
                    {
                        continue;
                    }
                    used[slot] = true;
                    if (walk(color, next))
                    {
                        return true;
                    }
                    used[slot] = false;
                }
            }
            return false;
        };
        connectPair = [&](std::size_t color)
        {
            while (color < flowers.size() && flowers[color].empty())
            {
                ++color;
            }
            if (color == flowers.size())
            {
                return true;
            }
            const auto start = flowers[color][0];
            const int slot = start.row * board.columns() + start.column;
            used[slot] = true;
            const bool solved = walk(color, start);
            used[slot] = false;
            return solved;
        };
        return connectPair(0);
    }

    void testValidationAndResize()
    {
        FlowerBoard board(3, 5);
        require(solveFlower(board).status == FlowerSolveStatus::Invalid, "Empty setup should request a pair.");
        board.setColor({0, 0}, 7);
        require(solveFlower(board).status == FlowerSolveStatus::Invalid, "Unpaired flower was accepted.");
        board.setColor({0, 4}, 7);
        require(board.validationError().empty(), "Using just the seventh color should be allowed.");
        verifyPaths(board, solveFlower(board));
        board.setColor({1, 4}, 7);
        require(solveFlower(board).status == FlowerSolveStatus::Invalid, "Three flowers of one color were accepted.");

        board.resize(2, 2);
        require(board.colorAt({0, 0}) == 7 && board.flowers()[6].size() == 1, "Resize did not preserve overlapping cells.");
        require(!board.validationError().empty(), "Resize hid an incomplete pair.");
        board.resize(4, 6);
        require(board.colorAt({3, 5}) == 0, "New cells were not empty.");
        bool rejected = false;
        try
        {
            board.setColor({0, 0}, 8);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "An eighth color was accepted.");
        rejected = false;
        try
        {
            board.resize(0, 5);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected && board.rows() == 4 && board.columns() == 6, "Invalid resize modified the board.");
    }

    void testRulesAndLimits()
    {
        FlowerBoard board(3, 3);
        board.setColor({1, 0}, 1);
        board.setColor({1, 2}, 1);
        const auto original = board;
        const auto result = solveFlower(board);
        verifyPaths(board, result);
        require(result.paths[0].size() == 3, "Unused cells should be allowed.");
        require(board == original, "Solving changed the flowers.");

        FlowerSearchOptions options;
        options.maxNodes = 0;
        const auto limited = solveFlower(board, options);
        require(limited.status == FlowerSolveStatus::LimitReached && limited.paths[0].empty(),
            "Node exhaustion was reported as impossible or returned partial paths.");
        options.maxNodes = 1000;
        options.timeLimit = std::chrono::milliseconds(0);
        require(solveFlower(board, options).status == FlowerSolveStatus::LimitReached, "Time limit was not honored.");
        std::stop_source stop;
        stop.request_stop();
        require(solveFlower(board, {}, stop.get_token()).status == FlowerSolveStatus::Cancelled, "Cancellation was ignored.");

        FlowerBoard crossing(2, 2);
        crossing.setColor({0, 0}, 1);
        crossing.setColor({1, 1}, 1);
        crossing.setColor({0, 1}, 2);
        crossing.setColor({1, 0}, 2);
        require(solveFlower(crossing).status == FlowerSolveStatus::NoSolution, "Crossing or diagonal paths were permitted.");

        FlowerBoard seven(7, 7);
        for (int row = 0; row < 7; ++row)
        {
            seven.setColor({row, 0}, row + 1);
            seven.setColor({row, 6}, row + 1);
        }
        verifyPaths(seven, solveFlower(seven));

        FlowerBoard large(30, 30);
        large.setColor({0, 0}, 4);
        large.setColor({29, 29}, 4);
        verifyPaths(large, solveFlower(large));

        FlowerBoard narrow(1, 5);
        narrow.setColor({0, 0}, 2);
        narrow.setColor({0, 4}, 2);
        verifyPaths(narrow, solveFlower(narrow));
    }

    void testAllTwoPairPlacements()
    {
        int checked = 0;
        for (int first = 0; first < 9; ++first)
        {
            for (int second = first + 1; second < 9; ++second)
            {
                for (int third = 0; third < 9; ++third)
                {
                    for (int fourth = third + 1; fourth < 9; ++fourth)
                    {
                        if (third == first || third == second || fourth == first || fourth == second)
                        {
                            continue;
                        }
                        FlowerBoard board(3, 3);
                        board.setColor({first / 3, first % 3}, 1);
                        board.setColor({second / 3, second % 3}, 1);
                        board.setColor({third / 3, third % 3}, 7);
                        board.setColor({fourth / 3, fourth % 3}, 7);
                        const auto result = solveFlower(board);
                        require(result.status == FlowerSolveStatus::Solved || result.status == FlowerSolveStatus::NoSolution,
                            "Small board search did not complete.");
                        require((result.status == FlowerSolveStatus::Solved) == oracle(board),
                            "Solver disagrees with independent exhaustive path enumeration.");
                        if (result.status == FlowerSolveStatus::Solved)
                        {
                            verifyPaths(board, result);
                        }
                        ++checked;
                    }
                }
            }
        }
        require(checked == 756, "Incomplete two-pair coverage.");
    }

    void testReportedPuzzle()
    {
        for (int transform = 0; transform < 8; ++transform)
        {
            const auto board = reportedFlowerPuzzle(transform);
            FlowerSearchOptions options;
            options.timeLimit = std::chrono::seconds(3);
            options.maxNodes = 50000;
            std::size_t previews = 0;
            options.progress = [&](const FlowerSearchPreview& preview)
            {
                ++previews;
                require(preview.visitedNodes > 0, "Preview is missing search progress.");
                for (const auto* paths : {&preview.paths, &preview.tails})
                {
                    for (const auto& path : *paths)
                    {
                        for (std::size_t index = 1; index < path.size(); ++index)
                        {
                            require(std::abs(path[index].row - path[index - 1].row) +
                                std::abs(path[index].column - path[index - 1].column) == 1,
                                "A preview connects unrelated partial path segments.");
                        }
                    }
                }
            };
            const auto result = solveFlower(board, options);
            verifyPaths(board, result);
            require(previews > 0, "The solver did not publish a preview.");
        }

        std::stop_source stop;
        FlowerSearchOptions options;
        options.progress = [&](const FlowerSearchPreview&)
        {
            stop.request_stop();
        };
        const auto cancelled = solveFlower(reportedFlowerPuzzle(), options, stop.get_token());
        require(cancelled.status == FlowerSolveStatus::Cancelled && cancelled.paths[0].empty(),
            "Cancellation during constraint search returned tentative paths as a solution.");
    }

    void testThreePairPlacements()
    {
        std::mt19937 random(57109);
        for (int trial = 0; trial < 128; ++trial)
        {
            FlowerBoard board(4, 4);
            std::array<int, 16> cells;
            for (int index = 0; index < 16; ++index)
            {
                cells[index] = index;
            }
            std::shuffle(cells.begin(), cells.end(), random);
            for (int endpoint = 0; endpoint < 6; ++endpoint)
            {
                board.setColor({cells[endpoint] / 4, cells[endpoint] % 4}, endpoint / 2 + 1);
            }
            const bool possible = oracle(board);
            const auto result = solveFlower(board);
            require(result.status == (possible ? FlowerSolveStatus::Solved : FlowerSolveStatus::NoSolution),
                "Three-pair solver disagrees with independent path enumeration.");
            if (possible)
            {
                verifyPaths(board, result);
            }
        }
    }

    void testInterruptionAtCompletion()
    {
        FlowerBoard board(1, 2);
        board.setColor({0, 0}, 1);
        board.setColor({0, 1}, 1);
        std::stop_source stop;
        FlowerSearchOptions options;
        options.progress = [&](const FlowerSearchPreview&) { stop.request_stop(); };
        const auto cancelled = solveFlower(board, options, stop.get_token());
        require(cancelled.status == FlowerSolveStatus::Cancelled && cancelled.paths[0].empty(),
            "Cancellation from the final preview was ignored.");

        options.timeLimit = std::chrono::milliseconds(30);
        options.progress = [&](const FlowerSearchPreview&) { std::this_thread::sleep_for(options.timeLimit * 2); };
        const auto expired = solveFlower(board, options);
        require(expired.status == FlowerSolveStatus::LimitReached && expired.paths[0].empty(),
            "A final preview that exhausted the time budget was ignored.");
    }

    void testConstraintEngine()
    {
        std::mt19937 random(42941);
        for (int trial = 0; trial < 2000; ++trial)
        {
            FlowerConstraints solver;
            const int variables = 3 + static_cast<int>(random() % 6);
            for (int variable = 0; variable < variables; ++variable)
            {
                solver.addVariable();
            }
            std::vector<std::vector<int>> clauses;
            const int count = 4 + static_cast<int>(random() % 40);
            for (int index = 0; index < count; ++index)
            {
                std::vector<int> clause;
                const std::size_t size = trial % 2 == 0 ? 3 : 1 + random() % 3;
                while (clause.size() < size)
                {
                    const int variable = 1 + static_cast<int>(random() % variables);
                    if (std::none_of(clause.begin(), clause.end(), [&](const int literal)
                        { return std::abs(literal) == variable; }))
                    {
                        clause.push_back((random() & 1U) != 0 ? variable : -variable);
                    }
                }
                clauses.push_back(clause);
                solver.addClause(std::move(clause));
            }

            bool possible = false;
            for (unsigned int assignment = 0; assignment < (1U << variables); ++assignment)
            {
                possible = std::all_of(clauses.begin(), clauses.end(), [&](const auto& clause)
                {
                    return std::any_of(clause.begin(), clause.end(), [&](const int literal)
                    {
                        const bool value = (assignment & (1U << (std::abs(literal) - 1))) != 0;
                        return value == (literal > 0);
                    });
                });
                if (possible)
                {
                    break;
                }
            }
            const auto status = solver.solve({}, {});
            require(status == (possible ? FlowerSolveStatus::Solved : FlowerSolveStatus::NoSolution),
                "Constraint engine disagrees with exhaustive Boolean enumeration.");
            if (possible)
            {
                for (const auto& clause : clauses)
                {
                    require(std::any_of(clause.begin(), clause.end(), [&](const int literal)
                        { return solver.value(std::abs(literal)) == (literal > 0 ? 1 : 0); }),
                        "Constraint engine returned an assignment that violates a clause.");
                }
            }
        }
    }
}

int main()
{
    return runTests([]
    {
        testValidationAndResize();
        testRulesAndLimits();
        testAllTwoPairPlacements();
        testThreePairPlacements();
        testConstraintEngine();
        testReportedPuzzle();
        testInterruptionAtCompletion();
    });
}
