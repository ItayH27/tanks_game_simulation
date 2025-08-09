#include "../../common/GameManagerRegistration.h"
#include "../sim_include/GameManagerRegistrar.h"

GameManagerRegistration::GameManagerRegistration(GameManagerFactory factory) {
    auto& regsitrar = GameManagerRegistrar::getGameManagerRegistrar();
    regsitrar.addFactoryToLast(std::move(factory));
}