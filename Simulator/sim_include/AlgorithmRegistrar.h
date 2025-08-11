#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <cassert>
#include <utility>
#include <cstddef>
#include "../../common/TankAlgorithm.h"
#include "../../common/Player.h"

using std::unique_ptr, std::vector, std::string, std::function, std::move, std::string;

class AlgorithmRegistrar {
public:
    class AlgorithmAndPlayerFactories {
        string so_name;
        TankAlgorithmFactory tankAlgorithmFactory;
        PlayerFactory playerFactory;
    public:
        AlgorithmAndPlayerFactories(const string& so_name) : so_name(so_name) {}
        void setTankAlgorithmFactory(TankAlgorithmFactory&& factory) {
            assert(tankAlgorithmFactory == nullptr);
            tankAlgorithmFactory = move(factory);
        }
        void setPlayerFactory(PlayerFactory&& factory) {
            assert(playerFactory == nullptr);
            playerFactory = move(factory);
        }
        const TankAlgorithmFactory& getTankAlgorithmFactory() const {
            return tankAlgorithmFactory;
        }
        const PlayerFactory& getPlayerFactory() const {
            return playerFactory;
        }
        const string& name() const { return so_name; }
        unique_ptr<Player> createPlayer(int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells) const {
            return playerFactory(player_index, x, y, max_steps, num_shells);
        }
        unique_ptr<TankAlgorithm> createTankAlgorithm(int player_index, int tank_index) const {
            return tankAlgorithmFactory(player_index, tank_index);
        }
        bool hasPlayerFactory() const {
            return playerFactory != nullptr;
        }
        bool hasTankAlgorithmFactory() const {
            return tankAlgorithmFactory != nullptr;
        }
    };
    vector<AlgorithmAndPlayerFactories> algorithms;
    static AlgorithmRegistrar registrar;

    static AlgorithmRegistrar& getAlgorithmRegistrar();
    void createAlgorithmFactoryEntry(const string& name) {
        algorithms.emplace_back(name);
    }
    void addPlayerFactoryToLastEntry(PlayerFactory&& factory) {
        algorithms.back().setPlayerFactory(move(factory));
    }
    void addTankAlgorithmFactoryToLastEntry(TankAlgorithmFactory&& factory) {
        algorithms.back().setTankAlgorithmFactory(move(factory));
    }
    struct BadRegistrationException {
        string name;
        bool hasName, hasPlayerFactory, hasTankAlgorithmFactory;
    };
    void validateLastRegistration() {
        const auto& last = algorithms.back();
        bool hasName = (last.name() != "");
        if(!hasName || !last.hasPlayerFactory() || !last.hasTankAlgorithmFactory() ) {
            throw BadRegistrationException{
                .name = last.name(),
                .hasName = hasName,
                .hasPlayerFactory = last.hasPlayerFactory(),
                .hasTankAlgorithmFactory = last.hasTankAlgorithmFactory()
            };
        }
    }
    void removeLast() {
        algorithms.pop_back();
    }
    auto begin() const {
        return algorithms.begin();
    }
    auto end() const {
        return algorithms.end();
    }
    size_t count() const { return algorithms.size(); }
    void clear() { algorithms.clear(); }
};