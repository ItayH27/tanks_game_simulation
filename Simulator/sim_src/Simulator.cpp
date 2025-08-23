#include "Simulator.h"

Simulator::Simulator(bool verbose, size_t numThreads)
    : verbose_(verbose), numThreads_(numThreads), logger_(utils::Logger::get()) {}

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

    bool hasErrors = false;
    int i = 0, extraRows = 0, extraCols = 0;
    string line;

    // Ensure size & prefill with spaces (in case caller didn't)
    if ((int)gameBoard.size() != mapData.rows ||
        gameBoard.empty() || (int)gameBoard[0].size() != mapData.cols) {
        gameBoard.assign(mapData.rows, vector<char>(mapData.cols, ' '));
    } else {
        for (auto& row : gameBoard) fill(row.begin(), row.end(), ' ');
    }

    auto allowed = [](char c) {
        return c == '#' || c == '@' || c == ' ' || c == '1' || c == '2';
    };

    while (getline(file, line)) {
        // Strip CR if the file uses Windows endings
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (i >= mapData.rows) {
            ++extraRows;
            hasErrors = true;
            continue;
        }

        if ((int)line.size() > mapData.cols) {
            int over = (int)line.size() - mapData.cols;
            extraCols += over;
            inputErrors << "Error recovered from: Extra " << over
                        << " columns at row " << i << " ignored.\n";
            hasErrors = true;
        }

        for (int j = 0; j < mapData.cols; ++j) {
            char cell = (j < (int)line.size()) ? line[j] : ' ';
            if (!allowed(cell)) {
                inputErrors << "Error recovered from: Unknown character '"
                            << (cell == '\r' ? '\\' : cell)
                            << "' at row " << i << ", column " << j
                            << ". Treated as space.\n";
                cell = ' ';
                hasErrors = true;
            }
            gameBoard[i][j] = cell; // assign AFTER validation
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
    mapData.failedInit = false; // Reset failedInit flag
    string line;

    // input_error.txt initialisation
    bool has_errors = false; // Flag to indicate if there are any errors in the file
    ofstream input_errors("input_errors.txt"); // Open file to store errors

    // Open file
    ifstream file(file_path);
    if (!file) { // Failed to open file
        logger_.error("Error: Failed to open file: ", file_path);
        std::cerr << "Error: Failed to open file: " << file_path << endl;
        remove("input_errors.txt");
        mapData.failedInit = true;
        return mapData;
    }

    if (!extractValues(mapData, file, input_errors)) { return mapData; }

    vector<vector<char>> gameBoard;
    gameBoard.resize(mapData.rows, vector<char>(mapData.cols, ' '));
    tie(has_errors, extraRows, extraCols) = fillGameBoard(gameBoard, file, mapData, input_errors);
    mapData.satelliteView = std::make_unique<ExtSatelliteView>(mapData.cols, mapData.rows, gameBoard);

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
