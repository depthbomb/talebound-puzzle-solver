#pragma once

#include "puzzles/flower/board.h"

inline FlowerBoard reportedFlowerPuzzle(const int transform = 0)
{
    constexpr std::array<std::array<FlowerCell, 2>, 7> pairs = {{
        {{{0, 10}, {9, 0}}},
        {{{5, 5}, {6, 1}}},
        {{{1, 1}, {9, 1}}},
        {{{5, 1}, {7, 1}}},
        {{{1, 2}, {10, 10}}},
        {{{3, 2}, {10, 0}}},
        {{{3, 8}, {9, 2}}}
    }};

    FlowerBoard board(11, 11);
    for (int color = 0; color < 7; ++color)
    {
        for (auto cell : pairs[color])
        {
            // Four rotations, with and without a mirror, cover all eight orientations.
            if (transform >= 4)
            {
                cell.column = 10 - cell.column;
            }

            for (int rotation = 0; rotation < transform % 4; ++rotation)
            {
                cell = {cell.column, 10 - cell.row};
            }

            board.setColor(cell, color + 1);
        }
    }

    return board;
}
