#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

using namespace std;

struct Options {
  vector<string> paths;
  bool json = false;
  bool strict = false;
  optional<string> writeNormalized; // only valid in single-file, non-stats mode
  bool stats = false;
  bool csv = false;
};

static void printUsage(const char* argv0) {
  cerr <<
R"(Map Validator & Stats (TAU Tanks Assignment 2 format)

Usage:
  Validate one file (human):
    )" << argv0 << R"( <map.txt> [--strict] [--json] [--write-normalized <out.txt>]

  Stats for one or many files:
    )" << argv0 << R"( --stats [--csv] <map1.txt> [<map2.txt> ...]

Flags:
  --json              Print JSON summary (validate mode only)
  --strict            Error if raw file deviates from Rows/Cols (validate mode)
  --write-normalized  Write normalized map to a file (validate mode, single input)
  --stats             Print map statistics (enables multi-file support)
  --csv               CSV output for stats mode

Exit codes:
  0 OK
  1 Usage error / bad CLI
  2 Open/read failure (any file)
  3 Header parse/validation error
  4 Grid validation error (illegal chars, etc.)
  5 Strictness violation
)";
}

static bool startsWithInsensitive(string_view s, string_view prefix) {
  if (s.size() < prefix.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
  }
  return true;
}

static string trimCopy(string s) {
  auto issp = [](unsigned char c){ return std::isspace(c); };
  s.erase(s.begin(), find_if(s.begin(), s.end(), [&](unsigned char c){return !issp(c);} ));
  s.erase(find_if(s.rbegin(), s.rend(), [&](unsigned char c){return !issp(c);} ).base(), s.end());
  return s;
}

static optional<uint64_t> parseAfterEqualsFlexible(string_view line) {
  auto pos = line.find('=');
  if (pos == string::npos) return nullopt;
  string val = string(line.substr(pos + 1));
  val = trimCopy(val);
  if (val.empty()) return nullopt;
  uint64_t out = 0;
  auto sv = string_view(val);
  auto begin = sv.data();
  auto end = sv.data() + sv.size();
  auto [ptr, ec] = std::from_chars(begin, end, out);
  if (ec != std::errc() || ptr != end) return nullopt;
  return out;
}

struct Report {
  bool ok = false;
  vector<string> errors;
  vector<string> warnings;
  // parsed header
  string name;
  uint64_t maxSteps = 0, numShells = 0, rows = 0, cols = 0;
  size_t tank1Count = 0, tank2Count = 0;
  // normalized grid produced for this map
  vector<string> grid;
};

struct Stats {
  size_t area = 0;
  size_t walls = 0, mines = 0, empty = 0, t1 = 0, t2 = 0;
  double pctWalls = 0, pctMines = 0, pctEmpty = 0;
};

static void printHumanValidate(const Report& r) {
  if (!r.errors.empty()) {
    cerr << "ERRORS:\n";
    for (auto& e : r.errors) cerr << "  - " << e << "\n";
  }
  if (!r.warnings.empty()) {
    cerr << "WARNINGS:\n";
    for (auto& w : r.warnings) cerr << "  - " << w << "\n";
  }
  if (r.ok) {
    cout << "OK\n";
    cout << "Name: " << r.name << "\n";
    cout << "MaxSteps=" << r.maxSteps << ", NumShells=" << r.numShells
         << ", Rows=" << r.rows << ", Cols=" << r.cols << "\n";
    cout << "Tanks: P1=" << r.tank1Count << ", P2=" << r.tank2Count << "\n";
  }
}

static void printJsonValidate(const Report& r) {
  auto esc = [](const string& s){
    string o; o.reserve(s.size()+8);
    for (char c: s) { if (c=='"' || c=='\\') o.push_back('\\'); o.push_back(c); }
    return o;
  };
  cout << "{";
  cout << "\"ok\":" << (r.ok ? "true":"false") << ",";
  cout << "\"name\":\"" << esc(r.name) << "\",";
  cout << "\"maxSteps\":" << r.maxSteps << ",";
  cout << "\"numShells\":" << r.numShells << ",";
  cout << "\"rows\":" << r.rows << ",";
  cout << "\"cols\":" << r.cols << ",";
  cout << "\"tank1\":" << r.tank1Count << ",";
  cout << "\"tank2\":" << r.tank2Count << ",";
  cout << "\"errors\":[";
  for (size_t i=0;i<r.errors.size();++i) {
    if (i) cout << ",";
    cout << "\"" << esc(r.errors[i]) << "\"";
  }
  cout << "],\"warnings\":[";
  for (size_t i=0;i<r.warnings.size();++i) {
    if (i) cout << ",";
    cout << "\"" << esc(r.warnings[i]) << "\"";
  }
  cout << "]}";
}

static bool legalChar(char c) {
  return c==' ' || c=='#' || c=='@' || c=='1' || c=='2';
}

static Report loadAndValidateOne(const string& path, bool strict) {
  Report r;
  ifstream in(path);
  if (!in) {
    r.errors.push_back("Cannot open file: " + path);
    return r;
  }

  // header
  if (!getline(in, r.name)) { r.errors.push_back("Missing line 1: map name/description"); }
  string l2,l3,l4,l5;
  if (!getline(in, l2)) r.errors.push_back("Missing line 2: MaxSteps = <NUM>");
  if (!getline(in, l3)) r.errors.push_back("Missing line 3: NumShells = <NUM>");
  if (!getline(in, l4)) r.errors.push_back("Missing line 4: Rows = <NUM>");
  if (!getline(in, l5)) r.errors.push_back("Missing line 5: Cols = <NUM>");
  if (!r.errors.empty()) return r;

  if (!startsWithInsensitive(l2, "MaxSteps")) r.errors.push_back("Line 2 must start with 'MaxSteps'");
  if (!startsWithInsensitive(l3, "NumShells")) r.errors.push_back("Line 3 must start with 'NumShells'");
  if (!startsWithInsensitive(l4, "Rows")) r.errors.push_back("Line 4 must start with 'Rows'");
  if (!startsWithInsensitive(l5, "Cols")) r.errors.push_back("Line 5 must start with 'Cols'");

  auto v2 = parseAfterEqualsFlexible(l2);
  auto v3 = parseAfterEqualsFlexible(l3);
  auto v4 = parseAfterEqualsFlexible(l4);
  auto v5 = parseAfterEqualsFlexible(l5);
  if (!v2) r.errors.push_back("Line 2: cannot parse number after '='");
  if (!v3) r.errors.push_back("Line 3: cannot parse number after '='");
  if (!v4) r.errors.push_back("Line 4: cannot parse number after '='");
  if (!v5) r.errors.push_back("Line 5: cannot parse number after '='");
  if (!r.errors.empty()) return r;

  r.maxSteps = *v2;
  r.numShells = *v3;
  r.rows = *v4;
  r.cols = *v5;
  if (r.rows == 0 || r.cols == 0) {
    r.errors.push_back("Rows and Cols must be positive");
    return r;
  }

  // raw grid
  vector<string> raw;
  string line;
  while (getline(in, line)) raw.push_back(line);

  // strict check: raw must exactly match dimensions
  if (strict) {
    if (raw.size() != r.rows) {
      r.errors.push_back("Strict: number of grid rows != Rows (" + to_string(raw.size()) + " != " + to_string(r.rows) + ")");
    }
    for (size_t i=0;i<raw.size();++i) {
      if (raw[i].size() != r.cols) {
        r.errors.push_back("Strict: grid row " + to_string(i) + " length != Cols (" + to_string(raw[i].size()) + " != " + to_string(r.cols) + ")");
      }
    }
    if (!r.errors.empty()) return r;
  }

  // normalize to rows x cols
  r.grid = raw;
  r.grid.resize(r.rows);
  for (auto& row : r.grid) {
    if (row.size() < r.cols) row.append(r.cols - row.size(), ' ');
    if (row.size() > r.cols) row.resize(r.cols);
  }

  // validate chars + count tanks
  for (size_t y=0; y<r.rows; ++y) {
    for (size_t x=0; x<r.cols; ++x) {
      char c = r.grid[y][x];
      if (!legalChar(c)) {
        r.errors.push_back("Illegal char '" + string(1,c) + "' at (" + to_string(y) + "," + to_string(x) + ")");
      }
      if (c=='1') ++r.tank1Count;
      else if (c=='2') ++r.tank2Count;
    }
  }
  if (!r.errors.empty()) return r;

  // warnings for zero-tank scenarios
  if (r.tank1Count==0 && r.tank2Count==0)
    r.warnings.push_back("Both players have zero tanks (immediate tie).");
  else if (r.tank1Count==0)
    r.warnings.push_back("Player 1 has zero tanks (auto lose on start).");
  else if (r.tank2Count==0)
    r.warnings.push_back("Player 2 has zero tanks (auto lose on start).");

  r.ok = true;
  return r;
}

static Stats calcStats(const Report& r) {
  Stats s;
  s.area = static_cast<size_t>(r.rows * r.cols);
  for (size_t y=0; y<r.rows; ++y) {
    for (size_t x=0; x<r.cols; ++x) {
      char c = r.grid[y][x];
      switch (c) {
        case '#': ++s.walls; break;
        case '@': ++s.mines; break;
        case '1': ++s.t1;    break;
        case '2': ++s.t2;    break;
        case ' ': ++s.empty; break;
        default: /* already validated */ break;
      }
    }
  }
  if (s.area > 0) {
    s.pctWalls = 100.0 * double(s.walls) / double(s.area);
    s.pctMines = 100.0 * double(s.mines) / double(s.area);
    s.pctEmpty = 100.0 * double(s.empty) / double(s.area);
  }
  return s;
}

static void printHumanStats(const string& path, const Report& r, const Stats& s) {
  cout << "== " << path << " ==\n";
  cout << "Name: " << r.name << "\n";
  cout << "Size: " << r.rows << " x " << r.cols << " (area=" << s.area << ")\n";
  cout << "MaxSteps=" << r.maxSteps << ", NumShells=" << r.numShells << "\n";
  cout << "Counts: walls=" << s.walls << ", mines=" << s.mines
       << ", empty=" << s.empty << ", tanks1=" << (r.tank1Count)
       << ", tanks2=" << (r.tank2Count) << "\n";
  cout << "Percents: %walls=" << s.pctWalls
       << ", %mines=" << s.pctMines
       << ", %empty=" << s.pctEmpty << "\n";
  if (!r.warnings.empty()) {
    cout << "Warnings:\n";
    for (auto& w : r.warnings) cout << "  - " << w << "\n";
  }
  cout << "\n";
}

static void printCsvHeader() {
  cout << "file,name,rows,cols,maxSteps,numShells,area,walls,mines,empty,tanks1,tanks2,pctWalls,pctMines,pctEmpty\n";
}

static void printCsvRow(const string& path, const Report& r, const Stats& s) {
  auto csvEsc = [](const string& v)->string{
    if (v.find_first_of(",\"\n") == string::npos) return v;
    string out="\"";
    for (char c: v) { if (c=='"' ) out.push_back('"'); out.push_back(c); }
    out.push_back('"');
    return out;
  };
  cout
    << csvEsc(path) << ","
    << csvEsc(r.name) << ","
    << r.rows << ","
    << r.cols << ","
    << r.maxSteps << ","
    << r.numShells << ","
    << s.area << ","
    << s.walls << ","
    << s.mines << ","
    << s.empty << ","
    << r.tank1Count << ","
    << r.tank2Count << ","
    << s.pctWalls << ","
    << s.pctMines << ","
    << s.pctEmpty << "\n";
}

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Options opt;

  // --- CLI parse
  for (int i=1; i<argc; ++i) {
    string a = argv[i];
    if (a == "--json") { opt.json = true; }
    else if (a == "--strict") { opt.strict = true; }
    else if (a == "--write-normalized") {
      if (i+1 >= argc) { printUsage(argv[0]); return 1; }
      opt.writeNormalized = argv[++i];
    }
    else if (a == "--stats") { opt.stats = true; }
    else if (a == "--csv") { opt.csv = true; }
    else if (!a.empty() && a[0] == '-') {
      printUsage(argv[0]); return 1;
    } else {
      opt.paths.push_back(a);
    }
  }

  // constraints
  if (opt.stats) {
    if (opt.paths.empty()) { printUsage(argv[0]); return 1; }
    if (opt.writeNormalized) {
      cerr << "--write-normalized is only available in validate mode with a single input.\n";
      return 1;
    }
    // Stats mode: multi-file OK. We'll validate+normalize in memory for each, then compute stats.
    bool anyOpenFail = false, anyHeaderFail = false, anyGridFail = false, anyStrictFail = false;
    if (opt.csv) printCsvHeader();

    for (const auto& p : opt.paths) {
      Report r = loadAndValidateOne(p, /*strict*/false); // stats mode uses non-strict normalization for robustness
      if (!r.errors.empty()) {
        // classify errors
        anyOpenFail |= (r.errors.size() == 1 && r.errors[0].rfind("Cannot open file:",0)==0);
        // crude classification to map to exit codes later:
        for (const auto& e: r.errors) {
          if (e.find("Missing line") != string::npos || e.find("parse number") != string::npos || e.find("must start with") != string::npos || e.find("Rows and Cols") != string::npos)
            anyHeaderFail = true;
          if (e.find("Illegal char") != string::npos) anyGridFail = true;
          if (e.rfind("Strict:",0)==0) anyStrictFail = true;
        }
        // still show minimal info
        cerr << "ERROR in " << p << ":\n";
        for (auto& e : r.errors) cerr << "  - " << e << "\n";
        continue;
      }
      r.ok = true;

      Stats s = calcStats(r);
      if (opt.csv) {
        printCsvRow(p, r, s);
      } else {
        printHumanStats(p, r, s);
      }
    }

    // pick the most relevant non-zero exit if any failures
    if (anyOpenFail) return 2;
    if (anyHeaderFail) return 3;
    if (anyGridFail) return 4;
    if (anyStrictFail) return 5;
    return 0;
  }

  // Validate mode (single file expected)
  if (opt.paths.size() != 1) { printUsage(argv[0]); return 1; }
  const string& path = opt.paths[0];

  Report r = loadAndValidateOne(path, opt.strict);
  if (!r.errors.empty()) {
    // choose exit code by first matching error
    for (const auto& e: r.errors) {
      if (e.rfind("Cannot open file:",0)==0) { if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); } return 2; }
      if (e.find("Missing line") != string::npos || e.find("parse number") != string::npos || e.find("must start with") != string::npos || e.find("Rows and Cols") != string::npos) { if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); } return 3; }
      if (e.find("Illegal char") != string::npos) { if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); } return 4; }
      if (e.rfind("Strict:",0)==0) { if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); } return 5; }
    }
    if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); }
    return 3;
  }

  // Optionally write normalized file
  if (opt.writeNormalized) {
    ofstream out(*opt.writeNormalized);
    if (!out) {
      r.warnings.push_back("Failed to open --write-normalized output file: " + *opt.writeNormalized);
    } else {
      out << r.name << "\n";
      out << "MaxSteps = " << r.maxSteps << "\n";
      out << "NumShells = " << r.numShells << "\n";
      out << "Rows = " << r.rows << "\n";
      out << "Cols = " << r.cols << "\n";
      for (size_t y=0; y<r.rows; ++y) out << r.grid[y] << "\n";
    }
  }

  r.ok = true;
  if (opt.json) { printJsonValidate(r); cout << "\n"; } else { printHumanValidate(r); }
  return 0;
}