#pragma once

#include <map>
#include <utility>

using std::pair, std::map;

enum class Direction { U, UR, R, DR, D, DL, L, UL };

// Mapping of directions to their respective coordinate changes
static const map<Direction, std::pair<int, int>> directionMap = {
    {Direction::U, {0, -1}},
    {Direction::UR, {1, -1}},
    {Direction::R, {1, 0}},
    {Direction::DR, {1, 1}},
    {Direction::D, {0, 1}},
    {Direction::DL, {-1, 1}},
    {Direction::L, {-1, 0}},
    {Direction::UL, {-1, -1}}
};


// Based on the following grid system:
// 0----------------------------------→ x
// |
// |
// |
// |            1  DR{1, 1}
// |              ➘
// |
// |
// |
// |
// |
// |
// ↓
// y