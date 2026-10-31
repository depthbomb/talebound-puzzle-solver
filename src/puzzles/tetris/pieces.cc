#include "puzzles/tetris/pieces.h"

#include <algorithm>
#include <numeric>

namespace
{
    constexpr std::array<const char*, static_cast<std::size_t>(TetrisPiece::Count)> Names = {
        "T", "2 x 2 (O)", "L", "Mirrored L (J)", "S", "Mirrored S (Z)", "Line (I)"};
    constexpr std::array<TetrisShape, static_cast<std::size_t>(TetrisPiece::Count)> Shapes = {
        {{{{0, 0}, {0, 1}, {0, 2}, {1, 1}}}, {{{0, 0}, {0, 1}, {1, 0}, {1, 1}}}, {{{0, 0}, {1, 0}, {2, 0}, {2, 1}}},
            {{{0, 1}, {1, 1}, {2, 0}, {2, 1}}}, {{{0, 1}, {0, 2}, {1, 0}, {1, 1}}}, {{{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
            {{{0, 0}, {1, 0}, {2, 0}, {3, 0}}}}};

    TetrisShape normalize(TetrisShape shape)
    {
        int top  = shape.front().row;
        int left = shape.front().column;
        for (const auto cell : shape)
        {
            top  = std::min(top, cell.row);
            left = std::min(left, cell.column);
        }

        for (auto& cell : shape)
        {
            cell.row -= top;
            cell.column -= left;
        }

        // A shared origin and cell order make rotated shapes directly comparable.
        std::sort(shape.begin(), shape.end(),
            [](const TetrisCell first, const TetrisCell second)
            {
                return first.row != second.row ? first.row < second.row : first.column < second.column;
            });
        return shape;
    }

    std::array<std::vector<TetrisShape>, Shapes.size()> buildRotations()
    {
        std::array<std::vector<TetrisShape>, Shapes.size()> result;
        for (std::size_t piece = 0; piece < Shapes.size(); ++piece)
        {
            auto shape = Shapes[piece];
            for (int turn = 0; turn < 4; ++turn)
            {
                shape = normalize(shape);
                if (std::find(result[piece].begin(), result[piece].end(), shape) == result[piece].end())
                {
                    result[piece].push_back(shape);
                }

                // Rotate only: flipping would turn an L into a J, or an S into a Z.
                for (auto& cell : shape)
                {
                    cell = {cell.column, -cell.row};
                }
            }
        }

        return result;
    }
}

const char* tetrisPieceName(const TetrisPiece piece)
{
    return Names.at(static_cast<std::size_t>(piece));
}

const TetrisShape& tetrisBaseShape(const TetrisPiece piece)
{
    return Shapes.at(static_cast<std::size_t>(piece));
}

const std::vector<TetrisShape>& tetrisRotations(const TetrisPiece piece)
{
    static const auto rotations = buildRotations();
    return rotations.at(static_cast<std::size_t>(piece));
}

std::string validateTetrisInventory(const TetrisInventory& inventory, const TetrisVariant variant)
{
    for (const int count : inventory)
    {
        if (count < 0)
        {
            return "Piece counts cannot be negative.";
        }
    }

    const TetrisBoard board(variant);
    const auto total = std::accumulate(inventory.begin(), inventory.end(), std::int64_t{0});
    if (total != board.requiredPieces())
    {
        const std::string problem = total < board.requiredPieces() ? "Not enough pieces. " : "Too many pieces. ";
        return problem + std::to_string(total) + " pieces cover " + std::to_string(total * 4) + " of " +
               std::to_string(board.cellCount()) + " cells.\nThis grid needs " +
               std::to_string(board.requiredPieces()) + " pieces.";
    }

    return "";
}
