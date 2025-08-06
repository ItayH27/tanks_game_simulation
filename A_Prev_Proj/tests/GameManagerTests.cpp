#include <gtest/gtest.h>
#include "GameManager.h"
#include "ExtPlayerFactory.h"
#include "ExtTankAlgorithmFactory.h"
#include <fstream>
#include <sstream>
#include <memory>

class GameManagerTest : public ::testing::Test {
protected:
    static std::string createTempMapFile(const std::string& content) {
        std::string filePath = "/tmp/test_map.txt";
        std::ofstream out(filePath);
        out << content;
        out.close();
        return filePath;
    }

    static std::string readFile(const string& filePath = "output_test_map.txt") {
        std::ifstream log(filePath);
        std::stringstream buffer;
        buffer << log.rdbuf();
        return buffer.str();
    }
};

TEST_F(GameManagerTest, ValidMapShouldLoadCorrectly) {
    const std::string mapContent =
        "Test Map\n"
        "MaxSteps=5\n"
        "NumShells=3\n"
        "Rows=2\n"
        "Cols=4\n"
        "1  #\n"
        " 2 @\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    EXPECT_NO_THROW(gm.readBoard(path));
}

TEST_F(GameManagerTest, RedundantRowsAndColsShouldBeIgnored) {
    const std::string mapContent =
        "Overflow Map\n"
        "MaxSteps=10\n"
        "NumShells=5\n"
        "Rows=2\n"
        "Cols=3\n"
        "1  # extra\n"
        "@2   more\n"
        "ignored\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    EXPECT_NO_THROW(gm.readBoard(path));

    std::ifstream expectedFile("tests/outputs/input_errors_redundant_rows_cols.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();
    std::string actualOutput = readFile("input_errors.txt");

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}

TEST_F(GameManagerTest, GameEndsAtMaxSteps) {
    const std::string mapContent =
        "Step Limit Test\n"
        "MaxSteps=1\n"
        "NumShells=0\n"
        "Rows=2\n"
        "Cols=2\n"
        "1 \n"
        " 2\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    EXPECT_NE(log_output.find("reached max steps = 1"), std::string::npos);
}

TEST_F(GameManagerTest, Player1HasNoTanks_Player2Wins) {
    const std::string mapContent =
        "P2 Only\n"
        "MaxSteps=2\n"
        "NumShells=5\n"
        "Rows=2\n"
        "Cols=4\n"
        "    \n"
        "  2 \n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    EXPECT_NE(log_output.find("Player 2 won with 1 tanks still alive"), std::string::npos);
}

TEST_F(GameManagerTest, Player2HasNoTanks_Player1Wins) {
    const std::string mapContent =
        "P1 Only\n"
        "MaxSteps=2\n"
        "NumShells=5\n"
        "Rows=2\n"
        "Cols=4\n"
        "1   \n"
        "    \n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    EXPECT_NE(log_output.find("Player 1 won with 1 tanks still alive"), std::string::npos);
}

TEST_F(GameManagerTest, InvalidMapFormatFailsGracefully) {
    const std::string mapContent =
        "Bad Map\n"
        "MaxSteps == 500\n"
        "NumShells = 10\n"
        "Rows = 2\n"
        "Cols = 2\n"
        "1 \n"
        " 2\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    EXPECT_TRUE(gm.failedInit());
}

TEST_F(GameManagerTest, MapWithUnknownCharactersIsAcceptedAsEmpty) {
    const std::string mapContent =
        "Map With Unknown\n"
        "MaxSteps=10\n"
        "NumShells=5\n"
        "Rows=2\n"
        "Cols=4\n"
        "1 X # %\n"
        "Y 2 @ Z\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    EXPECT_NO_THROW(gm.readBoard(path));

    std::ifstream expectedFile("tests/outputs/input_errors_invalid_chars.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();
    std::string actualOutput = readFile("input_errors.txt");

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}

TEST_F(GameManagerTest, MultiA) {
    const std::string mapContent =
        "2-Wall of China\n"
        "MaxSteps=2000\n"
        "NumShells=200\n"
        "Rows=12\n"
        "Cols=19\n"
        "###################\n"
        " ##       @ @     #\n"
        "1##  @         @  #\n"
        "#  ##     @    @   \n"
        "#@  ##  1    @   @ \n"
        "     ##     @  @  #\n"
        "  @  @##       @  #\n"
        " @     ##   @    @ \n"
        "    2   ##     #  @\n"
        "  @      ##  @ 2  #\n"
        "#    @    ##   @  #\n"
        "###    @   ##     #\n";


    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    std::ifstream expectedFile("tests/outputs/multi_a_output.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_EQ(actualOutput, expectedOutput);
}

TEST_F(GameManagerTest, FluidBackwardsMovement) {
    const std::string mapContent =
        "Fluent backwards\n"
        "MaxSteps=20\n"
        "NumShells=5\n"
        "Rows=7\n"
        "Cols=19\n"
        "###################\n"
        "###############  2#\n"
        "############   ####\n"
        "###########  ######\n"
        "##########  #######\n"
        "#1         ########\n"
        "###################\n";


    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    std::ifstream expectedFile("tests/outputs/fluid_backwards_partial.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}

TEST_F(GameManagerTest, NoAmmoTimerTrigered) {
    const std::string mapContent =
         "Shells Collide\n"
         "MaxSteps=100\n"
         "NumShells=4\n"
         "Rows=3\n"
         "Cols=11\n"
         "###########\n"
         "#2 ##### 1#\n"
         "###########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();

    const std::string expected_result = "Tie, both players have zero shells for " +
        std::to_string(GAME_OVER_NO_AMMO) + " steps";

    EXPECT_NE(log_output.find(expected_result), std::string::npos);
}

TEST_F(GameManagerTest, TanksCantMoveBecauseMines) {
    const std::string mapContent =
        "IsolatedDuel\n"
        "MaxSteps=20\n"
        "NumShells=50\n"
        "Rows=7\n"
        "Cols=11\n"
        "           \n"
        "     @@@   \n"
        "     @1@   \n"
        "     @@@   \n"
        "           \n"
        "      @@@  \n"
        "      @2@  \n"
        "      @@@  \n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();

    EXPECT_TRUE(log_output.find("won") == std::string::npos);
    EXPECT_NE(log_output.find("reached max steps = 20"), std::string::npos);
}

TEST_F(GameManagerTest, ExtraRowsAndColsAreOmitted) {
    const std::string mapContent =
        "ExtraRowsColsTest\n"
        "MaxSteps = 10\n"
        "NumShells = 5\n"
        "Rows = 2\n"
        "Cols = 5\n"
        "1   2@@@@@\n"  // valid row with extra columns
        "#####@@@@@\n"  // valid row with extra columns
        "ExtraRow1\n"   // extra row
        "ExtraRow2\n";  // extra row

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);

    auto [cols, rows] = gm.getGameboardSize();

    EXPECT_EQ(rows, 2);  // Only 2 rows as specified
    EXPECT_EQ(cols, 5);  // Only 5 columns as specified
}


TEST_F(GameManagerTest, InvalidInputMaxSteps) {
    const std::string mapContent =
         "Wall Damage\n"
         "NumShells=10\n"
         "Rows=3\n"
         "Cols=11\n"
         "###########\n"
         "#2   #   1#\n"
         "###########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };

    testing::internal::CaptureStderr();
    gm.readBoard(path);
    std::string cerr_output = testing::internal::GetCapturedStderr();
    EXPECT_NE(cerr_output.find("Error: Missing MaxSteps"), std::string::npos);
}

TEST_F(GameManagerTest, InvalidInputNumShells) {
    const std::string mapContent =
         "Wall Damage\n"
         "MaxSteps=100\n"
         "Rows=3\n"
         "Cols=11\n"
         "###########\n"
         "#2   #   1#\n"
         "###########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };

    testing::internal::CaptureStderr();
    gm.readBoard(path);
    std::string cerr_output = testing::internal::GetCapturedStderr();
    EXPECT_NE(cerr_output.find("Error: Missing NumShells"), std::string::npos);
}

TEST_F(GameManagerTest, InvalidInputRows) {
    const std::string mapContent =
         "Wall Damage\n"
         "MaxSteps=100\n"
         "NumShells=10\n"
         "Cols=11\n"
         "###########\n"
         "#2   #   1#\n"
         "###########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };

    testing::internal::CaptureStderr();
    gm.readBoard(path);
    std::string cerr_output = testing::internal::GetCapturedStderr();
    EXPECT_NE(cerr_output.find("Error: Missing Rows"), std::string::npos);
}

TEST_F(GameManagerTest, InvalidInputCols) {
    const std::string mapContent =
         "Wall Damage\n"
         "MaxSteps=100\n"
         "NumShells=10\n"
         "Rows=3\n"
         "###########\n"
         "#2   #   1#\n"
         "###########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };

    testing::internal::CaptureStderr();
    gm.readBoard(path);
    std::string cerr_output = testing::internal::GetCapturedStderr();
    EXPECT_NE(cerr_output.find("Error: Missing Cols"), std::string::npos);
}

TEST_F(GameManagerTest, WorkWithOtherTanksAndPlayers) {
    class DummyPlayer : public Player {
    public:
        DummyPlayer(const int player_index, const size_t x, const size_t y, const size_t max_steps, const size_t num_shells)
            : Player(player_index, x, y, max_steps, num_shells) {}

        void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) override {
            (void) satellite_view;
            (void) tank;
        }
    };

    class DummyTank : public TankAlgorithm {
    public:
        ActionRequest getAction() override { return ActionRequest::MoveForward; }
        void updateBattleInfo(BattleInfo&) override {}
    };

    class DummyTankFactory : public TankAlgorithmFactory {
    public:
        std::unique_ptr<TankAlgorithm> create(int, int) const override {
                    return std::make_unique<DummyTank>();
                }
    };
    class DummyPlayerFactory : public PlayerFactory {
    public:
        unique_ptr<Player> create(int player_index, size_t x, size_t y,
                                  size_t max_steps, size_t num_shells) const override {
            return std::make_unique<DummyPlayer>(player_index, x, y, max_steps, num_shells);
        }
    };

    const std::string mapContent =
        "ExtraRowsColsTest\n"
        "MaxSteps = 10\n"
        "NumShells = 5\n"
        "Rows = 1\n"
        "Cols = 10\n"
        "@ # 1 2  @\n";


    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<DummyPlayerFactory>(),
        std::make_unique<DummyTankFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    std::ifstream expectedFile("tests/outputs/WorkWithOthers.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_EQ(actualOutput, expectedOutput);
}

TEST_F(GameManagerTest, InputA) {
    const std::string mapContent =
        "Input A - Wall of China\n"
        "MaxSteps=31\n"
        "NumShells=200\n"
        "Rows= 12\n"
        "Cols=19\n"
        "###################\n"
        " ##       @ @     #\n"
        "1 ##  @         @ #\n"
        "#  ##     @    @  \n"
        "#@  ##  1    @   @\n"
        "     ##     @  @  #\n"
        "  @  @##       @  #\n"
        " @     ##   @    @\n"
        "    2   ##     #  @\n"
        "  @      ##  @ 2  #\n"
        "#    @    ##   @  #\n"
        "###    @   ##     #\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::string log_output = readFile();
    std::ifstream expectedFile("tests/outputs/output_a.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}

TEST_F(GameManagerTest, InputB) {
    const std::string mapContent =
        "Input B - Wide Open\n"
        "MaxSteps=24\n"
        "NumShells=100\n"
        "Rows=8\n"
        "Cols=25\n"
        "       1        @      2\n"
        "  #        ##      @    \n"
        "     @       ###       ##\n"
        " ##    #     1     ##     \n"
        "     ##         @     ##\n"
        "   @     ##   2    #\n"
        "  #      ###       ##\n"
        "      ##      @      ##\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::ifstream expectedFile("tests/outputs/output_b.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}

TEST_F(GameManagerTest, InputC) {
    const std::string mapContent =
     "Close Calls\n"
     "MaxSteps=51\n"
     "NumShells=20\n"
     "Rows=10\n"
     "Cols=9\n"
     "#########\n"
     "#1 @   2#\n"
     "#       #\n"
     "# # @#  #\n"
     "#  #    #\n"
     "# #   # #\n"
     "# #  #  #\n"
     "#  #@ # #\n"
     "#2   @ 1#\n"
     "#########\n";

    const std::string path = createTempMapFile(mapContent);
    GameManager gm{
        std::make_unique<ExtPlayerFactory>(),
        std::make_unique<ExtTankAlgorithmFactory>()
    };
    gm.readBoard(path);
    gm.run();
    std::ifstream expectedFile("tests/outputs/output_c.txt");
    ASSERT_TRUE(expectedFile.is_open()) << "Expected result file not found.";

    std::stringstream expectedBuffer;
    expectedBuffer << expectedFile.rdbuf();
    std::string expectedOutput = expectedBuffer.str();

    std::string actualOutput = readFile();

    EXPECT_NE(actualOutput.find(expectedOutput), std::string::npos);
}


// ===============================================
// TESTS That can't be checked in prod environment
// ===============================================

// TEST_F(GameManagerTest, ShellsCollide) {
//     const std::string mapContent =
//          "Shells Collide\n"
//          "MaxSteps=100\n"
//          "NumShells=10\n"
//          "Rows=3\n"
//          "Cols=11\n"
//          "###########\n"
//          "#2       1#\n"
//          "###########\n";
//
//     const std::string path = createTempMapFile(mapContent);
//     GameManager gm{
//         std::make_unique<ExtPlayerFactory>(),
//         std::make_unique<ExtTankAlgorithmFactory>()
//     };
//     gm.readBoard(path);
//     testing::internal::CaptureStdout();
//     gm.run();
//     std::string cout_output = testing::internal::GetCapturedStdout();
//     std::string log_output = readFile();
//     EXPECT_NE(cout_output.find("Shells collided"), std::string::npos);
// }

// TEST_F(GameManagerTest, ShootWallTwiceDestroysIt) {
//     testing::internal::CaptureStdout();
//     std::string mapContent =
//          "Wall Damage\n"
//          "MaxSteps=100\n"
//          "NumShells=10\n"
//          "Rows=3\n"
//          "Cols=11\n"
//          "###########\n"
//          "#2   #   1#\n"
//          "###########\n";
//
//     const std::string path = createTempMapFile(mapContent);
//     GameManager gm{
//         std::make_unique<ExtPlayerFactory>(),
//         std::make_unique<ExtTankAlgorithmFactory>()
//     };
//     gm.readBoard(path);
//     gm.run();
//     std::string cout_output = testing::internal::GetCapturedStdout();
//     std::string log_output = readFile();
//     EXPECT_NE(cout_output.find("weakened wall"), std::string::npos);
//     EXPECT_NE(cout_output.find("destroyed wall"), std::string::npos);
// }