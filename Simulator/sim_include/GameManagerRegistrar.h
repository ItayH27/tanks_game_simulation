#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cassert>
#include <stdexcept>

#include "../common/AbstractGameManager.h"

class GameManagerRegistrar {
    struct GameManagerEntry {
        std::string soName;
        GameManagerFactory factory;

        GameManagerEntry(const std::string& so) : soName(so), factory(nullptr) {}
        void setFactory(GameManagerFactory&& f) {
            assert(factory == nullptr);
            factory = std::move(f);
        }

        bool hasFactory() const { return factory != nullptr; }
        std::unique_ptr<AbstractGameManager> create(bool verbose) const {
            return factory(verbose);
        }
        const std::string& name() const { return soName; }
    };

    std::vector<GameManagerEntry> managers_;
    static GameManagerRegistrar registrar;

public:
    static GameManagerRegistrar& getGameManagerRegistrar();

    void createEntry(const std::string& name) {
        managers_.emplace_back(name);
    }

    void addFactoryToLast(GameManagerFactory&& f) {
        managers_.back().setFactory(std::move(f));
    }

    void validateLast() {
        if (!managers_.back().hasFactory()) {
            throw std::runtime_error("Missing GameManager factory for: " + managers_.back().name());
        }
    }

    void removeLast() { managers_.pop_back(); }
    void eraseByName(const string& name) {
        managers_.erase(
            std::remove_if(managers_.begin(), managers_.end(),
                           [&](const auto& a){ return a.name() == name; }),
            managers_.end()
        );
    }

    bool empty() { return managers_.empty(); }

    auto begin() const { return managers_.begin(); }
    auto end() const { return managers_.end(); }
    GameManagerRegistrar::GameManagerEntry managerByName(const std::string& name) {
    auto it = std::find_if(managers_.begin(), managers_.end(),
                           [&](const GameManagerEntry& entry) { return entry.name() == name; });
        if (it != managers_.end()) {
            return *it;
        }
        return GameManagerEntry(""); // Return an empty entry if not found
    }
    void clear() { managers_.clear(); }
};
