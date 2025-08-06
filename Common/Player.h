#pragma once

#include <memory>
#include <functional>
#include <cstddef>
#include "BattleInfo.h"
#include "SatelliteView.h"
#include "TankAlgorithm.h"

using std::unique_ptr, std::function;

class Player {
public:
	virtual ~Player() {}
	virtual void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) = 0;
};

using PlayerFactory = function<unique_ptr<Player> (int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells)>;
