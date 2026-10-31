#include "puzzles/tetris/board.h"

#include <bit>

TetrisBoard::TetrisBoard(const TetrisVariant variant) : m_variant(variant) {}

TetrisVariant TetrisBoard::variant() const
{
    return m_variant;
}

const char* TetrisBoard::name() const
{
    return m_variant == TetrisVariant::Rectangle ? "4 x 8 rectangle" : "8 x 8 plus";
}

int TetrisBoard::rows() const
{
    return m_variant == TetrisVariant::Rectangle ? 4 : 8;
}

int TetrisBoard::cellCount() const
{
    return std::popcount(mask());
}

int TetrisBoard::requiredPieces() const
{
    return cellCount() / 4;
}

std::uint64_t TetrisBoard::mask() const
{
    // Each byte is a row. The plus loses two cells at each end of its top and bottom two rows.
    return m_variant == TetrisVariant::Rectangle ? 0x00000000ffffffffULL : 0x3c3cffffffff3c3cULL;
}

bool TetrisBoard::hasCell(const TetrisCell cell) const
{
    if (cell.row < 0 || cell.row >= rows() || cell.column < 0 || cell.column >= Columns)
    {
        return false;
    }

    return (mask() & (std::uint64_t{1} << (cell.row * Columns + cell.column))) != 0;
}
