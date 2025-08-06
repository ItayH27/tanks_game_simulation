#include "ObservantPlayer.h"


// Function to init gameboard and shell locations based on the satellite_view
void ObservantPlayer::initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
        pair<int, int>& tank_location) {

    gameboard.resize(y_, vector<char>(x_, ' ')); // Resize the gameboard to match satellite_view

    // Iterate over the satellite_view and update the gameboard
    for (int i = 0; i < y_; ++i){
        for (int j = 0; j < x_; ++j){
            const char obj = satellite_view.getObjectAt(j, i);
            gameboard[i][j] = obj; // Update the gameboard with the object from satellite_view

            if (obj == '*') { // Found shell
                shells_location.emplace_back(j, i);
            }

            else if (obj == '%') { // Found self
                tank_location = {j, i};

            }
        }
    }
}

