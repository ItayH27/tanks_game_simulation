#include "../../common/GameManagerRegistration.h"
#include "../sim_include/GameManagerRegistrar.h"

GameManagerRegistration::GameManagerRegistration(GameManagerFactory factory) {
    auto& registrar = GameManagerRegistrar::getGameManagerRegistrar();
    registrar.addFactoryToLast(std::move(factory));
}