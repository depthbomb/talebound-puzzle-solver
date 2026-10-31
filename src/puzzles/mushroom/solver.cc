#include "puzzles/mushroom/solver.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <utility>

std::optional<MushroomSolution> solveMushroom(const MushroomBoard& initial)
{
    const auto cells = initial.cells();
    const auto count = cells.size();
    std::array<std::bitset<MushroomBoard::CellCount + 1>, MushroomBoard::CellCount> equations{};
    std::vector<std::size_t> pivots;

    // One row per mushroom, one column per possible click. Leave holes out of both.
    for (std::size_t row = 0; row < count; ++row)
    {
        for (std::size_t column = 0; column < count; ++column)
        {
            const auto distance =
                std::abs(cells[row].row - cells[column].row) + std::abs(cells[row].column - cells[column].column);
            equations[row].set(column, distance <= 1);
        }

        // The last bit says whether this mushroom needs an odd number of toggles.
        equations[row].set(count, initial.isOn(cells[row]));
    }

    // Clicking twice cancels out, so the equations only need XOR and one bit per click.
    for (std::size_t column = 0; column < count; ++column)
    {
        const auto rank = pivots.size();
        auto pivot      = rank;
        while (pivot < count && !equations[pivot].test(column))
        {
            ++pivot;
        }

        if (pivot == count)
        {
            continue;
        }

        std::swap(equations[rank], equations[pivot]);
        for (std::size_t row = 0; row < count; ++row)
        {
            if (row != rank && equations[row].test(column))
            {
                equations[row] ^= equations[rank];
            }
        }

        pivots.push_back(column);
    }

    // A row with no clicks left but an on-state on the right means the board is impossible.
    for (std::size_t row = pivots.size(); row < count; ++row)
    {
        if (equations[row].test(count))
        {
            return std::nullopt;
        }
    }

    std::uint64_t particular = 0;
    std::bitset<MushroomBoard::CellCount> isPivot;
    for (std::size_t row = 0; row < pivots.size(); ++row)
    {
        isPivot.set(pivots[row]);
        if (equations[row].test(count))
        {
            particular |= std::uint64_t{1} << pivots[row];
        }
    }

    // Each free column gives a set of clicks we can toggle without changing the outcome.
    std::vector<std::uint64_t> basis;
    for (std::size_t column = 0; column < count; ++column)
    {
        if (isPivot.test(column))
        {
            continue;
        }

        auto vector = std::uint64_t{1} << column;
        for (std::size_t row = 0; row < pivots.size(); ++row)
        {
            if (equations[row].test(column))
            {
                vector |= std::uint64_t{1} << pivots[row];
            }
        }

        basis.push_back(vector);
    }

    // Try combinations of those extra click sets and keep the shortest valid solution.
    auto best = particular;
    for (std::uint64_t choice = 1; choice < (std::uint64_t{1} << basis.size()); ++choice)
    {
        auto candidate = particular;
        for (std::size_t index = 0; index < basis.size(); ++index)
        {
            if ((choice & (std::uint64_t{1} << index)) != 0)
            {
                candidate ^= basis[index];
            }
        }

        if (std::popcount(candidate) < std::popcount(best))
        {
            best = candidate;
        }
    }

    MushroomSolution result;
    for (std::size_t index = 0; index < count; ++index)
    {
        if ((best & (std::uint64_t{1} << index)) != 0)
        {
            result.push_back(cells[index]);
        }
    }

    return result;
}
