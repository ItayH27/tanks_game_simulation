#include <bits/stdc++.h>
using namespace std;

/*
  Random Map Generator for TAU Tanks game.

  Output format (strict):
    Line 1: map name / description
    Line 2: MaxSteps = <NUM>
    Line 3: NumShells = <NUM>
    Line 4: Rows = <NUM>
    Line 5: Cols = <NUM>
    Lines 6+: the grid, exactly Rows x Cols chars

  Valid map chars:
    ' ' (space) = empty
    '#' = wall
    '@' = mine
    '1'...'9' (we'll use '1' for P1 tanks, '2' for P2 tanks)
  (We ensure at least one tank for each player unless --tanks1/--tanks2 set to 0)
*/

struct Options {
  string outPath = "random_map.txt";
  string name = "Random Battlefield";
  size_t rows = 20;
  size_t cols = 40;
  size_t maxSteps = 5000;
  size_t numShells = 20;
  // content controls
  double pWall = 0.12;   // probability for '#'
  double pMine = 0.03;   // probability for '@'
  size_t tanks1 = 2;     // how many '1'
  size_t tanks2 = 2;     // how many '2'
  bool borderWalls = true;
  uint64_t seed = std::random_device{}();

  // clamp + sanity
  void validate() {
    rows = max<size_t>(1, rows);
    cols = max<size_t>(1, cols);
    if (pWall < 0) pWall = 0; if (pWall > 1) pWall = 1;
    if (pMine < 0) pMine = 0; if (pMine > 1) pMine = 1;
  }
};

static void usage(const char* argv0) {
  cerr <<
    "Random Tanks Map Generator\n"
    "Usage:\n"
    "  " << argv0 << " [--out <file>] [--name <string>]\n"
    "                [--rows <N>] [--cols <N>] [--max-steps <N>] [--num-shells <N>]\n"
    "                [--p-wall <0..1>] [--p-mine <0..1>] [--tanks1 <N>] [--tanks2 <N>]\n"
    "                [--no-border-walls] [--seed <N>]\n"
    "\nExamples:\n"
    "  " << argv0 << " --rows 25 --cols 60 --tanks1 3 --tanks2 3 --out my_map.txt\n"
    "  " << argv0 << " --p-wall 0.08 --p-mine 0.02 --seed 12345\n";
}

static bool parseArgs(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    string a = argv[i];
    auto need = [&](int k){ if (i + k >= argc) { usage(argv[0]); return false; } return true; };

    if (a == "--out" && need(1))       { opt.outPath = argv[++i]; }
    else if (a == "--name" && need(1)) { opt.name = argv[++i]; }
    else if (a == "--rows" && need(1)) { opt.rows = stoul(argv[++i]); }
    else if (a == "--cols" && need(1)) { opt.cols = stoul(argv[++i]); }
    else if (a == "--max-steps" && need(1)) { opt.maxSteps = stoul(argv[++i]); }
    else if (a == "--num-shells" && need(1)) { opt.numShells = stoul(argv[++i]); }
    else if (a == "--p-wall" && need(1)) { opt.pWall = stod(argv[++i]); }
    else if (a == "--p-mine" && need(1)) { opt.pMine = stod(argv[++i]); }
    else if (a == "--tanks1" && need(1)) { opt.tanks1 = stoul(argv[++i]); }
    else if (a == "--tanks2" && need(1)) { opt.tanks2 = stoul(argv[++i]); }
    else if (a == "--no-border-walls")   { opt.borderWalls = false; }
    else if (a == "--seed" && need(1))   { opt.seed = stoull(argv[++i]); }
    else if (a == "-h" || a == "--help") { usage(argv[0]); exit(0); }
    else { cerr << "Unknown arg: " << a << "\n"; usage(argv[0]); return false; }
  }
  opt.validate();
  return true;
}

struct Cell { char c = ' '; };

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Options opt;
  if (!parseArgs(argc, argv, opt)) return 2;

  mt19937_64 rng(opt.seed);
  uniform_real_distribution<double> U(0.0, 1.0);

  // Build grid
  vector<vector<Cell>> g(opt.rows, vector<Cell>(opt.cols));

  // Fill with walls/mines by probability
  for (size_t r = 0; r < opt.rows; ++r) {
    for (size_t c = 0; c < opt.cols; ++c) {
      if (opt.borderWalls && (r == 0 || r + 1 == opt.rows || c == 0 || c + 1 == opt.cols)) {
        g[r][c].c = '#';
      } else {
        double x = U(rng);
        if (x < opt.pWall)      g[r][c].c = '#';
        else if (x < opt.pWall + opt.pMine) g[r][c].c = '@';
        else                     g[r][c].c = ' ';
      }
    }
  }

  auto empty_spots = [&](){
    vector<pair<size_t,size_t>> v;
    v.reserve(opt.rows*opt.cols);
    for (size_t r = 0; r < opt.rows; ++r)
      for (size_t c = 0; c < opt.cols; ++c)
        if (g[r][c].c == ' ') v.emplace_back(r,c);
    return v;
  };

  // Ensure room for tanks: carve a few spaces if needed near center
  auto carve_if_needed = [&](size_t want){
    auto v = empty_spots();
    if (v.size() >= want) return;
    // carve spaces randomly from non-space cells until we have enough
    vector<pair<size_t,size_t>> all;
    for (size_t r = 0; r < opt.rows; ++r)
      for (size_t c = 0; c < opt.cols; ++c)
        if (g[r][c].c != ' ') all.emplace_back(r,c);
    shuffle(all.begin(), all.end(), rng);
    for (auto [r,c] : all) {
      g[r][c].c = ' ';
      v.push_back({r,c});
      if (v.size() >= want) break;
    }
  };

  // Place tanks
  size_t totalTanks = opt.tanks1 + opt.tanks2;
  if (totalTanks > 0) carve_if_needed(totalTanks);

  auto v = empty_spots();
  shuffle(v.begin(), v.end(), rng);

  auto place_tokens = [&](size_t count, char ch) {
    for (size_t i = 0; i < count && !v.empty(); ++i) {
      auto [r,c] = v.back(); v.pop_back();
      g[r][c].c = ch;
    }
  };

  place_tokens(opt.tanks1, '1');
  place_tokens(opt.tanks2, '2');

  // Write file
  ofstream out(opt.outPath);
  if (!out) {
    cerr << "ERROR: cannot open output file: " << opt.outPath << "\n";
    return 3;
  }

  // Header lines (strictly as per spec)
  out << opt.name << "\n";
  out << "MaxSteps = " << opt.maxSteps << "\n";
  out << "NumShells = " << opt.numShells << "\n";
  out << "Rows = " << opt.rows << "\n";
  out << "Cols = " << opt.cols << "\n";

  // Grid
  for (size_t r = 0; r < opt.rows; ++r) {
    for (size_t c = 0; c < opt.cols; ++c) out << g[r][c].c;
    out << "\n";
  }

  cerr << "Wrote: " << opt.outPath << "\n";
  return 0;
}
