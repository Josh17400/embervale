// EMBERVALE art, v2 rig (RIG-BODY, P2a): the 8 hair styles and the beard, the sheen, secondary motion (STYLE_P2 4, 7).
// A style is a map per facing over the head box (16 columns: x = head.x - 2 + i; row 0 = the head's top painted row),
// in the hair's ramp (0 deepest .. 4 the sheen), lit from the top-left. The left facing is painted from the right
// facing's map, mirrored, with every run of hair re-lit left to right (the sheen stays top-left). A style may add a
// back layer (painted behind the body: hair down the back) besides its front layer.
#include <cstring>
#include <string>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

namespace {

struct HairMap {
  int rows = 0;
  const char* const* m = nullptr;
};

// ---- Short (STYLE_P2 19: the man hero's Chestnut short hair): a side part on the left, the fringe swept right
const char* const kShortDown[] = {
    ".....344322.....",
    "....34443221....",
    "...2333322211...",
    "...22221....1...",
    "...2........1...",
};
const char* const kShortUp[] = {
    ".....344322.....",
    "....34443221....",
    "...2333322211...",
    "...2332222111...",
    "...2222212111...",
    "....22221111....",
    "....22211111....",
    "...1222211111...",
    "....11221111....",
    ".....111111.....",
};
const char* const kShortRight[] = {
    ".....334432.....",
    "....23344322....",
    "...2333332221...",
    "...22333221.....",
    "...222211.......",
    "...2221.........",
    "...2211.........",
    "...1211.........",
    "....111.........",
    ".....11.........",
};

// ---- Braids (the woman hero's Chestnut braids): a centre part, curtain bangs, two plaits over the shoulders
const char* const kBraidsDown[] = {
    ".....341322.....",
    "....34313221....",
    "...2333232221...",
    "...233....221...",
    "...23......21...",
    "..32........12..",
    "..32........21..",
    "..23........12..",
    "..32........21..",
    "..23........12..",
    "..32........21..",
    "..23........12..",
    "..32........21..",
    "..23........12..",
    "..tT........tT..",
    "..32........21..",
    "...2........1...",
};
const char* const kBraidsUp[] = {
    ".....341322.....",
    "....33433222....",
    "...2333432221...",
    "...2334322211...",
    "...2233222111...",
    "..324322211112..",
    "..322322211121..",
    "..232222211112..",
    "..32122221121...",
    "..23.111111.12..",
    "..32........21..",
    "..23........12..",
    "..tT........tT..",
};
const char* const kBraidsRight[] = {
    ".....334322.....",
    "....23443322....",
    "...2334332221...",
    "...233322221....",
    "...22332.21.....",
    "...2221.........",
    "...2211.........",
    "...12232........",
    "....1123........",
    "......32........",
    "......23........",
    "......32........",
    "......23........",
    "......tT........",
    "......2.........",
};

// ---- Long (to the shoulders in front, down the back): a centre part, curtains over the brow, straight lengths
const char* const kLongDown[] = {
    ".....341322.....",
    "....34313221....",
    "...2333232221...",
    "...233....221...",
    "...23......21...",
    "..32........12..",
    "..32........12..",
    "..32........21..",
    "..23........12..",
    "..32........21..",
    "..21........11..",
    "..2..........1..",
};
const char* const kLongUp[] = {
    ".....344322.....",
    "....33433222....",
    "...2333432221...",
    "...2334322211...",
    "...2233222111...",
    "..223322211111..",
    "..232322211211..",
    "..233222212111..",
    "..223222211211..",
    "..232222211121..",
    "..223222111211..",
    "...2322211121...",
    "...2232211211...",
    "....22211111....",
    ".....121211.....",
};
const char* const kLongRight[] = {
    ".....334322.....",
    "....23443322....",
    "...2334332221...",
    "...23332221.....",
    "...223211.......",
    "..2232..........",
    "..2321..........",
    "..2321..........",
    "..2231..........",
    "..3221..........",
    "..2321..........",
    "..2221..........",
    "..3211..........",
    "..2211..........",
    "...21...........",
};

// ---- Bun (the village woman's Wheat hair): swept back from a centre part into a low knot at the nape
const char* const kBunDown[] = {
    ".....341322.....",
    "....33413222....",
    "...2332..2221...",
    "...232....221...",
    "...22......21...",
};
const char* const kBunUp[] = {
    ".....344322.....",
    "....33433222....",
    "...2333432221...",
    "...2334322211...",
    "...2233222111...",
    "....23322111....",
    "....22322111....",
    "....23443211....",
    "...2343332211...",
    "...2333222211...",
    "....1222111.....",
};
const char* const kBunRight[] = {
    ".....334322.....",
    "....23443322....",
    "...2334332221...",
    "...23332221.....",
    "...223221.......",
    "...2221.........",
    "...2211.........",
    "..12321.........",
    ".234321.........",
    ".233221.........",
    "..1221..........",
};

// ---- Bald (RIG-BODY, after the slice): no hair, the scalp's sheen top-left (codes a-e: the skin ramp 0..4)
const char* const kBaldDown[] = {".....ed.........", "....d..........."};
const char* const kBaldUp[] = {".....ed.........", "....dd..........", "...dd...........", "...d............", "...d............",
                               "...d............", "...c.......b....", "....c.....bb...."};
const char* const kBaldRight[] = {".....ed.........", "....d...........", "...d............", "", "", ".......dc.......", ".......cb......."};

// ---- Ponytail: slicked back from a centre part, the forehead clear, tied at the back; the tail hangs and swings
const char* const kPonyDown[] = {
    ".....341322.....",
    "....34413322....",
    "...2343313221...",
    "...22......21...",
    "...2........1...",
};
const char* const kPonyUp[] = {
    ".....344322.....",
    "....33433222....",
    "...2333432221...",
    "...2334322211...",
    "...2233222111...",
    "....23322111....",
    ".....232211.....",
    "......2321......",
    ".......tT.......",
    ".......32.......",
    ".......23.......",
    ".......32.......",
    ".......21.......",
    "........1.......",
};
const char* const kPonyRight[] = {
    ".....334432.....",
    "....23344322....",
    "...2333332221...",
    "...22333221.....",
    "...222211.......",
    "..tT2211........",
    ".2322111........",
    ".32.............",
    ".23.............",
    ".32.............",
    "..21............",
    "..1.............",
};
// (the front view's back layer: the tail peeks out beside the neck, behind the body)
const char* const kPonyDownBack[] = {
    "", "", "", "", "", "", "", "",
    "............21..",
    "............32..",
    "............21..",
    "............2...",
};

// ---- Mohawk: a crest from the brow to the nape rising 2 px over the crown, the sides shaved to stubble ('s')
const char* const kMohawkDown[] = {
    ".......43.......",
    "......3432......",
    ".....s3432s.....",
    "....ss2321ss....",
    "...sss.21.sss...",
    "...s........s...",
};
const char* const kMohawkUp[] = {
    ".......43.......",
    "......3432......",
    ".....s3432s.....",
    "....ss3421ss....",
    "...sss3421sss...",
    "...sss3321sss...",
    "...sss2321sss...",
    "...sss2321sss...",
    "....ss2321ss....",
    ".....s2221s.....",
    "......221.......",
    ".......1........",
};
const char* const kMohawkRight[] = {
    ".....34343......",
    "...3434343432...",
    "...s343432221...",
    "...s3222........",
    "...s32s.........",
    "...s32s.........",
    "...s21s.........",
    "...s21s.........",
    "...ss1s.........",
    "....ss..........",
};

// ---- Curls: a round mass of curls a px over the crown and out at the temples, framing the face to the jaw
const char* const kCurlsDown[] = {
    ".....343432.....",
    "...3434343221...",
    "..343433432321..",
    "..343232323221..",
    "..3232.32..321..",
    "..32........21..",
    "..23........12..",
    "..32........21..",
    "...21......12...",
};
const char* const kCurlsUp[] = {
    ".....343432.....",
    "...3434343221...",
    "..343433432321..",
    "..343232323221..",
    "..323432343221..",
    "..343234323211..",
    "..232343232121..",
    "..323232321211..",
    "...2323232121...",
    "....21212111....",
};
const char* const kCurlsRight[] = {
    ".....34343......",
    "...343434321....",
    "..34343434321...",
    "..3432323232....",
    "..32343232......",
    "..343232........",
    "..232321........",
    "..32321.........",
    "..2321..........",
    "...21...........",
};

// ---- the beard (men only): over the face's 12 columns (x = head.x + i), from the cheeks to a short point below the jaw
const char* const kBeardDown[] = {
    "", "", "", "", "", "",
    "..3......1..",
    ".33......21.",
    "..332.1221..",
    "..33222211..",
    "...322211...",
    "....2211....",
};
const char* const kBeardRight[] = {
    "", "", "", "", "",
    ".......2....",
    ".......2....",
    ".......32...",
    ".......32.2.",
    "......332221",
    "......3221..",
    ".......21...",
};

template <size_t N>
HairMap hm(const char* const (&a)[N]) { return HairMap{(int)N, a}; }

bool styleMaps(int style, int facing, HairMap& out) {
  switch ((Hair)style) {
    case Hair::Short: out = facing == FDown ? hm(kShortDown) : facing == FUp ? hm(kShortUp) : hm(kShortRight); break;
    case Hair::Braids: out = facing == FDown ? hm(kBraidsDown) : facing == FUp ? hm(kBraidsUp) : hm(kBraidsRight); break;
    case Hair::Long: out = facing == FDown ? hm(kLongDown) : facing == FUp ? hm(kLongUp) : hm(kLongRight); break;
    case Hair::Bun: out = facing == FDown ? hm(kBunDown) : facing == FUp ? hm(kBunUp) : hm(kBunRight); break;
    case Hair::Bald: out = facing == FDown ? hm(kBaldDown) : facing == FUp ? hm(kBaldUp) : hm(kBaldRight); break;
    case Hair::Ponytail: out = facing == FDown ? hm(kPonyDown) : facing == FUp ? hm(kPonyUp) : hm(kPonyRight); break;
    case Hair::Mohawk: out = facing == FDown ? hm(kMohawkDown) : facing == FUp ? hm(kMohawkUp) : hm(kMohawkRight); break;
    case Hair::Curls: out = facing == FDown ? hm(kCurlsDown) : facing == FUp ? hm(kCurlsUp) : hm(kCurlsRight); break;
    default: return false;
  }
  return true;
}
// rows painted above the head's top row (a crest or a mass of curls), and the first row that hangs and swings
int styleRise(int style) { return style == (int)Hair::Mohawk ? 2 : style == (int)Hair::Curls ? 1 : 0; }
int swingFrom(int style, int facing) {
  const bool side = facing == FRight || facing == FLeft;
  switch ((Hair)style) {
    case Hair::Braids: return side ? 9 : 11;
    case Hair::Ponytail: return side ? 7 : 9;
    case Hair::Long: return side ? 10 : 9;
    default: return 99;
  }
}
// the long lengths that fall out under headwear (and the rows they start at): the crown and the face stay covered
bool hasLengths(int style) {
  return style == (int)Hair::Long || style == (int)Hair::Braids || style == (int)Hair::Ponytail || style == (int)Hair::Curls;
}

// paints a map (hair codes, or skin codes a-e) over the head grid; the left facing lands each run mirrored with its codes
// kept in their left-to-right order (re-lit)
void paintMap(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, const HairMap& m, int rise, int cols, int colOff, int fromRow,
              bool clearFace) {
  const int top = h.y + 1;
  const uint32_t tie = K.trim[3], tieS = K.trim[1];   // ribbons in the look's accent colour
  const uint32_t stubble = mix(K.skin[1], K.hair[1], 0.35f);
  const int sw = swingFrom(K.hair_, P.facing);
  const bool side = P.facing == FRight || P.facing == FLeft;
  // kids: long lengths shorten to end 3 rows over the hips (the map's rows from kHang on are taken out, the rest closes up)
  constexpr int kHang = 6;
  int kidSkip = 0;
  if (B.age && m.rows - rise - 1 > kHang) {
    const int lastHr = m.rows - 1 - rise;
    const int lastY = top + headRow(B, lastHr);
    kidSkip = std::max(0, std::min(lastHr - kHang - 2, lastY - (B.hip + P.dy - 3)));
  }
  for (int r = 0; r < m.rows; r++) {
    const int mr = r - rise;
    if (kidSkip && mr >= kHang && mr < kHang + kidSkip) continue;
    const int hr = kidSkip && mr >= kHang + kidSkip ? mr - kidSkip : mr;
    if (hr < fromRow) continue;
    const int pr = hr < 0 ? hr : headRow(B, hr);
    if (hr >= 0 && pr < 0) continue;
    char row[24];
    int n = 0;
    for (const char* s = m.m[r]; *s && n < 23; s++) row[n++] = *s;
    row[n] = 0;
    if (P.sheen)   // the sheen slides a px along its band
      for (int i = 0; i + 1 < n; i++) if (row[i] == '4' && row[i + 1] == '3') { row[i] = '3'; row[i + 1] = '4'; i++; }
    int dx = 0;
    if (hr >= sw && P.hairSwing) dx = side ? (P.facing == FRight ? -P.hairSwing : P.hairSwing) : P.hairSwing;
    auto put = [&](int i, char ch) {
      if (ch == '.') return;
      uint32_t col;
      if (ch == 't') col = tie;
      else if (ch == 'T') col = tieS;
      else if (ch == 's') col = stubble;
      else if (ch >= 'a' && ch <= 'e') col = K.skin[ch - 'a'];
      else col = K.hair[ch - '0'];
      const int x = h.x + colOff + i + dx;
      if (clearFace && P.facing == FDown && i + colOff >= 2 && i + colOff <= 9) return;     // under a hat: the face stays clear
      if (clearFace && side && (P.facing == FRight ? i + colOff >= 5 : i + colOff <= 6)) return;
      f.set(x, top + pr, col);
    };
    if (P.facing != FLeft) { for (int i = 0; i < n; i++) put(i, row[i]); continue; }
    char mir[24];
    mirrorRelit(row, n, cols, mir);
    for (int i = 0; i < cols; i++) put(i, mir[i]);
  }
}

}  // namespace

void paintHair(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, bool backLayer) {
  const bool hides = headwearHides(K);
  const bool cloak = K.hood || K.headwear == 1 || K.headwear == 5 || K.headwear == 8;   // the hoods, the veil and the scarf cover every length too
  if (backLayer) {
    if (hides) return;
    if (K.hair_ == (int)Hair::Ponytail && P.facing == FDown) paintMap(f, B, K, P, h, hm(kPonyDownBack), 0, 16, -2, 0, false);
    return;
  }
  HairMap m;
  if (!styleMaps(K.hair_, P.facing, m)) return;
  if (hides && (cloak || !hasLengths(K.hair_))) return;
  // under the turban or the fur hat only the long lengths below the ears fall out, clear of the face
  paintMap(f, B, K, P, h, m, styleRise(K.hair_), 16, -2, hides ? 7 : -99, hides);
}

void paintBeard(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h) {
  if (P.facing == FUp) return;
  const HairMap m = P.facing == FDown ? hm(kBeardDown) : hm(kBeardRight);
  paintMap(f, B, K, P, h, m, 0, 12, 0, 0, false);
}

// the hair's contact shadow on the forehead (STYLE_P2 7): one ramp-1 row of skin under the hair
void hairContactShadow(Fig& f, const Kit& K, HeadAt h, uint8_t hairLayer) {
  const int top = h.y + 1;
  for (int y = top; y < top + 8; y++)
    for (int x = h.x; x < h.x + 12; x++) {
      if (f.layerAt(x, y) != hairLayer || f.layerAt(x, y + 1) == hairLayer || !f.solid(x, y + 1)) continue;
      const uint32_t s = f.get(x, y + 1);
      if (s == K.skin[2] || s == K.skin[3] || s == K.skin[4]) {
        const uint8_t keep = f.cur;
        f.cur = f.layerAt(x, y + 1);
        f.set(x, y + 1, K.skin[1]);
        f.cur = keep;
      }
    }
}

}  // namespace rig2
}  // namespace art
