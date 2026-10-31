#pragma once

#include "puzzles/tetris/pieces.h"

#include <chrono>
#include <stop_token>

enum class TetrisSolveStatus
{
    Solved,
    NoSolution,
    Invalid,
    Cancelled,
    LimitReached,
    Error
};

struct TetrisSearchOptions
{
public:
    std::chrono::milliseconds timeLimit = std::chrono::seconds(10);
    std::uint64_t maxNodes              = 2000000;
};

struct TetrisSolveResult
{
public:
    TetrisSolveStatus status = TetrisSolveStatus::NoSolution;
    std::vector<TetrisPlacement> placements;
    std::string message;
    std::uint64_t visitedNodes = 0;
};

TetrisSolveResult solveTetris(
    const TetrisInventory& inventory,
    const TetrisSearchOptions& options = {},
    std::stop_token stop = {}
);
