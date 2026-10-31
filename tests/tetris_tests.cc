#include "test_support.h"
#include "puzzles/tetris/solver.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <unordered_set>

namespace
{
    // Independent 4x4 bitmap fixtures: T, O, L, J, S, Z, I.
    constexpr std::array<unsigned int, 7> Shapes = {0x27, 0x33, 0x311, 0x322, 0x36, 0x63, 0x1111};

    unsigned int normalize(const unsigned int shape)
    {
        int top = 4;
        int left = 4;
        for (int bit = 0; bit < 16; ++bit)
        {
            if ((shape & (1U << bit)) != 0)
            {
                top = std::min(top, bit / 4);
                left = std::min(left, bit % 4);
            }
        }

        return shape >> (top * 4 + left);
    }

    std::unordered_set<unsigned int> rotations(unsigned int shape)
    {
        std::unordered_set<unsigned int> result;
        for (int turn = 0; turn < 4; ++turn)
        {
            result.insert(normalize(shape));
            unsigned int next = 0;
            for (int bit = 0; bit < 16; ++bit)
            {
                if ((shape & (1U << bit)) != 0)
                {
                    next |= 1U << ((3 - bit % 4) * 4 + bit / 4);
                }
            }

            shape = next;
        }

        return result;
    }

    unsigned int shapeBits(const TetrisShape& shape)
    {
        int top = TetrisRows;
        int left = TetrisColumns;
        for (const auto cell : shape)
        {
            top = std::min(top, cell.row);
            left = std::min(left, cell.column);
        }

        unsigned int result = 0;
        for (const auto cell : shape)
        {
            require(cell.row - top < 4 && cell.column - left < 4, "Piece extends beyond a 4x4 shape box.");
            result |= 1U << ((cell.row - top) * 4 + cell.column - left);
        }

        return result;
    }

    void validateSolution(const TetrisInventory& inventory, const TetrisSolveResult& result)
    {
        require(result.status == TetrisSolveStatus::Solved && result.placements.size() == 8, "Incomplete solution.");
        TetrisInventory used = {};
        std::uint32_t covered = 0;
        for (const auto& placement : result.placements)
        {
            const auto type = static_cast<std::size_t>(placement.piece);
            require(type < Shapes.size(), "Unknown piece type.");
            require(rotations(Shapes[type]).contains(shapeBits(placement.cells)), "A piece was reflected or distorted.");
            ++used[type];
            std::uint32_t mask = 0;
            for (const auto cell : placement.cells)
            {
                require(cell.row >= 0 && cell.row < 4 && cell.column >= 0 && cell.column < 8, "Out-of-bounds cell.");
                const auto bit = 1U << (cell.row * 8 + cell.column);
                require((covered & bit) == 0, "Overlapping cells.");
                covered |= bit;
                mask |= bit;
            }

            require(mask == placement.mask && std::popcount(mask) == 4, "Incorrect piece mask.");
        }

        require(covered == 0xffffffffU && used == inventory, "Gaps or wrong piece counts.");
    }

    struct OraclePlacement
    {
        std::uint32_t mask;
        unsigned int countIncrement;
    };

    // Enumerate all tilings by filling the first empty cell, independently of the production search.
    class Oracle
    {
    public:
        std::unordered_set<unsigned int> feasible;
        std::uint64_t nodes = 0;

    private:
        std::array<std::vector<OraclePlacement>, 32> m_placements;

    public:
        Oracle()
        {
            for (std::size_t type = 0; type < Shapes.size(); ++type)
            {
                for (const auto shape : rotations(Shapes[type]))
                {
                    for (int top = 0; top < 4; ++top)
                    {
                        for (int left = 0; left < 8; ++left)
                        {
                            std::uint32_t mask = 0;
                            bool fits = true;
                            for (int bit = 0; bit < 16; ++bit)
                            {
                                if ((shape & (1U << bit)) == 0)
                                {
                                    continue;
                                }

                                const int row = top + bit / 4;
                                const int column = left + bit % 4;
                                if (row >= 4 || column >= 8)
                                {
                                    fits = false;
                                    break;
                                }

                                mask |= 1U << (row * 8 + column);
                            }

                            if (fits)
                            {
                                m_placements[std::countr_zero(mask)].push_back({mask, 1U << (type * 4)});
                            }
                        }
                    }
                }
            }

            visit(0, 0);
        }

    private:
        void visit(const std::uint32_t occupied, const unsigned int counts)
        {
            ++nodes;
            if (occupied == 0xffffffffU)
            {
                feasible.insert(counts);
                return;
            }

            for (const auto placement : m_placements[std::countr_zero(~occupied)])
            {
                if ((placement.mask & occupied) == 0)
                {
                    visit(occupied | placement.mask, counts + placement.countIncrement);
                }
            }
        }
    };

    void checkInventory(TetrisInventory& inventory, const int type, const int remaining,
        const Oracle& oracle, int& checked, double& worstMilliseconds)
    {
        if (type + 1 < static_cast<int>(inventory.size()))
        {
            for (int count = 0; count <= remaining; ++count)
            {
                inventory[type] = count;
                checkInventory(inventory, type + 1, remaining - count, oracle, checked, worstMilliseconds);
            }

            return;
        }

        inventory[type] = remaining;
        unsigned int key = 0;
        for (std::size_t index = 0; index < inventory.size(); ++index)
        {
            key |= static_cast<unsigned int>(inventory[index]) << (index * 4);
        }

        const auto start = std::chrono::steady_clock::now();
        const auto result = solveTetris(inventory);
        const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        worstMilliseconds = std::max(worstMilliseconds, milliseconds);
        require(result.status == TetrisSolveStatus::Solved || result.status == TetrisSolveStatus::NoSolution,
            "An eight-piece inventory hit the search limit.");
        require((result.status == TetrisSolveStatus::Solved) == oracle.feasible.contains(key),
            "Solver disagrees with the independent exhaustive oracle.");
        if (result.status == TetrisSolveStatus::Solved)
        {
            validateSolution(inventory, result);
        }
        else
        {
            require(result.placements.empty(), "Failed search returned partial placements.");
        }

        ++checked;
    }

    void checkValidationAndLimits()
    {
        for (const auto inventory : std::array<TetrisInventory, 4>{{{}, {0, 7, 0, 0, 0, 0}, {0, 9, 0, 0, 0, 0}, {-1, 8, 1, 0, 0, 0}}})
        {
            const auto result = solveTetris(inventory);
            require(result.status == TetrisSolveStatus::Invalid && !result.message.empty() && result.placements.empty(),
                "Invalid inventory was accepted.");
        }

        const TetrisInventory squares = {0, 8, 0, 0, 0, 0};
        std::stop_source cancellation;
        cancellation.request_stop();
        require(solveTetris(squares, {}, cancellation.get_token()).status == TetrisSolveStatus::Cancelled,
            "Cancellation was ignored.");
        TetrisSearchOptions options;
        options.maxNodes = 0;
        require(solveTetris(squares, options).status == TetrisSolveStatus::LimitReached, "Node limit was ignored.");
        options.maxNodes = 2000000;
        options.timeLimit = std::chrono::milliseconds(0);
        require(solveTetris(squares, options).status == TetrisSolveStatus::LimitReached, "Time limit was ignored.");
    }
}

int main()
{
    return runTests([]
    {
        const std::array<std::size_t, 7> expectedRotations = {4, 1, 4, 4, 2, 2, 2};
        for (std::size_t type = 0; type < Shapes.size(); ++type)
        {
            const auto piece = static_cast<TetrisPiece>(type);
            require(shapeBits(tetrisBaseShape(piece)) == Shapes[type], "Incorrect base shape.");
            const auto& generated = tetrisRotations(piece);
            require(generated.size() == expectedRotations[type], "Missing or duplicate rotations.");
            const auto expected = rotations(Shapes[type]);
            for (const auto& shape : generated)
            {
                require(expected.contains(shapeBits(shape)), "Reflected rotation was generated.");
            }
        }

        checkValidationAndLimits();
        const auto start = std::chrono::steady_clock::now();
        const Oracle oracle;
        TetrisInventory inventory = {};
        int checked = 0;
        double worstMilliseconds = 0;
        checkInventory(inventory, 0, 8, oracle, checked, worstMilliseconds);
        require(checked == 3003, "Inventory enumeration is incomplete.");
        std::cout << "Passed all " << checked << " inventories: " << oracle.feasible.size()
            << " solvable; independent oracle explored " << oracle.nodes << " states.\n"
            << "Worst solver time: " << worstMilliseconds << " ms; total: "
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s.\n";
    });
}
