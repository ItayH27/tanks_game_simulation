#include "../sim_include/Simulator.h"
#include "sim_include/Simulator.h"

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

bool Simulator::extractValues(Simulator::MapData &mapData, ifstream& file) {
    string line;
    size_t line_number = 0;

    // Read and store map name from the first line
    if (!getline(file, line)) {
        cerr << "Error: Unable to read map name." << endl;
        return false;
    }
    mapData.name = line;
    ++line_number;

    // Line 2: MaxSteps
    if (!getline(file, line) || !extractLineValue(line, mapData.maxSteps, "MaxSteps", line_number)) { // Failed to read line
        std::cerr << "Error: Missing MaxSteps.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 3: NumShells
    if (!getline(file, line) || !extractLineValue(line, mapData.numShells, "NumShells", line_number)) { // Failed to read line
        std::cerr << "Error: Missing NumShells.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 4: Rows
    if (!getline(file, line) || !extractLineValue(line, mapData.rows, "Rows", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Rows.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 5: Cols
    if (!getline(file, line) || !extractLineValue(line, mapData.cols, "Cols", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Cols.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;
    return true;
}

// TODO: Add function to fill gameboard

// TODO: Adjust to compile with not being in Game manager
// TODO: Break into smaller functions
Simulator::MapData Simulator::readMap(const std::string& file_path) {
    MapData mapData;
    string line;

    // input_error.txt initialisation
    bool has_errors = false; // Flag to indicate if there are any errors in the file
    ofstream input_errors("input_errors.txt"); // Open file to store errors

    // Open game log
    fs::path actual_path(file_path);
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
        mapData.failedInit = true;
        return mapData;
    }

    if (!extractValues(mapData, file)) { return mapData; }

    // gameboard_.resize(height_, vector<char>(width_, ' ')); // Resize the gameboard to the declared dimensions

    vector<vector<char>> gameBoard;
    gameBoard.resize(mapData.rows, vector<char>(mapData.cols, ' '));
    int i = 0; // Current row
    int extra_rows = 0;
    int extra_cols = 0;

    // Read the rest of the file
    while (getline(file, line)) {
        if (i >= mapData.rows) { // Check if we have reached the end of the file
            ++extra_rows;
            continue;
        }

        // Check if line has more characters than expected width
        if (static_cast<int>(line.size()) > mapData.cols) {
            extra_cols += static_cast<int>(line.size()) - mapData.cols;
            input_errors << "Error recovered from: Extra " << (static_cast<int>(line.size()) - mapData.cols) <<
                " columns at row " << i << " ignored.\n";
            has_errors = true;
        }

        // Truncate the line to fit the declared width
        line = line.substr(0, mapData.cols);

        for (int j = 0; j < mapData.cols; ++j) { // Iterate through each cell in the line
            char cell = (j < static_cast<int>(line.size())) ? line[j] : ' ';  // Fill missing columns with spaces
            gameBoard[i][j] = cell; // Update the gameboard with the new cell
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

    // Delete input_errors.txt if no errors were found
    if (!has_errors) {
        remove("input_errors.txt");
    }
}
