#include "Shell.h"

namespace UserCommon_209277367_322542887 {

// Constructors
Shell::Shell(int x, int y, const Direction dir) : location_(x, y), direction_(dir), aboveMine_(false) {}
Shell::Shell(const pair<int, int>& loc, const Direction dir) : location_(loc), direction_(dir), aboveMine_(false) {}

// Getters
pair<int, int> Shell::getLocation() const { return location_; }
Direction Shell::getDirection() const { return direction_; }
bool Shell::isAboveMine() const { return aboveMine_; }

// Setters
void Shell::setAboveMine(const bool above) { aboveMine_ = above; }
void Shell::setLocation(int x, int y) { location_ = {x, y}; }

} // namespace UserCommon_209277367_322542887