#include "ConcentratedPlayer.h"

static constexpr double INF = std::numeric_limits<double>::max(); // Constant for infinity

// Function to init limited gameboard
void ConcentratedPlayer::initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
        pair<int, int>& tank_location) {

    // Initialize gameboard and enemy locations vector
    vector<pair<int, int>> enemy_location_vec;
    gameboard.resize(y_, vector<char>(x_));
    const char enemy_id = playerIndex_ == 1 ? '2' : '1';

    // Iterate over the satellite_view and update the gameboard
    for (int i = 0; i < y_; ++i){
        for (int j = 0; j < x_; ++j){
            const char obj = satellite_view.getObjectAt(j, i);
            if (obj == enemy_id) { // Found enemy tank
                enemy_location_vec.emplace_back(j, i); // Store enemy tank location
                gameboard[i][j] = ' ';
                continue;
            }
           gameboard[i][j] = obj; // Update gameboard with object from satellite_view

            if (obj == '*') { // Found shell
                shells_location.emplace_back(j, i);
            }

            else if (obj == '%') { // Found self
                tank_location = {j, i};
            }
        }
    }

    // Find the closest enemy's location
    auto [enemy_x , enemy_y] = findClosestEnemy(enemy_location_vec, tank_location);
    if (enemy_x != -1 && enemy_y != -1) {gameboard[enemy_y][enemy_x] = enemy_id; } // Update gameboard with enemy tank location
}

// Function to find the closest enemy's location, using euclidian distance
pair<int, int> ConcentratedPlayer::findClosestEnemy(const vector<pair<int, int>>& enemy_tanks, const pair<int, int>& tank_location) {
    // Init variables
    pair closest_enemy = {-1, -1};
    const int this_x = tank_location.first;
    const int this_y = tank_location.second;

    double min_distance = INF;
    for (const auto& enemy : enemy_tanks) { // Iterate over all tanks
        const int diff_x = enemy.first - this_x;
        const int diff_y = enemy.second - this_y;

        // Check if the current tank is closer than the closest tank found so far
        if (const double euclidian_distance = sqrt(diff_x * diff_x + diff_y * diff_y); euclidian_distance < min_distance) {
            min_distance = euclidian_distance; // Calculate euclidean distance
            closest_enemy = enemy;
        }
    }

    return closest_enemy;
}