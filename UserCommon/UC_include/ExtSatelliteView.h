#pragma once

# include "../../common/SatelliteView.h"
# include <vector>

using std::vector;

// class GM_209277367_322542887; // Forward declaration of GameManager class

class ExtSatelliteView final : public SatelliteView {
    size_t width_;
    size_t height_;
    vector<vector<char>> map_;

    public:
        // Rule of 5
        ExtSatelliteView(size_t width, size_t height, vector<vector<char>> map);
        ~ExtSatelliteView() override = default; // Default destructor
        ExtSatelliteView(const ExtSatelliteView&) = delete;
        ExtSatelliteView& operator=(const ExtSatelliteView&) = delete;
        ExtSatelliteView(ExtSatelliteView&&) noexcept = delete;
        ExtSatelliteView& operator=(ExtSatelliteView&&) noexcept = delete;

        // API function to get an object at a specific location
        char getObjectAt(size_t x, size_t y) const override;
};