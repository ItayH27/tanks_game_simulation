#include "../sim_include/Simulator.h"
#include "sim_include/Simulator.h"

Simulator::Simulator(bool verbose, size_t numThreads)
    : verbose_(verbose), numThreads_(numThreads) {}

/**
 * @brief Extracts an integer value from a configuration line in the map file.
 *
 * @param line The line to extract the value from.
 * @param value Reference to the integer where the extracted value will be stored.
 * @param key The key expected in the line (e.g., "MaxSteps").
 * @param line_number The current line number (used for error reporting).
 * @param mapData Reference to the map data structure to flag failure if needed.
 * @param inputErrors Stream to write input-related error messages.
 * @return true if the value was successfully extracted, false otherwise.
 */
bool Simulator::extractLineValue(const std::string &line, int &value, const std::string &key, const size_t line_number,
    Simulator::MapData &mapData, ofstream &inputErrors) {

    std::string no_space_line;
    for (const char ch : line) { // Remove spaces from the line
        if (ch != ' ') no_space_line += ch;
    }

    // Check if the line has the correct format
    const std::string format = key + "=%d"; // Format for the line
    if (sscanf(no_space_line.c_str(), format.c_str(), &value) != 1) {
        // TODO: What to do instead of this...
        inputErrors << "Error: Invalid " << key << " format on line " << line_number << ".\n";
        mapData.failedInit = true;
        return false;
    }

    return true; // Successfully extraction
}

/**
 * @brief Extracts key configuration values from the map file into the MapData struct.
 *
 * @param mapData The struct to store extracted configuration values.
 * @param file The input stream for the map file.
 * @param inputErrors Stream to write any parsing errors.
 * @return true if all required values are successfully extracted, false otherwise.
 */
bool Simulator::extractValues(Simulator::MapData &mapData, ifstream& file, ofstream &inputErrors) {
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
    if (!getline(file, line) || !extractLineValue(line, mapData.maxSteps, "MaxSteps", line_number,
        mapData, inputErrors)) { // Failed to read line
        std::cerr << "Error: Missing MaxSteps.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 3: NumShells
    if (!getline(file, line) || !extractLineValue(line, mapData.numShells, "NumShells", line_number,
        mapData, inputErrors)) { // Failed to read line
        std::cerr << "Error: Missing NumShells.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 4: Rows
    if (!getline(file, line) || !extractLineValue(line, mapData.rows, "Rows", line_number, mapData,
        inputErrors)) { // Failed to read line
        std::cerr << "Error: Missing Rows.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;

    // Line 5: Cols
    if (!getline(file, line) || !extractLineValue(line, mapData.cols, "Cols", line_number, mapData,
        inputErrors)) { // Failed to read line
        std::cerr << "Error: Missing Cols.\n";
        remove("input_errors.txt");
        mapData.failedInit = true;
        return false;
    }
    ++line_number;
    return true;
}

/**
 * @brief Fills the game board from the remaining lines in the map file.
 *
 * @param gameBoard 2D character grid representing the game map.
 * @param file Input file stream pointing to the map file content.
 * @param mapData The associated map metadata.
 * @param inputErrors Stream to record errors like extra rows/columns.
 * @return A tuple containing:
 *         - true if any errors were encountered,
 *         - the number of extra rows,
 *         - the number of extra columns.
 */
tuple<bool, int, int> Simulator::fillGameBoard(vector<vector<char>> &gameBoard, ifstream &file,
    Simulator::MapData &mapData, ofstream &inputErrors) {

    int i = 0, extraRows = 0, extraCols = 0;
    string line;
    bool hasErrors = false;

    // Read the rest of the file
    while (getline(file, line)) {
        if (i >= mapData.rows) { // Check if we have reached the end of the file
            ++extraRows;
            continue;
        }

        // Check if line has more characters than expected width
        if (static_cast<int>(line.size()) > mapData.cols) {
            extraCols += static_cast<int>(line.size()) - mapData.cols;
            inputErrors << "Error recovered from: Extra " << (static_cast<int>(line.size()) - mapData.cols) <<
                " columns at row " << i << " ignored.\n";
            hasErrors = true;
        }

        // Truncate the line to fit the declared width
        line = line.substr(0, mapData.cols);

        for (int j = 0; j < mapData.cols; ++j) { // Iterate through each cell in the line
            char cell = (j < static_cast<int>(line.size())) ? line[j] : ' ';  // Fill missing columns with spaces
            gameBoard[i][j] = cell; // Update the gameboard with the new cell
        }

        ++i;
    }

    return {hasErrors, extraRows, extraCols};
}

/**
 * @brief Logs and checks for extra rows or columns beyond declared dimensions.
 *
 * @param extraRows Number of extra rows in the input.
 * @param extraCols Number of extra columns in the input.
 * @param inputErrors Stream to write the error recovery information.
 * @return true if any extra rows or columns were detected, false otherwise.
 */
bool Simulator::checkForExtras(int extraRows, int extraCols, ofstream &inputErrors) {
    bool hasErrors = false;

    // Check for extra rows and columns
    if (extraRows > 0) {
        inputErrors << "Error recovered from: Extra " << extraRows << " rows beyond declared height ignored.\n";
        hasErrors = true;
    }
    // Check for extra columns
    if (extraCols > 0) {
        inputErrors << "Error recovered from: Extra " << extraCols << " columns beyond declared width ignored.\n";
        hasErrors = true;
    }

    return hasErrors;
}

/**
 * @brief Reads the entire map from file and initializes MapData with extracted parameters and game board.
 *
 * @param file_path Path to the map file.
 * @return Initialized MapData object. If any error occurs, MapData.failedInit will be set to true.
 */
Simulator::MapData Simulator::readMap(const std::string& file_path) {
    int extraRows = 0, extraCols = 0;
    MapData mapData;
    string line;

    // input_error.txt initialisation
    bool has_errors = false; // Flag to indicate if there are any errors in the file
    ofstream input_errors("input_errors.txt"); // Open file to store errors

    // TODO: Figure out if this is necessary and if it is pass it to the the game manager somehow
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

    if (!extractValues(mapData, file, input_errors)) { return mapData; }

    vector<vector<char>> gameBoard;
    gameBoard.resize(mapData.rows, vector<char>(mapData.cols, ' '));
    tie(has_errors, extraRows, extraCols) = fillGameBoard(gameBoard, file, mapData, input_errors);
    mapData.satelliteView = std::make_unique<ExtSatelliteView>(mapData.rows, mapData.cols, gameBoard);

    has_errors = has_errors ? has_errors : checkForExtras(extraRows, extraCols, input_errors);

    // Delete input_errors.txt if no errors were found
    if (has_errors) {
        mapData.inputErrors = &input_errors;
    }
    else {
        remove("input_errors.txt");
    }

    return mapData;
}

/**
 * @brief Generates a timestamp string suitable for use in filenames.
 *
 * @return String in format YYYYMMDD_HHMMSS representing current local time.
 */

string Simulator::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}
