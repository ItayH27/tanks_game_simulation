#include "GameManager.h"
#include "ExtPlayerFactory.h"
#include "ExtTankAlgorithmFactory.h"
#include <cstring>

int main(int argc, char** argv) {
    // Visual mode setup
    bool visual_mode = false;
    if (argc == 3 && (strcmp(argv[2], "-v") == 0 || strcmp(argv[2], "--visual") == 0)) {
        visual_mode = true;
        std::cout << "Open http://localhost:3001 in your browser to view the visualization.\n";
#ifdef __APPLE__
        std::system("open http://localhost:3001");
#elif __linux__
        std::system("xdg-open http://localhost:3001");
#elif _WIN32
        std::system("start http://localhost:3001");
#endif
    }

    // Factories setup
    unique_ptr<PlayerFactory> playerFactory = make_unique<ExtPlayerFactory>();
    unique_ptr<TankAlgorithmFactory> tankAlgorithmFactory = make_unique<ExtTankAlgorithmFactory>();

    // Game setup
    GameManager game(std::move(playerFactory), std::move(tankAlgorithmFactory));
    game.readBoard(argv[1]); // Read game board from command line argument
    game.setVisualMode(visual_mode); // Set visual mode
    if (game.failedInit()) {
        cout << "Failed to initialize game" << endl;
        return 1;
    }

    // Run game
    game.run();
    return 0;
}
