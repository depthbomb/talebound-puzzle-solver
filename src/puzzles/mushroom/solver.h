#pragma once

#include "puzzles/mushroom/board.h"

#include <optional>
#include <vector>

typedef std::vector<MushroomCell> MushroomSolution;

std::optional<MushroomSolution> solveMushroom(const MushroomBoard& initial);
