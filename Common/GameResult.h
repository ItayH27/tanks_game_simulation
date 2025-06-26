#pragma once

#include <memory>
#include <vector>
#include "SatelliteView.h"
#include "TankAlgorithm.h"

using std::unique_ptr, std::vector;

struct GameResult {
	int winner; // 0 = tie
	enum Reason { ALL_TANKS_DEAD, MAX_STEPS, ZERO_SHELLS };
Reason reason;
vector<size_t> remaining_tanks; // index 0 = player 1, etc.
unique_ptr<SatelliteView> gameState; // at end of game
size_t rounds; // total number of rounds
};
