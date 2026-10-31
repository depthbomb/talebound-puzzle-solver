#include "puzzles/mushroom/solver.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace
{
    struct MushroomEquations
    {
    public:
        std::vector<MushroomCell> cells;
        std::vector<std::size_t> pivots;
        std::vector<std::uint64_t> basis;
        std::array<std::uint64_t, MushroomBoard::CellCount> stateMasks = {};
    };

    MushroomEquations buildEquations(const MushroomVariant variant)
    {
        MushroomEquations result;
        result.cells = MushroomBoard(variant).cells();
        const auto count = result.cells.size();
        std::array<std::uint64_t, MushroomBoard::CellCount> coefficients = {};

        // The click rules depend only on the shape. Track each row operation on the starting state too.
        for (std::size_t row = 0; row < count; ++row)
        {
            result.stateMasks[row] = std::uint64_t{1} << row;
            for (std::size_t column = 0; column < count; ++column)
            {
                const auto distance = std::abs(result.cells[row].row - result.cells[column].row) +
                                      std::abs(result.cells[row].column - result.cells[column].column);
                if (distance <= 1)
                {
                    coefficients[row] |= std::uint64_t{1} << column;
                }
            }
        }

        // Clicking twice cancels out, so elimination only needs XOR and one bit per click.
        for (std::size_t column = 0; column < count; ++column)
        {
            const auto bit  = std::uint64_t{1} << column;
            const auto rank = result.pivots.size();
            auto pivot      = rank;
            while (pivot < count && (coefficients[pivot] & bit) == 0)
            {
                ++pivot;
            }

            if (pivot == count)
            {
                continue;
            }

            std::swap(coefficients[rank], coefficients[pivot]);
            std::swap(result.stateMasks[rank], result.stateMasks[pivot]);
            for (std::size_t row = 0; row < count; ++row)
            {
                if (row != rank && (coefficients[row] & bit) != 0)
                {
                    coefficients[row] ^= coefficients[rank];
                    result.stateMasks[row] ^= result.stateMasks[rank];
                }
            }

            result.pivots.push_back(column);
        }

        std::uint64_t pivotMask = 0;
        for (const auto pivot : result.pivots)
        {
            pivotMask |= std::uint64_t{1} << pivot;
        }

        // Free columns describe extra click sets that leave the board unchanged.
        for (std::size_t column = 0; column < count; ++column)
        {
            const auto bit = std::uint64_t{1} << column;
            if ((pivotMask & bit) != 0)
            {
                continue;
            }

            auto vector = bit;
            for (std::size_t row = 0; row < result.pivots.size(); ++row)
            {
                if ((coefficients[row] & bit) != 0)
                {
                    vector |= std::uint64_t{1} << result.pivots[row];
                }
            }

            result.basis.push_back(vector);
        }

        return result;
    }

    const MushroomEquations& equationsFor(const MushroomVariant variant)
    {
        // Reuse the elimination across edits; only the on/off state changes between solves.
        static const std::array<MushroomEquations, 3> equations = {
            buildEquations(MushroomVariant::Standard), buildEquations(MushroomVariant::Small),
            buildEquations(MushroomVariant::MiddleGaps)};
        return equations.at(static_cast<std::size_t>(variant));
    }
}

std::optional<MushroomSolution> solveMushroom(const MushroomBoard& initial)
{
    const auto& equations = equationsFor(initial.variant());
    std::uint64_t state = 0;
    for (std::size_t index = 0; index < equations.cells.size(); ++index)
    {
        if (initial.isOn(equations.cells[index]))
        {
            state |= std::uint64_t{1} << index;
        }
    }

    // An odd parity on a row with no possible clicks means this starting state is impossible.
    for (std::size_t row = equations.pivots.size(); row < equations.cells.size(); ++row)
    {
        if ((std::popcount(equations.stateMasks[row] & state) & 1) != 0)
        {
            return std::nullopt;
        }
    }

    std::uint64_t particular = 0;
    for (std::size_t row = 0; row < equations.pivots.size(); ++row)
    {
        if ((std::popcount(equations.stateMasks[row] & state) & 1) != 0)
        {
            particular |= std::uint64_t{1} << equations.pivots[row];
        }
    }

    // Try the equivalent click sets in a fixed order, keeping the shortest solution.
    auto best = particular;
    for (std::uint64_t choice = 1; choice < (std::uint64_t{1} << equations.basis.size()); ++choice)
    {
        auto candidate = particular;
        for (std::size_t index = 0; index < equations.basis.size(); ++index)
        {
            if ((choice & (std::uint64_t{1} << index)) != 0)
            {
                candidate ^= equations.basis[index];
            }
        }

        if (std::popcount(candidate) < std::popcount(best))
        {
            best = candidate;
        }
    }

    MushroomSolution result;
    for (std::size_t index = 0; index < equations.cells.size(); ++index)
    {
        if ((best & (std::uint64_t{1} << index)) != 0)
        {
            result.push_back(equations.cells[index]);
        }
    }

    return result;
}
