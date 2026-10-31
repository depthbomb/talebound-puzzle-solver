#include "puzzles/mushroom/board.h"

#include <array>
#include <stdexcept>

namespace
{
    std::size_t cellIndex(const MushroomBoard& board, const MushroomCell cell)
    {
        if (!board.hasCell(cell))
        {
            throw std::out_of_range("There is no Mushroom object at this position.");
        }

        return static_cast<std::size_t>(cell.row * MushroomBoard::Columns + cell.column);
    }
}

MushroomBoard::MushroomBoard(const MushroomVariant variant) : m_variant(variant) {}

int MushroomBoard::rows() const
{
    return m_variant == MushroomVariant::Standard ? Rows : 3;
}

int MushroomBoard::columns() const
{
    return m_variant == MushroomVariant::Standard ? Columns : 5;
}

MushroomVariant MushroomBoard::variant() const
{
    return m_variant;
}

bool MushroomBoard::hasCell(const MushroomCell cell) const
{
    if (cell.row < 0 || cell.row >= rows() || cell.column < 0 || cell.column >= columns())
    {
        return false;
    }

    if (m_variant == MushroomVariant::Small)
    {
        const bool corner =
            (cell.row == 0 || cell.row == rows() - 1) && (cell.column == 0 || cell.column == columns() - 1);
        return !corner;
    }

    if (m_variant == MushroomVariant::MiddleGaps)
    {
        return cell.row != 1 || (cell.column != 1 && cell.column != 3);
    }

    return true;
}

std::vector<MushroomCell> MushroomBoard::cells() const
{
    std::vector<MushroomCell> result;
    for (int row = 0; row < rows(); ++row)
    {
        for (int column = 0; column < columns(); ++column)
        {
            if (hasCell({row, column}))
            {
                result.push_back({row, column});
            }
        }
    }

    return result;
}

bool MushroomBoard::isOn(const MushroomCell cell) const
{
    return m_cells.test(cellIndex(*this, cell));
}

bool MushroomBoard::isClear() const
{
    return m_cells.none();
}

std::size_t MushroomBoard::countOn() const
{
    return m_cells.count();
}

void MushroomBoard::setCell(const MushroomCell cell, const bool on)
{
    m_cells.set(cellIndex(*this, cell), on);
}

void MushroomBoard::applyClick(const MushroomCell cell)
{
    m_cells.flip(cellIndex(*this, cell));

    constexpr std::array<MushroomCell, 4> offsets = {{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
    for (const auto offset : offsets)
    {
        const MushroomCell neighbor = {cell.row + offset.row, cell.column + offset.column};
        if (hasCell(neighbor))
        {
            m_cells.flip(cellIndex(*this, neighbor));
        }
    }
}

MushroomBoard MushroomBoard::example()
{
    constexpr std::array<MushroomCell, 10> cells = {
        {{0, 4}, {1, 1}, {1, 5}, {2, 3}, {3, 2}, {3, 4}, {4, 1}, {4, 5}, {5, 2}, {5, 4}}};

    MushroomBoard board;
    for (const auto cell : cells)
    {
        board.setCell(cell, true);
    }

    return board;
}
