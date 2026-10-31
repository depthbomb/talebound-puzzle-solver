#include "puzzles/tetris/solver.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::uint32_t FullBoard   = 0xffffffffU;
    constexpr std::uint32_t FirstColumn = 0x01010101U;
    constexpr std::uint32_t LastColumn  = 0x80808080U;

    struct PlacementTable
    {
    public:
        std::vector<TetrisPlacement> placements;
        std::array<std::vector<std::size_t>, TetrisCellCount> byCell;
    };

    PlacementTable buildPlacements()
    {
        PlacementTable result;
        for (std::size_t type = 0; type < static_cast<std::size_t>(TetrisPiece::Count); ++type)
        {
            const auto piece = static_cast<TetrisPiece>(type);
            for (const auto& shape : tetrisRotations(piece))
            {
                int height = 0;
                int width  = 0;
                for (const auto cell : shape)
                {
                    height = std::max(height, cell.row + 1);
                    width  = std::max(width, cell.column + 1);
                }

                for (int row = 0; row + height <= TetrisRows; ++row)
                {
                    for (int column = 0; column + width <= TetrisColumns; ++column)
                    {
                        TetrisPlacement placement = {piece, shape, 0};
                        for (auto& cell : placement.cells)
                        {
                            cell.row += row;
                            cell.column += column;
                            placement.mask |= std::uint32_t{1} << (cell.row * TetrisColumns + cell.column);
                        }

                        const auto index = result.placements.size();
                        result.placements.push_back(placement);
                        for (const auto cell : placement.cells)
                        {
                            result.byCell[cell.row * TetrisColumns + cell.column].push_back(index);
                        }
                    }
                }
            }
        }

        return result;
    }

    bool regionsCanBeFilled(std::uint32_t free)
    {
        while (free != 0)
        {
            // Start with the lowest set bit, then flood-fill its empty region using bit shifts.
            std::uint32_t region = free & (0U - free);
            while (true)
            {
                // Mask the side columns so a horizontal step can't wrap onto the next row.
                const auto neighbors = (region << TetrisColumns) | (region >> TetrisColumns) |
                                       ((region & ~FirstColumn) >> 1) | ((region & ~LastColumn) << 1);
                const auto expanded = region | (neighbors & free);
                if (expanded == region)
                {
                    break;
                }

                region = expanded;
            }

            // A piece can't cross a filled cell, so every separate gap needs a multiple of four cells.
            if (std::popcount(region) % 4 != 0)
            {
                return false;
            }

            free &= ~region;
        }

        return true;
    }

    class TetrisSearch
    {
    private:
        TetrisInventory m_remaining;
        const TetrisSearchOptions& m_options;
        const std::stop_token m_stop;
        const std::chrono::steady_clock::time_point m_deadline;
        const PlacementTable& m_table;
        std::unordered_set<std::uint64_t> m_dead;
        std::vector<TetrisPlacement> m_placed;
        TetrisSolveResult m_result;

    public:
        TetrisSearch(
            const TetrisInventory& inventory,
            const TetrisSearchOptions& options,
            const std::stop_token stop,
            const PlacementTable& table
        ) : m_remaining(inventory), m_options(options), m_stop(stop),
            m_deadline(std::chrono::steady_clock::now() + options.timeLimit), m_table(table)
        {
            m_dead.reserve(4096);
            m_placed.reserve(TetrisRequiredPieces);
        }

        TetrisSolveResult run()
        {
            if (search(0))
            {
                m_result.status = TetrisSolveStatus::Solved;
                // Give pieces stable labels in reading order, regardless of the search's placement order.
                std::sort(m_placed.begin(), m_placed.end(),
                    [](const auto& first, const auto& second)
                    {
                        return std::countr_zero(first.mask) < std::countr_zero(second.mask);
                    });
                m_result.placements = std::move(m_placed);
            }

            return std::move(m_result);
        }

    private:
        bool search(const std::uint32_t occupied)
        {
            if (m_stop.stop_requested())
            {
                m_result.status = TetrisSolveStatus::Cancelled;
                return false;
            }

            if (m_result.visitedNodes >= m_options.maxNodes || std::chrono::steady_clock::now() >= m_deadline)
            {
                m_result.status = TetrisSolveStatus::LimitReached;
                return false;
            }

            ++m_result.visitedNodes;
            // On a checkerboard, only T pieces cover unequal colors. An odd number can't balance out.
            if (occupied == 0 && m_remaining[static_cast<std::size_t>(TetrisPiece::T)] % 2 != 0)
            {
                return false;
            }

            if (occupied == FullBoard)
            {
                return true;
            }

            // Four bits hold each remaining count (0..8); the low 32 bits hold the occupied cells.
            std::uint64_t key = 0;
            for (const int count : m_remaining)
            {
                key = (key << 4) | static_cast<std::uint64_t>(count);
            }

            key = (key << TetrisCellCount) | occupied;
            if (m_dead.contains(key) || !regionsCanBeFilled(~occupied))
            {
                return false;
            }

            // Try the tightest spot first; finding an impossible cell here saves a lot of guessing.
            int chosen         = -1;
            std::size_t fewest = std::numeric_limits<std::size_t>::max();
            std::uint32_t free = ~occupied;
            while (free != 0)
            {
                const int cell = std::countr_zero(free);
                free &= free - 1;
                std::size_t choices = 0;
                for (const auto index : m_table.byCell[cell])
                {
                    const auto& placement = m_table.placements[index];
                    if (m_remaining[static_cast<std::size_t>(placement.piece)] > 0 && (placement.mask & occupied) == 0)
                    {
                        ++choices;
                    }
                }

                if (choices == 0)
                {
                    m_dead.insert(key);
                    return false;
                }

                if (choices < fewest)
                {
                    chosen = cell;
                    fewest = choices;
                }
            }

            for (const auto index : m_table.byCell[chosen])
            {
                const auto& placement = m_table.placements[index];
                const auto type       = static_cast<std::size_t>(placement.piece);
                if (m_remaining[type] == 0 || (placement.mask & occupied) != 0)
                {
                    continue;
                }

                --m_remaining[type];
                m_placed.push_back(placement);
                if (search(occupied | placement.mask))
                {
                    return true;
                }

                m_placed.pop_back();
                ++m_remaining[type];
                if (m_result.status != TetrisSolveStatus::NoSolution)
                {
                    // Cancellation or a time limit doesn't prove this position is a dead end.
                    return false;
                }
            }

            m_dead.insert(key);
            return false;
        }
    };
}

TetrisSolveResult solveTetris(
    const TetrisInventory& inventory,
    const TetrisSearchOptions& options,
    const std::stop_token stop
)
{
    if (const auto error = validateTetrisInventory(inventory); !error.empty())
    {
        TetrisSolveResult result;
        result.status  = TetrisSolveStatus::Invalid;
        result.message = error;
        return result;
    }

    // Legal placements depend on the board shape, so all inventories can share this table.
    static const auto table = buildPlacements();
    return TetrisSearch(inventory, options, stop, table).run();
}
