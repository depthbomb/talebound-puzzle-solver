#pragma once

#include <cstdint>

enum class TetrisVariant
{
    Rectangle,
    Plus
};

struct TetrisCell
{
public:
    int row;
    int column;

    bool operator==(const TetrisCell&) const = default;
};

class TetrisBoard
{
public:
    static constexpr int Columns  = 8;
    static constexpr int MaxRows  = 8;
    static constexpr int MaxCells = MaxRows * Columns;

private:
    TetrisVariant m_variant;

public:
    explicit TetrisBoard(TetrisVariant variant = TetrisVariant::Rectangle);

    TetrisVariant variant() const;
    const char* name() const;
    int rows() const;
    int cellCount() const;
    int requiredPieces() const;
    std::uint64_t mask() const;
    bool hasCell(TetrisCell cell) const;
};
