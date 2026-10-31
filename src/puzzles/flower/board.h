#pragma once

#include <array>
#include <string>
#include <vector>

struct FlowerCell
{
public:
    int row;
    int column;

    bool operator==(const FlowerCell&) const = default;
};

typedef std::array<std::vector<FlowerCell>, 7> FlowerPaths;

class FlowerBoard
{
public:
    static constexpr int ColorCount   = 7;
    static constexpr int MaxDimension = 30;

private:
    int m_rows;
    int m_columns;
    std::vector<int> m_cells;

public:
    FlowerBoard(int rows = 5, int columns = 5);

    int rows() const;
    int columns() const;
    int colorAt(FlowerCell cell) const;
    void setColor(FlowerCell cell, int color);
    void resize(int rows, int columns);
    FlowerPaths flowers() const;
    std::string validationError() const;
    bool operator==(const FlowerBoard&) const = default;

private:
    std::size_t index(FlowerCell cell) const;
};
