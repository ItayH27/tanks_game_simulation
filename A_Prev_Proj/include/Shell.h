#pragma once

# include "Direction.h"

using std::pair;

class Shell {
        pair<int, int> location_;
        Direction direction_;
        bool aboveMine_;
        
    public:
        Shell(int x, int y, Direction dir);
        Shell(const pair<int, int>& loc, Direction dir);
        Shell(const Shell&) = delete; // Copy constructor
        Shell& operator=(const Shell&) = delete; // Copy assignment
        Shell(Shell&&) noexcept = delete; // Move constructor
        Shell& operator=(Shell&&) noexcept = delete; // Move assignment
        ~Shell() = default;
        
        pair<int, int> getLocation() const;
        Direction getDirection() const;
        bool isAboveMine() const;
        void setAboveMine(bool above);
        void setLocation(int x, int y);
    };