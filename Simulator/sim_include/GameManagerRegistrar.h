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
    static GameManagerRegistrar instance_;

public:
    static GameManagerRegistrar& get();

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

    auto begin() const { return managers_.begin(); }
    auto end() const { return managers_.end(); }
    void clear() { managers_.clear(); }
};
