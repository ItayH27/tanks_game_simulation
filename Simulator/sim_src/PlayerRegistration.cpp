#include "../../common/PlayerRegistration.h"
#include "../sim_include/AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto& regsitrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    regsitrar.addPlayerFactoryToLastEntry(std::move(factory));
}