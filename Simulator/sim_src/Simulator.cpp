#include "../sim_include/Simulator.h"
#include <fstream>
#include <iostream>

bool Simulator::extractLineValue(const std::string &line, int &value, const std::string &key, const size_t line_number) {
    std::string no_space_line;
    for (const char ch : line) { // Remove spaces from the line
        if (ch != ' ') no_space_line += ch;
    }

    // Check if the line has the correct format
    const std::string format = key + "=%d"; // Format for the line
    if (sscanf(no_space_line.c_str(), format.c_str(), &value) != 1) {
        // TODO: What to do instead of this...
        errorLog_ << "Error: Invalid " << key << " format on line " << line_number << ".\n";
        failedInit_ = true;
        return false;
    }

    return true; // Successfully extraction
}

// TODO: Adjust to complie with not being in Gamemanager
// TODO: Break into smaller fucntions
void readMap(const std::string& file_path) {
    // input_error.txt initialisation
    bool has_errors = false; // Flag to indicate if there are any errors in the file
    ofstream input_errors("input_errors.txt"); // Open file to store errors

    // Open game log
    path actual_path(file_path);
    string file_name = actual_path.stem().string(); // Get the file name without extension
    gameLog_.open("output_" + file_name + ".txt");
    if (!gameLog_.is_open()) { // Failed to create game log
        std::cerr << "Failed to open game log file." << std::endl;
        remove("input_errors.txt");
        return;
    }

    // Open file
    ifstream file(file_path);
    if (!file) { // Failed to open file
        std::cerr << "Error: Failed to open file: " << file_path << endl;
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }

    string line;
    size_t line_number = 0;

    // Skip line 1 (map name)
    getline(file, line);
    ++line_number;

    // Line 2: MaxSteps
    if (!getline(file, line) || !extractLineValue(line, maxSteps_, "MaxSteps", line_number)) { // Failed to read line
        std::cerr << "Error: Missing MaxSteps.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 3: NumShells
    if (!getline(file, line) || !extractLineValue(line, numShells_, "NumShells", line_number)) { // Failed to read line
        std::cerr << "Error: Missing NumShells.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 4: Rows
    if (!getline(file, line) || !extractLineValue(line, height_, "Rows", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Rows.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 5: Cols
    if (!getline(file, line) || !extractLineValue(line, width_, "Cols", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Cols.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    gameboard_.resize(height_, vector<char>(width_, ' ')); // Resize the gameboard to the declared dimensions

    int tank_1_count = 0, tank_2_count = 0; // Count the number of tanks in each player
    int i = 0; // Current row
    int extra_rows = 0;
    int extra_cols = 0;

    // Read the rest of the file
    while (getline(file, line)) {
        if (i >= height_) { // Check if we have reached the end of the file
            ++extra_rows;
            continue;
        }

        // Check if line has more characters than expected width
        if (static_cast<int>(line.size()) > width_) {
            extra_cols += static_cast<int>(line.size()) - width_;
            input_errors << "Error recovered from: Extra " << (static_cast<int>(line.size()) - width_) << " columns at row "
                    << i << " ignored.\n";
            has_errors = true;
        }

        // Truncate the line to fit the declared width
        line = line.substr(0, width_);

        for (int j = 0; j < width_; ++j) { // Iterate through each cell in the line
            char cell = (j < static_cast<int>(line.size())) ? line[j] : ' ';  // Fill missing columns with spaces

            if (cell == '1') { // Cell contains a tank of player 1
                auto tank = tankFactory_->create(1, tank_1_count); // Create a new tank for player 1
                auto tank_info = make_unique<TankInfo>(tank_1_count, make_pair(j, i),
                    numShells_, 1, std::move(tank)); // Create a new TankInfo for the tank
                tanks_.push_back(std::move(tank_info)); // Add tank 1 to vector
                ++tank_1_count;

            } else if (cell == '2') { // Cell contains a tank of player 2
                auto tank = tankFactory_->create(2, tank_2_count); // Create a new tank for player 2
                auto tank_info = make_unique<TankInfo>(tank_2_count, make_pair(j, i),
                    numShells_, 2, std::move(tank)); // Create a new TankInfo for the tank
                tanks_.push_back(std::move(tank_info)); // Add tank 2 to vector
                ++tank_2_count;

            } else if (cell != '#' && cell != '@' && cell != ' ') { // Unknown character
                input_errors << "Error recovered from: Unknown character '" << cell << "' at row " << i << ", column " << j << ". Treated as space.\n";
                cell = ' ';
                has_errors = true;
            }

            gameboard_[i][j] = cell; // Update the gameboard with the new cell
        }

        ++i;
    }

    // Check for extra rows and columns
    if (extra_rows > 0) {
        input_errors << "Error recovered from: Extra " << extra_rows << " rows beyond declared height ignored.\n";
        has_errors = true;
    }
    // Check for extra columns
    if (extra_cols > 0) {
        input_errors << "Error recovered from: Extra " << extra_cols << " columns beyond declared width ignored.\n";
        has_errors = true;
    }

    // Check for missing tanks
    if (tank_1_count == 0 || tank_2_count == 0) { // Check for immediate game termination conditions
        if (tank_1_count == 0 && tank_2_count == 0) { // Check if both players have no tanks
            gameLog_ << "Tie, both players have zero tanks\n";

        } else if (tank_1_count == 0) { // Check if player 1 has no tanks
            gameLog_ << "Player 2 won with " << tank_2_count << " tanks still alive\n";

        } else if (tank_2_count == 0) { // Check if player 2 has no tanks
            gameLog_ << "Player 1 won with " << tank_1_count << " tanks still alive\n";
        }

        // Update the game state and flush the logs
        gameOver_ = true;
        gameLog_.flush();
        gameLog_.close();
    }

    // Delete input_errors.txt if no errors were found
    if (!has_errors) {
        remove("input_errors.txt");
    }
}
