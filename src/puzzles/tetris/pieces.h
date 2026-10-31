#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr int TetrisRows           = 4;
inline constexpr int TetrisColumns        = 8;
inline constexpr int TetrisCellCount      = TetrisRows * TetrisColumns;
inline constexpr int TetrisRequiredPieces = TetrisCellCount / 4;

enum class TetrisPiece
{
    T,
    Square,
    L,
    J,
    S,
    Z,
    Line,
    Count
};

struct TetrisCell
{
public:
    int row;
    int column;

    bool operator==(const TetrisCell&) const = default;
};

typedef std::array<TetrisCell, 4> TetrisShape;
typedef std::array<int, static_cast<std::size_t>(TetrisPiece::Count)> TetrisInventory;

struct TetrisPlacement
{
public:
    TetrisPiece piece;
    TetrisShape cells;
    std::uint32_t mask;
};

const char* tetrisPieceName(TetrisPiece piece);
const TetrisShape& tetrisBaseShape(TetrisPiece piece);
const std::vector<TetrisShape>& tetrisRotations(TetrisPiece piece);
std::string validateTetrisInventory(const TetrisInventory& inventory);
