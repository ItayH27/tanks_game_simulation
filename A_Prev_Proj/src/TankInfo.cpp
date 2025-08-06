# include "TankInfo.h"

#include <utility>

// TankInfo class constructor
TankInfo::TankInfo(const int id, pair<int, int> loc, const int ammo, const int player_id, unique_ptr<TankAlgorithm> tank)
    : id_(id), location_(std::move(loc)), ammo_(ammo), playerId_(player_id), turnsToShoot_(0), turnsToBackwards_(2),
    backwardsFlag_(false), justMovedBackwards_(false), tank_(std::move(tank)), turnsDead_(0)
    { // Tans orientation dependant on players id
        dir_ = player_id == 1 ? Direction::L : Direction::R;
    }

// Get tank ID
int TankInfo::getID() const {
    return id_;
}

// Get tank location
const pair<int, int>& TankInfo::getLocation() const {
    return location_;
}

// Get tank direction
Direction TankInfo::getDirection() const {
    return dir_;
}

// Get amount of ammo
int TankInfo::getAmmo() const {
    return ammo_;
}

// Get tank algorithm
unique_ptr<TankAlgorithm>& TankInfo::getTank() {
    return tank_;
}

// Get tank algorithm
const unique_ptr<TankAlgorithm>& TankInfo::getTank() const {
    return tank_;
}

// Get player ID
int TankInfo::getPlayerId() const {
    return playerId_;
}

// Get turns to shoot
int TankInfo::getTurnsToShoot() const {
    return turnsToShoot_;
}

// Get turns to backwards
int TankInfo::getTurnsToBackwards() const {
    return turnsToBackwards_;
}

// Get backwards flag
bool TankInfo::isMovingBackwards() const {
    return backwardsFlag_;
}

// Get just moved backwards flag
bool TankInfo::justMovedBackwards() const {
    return justMovedBackwards_;
}

// Get is alive flag
int TankInfo::getIsAlive() const {
    return turnsDead_;
}

// Set tank location - from pair
void TankInfo::setLocation(const pair<int, int> &loc) {
    location_ = loc;
}

// Set tank location - from x and y
void TankInfo::setLocation(const int x, const int y) {
    location_.first = x;
    location_.second = y;
}

// Set tank direction 
void TankInfo::setDirection(const Direction dir) {
    this->dir_ = dir;
}

// Set amount of ammo
void TankInfo::setAmmo(const int ammo) {
    this->ammo_ = ammo;
}

// Decrease turns to shoot
void TankInfo::decreaseTurnsToShoot() {
    turnsToShoot_ = turnsToShoot_ > 0 ? turnsToShoot_ - 1 : 0;
}

// Decrease turns to backwards
void TankInfo::decreaseTurnsToBackwards() {
    --turnsToBackwards_;
}

// Restart turns to backwards
void TankInfo::restartTurnsToBackwards() {
    turnsToBackwards_ = 2;
}

// Zero turns to backwards
void TankInfo::zeroTurnsToBackwards() {
    turnsToBackwards_ = 0;
}

// Switch backwards flag
void TankInfo::switchBackwardsFlag() {
    backwardsFlag_ = !backwardsFlag_;
}

// Switch just moved backwards flag
void TankInfo::switchJustMovedBackwardsFlag() {
    justMovedBackwards_ = !justMovedBackwards_;
}

// Reset turns to shoot
void TankInfo::resetTurnsToShoot() {
    turnsToShoot_ = 4;
}

// Decrease tanks ammo
void TankInfo::decreaseAmmo() {
    ammo_ = std::max(0, ammo_ - 1);
}

// Increase turns dead
void TankInfo::increaseTurnsDead() {
    turnsDead_ += 1;
    setLocation(-1, -1);
}

