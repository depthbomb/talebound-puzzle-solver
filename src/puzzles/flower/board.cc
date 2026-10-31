#include "puzzles/flower/board.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

FlowerBoard::FlowerBoard(const int rows, const int columns) : m_rows(rows), m_columns(columns)
{
    if (rows < 1 || rows > MaxDimension || columns < 1 || columns > MaxDimension)
    {
        throw std::invalid_argument("Flower grid dimensions must be between 1 and 30.");
    }

    m_cells.resize(static_cast<std::size_t>(rows * columns));
}

int FlowerBoard::rows() const
{
    return m_rows;
}

int FlowerBoard::columns() const
{
    return m_columns;
}

int FlowerBoard::colorAt(const FlowerCell cell) const
{
    return m_cells[index(cell)];
}

void FlowerBoard::setColor(const FlowerCell cell, const int color)
{
    if (color < 0 || color > ColorCount)
    {
        throw std::invalid_argument("Flower color must be between 0 (empty) and 7.");
    }

    m_cells[index(cell)] = color;
}

void FlowerBoard::resize(const int rows, const int columns)
{
    // Copy the overlap into a fresh board; cells beyond the new edges are intentionally cropped.
    FlowerBoard resized(rows, columns);
    for (int row = 0; row < std::min(rows, m_rows); ++row)
    {
        for (int column = 0; column < std::min(columns, m_columns); ++column)
        {
            resized.setColor({row, column}, colorAt({row, column}));
        }
    }

    *this = std::move(resized);
}

FlowerPaths FlowerBoard::flowers() const
{
    FlowerPaths result;
    for (int row = 0; row < m_rows; ++row)
    {
        for (int column = 0; column < m_columns; ++column)
        {
            if (const int color = colorAt({row, column}); color != 0)
            {
                result[color - 1].push_back({row, column});
            }
        }
    }

    return result;
}

std::string FlowerBoard::validationError() const
{
    const auto pairs = flowers();
    bool anyFlowers  = false;
    for (std::size_t color = 0; color < pairs.size(); ++color)
    {
        if (!pairs[color].empty())
        {
            anyFlowers = true;
            if (pairs[color].size() != 2)
            {
                return "Color " + std::to_string(color + 1) + " needs exactly two flowers (currently " +
                       std::to_string(pairs[color].size()) + ").";
            }
        }
    }

    return anyFlowers ? "" : "Place a pair of flowers for at least one color.";
}

std::size_t FlowerBoard::index(const FlowerCell cell) const
{
    if (cell.row < 0 || cell.row >= m_rows || cell.column < 0 || cell.column >= m_columns)
    {
        throw std::out_of_range("Flower cell is outside the grid.");
    }

    return static_cast<std::size_t>(cell.row * m_columns + cell.column);
}
