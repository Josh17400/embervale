// EMBERVALE art: the M1 economy's market stalls by trade and the specialisations' yard props (owner 2026-10-05: "the
// market stalls look TERRIBLE"; rpg/world/economy.h). See rpg/art.h for the conventions and rpg/art/art_internal.h for
// the shared helpers.
//
// A stall is a counter three tiles wide under a sloping cloth awning, seen from the customers' side in the game's 3/4
// view: the awning's top runs back and up from its scalloped valance (lit from the top-left, the stripes running down
// the slope), the posts stand at the corners, the shade under the awning shows the stall's back cloth and whatever
// hangs from the rail (strings of onions, fish, hams, bolts of cloth, tools), and the counter's top carries the trade's
// goods in baskets, crates and on boards. Every trade reads by its silhouettes and colours at 1x on a phone.
#include "rpg/art/art_internal.h"

namespace art {

namespace {

constexpr int SW = 48, SH = 54;
// rows and columns of the stall (canvas px). The awning stands high: the keeper, a tile behind the counter, shows from
// the counter's top up to the valance (head and shoulders under the awning, the legs behind the counter)
constexpr int AW_BACK = 3, AW_FRONT = 11;      // the awning's top: back edge .. front edge
constexpr int VAL0 = 12, VAL1 = 16;            // the valance (the scallops' tips on VAL1)
constexpr int RAIL = 17;                       // the hanging rail under the awning
constexpr int TOP0 = 37, TOP1 = 41;            // the counter's top (back .. front edge)
constexpr int FACE0 = 42, FACE1 = 51;          // the counter's front
constexpr int BASE = 52;                       // the ground line
constexpr int CX0 = 4, CX1 = 43;               // the counter's ends

struct Awn { uint32_t a, b; bool striped; };
Awn awningOf(int v) {
  static const Awn t[kStallAwnings] = {
      {rgba(184, 52, 48), rgba(238, 226, 198), true},    // red and cream
      {rgba(52, 92, 168), rgba(236, 228, 204), true},    // blue and cream
      {rgba(56, 128, 76), rgba(232, 222, 184), true},    // green and cream
      {rgba(214, 156, 52), rgba(124, 74, 40), false},    // ochre, a brown trim
      {rgba(126, 66, 140), rgba(236, 224, 206), true},   // purple and cream
      {rgba(214, 196, 156), rgba(168, 58, 50), false},   // natural canvas, a red trim
      {rgba(44, 124, 128), rgba(232, 216, 160), false},  // teal, a cream trim
      {rgba(150, 92, 52), rgba(222, 172, 74), true},     // brown and ochre
  };
  return t[((v % kStallAwnings) + kStallAwnings) % kStallAwnings];
}

void hl(Canvas& c, int x0, int x1, int y, uint32_t col) { for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) c.set(x, y, col); }
void vl(Canvas& c, int x, int y0, int y1, uint32_t col) { for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) c.set(x, y, col); }

// a wooden post, lit on its west side
void post(Canvas& c, int x, int y0, int y1, int k = 0) {
  for (int y = y0; y <= y1; y++) { c.set(x, y, kWood[3 - k]); c.set(x + 1, y, kWood[1 - (k > 0)]); }
}

// a small wicker basket (front rim at rows y..y+3) holding a heap of round produce of ramp R
void basketOf(Canvas& c, int x0, int w, int y, const Ramp& R, uint32_t seed, float ballR = 1.6f) {
  const Ramp Wk = ramp(rgba(170, 120, 62), 0.75f);
  // the heap
  for (int i = 0; i < w / 2 + 1; i++) {
    float bx = x0 + 1.5f + i * ((w - 3) / (float)std::max(1, w / 2)) + (hashf(i, 1, seed) - 0.5f);
    float by = y - 0.5f - (hashf(i, 2, seed) * 1.5f);
    ball(c, bx, by, ballR, ballR * 0.9f, R, 0.06f);
  }
  for (int i = 0; i < w / 3; i++) ball(c, x0 + 3.0f + i * 3.2f, y - 2.6f, ballR * 0.9f, ballR * 0.8f, R, 0.06f);
  // the wicker
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < w; i++) {
      int k = j == 0 ? 4 : ((i + j) % 2 ? 2 : 3);
      if (i == w - 1) k = 1;
      if (j == 3) k = std::min(k, 1);
      c.set(x0 + i, y + j, Wk[k]);
    }
}

// a little wooden crate seen from the front-top (front rows y..y+h-1), boards lit on top
void crateAt(Canvas& c, int x0, int w, int y, int h) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int k = j == 0 ? 4 : 2;
      if (i == 0 || i == w - 1 || j == h - 1) k = j == 0 ? 3 : 1;
      if (j > 0 && j < h - 1 && (j == h / 2)) k = 1;
      c.set(x0 + i, y + j, kWood[k]);
    }
}

// a fish lying on its side, head west (x, y = its middle), length len
void fishAt(Canvas& c, float x, float y, int len, const Ramp& R, bool flip = false) {
  const int s = flip ? -1 : 1;
  for (int i = 0; i < len; i++) {
    float t = (float)i / (len - 1);
    float half = 1.6f * std::sin(t * 3.14159f) + 0.3f;
    int px = (int)std::floor(x + s * (i - len * 0.5f));
    for (int j = -(int)std::ceil(half); j <= (int)std::ceil(half); j++) {
      int k = j < 0 ? 4 - (j == -(int)std::ceil(half) ? 1 : 0) : (j == 0 ? 3 : 1);
      c.set(px, (int)std::floor(y) + j, R[k]);
    }
  }
  int tx = (int)std::floor(x + s * (len * 0.5f));
  c.set(tx, (int)y - 2, R[1]); c.set(tx, (int)y + 1, R[1]); c.set(tx + s, (int)y - 2, R[2]); c.set(tx + s, (int)y + 2, R[1]);
  c.set((int)std::floor(x - s * (len * 0.5f - 1.5f)), (int)y - 1, kInk);   // the eye
}

// ---------------------------------------------------------------- the stall frame
void stallBack(Canvas& c, const Awn& A) {
  // the shade under the awning: the stall's back cloth, darkest under the awning, a little light reaching its foot
  // (the stall is open at the back below the rail: its keeper, standing behind the counter, shows between the counter
  // and the awning; the goods hang to either side of the keeper)
  const Ramp B = ramp(mix(A.striped ? A.a : A.b, rgba(70, 50, 60), 0.55f), 0.7f);
  for (int y = VAL0; y <= RAIL + 1; y++)
    for (int x = 5; x <= 42; x++) c.set(x, y, B[y < RAIL - 2 ? 0 : 1]);
  // the back posts in the shade, the rail
  post(c, 5, AW_BACK + 1, TOP0, 2);
  post(c, 41, AW_BACK + 1, TOP0, 2);
  hl(c, 5, 42, RAIL, kWoodDark[2]);
  hl(c, 5, 42, RAIL + 1, kWoodDark[0]);
}

void stallCounter(Canvas& c, const Awn& A, int trade) {
  // the top: boards running along, lit, the front edge catching the light; the back third in the awning's shade
  for (int y = TOP0; y <= TOP1; y++)
    for (int x = CX0; x <= CX1; x++) {
      int k = y == TOP1 ? 4 : (y <= TOP0 + 1 ? 2 : 3);
      if ((x * 7 + y * 3) % 11 == 0 && y != TOP1) k--;
      c.set(x, y, kWood[k]);
    }
  // the front: upright boards, a cloth skirt in the awning's trim for the cloth and bread stalls, a dark foot
  const bool skirt = trade == 2 || trade == 5 || trade == 3;
  const Ramp S = ramp(A.striped ? A.a : A.b, 0.8f);
  for (int y = FACE0; y <= FACE1; y++)
    for (int x = CX0; x <= CX1; x++) {
      int k = (x - CX0) % 5 == 0 ? 1 : 2;
      if (x == CX0) k = 3;
      if (y == FACE0) k = 1;                 // under the top's lip
      if (y >= FACE1 - 1) k = std::min(k, 1);
      uint32_t col = kWood[k];
      if (skirt && y >= FACE0 + 1 && y <= FACE0 + 5) {
        int sk = y == FACE0 + 5 ? 1 : ((x / 4) % 2 ? 2 : 3);
        if (y == FACE0 + 5 && (x % 4) >= 2) continue;   // a zigzag hem
        col = S[sk];
      }
      c.set(x, y, col);
    }
  hl(c, CX0, CX1, BASE, kWoodDark[0]);
}

void stallFront(Canvas& c, const Awn& A, int awning) {
  // the front posts
  post(c, 2, VAL1 - 1, BASE);
  post(c, 44, VAL1 - 1, BASE, 1);
  // the awning's top: a sloping plane, its stripes running down the slope; lit from the top-left, the back edge
  // brightest, a fold of shade along the front where it rolls over into the valance
  const Ramp RA = ramp(A.a, 0.85f), RB = ramp(A.b, 0.7f);
  for (int y = AW_BACK; y <= AW_FRONT; y++) {
    const float t = (float)(y - AW_BACK) / (AW_FRONT - AW_BACK);   // 0 back .. 1 front
    const int xa = (int)std::lround(3 - 3 * t), xb = (int)std::lround(44 + 3 * t);
    for (int x = xa; x <= xb; x++) {
      const float u = (float)(x - xa) / std::max(1, xb - xa);   // 0 west .. 1 east
      const int stripe = (int)std::floor((x - 24) / 4.0f + 100 - (x - 24) * t * 0.06f) & 1;
      const Ramp& R = A.striped ? (stripe ? RB : RA) : RA;
      float l = 0.9f - u * 0.55f - t * 0.2f;
      if (y == AW_BACK) l += 0.4f;
      if (y == AW_FRONT) l -= 0.5f;
      if (y == 7) l -= 0.12f;   // the cloth sags a little over the middle spar
      int k = l > 0.75f ? 4 : (l > 0.35f ? 3 : (l > -0.05f ? 2 : 1));
      c.set(x, y, R[k]);
    }
  }
  // the ridge spar along the back edge
  hl(c, 3, 44, AW_BACK - 1, kWood[3]);
  c.set(3, AW_BACK - 1, kWood[4]);
  // the valance: a band of cloth hanging straight down from the front edge, scalloped (or zigzag) along its foot
  const bool zig = (awning & 2) != 0;
  for (int y = VAL0; y <= VAL1; y++)
    for (int x = 0; x < SW; x++) {
      const int cell = x / 4, inC = x % 4;
      if (y >= VAL1 - 1) {
        // the scallops: round tongues (or points) every 4 px
        if (zig) { if ((y == VAL1 - 1 && inC == 3) || (y == VAL1 && (inC == 0 || inC == 3))) continue; }
        else if ((y == VAL1 - 1 && inC == 3) || (y == VAL1 && inC != 1 && inC != 2)) continue;
      }
      const bool bandB = A.striped ? (cell & 1) : (y >= VAL0 + 1 && y <= VAL0 + 1);
      const Ramp& R = A.striped ? (bandB ? RB : RA) : (y == VAL0 + 1 ? RB : RA);
      int k = y == VAL0 ? 3 : 2;
      if (y >= VAL1 - 1) k = 1;
      if (x >= SW - 3) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
  (void)awning;
}

// ---------------------------------------------------------------- the trades' goods
void goodsProduce(Canvas& c, uint32_t seed) {
  // onions and garlic in strings, a bunch of herbs drying
  for (int s = 0; s < 3; s++) {
    const int x = s == 0 ? 8 : (s == 1 ? 13 : 35);
    for (int k = 0; k < 4; k++) ball(c, x + (k & 1), RAIL + 2.5f + k * 1.8f, 1.4f, 1.2f, s == 1 ? kBone : ramp(rgba(196, 140, 74)), 0.05f);
  }
  for (int k = 0; k < 4; k++) vl(c, 38 + k, RAIL + 1, RAIL + 4 + (k & 1), k % 2 ? kLeaf[2] : kLeaf[3]);
  // baskets of apples, oranges, cabbages and plums along the counter
  const Ramp apples = ramp(rgba(208, 48, 44), 0.8f), oranges = ramp(rgba(240, 150, 44), 0.75f), cabb = ramp(rgba(112, 170, 70), 0.8f),
             plums = ramp(rgba(120, 60, 140), 0.8f), lemons = ramp(rgba(236, 212, 70), 0.7f);
  const Ramp* sets[5] = {&apples, &cabb, &oranges, &plums, &lemons};
  for (int i = 0; i < 4; i++) basketOf(c, 6 + i * 9, 8, TOP0 + 1, *sets[(i + seed) % 5], seed + i * 7, i % 2 ? 2.0f : 1.6f);
}

void goodsFish(Canvas& c, uint32_t seed) {
  // dried fish hanging by their tails
  const Ramp dried = ramp(rgba(176, 140, 92), 0.75f);
  for (int s = 0; s < 4; s++) {
    const int x = s < 2 ? 8 + s * 6 : 34 + (s - 2) * 6;
    for (int j = 0; j < 7; j++) {
      const int w = j < 1 ? 1 : (j < 5 ? 2 : 1);
      for (int i = -w + 1; i <= w - 1 + (j > 1 && j < 5); i++) c.set(x + i, RAIL + 2 + j, dried[i < 0 ? 3 : (i == 0 ? 2 : 1)]);
    }
    c.set(x - 1, RAIL + 1, dried[1]); c.set(x + 1, RAIL + 1, dried[1]);
  }
  // a bed of crushed ice on the counter, fish laid across it, a big salmon in the middle
  for (int y = TOP0 - 1; y <= TOP1 - 1; y++)
    for (int x = CX0 + 1; x <= CX1 - 1; x++) {
      int k = ((x * 5 + y * 3) % 7 == 0) ? 4 : 3;
      if (y == TOP0 - 1) k = 2;
      c.set(x, y, kSnow[k]);
    }
  const Ramp silver = ramp5(rgba(46, 60, 92), rgba(84, 108, 138), rgba(140, 162, 182), rgba(196, 210, 220), rgba(240, 246, 250));
  const Ramp mack = ramp5(rgba(30, 60, 70), rgba(52, 100, 108), rgba(108, 150, 150), rgba(176, 204, 196), rgba(232, 240, 230));
  const Ramp salmon = ramp(rgba(226, 120, 96), 0.75f);
  for (int i = 0; i < 6; i++) {
    const float x = 9.0f + i * 5.6f, y = TOP0 + 1.5f + (i & 1);
    fishAt(c, x, y, 7, i % 3 == 1 ? mack : silver, (i + seed) % 2);
  }
  fishAt(c, 24.0f, TOP0 - 0.5f, 11, salmon);
}

void goodsCloth(Canvas& c, uint32_t seed) {
  // lengths of cloth hanging from the rail in folds
  static const uint32_t cl[6] = {rgba(64, 92, 176), rgba(190, 58, 58), rgba(228, 196, 96), rgba(64, 140, 96), rgba(150, 76, 156), rgba(214, 120, 64)};
  for (int s = 0; s < 4; s++) {
    const Ramp R = ramp(cl[(s + seed) % 6], 0.7f);
    const int x0 = s < 2 ? 6 + s * 5 : 32 + (s - 2) * 5, len = 8 + (int)((s * 5 + seed) % 3);
    for (int j = 0; j < len; j++)
      for (int i = 0; i < 5; i++) {
        int k = i == 0 ? 3 : (i == 4 ? 1 : ((i + j / 3) % 2 ? 2 : 3));
        if (j == len - 1 && (i == 0 || i == 4)) continue;
        c.set(x0 + i, RAIL + 1 + j, R[k]);
      }
  }
  // bolts and folded stacks on the counter
  for (int s = 0; s < 4; s++) {
    const Ramp R = ramp(cl[(s * 2 + seed + 1) % 6], 0.7f);
    const int x0 = 6 + s * 9, h = 4 + (int)((s * 3 + seed) % 3);
    for (int j = 0; j < h; j++)
      for (int i = 0; i < 8; i++) {
        int k = j == 0 ? 4 : ((j & 1) ? 2 : 3);
        if (i == 7) k = std::max(0, k - 2);
        c.set(x0 + i, TOP1 - 1 - j, R[k]);
      }
  }
}

void goodsPottery(Canvas& c, uint32_t seed) {
  const Ramp terra = ramp(rgba(190, 102, 60), 0.8f), glaze = ramp(rgba(60, 110, 170), 0.8f), green = ramp(rgba(96, 140, 88), 0.8f),
             cream = ramp(rgba(220, 204, 170), 0.6f);
  // a shelf across the back with jugs and cups
  for (int sd = 0; sd < 2; sd++) { hl(c, 6 + sd * 25, 16 + sd * 25, TOP0 - 4, kWood[3]); hl(c, 6 + sd * 25, 16 + sd * 25, TOP0 - 3, kWood[1]); }
  for (int i = 0; i < 4; i++) {
    const Ramp& R = (i + seed) % 3 == 0 ? glaze : ((i + seed) % 3 == 1 ? terra : cream);
    const float x = i < 2 ? 8.5f + i * 6.0f : 33.5f + (i - 2) * 6.0f;
    ball(c, x, TOP0 - 7.0f, 2.2f, 2.4f, R, 0.05f);
    hl(c, (int)x - 1, (int)x + 1, TOP0 - 10, R[1]);
  }
  // big jugs, vases and stacked bowls on the counter
  for (int i = 0; i < 5; i++) {
    const Ramp& R = (i + seed) % 4 == 0 ? glaze : ((i + seed) % 4 == 2 ? green : terra);
    const float x = 8.0f + i * 7.7f;
    if (i % 2 == 0) {
      ball(c, x, TOP1 - 3.0f, 3.0f, 3.2f, R, 0.06f);
      box(c, (int)x - 1, TOP1 - 8, (int)x + 1, TOP1 - 6, R[2]);
      hl(c, (int)x - 2, (int)x + 2, TOP1 - 9, R[3]);
      c.set((int)x + 3, TOP1 - 5, R[1]); c.set((int)x + 4, TOP1 - 4, R[1]);   // the handle
    } else {
      for (int b = 0; b < 3; b++) {
        hl(c, (int)x - 3 + b, (int)x + 3 - b, TOP1 - 2 - b * 2, R[2 + (b == 2)]);
        hl(c, (int)x - 2 + b, (int)x + 2 - b, TOP1 - 1 - b * 2, R[1]);
      }
    }
  }
}

void goodsMeat(Canvas& c, uint32_t seed) {
  const Ramp ham = ramp(rgba(170, 70, 58), 0.8f), saus = ramp(rgba(140, 66, 52), 0.7f), fat = kBone;
  // hams on hooks and strings of sausages
  for (int s = 0; s < 2; s++) {
    const int x = s == 0 ? 9 : 37;
    c.set(x, RAIL + 1, kIron[3]);
    ball(c, x + 0.5f, RAIL + 6.5f, 3.0f, 4.2f, ham, 0.05f);
    c.set(x, RAIL + 2, fat[3]); c.set(x + 1, RAIL + 2, fat[2]);
  }
  for (int s = 0; s < 2; s++) {
    const int x = s == 0 ? 15 : 31;
    for (int k = 0; k < 3; k++) ball(c, x + 0.5f, RAIL + 2.5f + k * 2.6f, 1.2f, 1.5f, saus, 0.04f);
  }
  // a chopping board with cuts, a block with a cleaver, cheese wheels
  hl(c, 6, 26, TOP1 - 1, kWood[2]);
  for (int i = 0; i < 3; i++) {
    const int x = 8 + i * 6;
    for (int j = 0; j < 3; j++) hl(c, x, x + 4, TOP1 - 2 - j, j == 2 ? fat[3] : ham[j == 0 ? 2 : 3]);
    c.set(x + 4, TOP1 - 3, ham[1]);
  }
  box(c, 29, TOP1 - 5, 35, TOP1 - 1, kWood[2]);
  hl(c, 29, 35, TOP1 - 5, kWood[4]);
  box(c, 31, TOP1 - 9, 34, TOP1 - 6, kIron[3]);   // the cleaver's blade
  hl(c, 31, 34, TOP1 - 9, kIron[4]);
  vl(c, 32, TOP1 - 12, TOP1 - 10, kWood[3]);
  const Ramp cheese = ramp(rgba(232, 196, 90), 0.75f);
  ball(c, 39.5f, TOP1 - 2.5f, 3.2f, 2.0f, cheese, 0.05f);
  (void)seed;
}

void goodsBread(Canvas& c, uint32_t seed) {
  const Ramp crust = ramp(rgba(204, 138, 64), 0.8f), dark = ramp(rgba(150, 92, 50), 0.75f);
  // pretzels and a garland of rolls from the rail
  for (int s = 0; s < 3; s++) {
    const int x = s == 0 ? 9 : (s == 1 ? 15 : 37);
    c.set(x, RAIL + 1, kWood[2]);
    for (int a = 0; a < 16; a++) {
      const float ang = a * 0.3927f;
      c.set(x + (int)std::lround(std::cos(ang) * 2.6f), RAIL + 5 + (int)std::lround(std::sin(ang) * 2.2f), crust[a < 8 ? 2 : 3]);
    }
    c.set(x, RAIL + 5, crust[1]);
  }
  // baskets of loaves, round cobs and long breads
  const Ramp Wk = ramp(rgba(170, 120, 62), 0.75f);
  for (int i = 0; i < 3; i++) {
    const int x0 = 6 + i * 12;
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 11; k++) c.set(x0 + k, TOP1 - 2 + j, Wk[j == 0 ? 4 : ((k + j) % 2 ? 2 : 3)]);
    const Ramp& R = (i + seed) % 3 == 1 ? dark : crust;
    if (i % 2 == 0) {
      ball(c, x0 + 3.0f, TOP1 - 4.0f, 2.8f, 2.0f, R, 0.05f);
      ball(c, x0 + 7.5f, TOP1 - 4.5f, 2.8f, 2.0f, R, 0.05f);
      ball(c, x0 + 5.0f, TOP1 - 6.5f, 2.6f, 1.8f, R, 0.05f);
    } else {
      for (int l = 0; l < 3; l++) {
        const int y = TOP1 - 4 - l;
        hl(c, x0 + 1 + l, x0 + 9 + l, y, R[3]);
        hl(c, x0 + 1 + l, x0 + 9 + l, y + 1, R[1]);
        for (int k = 0; k < 3; k++) c.set(x0 + 3 + l + k * 3, y, R[4]);   // the scoring
      }
    }
  }
  // a sack of flour at the end
  ball(c, 41.0f, TOP1 - 4.0f, 2.6f, 4.0f, kCloth, 0.06f);
  hl(c, 40, 42, TOP1 - 8, kWood[1]);
}

void goodsTools(Canvas& c, uint32_t seed) {
  // a pegboard of tools at the back: hammer, axe, saw, sickle, pick, tongs
  for (int y = RAIL + 2; y <= TOP0 - 2; y++)
    for (int x = 6; x <= 41; x++) if (x <= 16 || x >= 31) c.set(x, y, kWoodDark[(x + y) % 5 == 0 ? 1 : 2]);
  auto handle = [&](int x, int y0, int y1) { vl(c, x, y0, y1, kWood[3]); };
  handle(9, RAIL + 4, TOP0 - 3); box(c, 7, RAIL + 3, 11, RAIL + 4, kIron[3]); hl(c, 7, 11, RAIL + 3, kIron[4]);    // hammer
  handle(14, RAIL + 3, TOP0 - 3); box(c, 15, RAIL + 3, 16, RAIL + 6, kIron[2]); vl(c, 16, RAIL + 3, RAIL + 7, kIron[4]);   // axe
  for (int a = 0; a < 9; a++) c.set(34 + (int)std::lround(std::cos(a * 0.35f) * 3.0f), RAIL + 5 - (int)std::lround(std::sin(a * 0.35f) * 3.0f), kIron[3]);   // sickle
  handle(34, RAIL + 5, RAIL + 9);
  handle(38, RAIL + 4, TOP0 - 3); hl(c, 36, 40, RAIL + 4, kIron[3]); c.set(35, RAIL + 5, kIron[2]); c.set(41, RAIL + 5, kIron[2]);   // pick
  // on the counter: horseshoes, knives on a cloth, a pot, nails in a box
  hl(c, 6, 20, TOP1 - 1, kCloth[3]);
  for (int i = 0; i < 3; i++) {
    const int x = 7 + i * 5;
    for (int a = 0; a < 7; a++) c.set(x + (int)std::lround(std::cos(a * 0.52f) * 1.8f), TOP1 - 3 - (int)std::lround(std::sin(a * 0.52f) * 1.8f), kIron[a < 3 ? 4 : 2]);
  }
  for (int i = 0; i < 3; i++) { hl(c, 23 + i * 3, 24 + i * 3, TOP1 - 3, kIron[4]); hl(c, 23 + i * 3, 24 + i * 3, TOP1 - 2, kWood[1]); }
  ball(c, 36.0f, TOP1 - 3.5f, 3.6f, 2.6f, kIron, 0.06f);   // the pot
  hl(c, 33, 39, TOP1 - 6, kIron[1]);
  (void)seed;
}

// (M1 fixer) the lumber trade: a felling axe, a bow saw and a coil of rope hung from the rail; on the counter planks in a
// stack (their sawn ends toward us), bundles of split kindling tied with cord, and a short log on its side, rings showing
void goodsTimber(Canvas& c, uint32_t seed) {
  const Ramp bark = ramp(rgba(112, 78, 52), 0.75f), pale = ramp(rgba(214, 176, 120), 0.7f), rope = ramp(rgba(196, 168, 112), 0.7f);
  // the axe: a long haft, the head to the west
  vl(c, 9, RAIL + 2, TOP0 - 4, kWood[3]);
  vl(c, 10, RAIL + 2, TOP0 - 4, kWood[1]);
  box(c, 5, RAIL + 2, 8, RAIL + 5, kIron[2]);
  vl(c, 5, RAIL + 1, RAIL + 6, kIron[4]);
  c.set(6, RAIL + 2, kIron[3]);
  // the bow saw: a curved frame over a toothed blade
  for (int a = 0; a <= 12; a++) {
    const int x = 14 + a, y = RAIL + 2 + (int)std::lround(std::sin(a * 0.2618f) * -2.0f) + 2;
    c.set(x, y, kWood[2]);
  }
  hl(c, 14, 26, RAIL + 7, kIron[3]);
  for (int x = 14; x <= 26; x += 2) c.set(x, RAIL + 8, kIron[1]);
  vl(c, 14, RAIL + 3, RAIL + 7, kWood[3]); vl(c, 26, RAIL + 3, RAIL + 7, kWood[1]);
  // a coil of rope on a peg
  c.set(37, RAIL + 1, kWood[2]);
  for (int a = 0; a < 18; a++) {
    const float ang = a * 0.349f;
    c.set(37 + (int)std::lround(std::cos(ang) * 3.0f), RAIL + 5 + (int)std::lround(std::sin(ang) * 2.6f), rope[a < 9 ? 3 : 1]);
  }
  for (int a = 0; a < 12; a++) {
    const float ang = a * 0.52f;
    c.set(37 + (int)std::lround(std::cos(ang) * 1.6f), RAIL + 5 + (int)std::lround(std::sin(ang) * 1.4f), rope[2]);
  }
  // the planks: a stack of six boards, their ends toward us
  for (int j = 0; j < 6; j++) {
    const int y = TOP1 - 1 - j * 2;
    for (int i = 0; i < 13; i++) {
      const int x = 5 + i + (j & 1);
      int k = i == 0 ? 4 : (i == 12 ? 1 : 3);
      c.set(x, y - 1, pale[k]);
      c.set(x, y, pale[std::max(0, k - 2)]);
    }
  }
  // bundles of kindling tied with cord
  for (int b = 0; b < 2; b++) {
    const int x0 = 20 + b * 8;
    for (int i = 0; i < 6; i++)
      for (int j = 0; j < 5; j++) {
        const int k = (i + j + (int)seed) % 3 == 0 ? 2 : (j == 0 ? 4 : 3);
        c.set(x0 + i, TOP1 - 1 - j - (i == 2 || i == 3), pale[k]);
      }
    vl(c, x0 + 2, TOP1 - 5, TOP1 - 1, rope[1]);
  }
  // a log on its side: the bark along, the sawn end with its rings
  ball(c, 40.0f, TOP1 - 4.0f, 3.6f, 3.6f, bark, 0.05f);
  for (int r = 0; r < 3; r++)
    for (int a = 0; a < 16; a++) {
      const float ang = a * 0.3927f, rr = 2.6f - r * 0.9f;
      c.set(40 + (int)std::lround(std::cos(ang) * rr), TOP1 - 4 + (int)std::lround(std::sin(ang) * rr), pale[r == 1 ? 2 : 4]);
    }
  c.set(40, TOP1 - 4, pale[1]);
}

// ---------------------------------------------------------------- (M1 fixer round 2) the other stall forms
// the trade's goods drawn on their own canvas (the hanging part first, the counter's top over its foot)
Canvas tradeGoods(int trade, uint32_t seed) {
  Canvas goods(SW, SH);
  switch (trade % kStallTrades) {
    case 0: goodsProduce(goods, seed); break;
    case 1: goodsFish(goods, seed); break;
    case 2: goodsCloth(goods, seed); break;
    case 3: goodsPottery(goods, seed); break;
    case 4: goodsMeat(goods, seed); break;
    case 5: goodsBread(goods, seed); break;
    case 7: goodsTimber(goods, seed); break;
    default: goodsTools(goods, seed); break;
  }
  // the hanging goods sit in the awning's shade
  for (int y = 0; y < TOP0 - 1; y++)
    for (int x = 0; x < SW; x++) {
      uint32_t p = goods.get(x, y);
      if (chA(p)) goods.set(x, y, darken(p, y < RAIL + 6 ? 0.24f : 0.12f));
    }
  return goods;
}

// packed up for the night: the stock under a tied cover along the counter, a curtain let down over the opening
void closedCover(Canvas& c, uint32_t cloth, bool curtain) {
  const Ramp C = ramp(cloth, 0.75f);
  if (curtain)   // the curtain hangs from the rail to the counter, in folds
    for (int y = RAIL; y <= TOP0 - 1; y++)
      for (int x = 5; x <= 42; x++) {
        const int f = (x - 5) % 6;
        int k = f < 2 ? 3 : (f < 4 ? 2 : 1);
        if (y == RAIL) k = 1;
        if (y >= TOP0 - 2 && (x % 6) == 4) continue;   // the hem lifts between the folds
        c.set(x, y, darken(C[k], 0.25f));
      }
  // the cover: a sheet over the heaped stock, lumps showing through, cords across
  const Ramp S = ramp(rgba(196, 180, 146), 0.7f);
  for (int x = CX0 + 1; x <= CX1 - 1; x++) {
    const float u = (x - CX0) / (float)(CX1 - CX0);
    const int hump = (int)std::lround(2.2f + std::sin(u * 18.0f) * 1.3f + std::sin(u * 7.0f) * 0.8f);
    for (int y = TOP0 - hump; y <= TOP1 - 1; y++) {
      int k = y == TOP0 - hump ? 4 : (y < TOP0 ? 3 : 2);
      if (std::cos(u * 18.0f) > 0.55f && y > TOP0 - hump) k--;
      c.set(x, y, S[k]);
    }
  }
  for (int x : {12, 24, 36})
    for (int y = TOP0 - 4; y <= TOP1; y++) if (chA(c.get(x, y))) c.set(x, y, kWood[1]);
}

// form 1: a peaked canvas tent over a trestle with a long cloth
void tentBack(Canvas& c, const Awn& A) {
  // the back cloth down to the rail; below it the tent is open at the back (its keeper, standing behind the
  // trestle, shows between the cloth and the counter)
  const Ramp B = ramp(mix(A.a, rgba(90, 70, 70), 0.5f), 0.7f);
  for (int y = 13; y <= RAIL + 1; y++)
    for (int x = 3; x <= 44; x++) c.set(x, y, B[y < RAIL - 2 ? 1 : 2]);
  hl(c, 4, 43, RAIL, kWoodDark[2]);
  post(c, 5, RAIL, TOP0, 2);
  post(c, 41, RAIL, TOP0, 2);
}
void tentCounter(Canvas& c, const Awn& A) {
  // the trestle's top under a linen cloth, the cloth falling to a hem with its folds, the legs below
  const Ramp L = ramp(A.striped ? A.b : rgba(232, 222, 196), 0.6f), T = ramp(A.striped ? A.a : A.b, 0.8f);
  for (int y = TOP0; y <= TOP1; y++)
    for (int x = CX0; x <= CX1; x++) c.set(x, y, L[y == TOP1 ? 4 : (y <= TOP0 + 1 ? 2 : 3)]);
  for (int y = FACE0; y <= FACE1 - 2; y++)
    for (int x = CX0; x <= CX1; x++) {
      const int f = (x - CX0) % 7;
      int k = f == 0 ? 1 : (f < 3 ? 3 : (f < 5 ? 2 : 1));
      if (y == FACE0) k = 1;
      uint32_t col = L[k];
      if (y >= FACE1 - 4) col = T[k >= 2 ? 2 : 1];   // a coloured band along the hem
      c.set(x, y, col);
    }
  for (int x = CX0; x <= CX1; x++) if (x % 3) c.set(x, FACE1 - 1, T[1]);   // the fringe
  for (int lx : {7, 40}) { vl(c, lx, FACE1 - 1, BASE, kWoodDark[2]); vl(c, lx + 1, FACE1, BASE, kWoodDark[1]); }
}
void tentFront(Canvas& c, const Awn& A) {
  const Ramp RA = ramp(A.a, 0.85f), RB = ramp(A.b, 0.7f);
  // the roof: a ridge across the top, the front slope falling to the eave; stripes run down the slope
  const int R0 = 1, E = 13;
  for (int y = R0; y <= E; y++) {
    const float t = (float)(y - R0) / (E - R0);
    const int xa = (int)std::lround(6 - 6 * t), xb = (int)std::lround(41 + 6 * t);
    for (int x = xa; x <= xb; x++) {
      const float u = (float)(x - xa) / std::max(1, xb - xa);
      const float sx = 24 + (x - 24) / (0.86f + 0.14f * t);   // the stripes splay toward the eave
      const int stripe = (int)std::floor(sx / 5.0f) & 1;
      const Ramp& R = A.striped ? (stripe ? RB : RA) : RA;
      float l = 0.95f - u * 0.5f - t * 0.35f;
      if (y == R0) l += 0.3f;
      int k = l > 0.78f ? 4 : (l > 0.42f ? 3 : (l > 0.05f ? 2 : 1));
      c.set(x, y, R[k]);
    }
  }
  hl(c, 6, 41, R0 - 1, kWood[3]);   // the ridge pole
  c.set(5, R0, kWood[2]); c.set(42, R0, kWood[1]);
  // the hem along the eave and its shadow line
  const Ramp& H = A.striped ? RA : RB;
  for (int x = 0; x < SW; x++) {
    c.set(x, E + 1, H[x >= SW - 3 ? 1 : 2]);
    c.set(x, E + 2, H[1]);
    if (x % 4 == 1) c.set(x, E + 3, H[1]);
  }
  // the poles at the front corners and the side curtains tied back to them
  for (int x : {1, 45}) { vl(c, x, E + 1, BASE, kWood[3]); vl(c, x + 1, E + 1, BASE, kWood[1]); }
  const Ramp& Cu = A.striped ? RB : RA;
  for (int sd = 0; sd < 2; sd++)
    for (int y = E + 3; y <= E + 20; y++) {
      const int tie = E + 14;
      const int w = y < tie ? std::max(2, 7 - (y - E - 3) * 5 / 11) : 2 + (y - tie) / 2;
      for (int i = 0; i < w; i++) {
        const int x = sd == 0 ? 3 + i : 44 - i;
        int k = i == 0 ? 3 : (i == w - 1 ? 1 : 2);
        if (sd == 1) k = std::max(1, k - 1);
        c.set(x, y, Cu[k]);
      }
      if (y == tie)
        for (int i = 0; i < 3; i++) c.set(sd == 0 ? 3 + i : 44 - i, y, kWood[1]);
    }
}

// form 2: a timber booth under a shingle roof, a painted board with the trade's sign hung from its eave
void boothBack(Canvas& c) {
  // the plank back wall down to the rail; below it the booth is open at the back for its keeper
  for (int y = 13; y <= RAIL + 1; y++)
    for (int x = 4; x <= 43; x++) {
      int k = (x - 4) % 6 == 0 ? 0 : 1;
      if (y < RAIL - 1) k = 0;
      c.set(x, y, kWoodDark[k]);
    }
  post(c, 5, RAIL, TOP0, 2);
  post(c, 41, RAIL, TOP0, 2);
  hl(c, 4, 43, RAIL, kWoodDark[3]);
  hl(c, 4, 43, RAIL + 1, kWoodDark[0]);
}
void boothSign(Canvas& c, const Awn& A, int trade) {
  // the board, hung from two short chains under the eave
  const Ramp R = ramp(A.a, 0.8f);
  const int x0 = 17, x1 = 30, y0 = 15, y1 = 21;
  c.set(x0 + 2, y0 - 1, kIron[2]); c.set(x1 - 2, y0 - 1, kIron[2]);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const bool rim = y == y0 || y == y1 || x == x0 || x == x1;
      c.set(x, y, rim ? kWood[y == y0 ? 3 : 1] : R[y == y0 + 1 ? 3 : 2]);
    }
  // the trade's sign in the board's light colour: an 8x4 glyph
  static const uint8_t glyph[kStallTrades][4] = {
      {0x18, 0x3C, 0x3C, 0x18},   // produce: an apple
      {0x08, 0x7E, 0xFC, 0x48},   // fish
      {0x7E, 0x42, 0x7E, 0x42},   // cloth: a folded bolt
      {0x3C, 0x18, 0x3C, 0x3C},   // pottery: a jug
      {0x38, 0x7C, 0x7E, 0x0C},   // meat: a ham
      {0x3C, 0x7E, 0x7E, 0x00},   // bread: a loaf
      {0x7E, 0x18, 0x18, 0x18},   // tools: a hammer
      {0x66, 0xFF, 0xFF, 0x66},   // timber: logs
  };
  const uint32_t ink = A.striped ? lighten(A.b, 0.2f) : lighten(A.b, 0.4f);
  for (int r = 0; r < 4; r++)
    for (int b = 0; b < 8; b++)
      if (glyph[trade % kStallTrades][r] & (0x80 >> b)) c.set(x0 + 3 + b, y0 + 2 + r, ink);
}
void boothFront(Canvas& c, int awning) {
  // shingles (wood, or slate under the cooler cloths), lit from the top-left, rows overlapping toward the eave
  const bool slate = (awning % 4) == 1 || (awning % 4) == 2;
  const Ramp& S = slate ? kStone : kWoodDark;
  const int R0 = 1, E = 11;
  for (int y = R0; y <= E; y++) {
    const float t = (float)(y - R0) / (E - R0);
    const int xa = (int)std::lround(4 - 4 * t), xb = (int)std::lround(43 + 4 * t);
    const int row = (y - R0) / 3, inRow = (y - R0) % 3;
    for (int x = xa; x <= xb; x++) {
      const float u = (float)(x - xa) / std::max(1, xb - xa);
      int k = inRow == 0 ? 4 : (inRow == 1 ? 3 : 2);
      if (u > 0.6f) k--;
      if (((x + row * 3) % 6) == 0 && inRow) k = 1;   // the joints, staggered row by row
      if (hashf(x, y, 71u + (uint32_t)awning) < 0.08f) k = std::max(1, k - 1);
      c.set(x, y, S[std::clamp(k, 0, 4)]);
    }
  }
  hl(c, 4, 43, R0 - 1, kWood[2]);
  // the fascia board along the eave
  for (int x = 0; x < SW; x++) { c.set(x, E + 1, kWood[x < 3 ? 4 : 3]); c.set(x, E + 2, kWood[2]); c.set(x, E + 3, kWood[1]); }
  // thick corner posts with a brace up to the eave
  for (int x : {1, 44})
    for (int y = E + 1; y <= BASE; y++) { c.set(x, y, kWood[3]); c.set(x + 1, y, kWood[2]); c.set(x + 2, y, kWood[1]); }
  for (int k = 0; k < 4; k++) { c.set(4 + k, E + 4 + k, kWood[2]); c.set(43 - k, E + 4 + k, kWood[1]); }
}
void boothShutter(Canvas& c) {
  for (int y = RAIL; y <= TOP0 - 1; y++)
    for (int x = 4; x <= 43; x++) {
      const int pl = (y - RAIL) % 4;
      int k = pl == 0 ? 1 : (pl == 1 ? 3 : 2);
      if (x == 4) k = 3;
      c.set(x, y, kWood[k]);
    }
  box(c, 22, RAIL + 7, 25, RAIL + 9, kIron[2]);   // the hasp
  hl(c, 22, 25, RAIL + 7, kIron[4]);
}

void paintStall(Canvas& c, int trade, int awning, int form, bool closed) {
  const Awn A = awningOf(awning);
  const uint32_t seed = (uint32_t)(trade * 31 + awning * 7);
  const int t = trade % kStallTrades;
  switch (form) {
    case 1: {
      tentBack(c, A);
      Canvas g = tradeGoods(t, seed);
      if (!closed)   // what hangs under the roof
        for (int y = 0; y < TOP0 - 1; y++) for (int x = 0; x < SW; x++) if (chA(g.get(x, y))) c.set(x, y, g.get(x, y));
      tentCounter(c, A);
      if (!closed) {   // the counter's goods over the cloth
        for (int y = TOP0 - 12; y < SH; y++)
          for (int x = 0; x < SW; x++)
            if (chA(g.get(x, y)) && y >= TOP0 - 10) c.set(x, y, g.get(x, y));
      } else closedCover(c, A.a, true);
      tentFront(c, A);
      break;
    }
    case 2:
      boothBack(c);
      if (closed) { stallCounter(c, A, 6); boothShutter(c); closedCover(c, A.a, false); }
      else { stallCounter(c, A, t); blit(c, tradeGoods(t, seed), 0, 0); }
      boothFront(c, awning);
      boothSign(c, A, t);
      break;
    default:
      stallBack(c, A);
      stallCounter(c, A, t);
      if (!closed) blit(c, tradeGoods(t, seed), 0, 0);
      else closedCover(c, A.a, true);
      stallFront(c, A, awning);
      break;
  }
}

// ---------------------------------------------------------------- (M1 fixer round 2) open tables and ground cloths
// The table: the canvas's left 16 px are empty (the prop stands on its west tile, the table spans it and the next)
constexpr int TX0 = 17, TX1 = 46, TTOP0 = 29, TTOP1 = 32, TBASE = 42;
uint32_t shadeCloth(int shade) {
  static const uint32_t s[kTableShades] = {0, rgba(226, 214, 184), rgba(176, 62, 54), rgba(64, 120, 92)};
  return s[((shade % kTableShades) + kTableShades) % kTableShades];
}
void tableFrame(Canvas& c, bool back) {
  if (back) {   // the trestles' far legs
    for (int lx : {21, 42})
      for (int y = TTOP1; y <= TBASE - 1; y++) c.set(lx + (y - TTOP1) / 3, y, kWoodDark[1]);
    return;
  }
  // the top: boards running along, lit; its front edge
  for (int y = TTOP0; y <= TTOP1; y++)
    for (int x = TX0; x <= TX1; x++) {
      int k = y == TTOP0 ? 4 : 3;
      if ((x - TX0) % 10 == 9) k = 2;
      if (x == TX1) k = 1;
      c.set(x, y, kWood[k]);
    }
  for (int x = TX0; x <= TX1; x++) { c.set(x, TTOP1 + 1, kWood[2]); c.set(x, TTOP1 + 2, kWood[1]); }
  // the trestles: an X at each end, a stretcher between them
  for (int lx : {20, 41})
    for (int y = TTOP1 + 3; y <= TBASE; y++) {
      const int d = (y - TTOP1 - 3) / 2;
      c.set(lx - d + 2, y, kWood[2]);
      c.set(lx + d, y, kWood[3]);
    }
  hl(c, 23, 41, TBASE - 4, kWood[1]);
}
void tableShade(Canvas& c, int shade, bool closed) {
  if (!shade) return;
  for (int x : {18, 45}) { vl(c, x, 4, TTOP0 - 1, kWood[2]); c.set(x, 3, kWood[4]); }
  const Ramp R = ramp(shadeCloth(shade), 0.75f), T = ramp(shade == 1 ? rgba(176, 62, 54) : rgba(232, 222, 190), 0.7f);
  if (closed) {   // the cloth rolled up and tied to its poles
    for (int x = 18; x <= 45; x++) { c.set(x, 4, R[3]); c.set(x, 5, R[1]); }
    return;
  }
  for (int y = 2; y <= 9; y++) {
    const float t = (y - 2) / 7.0f;
    const int xa = (int)std::lround(18 - 2 * t), xb = (int)std::lround(45 + 2 * t);
    for (int x = xa; x <= xb; x++) {
      const float u = (float)(x - xa) / (xb - xa);
      float l = 0.9f - u * 0.5f - t * 0.25f;
      int k = l > 0.7f ? 4 : (l > 0.4f ? 3 : 2);
      const bool stripe = shade == 1 && ((x / 4) & 1);
      c.set(x, y, stripe ? T[k] : R[k]);
    }
  }
  for (int x = 16; x <= 47; x++) { c.set(x, 10, R[1]); if ((x & 3) != 3) c.set(x, 11, T[1]); }   // the fringe
}
// the table's goods, standing on its top
void tableGoods(Canvas& c, int goods) {
  const int y = TTOP0 + 2;   // the line the goods stand on
  switch (((goods % kTableGoods) + kTableGoods) % kTableGoods) {
    case 0: {   // cheeses: wheels stacked, a cut wedge on a board, a knife
      const Ramp ch = ramp(rgba(236, 196, 92), 0.7f), rind = ramp(rgba(196, 120, 52), 0.7f);
      for (int i = 0; i < 2; i++) { ball(c, 22.5f + i * 9, y - 2.5f, 4.4f, 2.6f, rind, 0.05f); ball(c, 22.5f + i * 9, y - 3.2f, 4.0f, 1.8f, ch, 0.05f); }
      ball(c, 27.0f, y - 6.0f, 4.0f, 2.4f, rind, 0.05f);
      ball(c, 27.0f, y - 6.6f, 3.6f, 1.6f, ch, 0.05f);
      hl(c, 34, 44, y, kWood[2]); hl(c, 34, 44, y - 1, kWood[3]);
      for (int k = 0; k < 4; k++) hl(c, 36, 36 + 6 - k, y - 2 - k, ch[k == 0 ? 2 : 3]);
      c.set(36, y - 2, ch[4]);
      hl(c, 41, 44, y - 2, kIron[3]); c.set(40, y - 2, kWood[2]);
      break;
    }
    case 1: {   // eggs in two baskets, a brace of fowl laid out
      const Ramp egg = ramp(rgba(236, 226, 206), 0.5f), brown = ramp(rgba(206, 156, 104), 0.6f);
      basketOf(c, 19, 10, y - 3, egg, 3, 1.3f);
      basketOf(c, 30, 9, y - 3, brown, 5, 1.3f);
      const Ramp fowl = ramp(rgba(236, 196, 170), 0.6f);
      ball(c, 42.0f, y - 2.0f, 3.2f, 2.0f, fowl, 0.05f);
      c.set(45, y - 2, fowl[1]); c.set(46, y - 3, fowl[1]); c.set(39, y - 1, rgba(236, 160, 60));
      break;
    }
    case 2: {   // spices in little open sacks, a brass scale
      static const uint32_t sp[5] = {rgba(200, 64, 40), rgba(236, 186, 40), rgba(176, 120, 60), rgba(110, 150, 70), rgba(70, 54, 50)};
      for (int i = 0; i < 4; i++) {
        const int x0 = 19 + i * 6;
        for (int j = 0; j < 4; j++) hl(c, x0, x0 + 4, y - j, kCloth[j == 3 ? 4 : (j == 0 ? 1 : 2)]);
        ball(c, x0 + 2.5f, y - 4.0f, 2.4f, 1.4f, ramp(sp[i], 0.6f), 0.05f);
      }
      vl(c, 43, y - 8, y, kBrass[2]); hl(c, 39, 47, y - 8, kBrass[3]);
      hl(c, 39, 41, y - 5, kBrass[2]); hl(c, 45, 47, y - 5, kBrass[2]);
      ball(c, 44.0f, y - 1.0f, 2.5f, 1.0f, ramp(sp[4], 0.5f), 0.05f);
      break;
    }
    case 3: {   // flowers in wooden buckets
      static const uint32_t fl[6] = {rgba(220, 60, 80), rgba(248, 210, 70), rgba(170, 90, 200), rgba(250, 240, 230), rgba(240, 130, 50), rgba(90, 120, 220)};
      for (int i = 0; i < 3; i++) {
        const int x0 = 19 + i * 7;
        for (int j = 0; j < 5; j++) hl(c, x0, x0 + 5, y - j, kWood[j == 4 ? 4 : (j == 2 ? 1 : 2)]);
        for (int k = 0; k < 9; k++) {
          const int fx = x0 + (k * 5) % 6, fy = y - 6 - (k * 3) % 4;
          vl(c, fx, fy, y - 5, kLeaf[2]);
          c.set(fx, fy, fl[(i * 2 + k) % 6]); c.set(fx + 1, fy, darken(fl[(i * 2 + k) % 6], 0.3f));
        }
      }
      for (int k = 0; k < 4; k++) { hl(c, 40, 46, y - k, k == 3 ? kLeaf[3] : kLeaf[2]); c.set(41 + k * 2 - (k > 2), y - 4, fl[k % 6]); }
      break;
    }
    case 4: {   // honey jars, bundles of candles, a cake of wax
      const Ramp honey = ramp(rgba(226, 150, 40), 0.8f);
      for (int i = 0; i < 4; i++) {
        const int x0 = 19 + i * 5;
        ball(c, x0 + 2.0f, y - 2.5f, 2.2f, 2.8f, honey, 0.05f);
        hl(c, x0, x0 + 4, y - 6, kCloth[3]);
        c.set(x0 + 1, y - 3, honey[4]);
      }
      for (int b = 0; b < 2; b++)
        for (int k = 0; k < 4; k++) vl(c, 40 + b * 3 + (k & 1), y - 7 + (k >> 1), y, kBone[k & 1 ? 3 : 4]);
      box(c, 39, y - 3, 46, y, rgba(232, 200, 110));
      hl(c, 39, 46, y - 3, rgba(250, 230, 160));
      break;
    }
    case 5: {   // baskets and wickerwork for sale, stacked
      const Ramp Wk = ramp(rgba(176, 128, 66), 0.75f);
      for (int i = 0; i < 3; i++) {
        const int x0 = 19 + i * 8, h = 4 + (i == 1 ? 3 : 0);
        for (int j = 0; j < h; j++) for (int k = 0; k < 7; k++) c.set(x0 + k, y - j, Wk[j == h - 1 ? 4 : ((k + j) % 2 ? 2 : 3)]);
        if (i == 1)
          for (int k = 0; k < 6; k++) c.set(x0 + k, y - h - (k > 0 && k < 5 ? 2 : 1), Wk[3]);   // a handle
      }
      for (int j = 0; j < 3; j++) for (int k = 0; k < 6; k++) c.set(41 + k, y - j, Wk[(k + j) % 2 ? 2 : 3]);
      for (int j = 0; j < 3; j++) for (int k = 0; k < 5; k++) c.set(41 + k, y - 4 - j, Wk[(k + j) % 2 ? 3 : 4]);
      break;
    }
    case 6: {   // wool: skeins in a heap, a fleece folded, a drop spindle
      static const uint32_t yc[5] = {rgba(190, 60, 60), rgba(60, 90, 170), rgba(226, 196, 90), rgba(230, 226, 214), rgba(80, 130, 80)};
      for (int i = 0; i < 6; i++) ball(c, 21.0f + (i % 3) * 5.5f + (i / 3) * 2.5f, y - 2.0f - (i / 3) * 3.0f, 2.8f, 1.8f, ramp(yc[i % 5], 0.6f), 0.06f);
      furBall(c, 39.0f, y - 2.5f, 5.0f, 2.8f, ramp(rgba(226, 218, 200), 0.5f), 0.4f, 9);
      vl(c, 46, y - 8, y, kWood[3]);
      ball(c, 46.0f, y - 2.0f, 1.5f, 1.0f, kWood, 0.0f);
      break;
    }
    default: {   // apples by the basket, a cask of cider
      const Ramp ap = ramp(rgba(200, 52, 44), 0.8f), gr = ramp(rgba(150, 190, 70), 0.8f);
      basketOf(c, 19, 9, y - 3, ap, 7, 1.7f);
      basketOf(c, 29, 8, y - 3, gr, 9, 1.7f);
      for (int x = 38; x <= 45; x++)
        for (int yy = y - 6; yy <= y; yy++) c.set(x, yy, kWood[(x == 38) ? 3 : (x == 45 ? 1 : 2)]);
      hl(c, 38, 45, y - 5, kIron[2]); hl(c, 38, 45, y - 1, kIron[2]);
      hl(c, 38, 45, y - 6, kWood[4]);
      c.set(41, y - 3, kWoodDark[0]); c.set(42, y - 3, kWoodDark[1]);
      break;
    }
  }
}

// ---- the ground cloth (48x20): the cloth spread on the paving before its seller, goods laid out on it
constexpr int GX0 = 17, GX1 = 46, GTOP = 10, GBOT = 18;
uint32_t groundClothCol(int cloth) {
  static const uint32_t s[4] = {rgba(170, 56, 50), rgba(58, 74, 140), rgba(200, 150, 60), rgba(70, 120, 84)};
  return s[((cloth % 4) + 4) % 4];
}
void clothSpread(Canvas& c, int cloth) {
  const Ramp R = ramp(groundClothCol(cloth), 0.7f), T = ramp(rgba(232, 214, 170), 0.6f);
  for (int y = GTOP; y <= GBOT; y++) {
    const float t = (y - GTOP) / (float)(GBOT - GTOP);
    const int xa = (int)std::lround(GX0 + 2 - 2 * t), xb = (int)std::lround(GX1 - 2 + 2 * t);
    for (int x = xa; x <= xb; x++) {
      const bool border = y == GTOP + 1 || y == GBOT - 1 || x == xa + 1 || x == xb - 1;
      int k = (x + y) % 5 == 0 ? 2 : 3;
      if (y == GBOT) k = 1;
      c.set(x, y, border && y != GBOT ? T[2] : R[k]);
    }
  }
}
void clothGoods(Canvas& c, int goods) {
  const int y = GBOT - 3;
  switch (((goods % kClothGoods) + kClothGoods) % kClothGoods) {
    case 0: {   // pots and jugs
      const Ramp terra = ramp(rgba(190, 104, 62), 0.8f), dark = ramp(rgba(120, 76, 60), 0.7f), glaze = ramp(rgba(70, 120, 170), 0.8f);
      const Ramp* r[4] = {&terra, &glaze, &dark, &terra};
      for (int i = 0; i < 4; i++) {
        const float x = 21.0f + i * 6.0f, rr = i % 2 ? 2.6f : 3.2f;
        ball(c, x, y - rr + 1, rr, rr + 0.6f, *r[i], 0.05f);
        hl(c, (int)x - 1, (int)x + 1, (int)(y - 2 * rr), (*r[i])[3]);
      }
      for (int b = 0; b < 3; b++) hl(c, 42 - b, 44 + b, y - 1 - b * 2 + 2, terra[2 + (b == 2)]);
      break;
    }
    case 1: {   // rolled rugs and a folded stack
      static const uint32_t rc[3] = {rgba(160, 50, 50), rgba(60, 80, 150), rgba(200, 150, 60)};
      for (int i = 0; i < 3; i++) {
        const Ramp R = ramp(rc[i], 0.7f);
        const int yy = y + 1 - i * 2;
        for (int x = 20; x <= 34; x++) {
          c.set(x, yy - 1, R[3]); c.set(x, yy, R[2]); c.set(x, yy + 1, R[1]);
          if ((x + i) % 4 == 0) c.set(x, yy, R[4]);
        }
        c.set(35, yy, R[4]); c.set(35, yy - 1, R[3]);
      }
      for (int j = 0; j < 4; j++) hl(c, 38, 45, y + 2 - j * 2, ramp(rc[j % 3], 0.6f)[j == 3 ? 4 : 2]);
      break;
    }
    case 2: {   // gourds, pumpkins, turnips
      const Ramp pump = ramp(rgba(226, 124, 40), 0.8f), turn = ramp(rgba(226, 214, 220), 0.6f), cab = ramp(rgba(110, 166, 70), 0.8f);
      ball(c, 23.0f, y - 1.0f, 4.0f, 3.0f, pump, 0.05f); c.set(23, y - 4, kLeaf[2]);
      ball(c, 31.0f, y + 0.0f, 3.2f, 2.6f, pump, 0.05f); c.set(31, y - 3, kLeaf[2]);
      for (int i = 0; i < 3; i++) { ball(c, 37.0f + i * 3.0f, y + 1.0f, 1.6f, 1.6f, turn, 0.05f); c.set(37 + i * 3, y - 1, rgba(150, 60, 140)); }
      ball(c, 44.0f, y + 0.0f, 2.6f, 2.4f, cab, 0.05f);
      break;
    }
    default: {   // furs and pelts laid flat
      const Ramp fur = ramp(rgba(140, 96, 62), 0.7f), grey = ramp(rgba(150, 150, 150), 0.6f), fox = ramp(rgba(214, 112, 52), 0.7f);
      furBall(c, 25.0f, y + 0.5f, 6.0f, 2.4f, fur, 0.6f, 3);
      furBall(c, 36.0f, y + 0.5f, 5.0f, 2.2f, grey, 0.6f, 5);
      furBall(c, 43.0f, y + 0.0f, 3.0f, 1.8f, fox, 0.5f, 7);
      c.set(46, y + 1, kWhite);
      break;
    }
  }
}

// ---- the market cross: a stepped octagonal base, a shaft, a carved head
void marketCross(Canvas& c) {
  const int cx = c.w / 2, b = c.h - 2;
  const Ramp& S = kStoneWarm;
  struct Step { int rx, h; };
  const Step st[3] = {{14, 4}, {10, 4}, {7, 3}};
  int top = b;
  for (const Step& s : st) {
    const int ry = std::max(2, s.rx / 3);
    // the step's front face (lit on the left), then its top (an ellipse)
    for (int y = top - s.h; y <= top; y++)
      for (int x = cx - s.rx; x <= cx + s.rx; x++) {
        const float u = (x - cx) / (float)s.rx;
        int k = u < -0.4f ? 3 : (u < 0.3f ? 2 : 1);
        if (y == top) k = 0;
        if (((x - cx + 40) % 5) == 0 && y > top - s.h) k = std::max(0, k - 1);   // the faces of the octagon
        c.set(x, y, S[k]);
      }
    ellipse(c, cx + 0.5, top - s.h, s.rx + 0.5, ry, S[3]);
    ellipse(c, cx - 1.5, top - s.h - 0.5, s.rx * 0.6, ry * 0.5, S[4]);
    top -= s.h + ry - 1;
  }
  // the shaft, a moulded knop halfway
  const int knop = (top + 12) / 2;
  for (int y = 12; y <= top; y++) {
    const int hw = y > knop ? 3 : 2;
    for (int x = cx - hw; x <= cx + hw - 1; x++) {
      const float u = (x - cx + 0.5f) / hw;
      c.set(x, y, S[u < -0.3f ? 4 : (u < 0.4f ? 3 : 1)]);
    }
  }
  hl(c, cx - 4, cx + 3, knop, S[4]);
  hl(c, cx - 4, cx + 3, knop + 1, S[1]);
  // the head: a cross with flared arms
  for (int x = cx - 7; x <= cx + 6; x++) { c.set(x, 8, S[4]); c.set(x, 9, S[3]); c.set(x, 10, S[1]); }
  for (int y = 2; y <= 12; y++) { c.set(cx - 1, y, S[4]); c.set(cx, y, S[2]); }
  c.set(cx - 8, 9, S[3]); c.set(cx + 7, 9, S[1]); c.set(cx - 1, 1, S[4]); c.set(cx, 1, S[3]);
  // moss in the steps' corners
  for (int i = 0; i < 6; i++) c.set(cx - 12 + i * 5, b - 1 - (i % 2), kMoss[2 + (i % 2)]);
}

// ---- livestock: a sheep and a cow, grazing (frames 0-4 head down, 5-7 head up)
void sheep(Canvas& c, int frame) {
  const bool up = frame >= 5;
  const Ramp wool = ramp5(rgba(150, 140, 136), rgba(196, 188, 176), rgba(226, 220, 204), rgba(242, 238, 224), rgba(252, 250, 242));
  const uint32_t face = rgba(52, 44, 50), leg = rgba(60, 50, 54);
  const int b = c.h - 1;
  for (int lx : {6, 8, 13, 15}) vl(c, lx, b - 4, b, leg);
  furBall(c, 11.0f, b - 7.5f, 7.0f, 4.6f, wool, 0.55f, 17);
  // the head
  const int hx = 2, hy = up ? b - 11 : b - 6;
  ball(c, 5.5f, up ? b - 8.5f : b - 6.5f, 2.4f, 2.2f, wool, 0.05f);   // the wool on its neck
  for (int y = 0; y < 4; y++) for (int x = 0; x < 3; x++) c.set(hx + x, hy + y, face);
  c.set(hx, hy + 1, rgba(84, 74, 78));
  c.set(hx + 3, hy, face); c.set(hx + 3, hy - 1, wool[1]);   // the ear
  c.set(hx + 1, hy + 1, kWhite);
  if (!up) { c.set(hx, hy + 4, kLeaf[3]); c.set(hx + 1, hy + 4, kLeaf[2]); }   // a mouthful of grass
  c.set(18, b - 8 + (frame == 6 ? 1 : 0), wool[2]);   // the tail
}
void cow(Canvas& c, int frame) {
  const bool up = frame >= 5;
  const Ramp hide = ramp5(rgba(70, 40, 34), rgba(108, 62, 44), rgba(146, 88, 58), rgba(178, 116, 74), rgba(204, 146, 98));
  const int b = c.h - 1;
  for (int lx : {8, 10, 20, 22}) { vl(c, lx, b - 6, b, hide[1]); c.set(lx, b, kInk); }
  ball(c, 15.5f, b - 10.0f, 10.0f, 5.6f, hide, 0.08f);
  // a white blaze on its flank, a pale belly
  const Ramp pale = ramp(rgba(232, 226, 212), 0.45f);
  for (int x = 11; x <= 19; x++) c.set(x, b - 5, pale[2]);
  for (int y = b - 14; y <= b - 8; y++)   // a pale patch over the flank
    for (int x = 13; x <= 21; x++) {
      const float dx = (x + 0.5f - 17.0f) / 4.2f, dy = (y + 0.5f - (b - 11.0f)) / 3.0f;
      if (dx * dx + dy * dy + (hashf(x, y, 13) - 0.5f) * 0.5f < 1.0f) c.set(x, y, pale[y < b - 11 ? 3 : 2]);
    }
  ball(c, 17.0f, b - 4.5f, 1.6f, 1.0f, ramp(rgba(230, 160, 160), 0.5f), 0.0f);   // the udder
  // the head
  const int hx = 1, hy = up ? b - 16 : b - 9;
  box(c, hx, hy, hx + 5, hy + 5, hide[2]);
  box(c, hx, hy + 4, hx + 3, hy + 6, rgba(214, 170, 150));   // the muzzle
  hl(c, hx + 1, hx + 5, hy, hide[3]);
  c.set(hx + 2, hy + 2, kInk);
  c.set(hx + 5, hy - 1, kBone[3]); c.set(hx + 6, hy - 2, kBone[4]); c.set(hx, hy - 1, kBone[3]);   // the horns
  c.set(hx + 6, hy + 1, hide[1]);   // the ear
  if (!up) { c.set(hx + 1, hy + 7, kLeaf[3]); c.set(hx + 2, hy + 7, kLeaf[2]); }
  // the tail, swishing
  const int sw = frame == 6 ? 1 : (frame == 7 ? -1 : 0);
  vl(c, 26 + sw, b - 12, b - 5, hide[1]);
  c.set(26 + sw, b - 4, kInk); c.set(27 + sw, b - 4, kInk);
}

// ---- a lean-to for the flock: plank back wall, a sloping roof on front posts, a hay rack and hay
void penShelter(Canvas& c) {
  const int b = c.h - 2;
  // the back wall in the shade
  for (int y = 12; y <= b - 2; y++)
    for (int x = 3; x <= 44; x++) c.set(x, y, kWoodDark[(x - 3) % 5 == 0 ? 0 : (y < 18 ? 0 : 1)]);
  // the rack on the wall, hay in it
  for (int x = 8; x <= 38; x += 3)
    for (int k = 0; k < 8; k++) c.set(x + k / 3, 18 + k, kWood[1]);
  for (int x = 8; x <= 40; x++)
    for (int y = 17; y <= 20; y++) if (hashf(x, y, 5) < 0.7f) c.set(x, y, kThatch[2 + (y == 17)]);
  // hay heaped on the floor, a pail
  ball(c, 33.0f, b - 3.5f, 9.0f, 4.0f, kThatch, 0.12f);
  ball(c, 12.0f, b - 2.0f, 5.0f, 2.2f, kThatch, 0.12f);
  for (int y = b - 5; y <= b - 1; y++) hl(c, 20, 24, y, kWood[y == b - 5 ? 4 : 2]);
  hl(c, 20, 24, b - 3, kIron[2]);
  // the roof: thatch sloping from the back down to the eave, overhanging
  for (int y = 2; y <= 12; y++) {
    const float t = (y - 2) / 10.0f;
    const int xa = (int)std::lround(2 - 2 * t), xb = (int)std::lround(45 + 2 * t);
    for (int x = xa; x <= xb; x++) {
      int k = ((x * 3 + y) % 7 == 0) ? 1 : (t < 0.3f ? 4 : (t < 0.7f ? 3 : 2));
      if (x > xb - 10) k = std::max(1, k - 1);
      c.set(x, y, kThatch[k]);
    }
  }
  hl(c, 0, 47, 13, kThatch[1]);
  for (int x = 0; x < 48; x += 3) c.set(x, 14, kThatch[1]);
  // the posts
  for (int x : {2, 23, 44})
    for (int y = 13; y <= b; y++) { c.set(x, y, kWood[3]); c.set(x + 1, y, kWood[1]); }
}

// ---- (M1 fixer round 2) the mine hill: a grassy knoll whose south face is a cliff of layered rock, the timbered adit
// cut into its foot, the rails running out of it, boulders and scree at its shoulders. 80x76, standing on the adit's
// tile; the generator fills the hill's footprint (five tiles across, four deep) with Filler. variant 0..3: its shape,
// bushes and stones; land 0 green, 1 snow, 2 dry grass
void paintMineHill(Canvas& c, int variant, int land) {
  const int W = c.w, b = c.h - 1, cx = W / 2;
  const uint32_t s = 911u + (uint32_t)variant * 37u;
  const Ramp top = land == 1 ? kSnow
                 : (land == 2 ? ramp5(rgba(110, 92, 52), rgba(150, 126, 66), rgba(186, 160, 84), rgba(212, 188, 108), rgba(234, 216, 142))
                              : ramp5(rgba(38, 70, 48), rgba(52, 98, 52), rgba(72, 128, 58), rgba(98, 152, 66), rgba(132, 178, 80)));
  const Ramp& R = kStone;
  // the outline: a mound four tiles high in the middle, sloping to the ground at its shoulders
  int topY[96], brow[96];
  for (int x = 0; x < W; x++) {
    const float u = (x + 0.5f - cx) / (W * 0.5f);
    const float h = 60.0f * std::pow(std::max(0.0f, 1.0f - u * u), 0.42f) + (vnoise(x * 0.16f, 3.0f, s) - 0.5f) * 8.0f;
    topY[x] = b - (int)h;
    // the brow: where the grassy top breaks into the cliff (only across the middle; the shoulders stay grass)
    const float f = std::fabs(u);
    brow[x] = f < 0.8f ? b - 36 - (int)((vnoise(x * 0.3f, 9.0f, s + 3) - 0.5f) * 8.0f) + (int)(f * f * f * 40.0f) : b + 1;
    if (brow[x] < topY[x] + 6) brow[x] = topY[x] + 6;
  }
  // the grassy top and shoulders, lit from the top-left
  for (int x = 0; x < W; x++)
    for (int y = std::max(0, topY[x]); y <= b; y++) {
      if (y >= brow[x]) break;
      const float u = (x + 0.5f - cx) / (W * 0.5f);
      const float t = (float)(y - topY[x]) / std::max(1, b - topY[x]);
      float l = 0.75f - u * 0.45f - t * 0.55f + (hashf(x / 2, y / 2, s) - 0.5f) * 0.35f;
      if (y == topY[x]) l += 0.35f;
      int k = l > 0.8f ? 4 : (l > 0.45f ? 3 : (l > 0.1f ? 2 : 1));
      if (std::fabs(u) > 0.66f && y > b - 5) k = std::max(1, k - 1);   // the shoulders' foot in shade
      c.set(x, y, top[k]);
    }
  // tufts of darker grass on the top
  if (land != 1)
    for (int i = 0; i < 26; i++) {
      const int x = 6 + (int)(hashf(i, 1, s) * (W - 12)), y = topY[x] + 2 + (int)(hashf(i, 2, s) * std::max(1, brow[std::min(W - 1, x)] - topY[x] - 4));
      if (y < brow[x] - 1 && y > topY[x]) { c.set(x, y, top[1]); c.set(x + 1, y - 1, top[2]); }
    }
  // the cliff: courses of rock, lit on the left, cracks and ledges, darker toward its foot
  for (int x = 0; x < W; x++)
    for (int y = brow[x]; y <= b; y++) {
      const float u = (x + 0.5f - cx) / (W * 0.5f);
      const int course = (y - brow[x] + (int)(vnoise(x * 0.2f, 1.0f, s + 5) * 3.0f)) % 6;
      float l = 0.55f - u * 0.6f - (float)(y - brow[x]) / 60.0f;
      if (course == 0) l += 0.35f;          // the lit top of each course
      if (course == 5) l -= 0.35f;          // the shadow under it
      if (hash3(x / 3, (y + x / 5) / 4, s) % 9 == 0) l -= 0.4f;   // a crack
      int k = lightIndex(l, x, y, 0.15f);
      if (y == brow[x]) k = 4;
      c.set(x, y, R[k]);
    }
  // moss and grass hanging over the brow
  for (int x = 0; x < W; x++)
    if (brow[x] <= b && hashf(x, 7, s) < 0.55f) {
      const int n = 1 + (int)(hashf(x, 8, s) * 3.0f);
      for (int k = 0; k < n; k++) c.set(x, brow[x] + k, land == 1 ? kSnow[3] : top[k == 0 ? 2 : 1]);
    }
  // crags of the rock breaking through the turf on the top
  for (int i = 0; i < 2 + (variant >> 1); i++) {
    const int x = 16 + (int)(hashf(i, 41, s) * (W - 32));
    const int y = topY[x] + 8 + (int)(hashf(i, 42, s) * 8.0f);
    if (y + 4 < brow[x]) rock(c, (float)x, (float)y, 5.0f + hashf(i, 43, s) * 3.0f, 3.5f, R, s + (uint32_t)i, 6);
  }
  // a bush or two and a few stones on the top
  for (int i = 0; i < 2 + (variant & 1); i++) {
    const int x = 14 + (int)(hashf(i, 11, s) * (W - 28));
    const int y = topY[x] + 6 + (int)(hashf(i, 12, s) * 6.0f);
    if (y + 3 < brow[x]) ball(c, x, y, 4.0 + hashf(i, 13, s) * 2.0, 3.0, land == 1 ? kPine : kLeafDark, 0.12f);
  }
  for (int i = 0; i < 4; i++) {
    const int x = 10 + (int)(hashf(i, 21, s) * (W - 20));
    const int y = topY[x] + 4 + (int)(hashf(i, 22, s) * 10.0f);
    if (y + 2 < brow[x]) ball(c, x, y, 2.0, 1.5, R, 0.1f);
  }
  // boulders and scree at the foot of the cliff, either side of the adit
  for (int sd : {-1, 1}) {
    const float bx = cx + sd * (18.0f + hashf(sd + 2, 31, s) * 6.0f);
    ball(c, bx, b - 4.0, 6.0, 4.5, R, 0.12f);
    ball(c, bx + sd * 7.0f, b - 2.0, 3.5, 2.5, R, 0.12f);
    for (int i = 0; i < 6; i++) c.set((int)bx + sd * (2 + i * 2), b - (i & 1), R[1 + (i % 3)]);
  }
  // the adit: a dark mouth going back into the rock, framed by two props and a lintel, the rails running out of it
  const int ax0 = cx - 7, ax1 = cx + 7, atop = b - 19;
  for (int y = atop; y <= b; y++)
    for (int x = ax0; x <= ax1; x++) {
      const int depth = y - atop;
      c.set(x, y, depth < 2 ? rgba(52, 42, 54) : (depth < 5 && (x < ax0 + 3 || x > ax1 - 3) ? rgba(40, 32, 44) : kInk));
    }
  for (int y = atop - 2; y <= b; y++) {
    c.set(ax0 - 2, y, kWood[3]); c.set(ax0 - 1, y, kWood[2]); c.set(ax1 + 1, y, kWood[2]); c.set(ax1 + 2, y, kWood[0]);
    if (y % 5 == 0) { c.set(ax0 - 2, y, kWood[2]); c.set(ax1 + 2, y, kWood[1]); }
  }
  for (int x = ax0 - 4; x <= ax1 + 4; x++) { c.set(x, atop - 4, kWood[4]); c.set(x, atop - 3, kWood[3]); c.set(x, atop - 2, kWood[1]); }
  for (int y = b - 8; y <= b; y++) { c.set(cx - 3 - (b - y) / 4, y, kIron[3]); c.set(cx + 3 + (b - y) / 4, y, kIron[2]); }
  for (int y = b - 7; y <= b; y += 2) hl(c, cx - 4 - (b - y) / 4, cx + 4 + (b - y) / 4, y, kWood[1]);
  // the lantern hung from the lintel
  c.set(cx + 5, atop - 1, kIron[2]); c.set(cx + 5, atop, kGlow[4]); c.set(cx + 5, atop + 1, kGlow[3]);
  c.set(cx + 4, atop, kIron[1]); c.set(cx + 6, atop, kIron[1]);
}

// ---- a tile of mine track: two iron rails on wooden sleepers
void railTile(Canvas& c, int joins) {
  const bool n = joins & 1, e = joins & 2, s = joins & 4, w = joins & 8;
  const int cnt = n + e + s + w;
  const bool corner = cnt == 2 && !(n && s) && !(e && w);
  if (corner) {
    // a curve round the corner: two sleepers fanned across it, the rails sweeping over them (a lit top, a dark side)
    const float ox = e ? 16.0f : 0.0f, oy = s ? 16.0f : 0.0f, sx = e ? -1.0f : 1.0f, sy = s ? -1.0f : 1.0f;
    for (float ang : {0.26f, 0.79f, 1.31f})
      for (float r = 3.0f; r <= 13.0f; r += 0.5f) {
        const int x = (int)std::floor(ox + sx * std::cos(ang) * r), y = (int)std::floor(oy + sy * std::sin(ang) * r);
        if (x >= 0 && x < 16 && y >= 0 && y < 16) { c.set(x, y, kWoodDark[2]); if (y + 1 < 16) c.set(x, y + 1, kWoodDark[1]); }
      }
    for (int a = 0; a <= 64; a++) {
      const float ang = a * (1.5708f / 64.0f);
      const float cs = std::cos(ang), sn = std::sin(ang);
      for (int k = 0; k < 2; k++) {
        const float r = k ? 10.0f : 5.0f;
        const int x = (int)std::floor(ox + sx * cs * r), y = (int)std::floor(oy + sy * sn * r);
        const int x2 = (int)std::floor(ox + sx * cs * (r + 1.0f)), y2 = (int)std::floor(oy + sy * sn * (r + 1.0f));
        if (x2 >= 0 && x2 < 16 && y2 >= 0 && y2 < 16) c.set(x2, y2, kIron[1]);
        if (x >= 0 && x < 16 && y >= 0 && y < 16) c.set(x, y, kIron[3]);
      }
    }
    return;
  }
  const bool any = cnt > 0;
  const bool vert = !any || n || s, horiz = e || w;
  if (vert) {
    const int y0 = (!any || n) ? 0 : 5, y1 = (!any || s) ? 15 : 10;
    for (int y = y0 + 1; y <= y1; y += 4) { hl(c, 3, 12, y, kWoodDark[2]); hl(c, 3, 12, y + 1 > y1 ? y : y + 1, kWoodDark[1]); }
    for (int y = y0; y <= y1; y++) { c.set(5, y, kIron[3]); c.set(10, y, kIron[3]); c.set(6, y, kIron[1]); c.set(11, y, kIron[1]); }
  }
  if (horiz) {
    const int x0 = w ? 0 : 5, x1 = e ? 15 : 10;
    for (int x = x0 + 1; x <= x1; x += 4) { vl(c, x, 3, 12, kWoodDark[2]); vl(c, x + 1 > x1 ? x : x + 1, 3, 12, kWoodDark[1]); }
    for (int x = x0; x <= x1; x++) { c.set(x, 5, kIron[4]); c.set(x, 10, kIron[4]); c.set(x, 6, kIron[1]); c.set(x, 11, kIron[1]); }
  }
}

// ---------------------------------------------------------------- yard props
void sacks(Canvas& c) {
  const Ramp S = ramp(rgba(206, 184, 140), 0.7f);
  const int b = c.h - 2;
  ball(c, 6.5, b - 4.0, 4.6, 4.0, S, 0.06f);
  ball(c, 13.5, b - 4.0, 4.6, 4.0, S, 0.06f);
  ball(c, 10.0, b - 8.5, 4.4, 3.8, S, 0.06f);
  for (float x : {6.5f, 13.5f}) { hl(c, (int)x - 1, (int)x + 1, b - 8, S[1]); c.set((int)x, b - 9, S[3]); }
  hl(c, 9, 11, b - 12, S[1]);
  c.set(5, b - 5, rgba(150, 110, 70)); c.set(12, b - 4, rgba(150, 110, 70));   // the stencil marks
}

void baskets(Canvas& c) {
  basketOf(c, 1, 9, c.h - 6, ramp(rgba(208, 48, 44), 0.8f), 11);
  basketOf(c, 10, 9, c.h - 5, ramp(rgba(112, 170, 70), 0.8f), 23, 2.0f);
}

void dryingRack(Canvas& c) {
  const int b = c.h - 2;
  for (int x : {2, c.w - 4}) { vl(c, x, 6, b, kWood[3]); vl(c, x + 1, 6, b, kWood[1]); }
  hl(c, 1, c.w - 2, 6, kWood[3]);
  hl(c, 1, c.w - 2, 7, kWood[1]);
  hl(c, 3, c.w - 4, 17, kWood[2]);
  const Ramp dried = ramp(rgba(184, 150, 100), 0.75f), silver = ramp(rgba(150, 170, 186), 0.75f);
  for (int r = 0; r < 2; r++)
    for (int s = 0; s < 6; s++) {
      const int x = 6 + s * 4, y0 = r == 0 ? 8 : 18;
      const Ramp& R = (s + r) % 3 == 0 ? silver : dried;
      for (int j = 0; j < 7; j++) {
        const int w = j < 1 ? 0 : (j < 5 ? 1 : 0);
        for (int i = -w; i <= w; i++) c.set(x + i, y0 + j, R[i < 0 ? 3 : (i == 0 ? 2 : 1)]);
      }
    }
}

void hideRack(Canvas& c) {
  const int b = c.h - 2;
  // the frame: two posts and two bars, the hide laced into it with thongs
  for (int x : {2, c.w - 4}) { vl(c, x, 3, b, kWood[3]); vl(c, x + 1, 3, b, kWood[1]); }
  hl(c, 1, c.w - 2, 3, kWood[3]); hl(c, 1, c.w - 2, 4, kWood[1]);
  hl(c, 2, c.w - 3, b - 5, kWood[2]);
  const Ramp H = ramp(rgba(184, 136, 92), 0.7f);
  for (int y = 7; y <= b - 8; y++) {
    const float t = (y - 7) / (float)(b - 15);
    const float hw = 6.5f + std::sin(t * 3.14159f) * 2.5f + (y % 4 == 0 ? -1 : 0);
    for (int x = (int)(c.w / 2 - hw); x <= (int)(c.w / 2 + hw); x++) {
      const float u = (x - (c.w / 2 - hw)) / (2 * hw);
      int k = u < 0.3f ? 4 : (u < 0.75f ? 3 : 2);
      if ((x * 3 + y * 5) % 13 == 0) k--;
      c.set(x, y, H[k]);
    }
  }
  for (int y = 8; y <= b - 9; y += 3) { c.set(4, y, kLeather[1]); c.set(5, y, kLeather[2]); c.set(c.w - 6, y, kLeather[1]); c.set(c.w - 5, y, kLeather[2]); }
}

void oreCart(Canvas& c) {
  const int b = c.h - 2;
  // the rail stub
  hl(c, 0, c.w - 1, b - 1, kIron[2]);
  hl(c, 0, c.w - 1, b, kWoodDark[1]);
  for (int x = 2; x < c.w; x += 6) vl(c, x, b - 1, b, kWood[1]);
  // the tub: iron-banded planks, wider at the top
  for (int y = 7; y <= b - 5; y++) {
    const int in = (y - 7) / 4;
    for (int x = 3 + in; x <= c.w - 4 - in; x++) {
      int k = x == 3 + in ? 3 : (x >= c.w - 5 - in ? 1 : 2);
      if (y == 9 || y == b - 7) k = 1;
      c.set(x, y, (y == 9 || y == b - 7) ? kIron[k + 1] : kWood[k]);
    }
  }
  // the heap of ore
  const Ramp ore = ramp(rgba(110, 96, 104), 0.8f);
  for (int i = 0; i < 6; i++) ball(c, 6.0f + i * 2.8f, 6.0f - (i % 3 == 1 ? 1.5f : 0.0f), 2.4f, 2.0f, ore, 0.08f);
  c.set(9, 4, rgba(220, 150, 80)); c.set(15, 5, rgba(220, 150, 80)); c.set(12, 6, rgba(196, 210, 222));   // a glint of metal
  // the wheels
  for (int wx : {7, c.w - 8}) { ball(c, wx, b - 3.5, 2.6, 2.6, kIron, 0.05f); c.set(wx, b - 4, kIron[4]); }
}

void orePile(Canvas& c) {
  const Ramp ore = ramp(rgba(112, 98, 106), 0.85f);
  const int b = c.h - 2;
  for (int i = 0; i < 9; i++) {
    const float x = 4.0f + (i % 5) * 3.4f + (i / 5) * 1.6f, y = b - 2.5f - (i / 5) * 3.5f;
    ball(c, x, y, 2.8f, 2.2f, ore, 0.08f);
  }
  c.set(8, b - 6, rgba(222, 150, 80)); c.set(13, b - 3, rgba(222, 150, 80)); c.set(16, b - 5, rgba(196, 210, 222));
}

void mineEntrance(Canvas& c) {
  const int b = c.h - 2, cx = c.w / 2;
  // the outcrop: a knot of weathered boulders (back ones first), each lit from the top-left, moss on their crowns
  struct Lump { float x, y, rx, ry; };
  const Lump lumps[] = {{11, 15, 11, 10}, {31, 13, 12, 11}, {21, 9, 10, 8}, {5, 26, 6, 8}, {39, 26, 6, 8}, {14, 25, 10, 9}, {30, 25, 10, 9}};
  for (const Lump& L : lumps)
    for (int y = (int)(L.y - L.ry - 1); y <= std::min(b, (int)(L.y + L.ry + 1)); y++)
      for (int x = (int)(L.x - L.rx - 1); x <= (int)(L.x + L.rx + 1); x++) {
        const float dx = (x + 0.5f - L.x) / L.rx, dy = (y + 0.5f - L.y) / L.ry;
        const float d = dx * dx + dy * dy;
        if (d > 1.0f + (hashf(x, y, 47) - 0.5f) * 0.12f) continue;
        float l = lightAt(dx * 0.85f, dy * 0.85f) + (vnoise(x * 0.45f, y * 0.45f, 43) - 0.5f) * 0.5f;
        int k = lightIndex(l, x, y, 0.1f);
        if (d > 0.82f && dy > 0.1f) k = std::max(0, k - 1);   // the shaded underside of each boulder
        uint32_t col = kStone[k];
        if (dy < -0.45f && hashf(x / 2, y / 2, 53) < 0.7f) col = kMoss[std::min(4, k + 1)];   // moss and turf on top
        c.set(x, y, col);
      }
  for (int y = 0; y < c.h; y++)   // cracks between the stones
    for (int x = 1; x < c.w - 1; x++)
      if (solid(c, x, y) && hash3(x, y, 59) % 23 == 0 && c.get(x, y) != kMoss[3]) c.set(x, y, kStone[0]);
  // the adit: a dark mouth going back into the rock, framed by two props and a lintel, a lantern by it
  for (int y = 15; y <= b; y++)
    for (int x = cx - 7; x <= cx + 7; x++) {
      const int depth = y - 15;
      c.set(x, y, depth < 2 ? rgba(52, 42, 54) : (depth < 5 && (x < cx - 4 || x > cx + 4) ? rgba(40, 32, 44) : kInk));
    }
  for (int y = 13; y <= b; y++) {
    c.set(cx - 9, y, kWood[3]); c.set(cx - 8, y, kWood[2]); c.set(cx + 8, y, kWood[2]); c.set(cx + 9, y, kWood[0]);
    if (y % 5 == 0) { c.set(cx - 9, y, kWood[2]); c.set(cx + 9, y, kWood[1]); }
  }
  for (int x = cx - 11; x <= cx + 11; x++) { c.set(x, 11, kWood[4]); c.set(x, 12, kWood[3]); c.set(x, 13, kWood[1]); }
  c.set(cx - 11, 12, kWood[2]); c.set(cx + 11, 12, kWood[1]);
  // the rails running in under the frame, sleepers across
  for (int y = b - 7; y <= b; y++) { c.set(cx - 3 - (b - y) / 4, y, kIron[3]); c.set(cx + 3 + (b - y) / 4, y, kIron[2]); }
  for (int y = b - 6; y <= b; y += 2) hl(c, cx - 4 - (b - y) / 4, cx + 4 + (b - y) / 4, y, kWood[1]);
  // the lantern hung from the lintel
  c.set(cx + 5, 14, kIron[2]); c.set(cx + 5, 15, kGlow[4]); c.set(cx + 5, 16, kGlow[3]); c.set(cx + 4, 15, kIron[1]); c.set(cx + 6, 15, kIron[1]);
}

void logPile(Canvas& c) {
  const int b = c.h - 2;
  // trunks stacked four, three, two, lying away from the viewer: their bark running back behind, their sawn ends
  // toward us with the rings showing
  struct L { float x, y; };
  std::vector<L> logs;
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < 4 - r; i++) logs.push_back({6.0f + r * 3.5f + i * 7.0f, b - 3.5f - r * 5.4f});
  for (const L& l : logs)   // the bark behind each end (the log's top running back)
    for (int y = (int)l.y - 6; y <= (int)l.y; y++)
      for (int x = (int)l.x - 3; x <= (int)l.x + 3; x++) c.set(x, y, kBark[x <= (int)l.x - 2 ? 3 : (x >= (int)l.x + 2 ? 1 : 2)]);
  for (const L& l : logs) {
    ball(c, l.x, l.y, 3.3f, 3.1f, ramp(rgba(214, 170, 110), 0.6f), 0.02f);
    for (int a = 0; a < 14; a++) {   // the bark ring round the end, a growth ring inside
      const float ang = a * 0.4488f;
      c.set((int)std::floor(l.x + std::cos(ang) * 3.2f), (int)std::floor(l.y + std::sin(ang) * 3.0f), kBark[ang > 0.5f && ang < 3.6f ? 1 : 2]);
    }
    c.set((int)l.x, (int)l.y, kWood[1]);
    c.set((int)l.x - 1, (int)l.y - 1, kWood[2]);
  }
}

void trough(Canvas& c) {
  const int b = c.h - 2;
  for (int y = 3; y <= b - 2; y++)
    for (int x = 1; x < c.w - 1; x++) {
      int k = y == 3 ? 4 : (x == 1 ? 3 : (x == c.w - 2 ? 1 : 2));
      c.set(x, y, kWood[k]);
    }
  for (int x = 3; x < c.w - 3; x++) { c.set(x, 4, kWater[3]); c.set(x, 5, kWater[2]); }
  c.set(8, 4, kWater[4]); c.set(9, 4, kWater[4]);
  for (int x : {3, c.w - 4}) vl(c, x, b - 1, b, kWood[1]);
}

// The watermill's undershot wheel, seen a little from the side (foreshortened), turning: the rim, eight spokes, the
// paddles standing out of the rim, darker and wet where it dips into the race, white water churning at its foot
void waterWheelProp(Canvas& c, int frame) {
  const float cx = c.w * 0.5f, cy = 17.0f, R = 14.0f, Rx = 8.0f;
  const float rot = frame * 0.19635f;   // a sixteenth of a turn per frame: the paddles step round
  for (int y = 1; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float fx = (x + 0.5f - cx) / Rx, fy = (y + 0.5f - cy) / R;
      const float d = std::sqrt(fx * fx + fy * fy);
      if (d > 1.12f) continue;
      const float ang = std::atan2(fy, fx) + rot;
      const bool wet = y > cy + 8;
      if (d > 0.84f) {
        const bool paddle = std::fmod(ang + 62.83185f, 0.5236f) < 0.2f;
        if (d > 0.97f && !paddle) continue;
        int k = fx + fy < -0.35f ? 4 : (fx + fy > 0.45f ? 1 : (fx < 0 ? 3 : 2));
        if (d > 0.97f) k = std::max(1, k - 1);
        c.set(x, y, (wet ? kWoodDark : kWood)[wet ? std::max(0, k - 1) : k]);
      } else if (d < 0.2f) {
        c.set(x, y, kIron[d < 0.1f ? 4 : 2]);
      } else {
        const bool spoke = std::fmod(ang + 62.83185f, 0.7854f) < 0.17f;
        if (spoke) c.set(x, y, (wet ? kWoodDark : kWood)[fx < 0 ? 3 : 2]);
      }
    }
  // the race churning white at its foot
  for (int x = 1; x < c.w - 1; x++) {
    const int y = c.h - 4 + (int)((x * 7 + frame * 3) % 3 == 0);
    c.set(x, y, (x + frame) % 3 == 0 ? kWhite : kWater[4]);
    if ((x + frame) % 4 == 0) c.set(x, y - 1, kWater[3]);
    if ((x * 5 + frame) % 6 == 0) c.set(x, y + 1, kWhite);
  }
}

}  // namespace

Canvas marketStall(int trade, int awning) { return marketStallForm(trade, awning, 0, false); }

Canvas marketStallForm(int trade, int awning, int form, bool closed) {
  Canvas c(SW, SH);
  paintStall(c, ((trade % kStallTrades) + kStallTrades) % kStallTrades, awning, ((form % kStallForms) + kStallForms) % kStallForms, closed);
  outline(c, 0.95f);
  return c;
}

Canvas marketTable(int goods, int shade, bool closed) {
  Canvas c(propW(Prop::MarketTable), propH(Prop::MarketTable));
  shade = ((shade % kTableShades) + kTableShades) % kTableShades;
  tableFrame(c, true);
  if (shade) tableShade(c, shade, closed);
  tableFrame(c, false);
  if (!closed) tableGoods(c, goods);
  else {   // the stock under a tied cover
    const Ramp S = ramp(rgba(196, 180, 146), 0.7f);
    for (int x = TX0 + 1; x <= TX1 - 1; x++) {
      const float u = (x - TX0) / (float)(TX1 - TX0);
      const int hump = 2 + (int)std::lround(std::sin(u * 9.0f) * 1.5f + 1.5f);
      for (int y = TTOP0 + 1 - hump; y <= TTOP1; y++) c.set(x, y, S[y == TTOP0 + 1 - hump ? 4 : (y < TTOP0 ? 3 : 2)]);
    }
    for (int x : {25, 38})
      for (int y = TTOP0 - 4; y <= TTOP1; y++) if (chA(c.get(x, y))) c.set(x, y, kWood[1]);
  }
  outline(c, 0.95f);
  return c;
}

Canvas groundCloth(int goods, int cloth, bool closed) {
  Canvas c(propW(Prop::GroundCloth), propH(Prop::GroundCloth));
  if (!closed) { clothSpread(c, cloth); clothGoods(c, goods); }
  else {   // folded into a bundle, tied, waiting for morning
    const Ramp R = ramp(groundClothCol(cloth), 0.7f);
    ball(c, 31.0f, GBOT - 4.5f, 8.0f, 5.0f, R, 0.08f);
    vl(c, 31, GBOT - 9, GBOT, kWood[1]);
    c.set(30, GBOT - 10, kWood[2]); c.set(32, GBOT - 10, kWood[2]);
  }
  outline(c, 0.9f);
  return c;
}

Canvas mineHill(int variant, int land) {
  Canvas c(propW(Prop::MineHill), propH(Prop::MineHill));
  paintMineHill(c, variant & 3, land);
  // no ink line round the turf where it meets the ground (the knoll grows out of the land, it is not stuck on it):
  // the skyline and the cliff keep a dark edge, the grassy foot of the shoulders thins out into the grass
  outline(c, 0.9f);
  const int b = c.h - 1;
  for (int y = b - 6; y <= b; y++)
    for (int x = 0; x < c.w; x++) {
      const float u = std::fabs((x + 0.5f - c.w * 0.5f) / (c.w * 0.5f));
      if (u < 0.74f) continue;
      const float fade = (float)(y - (b - 6)) / 6.0f;
      if (bayer(x, y) < fade * 0.85f) c.set(x, y, 0);
    }
  return c;
}

Canvas mineRail(int joins) {
  Canvas c(16, 16);
  railTile(c, joins & 15);
  return c;
}

void paintEconomyProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::WaterWheel: waterWheelProp(c, frame); break;
    case Prop::Sacks: sacks(c); break;
    case Prop::Baskets: baskets(c); break;
    case Prop::DryingRack: dryingRack(c); break;
    case Prop::HideRack: hideRack(c); break;
    case Prop::OreCart: oreCart(c); break;
    case Prop::OrePile: orePile(c); break;
    case Prop::MineEntrance: mineEntrance(c); break;
    case Prop::LogPile: logPile(c); break;
    case Prop::Trough: trough(c); break;
    case Prop::MarketTable: tableFrame(c, true); tableFrame(c, false); tableGoods(c, 0); break;
    case Prop::GroundCloth: clothSpread(c, 0); clothGoods(c, 0); break;
    case Prop::MarketCross: marketCross(c); break;
    case Prop::Sheep: sheep(c, frame); break;
    case Prop::Cow: cow(c, frame); break;
    case Prop::MineRail: railTile(c, 5); break;
    case Prop::PenShelter: penShelter(c); break;
    case Prop::MineHill: paintMineHill(c, 0, 0); break;
    default: break;
  }
}

}  // namespace art
