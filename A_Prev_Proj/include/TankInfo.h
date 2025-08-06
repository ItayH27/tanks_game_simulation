#pragma once

# include "ExtTankAlgorithm.h"
# include <memory>
# include <utility>

using std::move, std::unique_ptr;

class TankInfo {
public:
    // Rule of five:
    TankInfo(int id, pair<int, int> loc, int ammo, int player_id, unique_ptr<TankAlgorithm> tank); // Constructor
    TankInfo(const TankInfo& other) = delete; // Copy constructor
    TankInfo& operator=(const TankInfo&) = delete; // Copy assignment - deleted due to unique_ptr
    TankInfo(TankInfo&&) noexcept = delete; // Move constructor
    TankInfo& operator=(TankInfo&&) noexcept = delete; // Move assignment
    ~TankInfo() = default; // Destructor

    int getID() const; // Get tank ID
    const pair<int, int>& getLocation() const; // Get tank location
    int getAmmo() const; // Get amount of ammo
    unique_ptr<TankAlgorithm>& getTank(); // Get tank algorithm
    const unique_ptr<TankAlgorithm>& getTank() const;
    int getPlayerId() const; // Get player ID
    int getTurnsToShoot() const; // Get turns to shoot
    int getTurnsToBackwards() const; // Get turns to backwards
    bool isMovingBackwards() const; // Get backwards flag
    bool justMovedBackwards() const; // Get just moved backwards flag
    Direction getDirection() const; // Get tank direction
    int getIsAlive() const; // Get alive flag

    void setDirection(Direction dir); // Set tank direction 
    void setLocation(const pair<int, int> &loc); // Set tank location
    void setLocation(int x, int y); // Set tank location
    void setAmmo(int ammo); // Set amount of ammo
    void decreaseTurnsToShoot(); // Decrease turns to shoot
    void decreaseTurnsToBackwards(); // Decrease turns to backwards
    void restartTurnsToBackwards(); // Set turns to backwards to 2
    void zeroTurnsToBackwards(); // Set turns to backwards to 0
    void switchBackwardsFlag(); // Switch backwards flag
    void switchJustMovedBackwardsFlag(); // Switch just moved backwards flag
    void resetTurnsToShoot(); // Resets turns to shoot
    void decreaseAmmo(); // Decrease amount of ammo
    void increaseTurnsDead();

private:
    const int id_; // Tank ID
    pair<int, int> location_; // Tank location (x, y)
    Direction dir_; // Tank direction
    int ammo_; // Amount of ammo
    const int playerId_; // Player ID
    int turnsToShoot_; // Turns to shoot
    int turnsToBackwards_; // Number of turns until can perform bacwards move
    bool backwardsFlag_; // Flag to check whether the tank still wants to move backwards
    bool justMovedBackwards_; // Flag to check whether the tank just moved backwards
    unique_ptr<TankAlgorithm> tank_; // Tank algorithm
    int turnsDead_; // Flag to check whether the tank is alive
};