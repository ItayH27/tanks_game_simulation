#include <bits/stdc++.h>
using namespace std;

/*
  mapdraw — render Assignment-2 map files to ASCII or BMP.

  Spec recap:
    - Header lines 1..5:
        name
        "MaxSteps = <NUM>"
        "NumShells = <NUM>"
        "Rows = <NUM>"
        "Cols = <NUM>"   // :contentReference[oaicite:3]{index=3}
      Spaces around '=' may or may not exist, accept both. // :contentReference[oaicite:4]{index=4}
    - Grid from line 6; pad/trim to Rows×Cols when short/long. // :contentReference[oaicite:5]{index=5}
    - Allowed chars: ' ', '#', '@', '1', '2'. // :contentReference[oaicite:6]{index=6}

  Usage:
    ./mapdraw --in map.txt [--ascii] [--bmp out.bmp] [--cell 12] [--no-color]
*/

struct Options {
  string inPath;
  bool ascii = true;
  bool color = true;
  optional<string> bmpOut;
  int cell = 16; // pixels per cell (square)
};

static void usage(const char* a0){
  cerr <<
  "mapdraw - render map to ASCII or BMP\n"
  "Usage:\n"
  "  " << a0 << " --in <map.txt> [--ascii] [--bmp <out.bmp>] [--cell <N>] [--no-color]\n";
}

static bool startsWithI(string_view s, string_view p){
  if (s.size()<p.size()) return false;
  for(size_t i=0;i<p.size();++i)
    if (tolower((unsigned char)s[i])!=tolower((unsigned char)p[i])) return false;
  return true;
}

static string trim(string s){
  auto sp=[](unsigned char c){return isspace(c);};
  s.erase(s.begin(), find_if(s.begin(),s.end(),[&](unsigned char c){return !sp(c);} ));
  s.erase(find_if(s.rbegin(),s.rend(),[&](unsigned char c){return !sp(c);} ).base(), s.end());
  return s;
}

static optional<uint64_t> parseAfterEq(string_view line){
  auto p=line.find('=');
  if (p==string::npos) return nullopt;
  string v = string(line.substr(p+1));
  v = trim(v);
  if (v.empty()) return nullopt;
  uint64_t x=0; auto sv=string_view(v);
  auto [ptr,ec]=from_chars(sv.data(), sv.data()+sv.size(), x);
  if (ec!=errc() || ptr!=sv.data()+sv.size()) return nullopt;
  return x;
}

struct Map {
  string name; uint64_t maxSteps=0, numShells=0, rows=0, cols=0;
  vector<string> grid; // normalized rows×cols
};

static Map readMap(const string& path){
  ifstream in(path);
  if(!in) throw runtime_error("open failed: " + path);

  Map m; string l2,l3,l4,l5;
  if(!getline(in,m.name)) throw runtime_error("missing line 1 (name)");
  if(!getline(in,l2)) throw runtime_error("missing line 2 (MaxSteps)");
  if(!getline(in,l3)) throw runtime_error("missing line 3 (NumShells)");
  if(!getline(in,l4)) throw runtime_error("missing line 4 (Rows)");
  if(!getline(in,l5)) throw runtime_error("missing line 5 (Cols)");

  if(!startsWithI(l2,"MaxSteps"))  throw runtime_error("line 2 must start with MaxSteps");
  if(!startsWithI(l3,"NumShells")) throw runtime_error("line 3 must start with NumShells");
  if(!startsWithI(l4,"Rows"))      throw runtime_error("line 4 must start with Rows");
  if(!startsWithI(l5,"Cols"))      throw runtime_error("line 5 must start with Cols");

  auto v2=parseAfterEq(l2); auto v3=parseAfterEq(l3);
  auto v4=parseAfterEq(l4); auto v5=parseAfterEq(l5);
  if(!v2||!v3||!v4||!v5) throw runtime_error("failed parsing one of header numbers");
  m.maxSteps=*v2; m.numShells=*v3; m.rows=*v4; m.cols=*v5;
  if(m.rows==0||m.cols==0) throw runtime_error("rows/cols must be positive");

  vector<string> raw; string line;
  while(getline(in,line)) raw.push_back(line);

  // normalize to rows×cols
  m.grid = std::move(raw);
  m.grid.resize(m.rows);
  for(auto& r : m.grid){
    if(r.size()<m.cols) r.append(m.cols-r.size(),' ');
    if(r.size()>m.cols) r.resize(m.cols);
  }

  // validate allowed chars
  auto ok=[&](char c){
    return c==' '||c=='#'||c=='@'||c=='1'||c=='2';
  };
  for(size_t y=0;y<m.rows;++y)
    for(size_t x=0;x<m.cols;++x)
      if(!ok(m.grid[y][x]))
        throw runtime_error(string("illegal char '")+m.grid[y][x]+
                            "' at ("+to_string(y)+","+to_string(x)+")");
  return m;
}

// ---------- ASCII rendering ----------
static void printAscii(const Map& m, bool color){
  // Simple palette
  auto paint=[&](char c){
    if(!color) { cout<<c; return; }
    // ANSI colors
    switch(c){
      case '#': cout<<"\x1b[38;5;240m#\x1b[0m"; break;        // gray walls
      case '@': cout<<"\x1b[38;5;208m@\x1b[0m"; break;        // orange mine
      case '1': cout<<"\x1b[38;5;39m1\x1b[0m"; break;         // blue P1
      case '2': cout<<"\x1b[38;5;196m2\x1b[0m"; break;        // red P2
      default:  cout<<' '; break;
    }
  };
  cout << m.name << "  ("<<m.rows<<"x"<<m.cols<<")\n";
  for(size_t y=0;y<m.rows;++y){
    for(size_t x=0;x<m.cols;++x) paint(m.grid[y][x]);
    cout<<"\n";
  }
}

// ---------- BMP (24-bit) writing ----------
#pragma pack(push,1)
struct BMPFileHeader {
  uint16_t bfType{0x4D42}; // 'BM'
  uint32_t bfSize{0};
  uint16_t bfReserved1{0};
  uint16_t bfReserved2{0};
  uint32_t bfOffBits{54};
};
struct BMPInfoHeader {
  uint32_t biSize{40};
  int32_t  biWidth{0};
  int32_t  biHeight{0};
  uint16_t biPlanes{1};
  uint16_t biBitCount{24};
  uint32_t biCompression{0};
  uint32_t biSizeImage{0};
  int32_t  biXPelsPerMeter{0};
  int32_t  biYPelsPerMeter{0};
  uint32_t biClrUsed{0};
  uint32_t biClrImportant{0};
};
#pragma pack(pop)

struct RGB { uint8_t b,g,r; }; // BMP uses BGR order

static RGB colorFor(char c){
  switch(c){
    case '#': return {128,128,128};   // gray
    case '@': return { 60,160,255};   // amber-ish (BGR)
    case '1': return {255,140,  0};   // blue-ish (BGR)
    case '2': return {  0,  0,255};   // red (BGR)
    default:  return { 30, 30, 30};   // dark floor
  }
}

static void writeBMP(const Map& m, const string& outPath, int cell){
  int W = int(m.cols)*cell;
  int H = int(m.rows)*cell;
  int rowStride = ((W*3 + 3) / 4) * 4; // padded to 4 bytes
  vector<uint8_t> pixels(size_t(rowStride)*H, 0);

  for(size_t y=0;y<m.rows;++y){
    for(size_t x=0;x<m.cols;++x){
      RGB rgb = colorFor(m.grid[y][x]);
      // fill cell block
      for(int dy=0; dy<cell; ++dy){
        int py = H-1 - (int(y)*cell + dy); // BMP is bottom-up
        uint8_t* row = pixels.data() + size_t(py)*rowStride;
        for(int dx=0; dx<cell; ++dx){
          int px = int(x)*cell + dx;
          uint8_t* p = row + px*3;
          p[0]=rgb.b; p[1]=rgb.g; p[2]=rgb.r;
        }
      }
    }
  }

  BMPFileHeader fh;
  BMPInfoHeader ih;
  ih.biWidth = W; ih.biHeight = H; ih.biSizeImage = rowStride*H;
  fh.bfSize = ih.biSizeImage + sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

  ofstream out(outPath, ios::binary);
  if(!out) throw runtime_error("cannot open bmp for write: " + outPath);
  out.write((char*)&fh, sizeof(fh));
  out.write((char*)&ih, sizeof(ih));
  out.write((char*)pixels.data(), pixels.size());
}

int main(int argc, char** argv){
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Options opt;
  for(int i=1;i<argc;++i){
    string a=argv[i];
    auto need=[&](int k){ return i+k<argc; };
    if(a=="--in" && need(1)) { opt.inPath = argv[++i]; }
    else if(a=="--ascii")    { opt.ascii = true; }
    else if(a=="--bmp" && need(1))  { opt.bmpOut = argv[++i]; }
    else if(a=="--cell" && need(1)) { opt.cell = max(1, stoi(argv[++i])); }
    else if(a=="--no-color") { opt.color = false; }
    else { cerr<<"Unknown or incomplete arg: "<<a<<"\n"; usage(argv[0]); return 1; }
  }
  if(opt.inPath.empty()){ usage(argv[0]); return 1; }

  try{
    Map m = readMap(opt.inPath);

    if(opt.ascii) printAscii(m, opt.color);

    if(opt.bmpOut){
      writeBMP(m, *opt.bmpOut, opt.cell);
      cerr << "Wrote BMP: " << *opt.bmpOut << " ("<<m.cols*opt.cell<<"x"<<m.rows*opt.cell<<")\n";
    }

    if(!opt.ascii && !opt.bmpOut){
      cerr << "Nothing to do. Add --ascii and/or --bmp <out.bmp>\n";
      return 1;
    }
    return 0;
  }catch(const exception& e){
    cerr << "ERROR: " << e.what() << "\n";
    return 2;
  }
}
