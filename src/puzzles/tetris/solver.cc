#include "puzzles/tetris/solver.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::uint64_t FirstColumn = 0x0101010101010101ULL;
    constexpr std::uint64_t LastColumn  = 0x8080808080808080ULL;

    struct CellPlacement
    {
    public:
        std::uint64_t mask;
        std::size_t index;
    };

    struct PlacementTable
    {
    public:
        typedef std::array<std::vector<CellPlacement>, static_cast<std::size_t>(TetrisPiece::Count)> PlacementsByType;

        std::vector<TetrisPlacement> placements;
        std::array<PlacementsByType, TetrisBoard::MaxCells> byCell;
    };

    struct SearchKey
    {
    public:
        std::uint64_t occupied;
        std::uint32_t remaining;

        bool operator==(const SearchKey&) const = default;
    };

    struct SearchKeyHash
    {
    public:
        std::size_t operator()(const SearchKey& key) const
        {
            return std::hash<std::uint64_t>{}(key.occupied ^ (key.remaining * 0x9e3779b97f4a7c15ULL));
        }
    };

    PlacementTable buildPlacements(const TetrisBoard& board)
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

                for (int row = 0; row + height <= board.rows(); ++row)
                {
                    for (int column = 0; column + width <= TetrisBoard::Columns; ++column)
                    {
                        TetrisPlacement placement = {piece, shape, 0};
                        for (auto& cell : placement.cells)
                        {
                            cell.row += row;
                            cell.column += column;
                            placement.mask |= std::uint64_t{1} << (cell.row * TetrisBoard::Columns + cell.column);
                        }

                        if ((placement.mask & board.mask()) != placement.mask)
                        {
                            continue;
                        }

                        const auto index = result.placements.size();
                        result.placements.push_back(placement);
                        for (const auto cell : placement.cells)
                        {
                            result.byCell[cell.row * TetrisBoard::Columns + cell.column][type].push_back(
                                {placement.mask, index});
                        }
                    }
                }
            }
        }

        return result;
    }

    bool regionsCanBeFilled(std::uint64_t free)
    {
        while (free != 0)
        {
            // Start with the lowest set bit, then flood-fill its empty region using bit shifts.
            std::uint64_t region = free & (0ULL - free);
            while (true)
            {
                // Mask the side columns so a horizontal step can't wrap onto the next row.
                const auto neighbors = (region << TetrisBoard::Columns) | (region >> TetrisBoard::Columns) |
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
        const std::uint64_t m_boardMask;
        std::unordered_set<SearchKey, SearchKeyHash> m_dead;
        std::vector<TetrisPlacement> m_placed;
        TetrisSolveResult m_result;

    public:
        TetrisSearch(
            const TetrisInventory& inventory,
            const TetrisSearchOptions& options,
            const std::stop_token stop,
            const TetrisBoard& board,
            const PlacementTable& table
        ) : m_remaining(inventory), m_options(options), m_stop(stop),
            m_deadline(std::chrono::steady_clock::now() + options.timeLimit), m_table(table), m_boardMask(board.mask())
        {
            m_dead.reserve(4096);
            m_placed.reserve(board.requiredPieces());
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
        bool search(const std::uint64_t occupied)
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

            if (occupied == m_boardMask)
            {
                return true;
            }

            // The same filled cells can leave different pieces to place, so cache both parts of the state.
            // Counts fit in four bits each (at most twelve); the board now needs its own 64-bit mask.
            SearchKey key = {occupied, 0};
            for (const int count : m_remaining)
            {
                key.remaining = (key.remaining << 4) | static_cast<std::uint32_t>(count);
            }

            const auto empty = m_boardMask & ~occupied;
            if (m_dead.contains(key) || !regionsCanBeFilled(empty))
            {
                return false;
            }

            // Try the tightest spot first; finding an impossible cell here saves a lot of guessing.
            int chosen         = -1;
            std::size_t fewest = std::numeric_limits<std::size_t>::max();
            std::uint64_t free = empty;
            while (free != 0)
            {
                const int cell = std::countr_zero(free);
                free &= free - 1;
                std::size_t choices = 0;
                // Grouping by shape skips exhausted piece types without fetching their placements.
                for (std::size_t type = 0; type < m_remaining.size() && choices < fewest; ++type)
                {
                    if (m_remaining[type] == 0)
                    {
                        continue;
                    }

                    for (const auto placement : m_table.byCell[cell][type])
                    {
                        if ((placement.mask & occupied) == 0)
                        {
                            ++choices;
                            // This cell can't beat the best one once it has that many choices.
                            if (choices >= fewest)
                            {
                                break;
                            }
                        }
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

            for (std::size_t type = 0; type < m_remaining.size(); ++type)
            {
                if (m_remaining[type] == 0)
                {
                    continue;
                }

                for (const auto placement : m_table.byCell[chosen][type])
                {
                    if ((placement.mask & occupied) != 0)
                    {
                        continue;
                    }

                    --m_remaining[type];
                    m_placed.push_back(m_table.placements[placement.index]);
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
            }

            m_dead.insert(key);
            return false;
        }
    };
}

TetrisSolveResult solveTetris(
    const TetrisInventory& inventory,
    const TetrisVariant variant,
    const TetrisSearchOptions& options,
    const std::stop_token stop
)
{
    if (const auto error = validateTetrisInventory(inventory, variant); !error.empty())
    {
        TetrisSolveResult result;
        result.status  = TetrisSolveStatus::Invalid;
        result.message = error;
        return result;
    }

    // Legal placements depend on the board shape, so all inventories can share this table.
    static const auto rectangle = buildPlacements(TetrisBoard(TetrisVariant::Rectangle));
    static const auto plus      = buildPlacements(TetrisBoard(TetrisVariant::Plus));
    const auto& table           = variant == TetrisVariant::Rectangle ? rectangle : plus;
    return TetrisSearch(inventory, options, stop, TetrisBoard(variant), table).run();
}
