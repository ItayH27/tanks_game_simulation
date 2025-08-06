#pragma once

#include "ActionRequest.h"
#include "Direction.h"
#include "ExtTankAlgorithm.h"

#include <utility>
#include <vector>
#include <queue>
#include <stack>
#include <optional>

using std::pair, std::vector, std::queue, std::stack, std::optional,  std::sqrt;

class Tank_BFS final : public ExtTankAlgorithm {
    
    public:
        Tank_BFS(int player_index, int tank_index); // Constructor
        ~Tank_BFS() override = default; // Destructor
        Tank_BFS(const Tank_BFS&) = delete; // Copy constructor deleted
        Tank_BFS& operator=(const Tank_BFS&) = delete; // Copy assignment operator deleted
        Tank_BFS(Tank_BFS&&) noexcept = delete; // Move constructor deleted
        Tank_BFS& operator=(Tank_BFS&&) noexcept = delete; // Move assignment operator deleted

        ActionRequest getAction() override; // Get the next action to take

        void algo(const vector<vector<char>>& gameboard) override; // BFS Algorithm to calculate the next actions

    private:
        stack<pair<int,int>> get_path_stack(const vector<vector<char>>& gameboard) const; // Get the path stack from the gameboard
};

class Tank_Sentry final : public ExtTankAlgorithm {

    public:
        Tank_Sentry(int player_index, int tank_index); // Constructor
        ~Tank_Sentry() override = default; // Destructor
        Tank_Sentry(const Tank_Sentry&) = delete; // Copy constructor deleted
        Tank_Sentry& operator=(const Tank_Sentry&) = delete; // Copy assignment operator deleted
        Tank_Sentry(Tank_Sentry&&) noexcept = delete; // Move constructor deleted
        Tank_Sentry& operator=(Tank_Sentry&&) noexcept = delete; // Move assignment operator deleted

        ActionRequest getAction() override; // Get the next action to take

        void algo(const vector<vector<char>>& gameboard) override; // Sentry Algorithm to calculate the next actions
        bool hasActiveTarget() const; // Check if the tank has an active target

    private:
        static int directionDiff(Direction from, Direction to); // Calculate the direction difference between two directions
        pair<int, int> targetLoc_; // Target location of the tank
};
