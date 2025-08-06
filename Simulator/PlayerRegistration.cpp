#include "Common/PlayerRegistration.h"
#include "AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto& regsitrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    regsitrar.addPlayerFactoryToLastEntry(std::move(factory));
}