#pragma once

#include "puzzles/tetris/board.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

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

typedef std::array<TetrisCell, 4> TetrisShape;
typedef std::array<int, static_cast<std::size_t>(TetrisPiece::Count)> TetrisInventory;

struct TetrisPlacement
{
public:
    TetrisPiece piece;
    TetrisShape cells;
    std::uint64_t mask;
};

const char* tetrisPieceName(TetrisPiece piece);
const TetrisShape& tetrisBaseShape(TetrisPiece piece);
const std::vector<TetrisShape>& tetrisRotations(TetrisPiece piece);
std::string validateTetrisInventory(
    const TetrisInventory& inventory,
    TetrisVariant variant = TetrisVariant::Rectangle
);
