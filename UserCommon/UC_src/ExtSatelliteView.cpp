# include "UC_include/ExtSatelliteView.h"

#include <utility>

// Constructor
ExtSatelliteView::ExtSatelliteView(const size_t width, const size_t height, vector<vector<char>> map)
    : width_(width), height_(height), map_(std::move(map)) {}

// Function to retrieve an object at a given location
char ExtSatelliteView::getObjectAt(const size_t x, const size_t y) const {
    if (x < width_ && y < height_) {
        return map_[y][x];
    }

    return '&'; // Return a space character if out of bounds
}