#include "test_support.h"
#include "puzzles/tetris/solver.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <limits>
#include <random>
#include <set>
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
        int top = 8;
        int left = 8;
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

    void validateSolution(const TetrisInventory& inventory, const TetrisSolveResult& result,
        const TetrisVariant variant = TetrisVariant::Rectangle)
    {
        const bool plus = variant == TetrisVariant::Plus;
        const int rows = plus ? 8 : 4;
        std::uint64_t expected = 0;
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                if (!plus || (row >= 2 && row < 6) || (column >= 2 && column < 6))
                {
                    expected |= std::uint64_t{1} << (row * 8 + column);
                }
            }
        }
        require(result.status == TetrisSolveStatus::Solved && result.placements.size() == std::popcount(expected) / 4,
            "Incomplete solution.");
        TetrisInventory used = {};
        std::uint64_t covered = 0;
        for (const auto& placement : result.placements)
        {
            const auto type = static_cast<std::size_t>(placement.piece);
            require(type < Shapes.size(), "Unknown piece type.");
            require(rotations(Shapes[type]).contains(shapeBits(placement.cells)), "A piece was reflected or distorted.");
            ++used[type];
            std::uint64_t mask = 0;
            for (const auto cell : placement.cells)
            {
                require(cell.row >= 0 && cell.row < rows && cell.column >= 0 && cell.column < 8, "Out-of-bounds cell.");
                const auto bit = std::uint64_t{1} << (cell.row * 8 + cell.column);
                require((expected & bit) != 0, "Piece covers a missing corner.");
                require((covered & bit) == 0, "Overlapping cells.");
                covered |= bit;
                mask |= bit;
            }

            require(mask == placement.mask && std::popcount(mask) == 4, "Incorrect piece mask.");
        }

        require(covered == expected && used == inventory, "Gaps or wrong piece counts.");
    }

    struct OraclePlacement
    {
        std::uint64_t mask;
        unsigned int countIncrement;
    };

    // Enumerate all tilings by filling the first empty cell, independently of the production search.
    class Oracle
    {
    public:
        std::unordered_set<unsigned int> feasible;
        std::uint64_t nodes = 0;

    private:
        std::array<std::vector<OraclePlacement>, 64> m_placements;
        std::uint64_t m_mask = 0;

    public:
        explicit Oracle(const bool plus = false)
        {
            const int rows = plus ? 8 : 4;
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < 8; ++column)
                {
                    if (!plus || (row >= 2 && row < 6) || (column >= 2 && column < 6))
                    {
                        m_mask |= std::uint64_t{1} << (row * 8 + column);
                    }
                }
            }
            for (std::size_t type = 0; type < Shapes.size(); ++type)
            {
                for (const auto shape : rotations(Shapes[type]))
                {
                    for (int top = 0; top < rows; ++top)
                    {
                        for (int left = 0; left < 8; ++left)
                        {
                            std::uint64_t mask = 0;
                            bool fits = true;
                            for (int bit = 0; bit < 16; ++bit)
                            {
                                if ((shape & (1U << bit)) == 0)
                                {
                                    continue;
                                }

                                const int row = top + bit / 4;
                                const int column = left + bit % 4;
                                if (row >= rows || column >= 8)
                                {
                                    fits = false;
                                    break;
                                }

                                mask |= std::uint64_t{1} << (row * 8 + column);
                            }

                            if (fits && (mask & m_mask) == mask)
                            {
                                m_placements[std::countr_zero(mask)].push_back({mask, 1U << (type * 4)});
                            }
                        }
                    }
                }
            }

            if (!plus)
            {
                visit(0, 0);
            }
        }

        bool canFill(const TetrisInventory& inventory) const
        {
            unsigned int counts = 0;
            for (std::size_t type = 0; type < inventory.size(); ++type)
            {
                counts |= static_cast<unsigned int>(inventory[type]) << (type * 4);
            }
            std::set<std::pair<std::uint64_t, unsigned int>> dead;
            const auto search = [&](auto&& self, const std::uint64_t occupied, const unsigned int remaining) -> bool
            {
                if (occupied == m_mask)
                {
                    return remaining == 0;
                }
                const auto key = std::make_pair(occupied, remaining);
                if (dead.contains(key))
                {
                    return false;
                }
                for (const auto placement : m_placements[std::countr_zero(m_mask & ~occupied)])
                {
                    const bool available = ((remaining / placement.countIncrement) & 15U) != 0;
                    if (available && (placement.mask & occupied) == 0 &&
                        self(self, occupied | placement.mask, remaining - placement.countIncrement))
                    {
                        return true;
                    }
                }
                dead.insert(key);
                return false;
            };
            return search(search, 0, counts);
        }

    private:
        void visit(const std::uint64_t occupied, const unsigned int counts)
        {
            ++nodes;
            if (occupied == m_mask)
            {
                feasible.insert(counts);
                return;
            }

            for (const auto placement : m_placements[std::countr_zero(m_mask & ~occupied)])
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
        for (const auto variant : {TetrisVariant::Rectangle, TetrisVariant::Plus})
        {
            const int required = variant == TetrisVariant::Rectangle ? 8 : 12;
            for (const int count : {0, required - 1, required + 1})
            {
                const auto result = solveTetris({0, count}, variant);
                const auto expected = count < required ? "Not enough pieces." : "Too many pieces.";
                require(result.status == TetrisSolveStatus::Invalid && result.message.starts_with(expected)
                    && result.placements.empty() && result.visitedNodes == 0, "Incorrect piece-count error.");
            }

            TetrisInventory huge;
            huge.fill(std::numeric_limits<int>::max());
            require(solveTetris(huge, variant).message.starts_with("Too many pieces."), "Piece count overflowed.");
            require(solveTetris({-1, required, 1}, variant).status == TetrisSolveStatus::Invalid,
                "Negative piece count was accepted.");

            const TetrisInventory squares = {0, required};
            std::stop_source cancellation;
            cancellation.request_stop();
            require(solveTetris(squares, variant, {}, cancellation.get_token()).status == TetrisSolveStatus::Cancelled,
                "Cancellation was ignored.");
            TetrisSearchOptions options;
            options.maxNodes = 0;
            require(solveTetris(squares, variant, options).status == TetrisSolveStatus::LimitReached,
                "Node limit was ignored.");
            options.maxNodes = 2000000;
            options.timeLimit = std::chrono::milliseconds(0);
            require(solveTetris(squares, variant, options).status == TetrisSolveStatus::LimitReached,
                "Time limit was ignored.");
        }
    }

    void checkPlusBoard()
    {
        const TetrisBoard board(TetrisVariant::Plus);
        require(board.rows() == 8 && board.cellCount() == 48 && board.requiredPieces() == 12, "Incorrect plus dimensions.");
        std::uint64_t expected = 0;
        for (int row = -1; row <= 8; ++row)
        {
            for (int column = -1; column <= 8; ++column)
            {
                const bool present = row >= 0 && row < 8 && column >= 0 && column < 8 &&
                    ((row >= 2 && row < 6) || (column >= 2 && column < 6));
                require(board.hasCell({row, column}) == present, "Incorrect plus cell or corner.");
                if (present)
                {
                    expected |= std::uint64_t{1} << (row * 8 + column);
                }
            }
        }
        require(board.mask() == expected, "Plus mask does not match its shape.");

        // The mixed fixtures tile a 4x8 rectangle with two extra squares on each arm.
        for (const auto inventory : std::array<TetrisInventory, 4>{{
            {0, 12}, {0, 0, 0, 0, 0, 0, 12}, {2, 5, 1, 1, 1, 2}, {0, 4, 0, 0, 0, 0, 8}}})
        {
            validateSolution(inventory, solveTetris(inventory, TetrisVariant::Plus), TetrisVariant::Plus);
        }
        const auto impossible = solveTetris({1, 11}, TetrisVariant::Plus);
        require(impossible.status == TetrisSolveStatus::NoSolution && impossible.placements.empty(),
            "Odd T-piece count should be impossible on the plus board.");
        validateSolution({0, 8}, solveTetris({0, 8}));
    }

    void checkPlusInventories()
    {
        const Oracle oracle(true);
        std::mt19937 random(49613);
        for (int trial = 0; trial < 64; ++trial)
        {
            TetrisInventory inventory = {};
            for (int piece = 0; piece < 12; ++piece)
            {
                ++inventory[random() % inventory.size()];
            }
            const bool possible = oracle.canFill(inventory);
            const auto result = solveTetris(inventory, TetrisVariant::Plus);
            require(result.status == (possible ? TetrisSolveStatus::Solved : TetrisSolveStatus::NoSolution),
                "Plus solver disagrees with independent first-cell tiling search.");
            if (possible)
            {
                validateSolution(inventory, result, TetrisVariant::Plus);
            }
            else
            {
                require(result.placements.empty(), "Impossible plus inventory returned placements.");
            }
        }
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
        checkPlusBoard();
        checkPlusInventories();
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
