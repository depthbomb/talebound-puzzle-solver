#pragma once

#include "puzzles/flower/board.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <stop_token>

enum class FlowerSolveStatus
{
    Solved,
    NoSolution,
    Invalid,
    Cancelled,
    LimitReached,
    Error
};

struct FlowerSearchPreview
{
public:
    FlowerPaths paths;

    // While searching, the two ends of a path may not meet yet. Draw them separately.
    FlowerPaths tails;
    std::uint64_t visitedNodes = 0;
};

struct FlowerSearchOptions
{
public:
    std::chrono::milliseconds timeLimit = std::chrono::seconds(10);
    std::uint64_t maxNodes              = std::numeric_limits<std::uint64_t>::max();

    // Runs on the thread doing the search; UI code must hand updates back to its own thread.
    std::function<void(const FlowerSearchPreview&)> progress;
};

struct FlowerSolveResult
{
public:
    FlowerSolveStatus status = FlowerSolveStatus::NoSolution;
    FlowerPaths paths;
    std::uint64_t visitedNodes = 0;
    std::string message;
};

FlowerSolveResult solveFlower(
    const FlowerBoard& board,
    const FlowerSearchOptions& options = {},
    std::stop_token stop = {}
);
