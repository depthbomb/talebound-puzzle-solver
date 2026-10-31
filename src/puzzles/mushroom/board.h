#pragma once

#include <bitset>
#include <cstddef>
#include <vector>

enum class MushroomVariant
{
    Standard,
    Small,
    MiddleGaps
};

struct MushroomCell
{
public:
    int row;
    int column;

    bool operator==(const MushroomCell&) const = default;
};

class MushroomBoard
{
public:
    static constexpr int Rows      = 6;
    static constexpr int Columns   = 6;
    static constexpr int CellCount = Rows * Columns;

private:
    MushroomVariant m_variant = MushroomVariant::Standard;
    // Keep the same six-column stride for every variant; holes just stay unused.
    std::bitset<CellCount> m_cells;

public:
    MushroomBoard() = default;
    explicit MushroomBoard(MushroomVariant variant);

    int rows() const;
    int columns() const;
    MushroomVariant variant() const;
    bool hasCell(MushroomCell cell) const;
    std::vector<MushroomCell> cells() const;
    bool isOn(MushroomCell cell) const;
    bool isClear() const;
    std::size_t countOn() const;
    void setCell(MushroomCell cell, bool on);
    void applyClick(MushroomCell cell);
    bool operator==(const MushroomBoard&) const = default;

    static MushroomBoard example();
};
