#pragma once

#include <memory>
#include <functional>
#include <cstddef>
#include "BattleInfo.h"
#include "SatelliteView.h"
#include "TankAlgorithm.h"
#include "Player.h"
#include "GameResult.h"

using std::unique_ptr, std::function;


class AbstractGameManager {
public:
	virtual ~AbstractGameManager() {}
virtual GameResult run(
size_t map_width, size_t map_height,
	const SatelliteView& map, // <= a snapshot, NOT updated
size_t max_steps, size_t num_shells,
Player& player1, Player& player2,
TankAlgorithmFactory player1_tank_algo_factory,
TankAlgorithmFactory player2_tank_algo_factory) = 0;
};

using GameManagerFactory = function<std::unique_ptr<AbstractGameManager>(bool verbose)>;