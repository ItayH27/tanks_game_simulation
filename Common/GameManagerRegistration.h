#pragma once

#include <memory>
#include "AbstractGameManager.h"

using std::make_unique;

struct GameManagerRegistration {
    GameManagerRegistration(GameManagerFactory);
};

#define REGISTER_GAME_MANAGER(class_name) \
GameManagerRegistration register_me_##class_name \
    ( [] (bool verbose) { return make_unique<class_name>(verbose); } );