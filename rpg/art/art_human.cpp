// EMBERVALE art: humans (player, NPCs, bandits) and the humanoid monsters that share the rig. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include <cstdlib>
#include <cstring>

#include "rpg/art/art_internal.h"

namespace art {

// =====================================================================================================
// 3. humans
// =====================================================================================================
namespace {

enum Facing { kDown = 0, kUp = 1, kSide = 2 };

// Per-frame body pose. Front/back views use A/B = screen left/right; the side view uses A = near, B = far.
struct Pose {
  int bob = 0, lean = 0;
  int liftA = 0, liftB = 0;    // foot lift (px)
  int stepA = 0, stepB = 0;    // side view: foot x offset from the hip
  int swingA = 0, swingB = 0;  // front/back: hand y offset; side: hand x offset
  int atk = 0;                 // 1 wind-up, 2 strike
  bool hurt = false;
};

Pose humanPose(int facing, int frame) {
  Pose p;
  if (frame >= 1 && frame <= 4) {
    int i = frame - 1;
    if (facing == kSide) {
      static const int sN[4] = {3, 0, -3, 1}, sF[4] = {-3, 1, 3, 0};
      static const int lN[4] = {0, 0, 0, 2}, lF[4] = {0, 2, 0, 0};
      static const int bob[4] = {0, -1, 0, -1}, hN[4] = {-2, 0, 2, 0};
      p.stepA = sN[i]; p.stepB = sF[i]; p.liftA = lN[i]; p.liftB = lF[i];
      p.bob = bob[i]; p.swingA = hN[i]; p.swingB = -hN[i];
    } else {
      static const int lL[4] = {2, 0, 0, 1}, lR[4] = {0, 1, 2, 0};
      static const int bob[4] = {-1, 0, -1, 0}, sL[4] = {-1, -1, 1, 1};
      p.liftA = lL[i]; p.liftB = lR[i]; p.bob = bob[i]; p.swingA = sL[i]; p.swingB = -sL[i];
    }
  } else if (frame == 5) {
    p.atk = 1;
    if (facing == kSide) { p.lean = -1; p.stepA = 2; p.stepB = -2; }
  } else if (frame == 6) {
    p.atk = 2;
    if (facing == kSide) { p.lean = 1; p.stepA = 3; p.stepB = -3; }
  } else if (frame == 7) {
    p.hurt = true;
    p.lean = -1;
    p.swingA = -1; p.swingB = -1;
  }
  return p;
}

// ---- head overlay maps. 12 columns starting at x = 2, rows starting at y = hy - 3 (hy = head top).
// digits = ramp index (0 darkest .. 4 highlight), 'e' = eye, 'g' = accent, 'v' = visor, 'b' = leather tie.
using Map = const char* const*;
constexpr int kMapRows = 17;

const char* const kSkinDown[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333332...",
  "..33333322..",
  "..33222221..",
  "..32222221..",
  "..32e22e21..",
  "..22e22e21..",
  "..12222211..",
  "...111111...",
};
const char* const kSkinUp[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333221...",
  "..33222211..",
  "..32222211..",
  "..22222211..",
  "..22222211..",
  "..22222111..",
  "..12221111..",
  "...111111...",
};
const char* const kSkinSide[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333322...",
  "..33333222..",
  "..33322222..",
  "..32222222..",
  "..322122e2..",
  "..222122e22.",
  "..12222222..",
  "...1122221..",
};

const char* const kHairShortD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  "..22.2212.1.",
  "..1......1..",
};
const char* const kHairShortU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22221111..",
  "..12211110..",
  "...111110...",
};
const char* const kHairShortS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  "..2332221.1.",
  "..2221......",
  "..221.......",
  "..21........",
  "...1........",
};
const char* const kHairLongD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  ".2233332211.",
  ".232.2212.1.",
  ".22......11.",
  ".22......11.",
  ".21......10.",
  ".21......10.",
  ".11......10.",
  ".1........0.",
};
const char* const kHairLongU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  ".2233332211.",
  ".2333222211.",
  ".2322222111.",
  ".2222221111.",
  ".2222211110.",
  ".1222111110.",
  "..12211110..",
  "..11111100..",
  "...1.1.0....",
};
const char* const kHairLongS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  ".223333321..",
  ".22332221.1.",
  ".22221......",
  ".2221.......",
  ".2221.......",
  ".2211.......",
  ".1211.......",
  ".111........",
  "..1.........",
};
const char* const kHairPonyD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  "..22.2212.11",
  "..1......111",
  ".........11.",
  ".........11.",
  "..........1.",
};
const char* const kHairPonyU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..222bb111..",
  "..12233110..",
  "...123210...",
  ".....221....",
  ".....221....",
  ".....211....",
  "......1.....",
};
const char* const kHairPonyS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  ".b2332221.1.",
  "1232221.....",
  "2221221.....",
  "221.21......",
  "21...1......",
  "1...........",
};
const char* const kHairMohawkD[kMapRows] = {
  ".....43.....",
  "....3432....",
  "....3321....",
  "....3221....",
  ".....21.....",
};
const char* const kHairMohawkU[kMapRows] = {
  ".....43.....",
  "....3432....",
  "....3321....",
  "....3221....",
  "....3221....",
  "....2211....",
  "....2211....",
  ".....11.....",
};
const char* const kHairMohawkS[kMapRows] = {
  nullptr,
  "......443...",
  "...3443321..",
  "..2332221...",
  "..21........",
};
const char* const kHairBraidD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23332221..",
  "..2232.211..",
  "..3......2..",
  "..1......1..",
  "..3......2..",
  "..1......1..",
  "..3......2..",
  "..1......1..",
  "..b......b..",
};
const char* const kHairBraidU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22221111..",
  "..12211110..",
  "...321120...",
  "...2...1....",
  "...3...2....",
  "...1...1....",
  "...b...b....",
};
const char* const kHairBraidS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  "..2332221.1.",
  "..2221......",
  "..221.......",
  "..213.......",
  "...21.......",
  "...31.......",
  "...21.......",
  "...b........",
};

// M0: top-knot bun (slicked back, forehead free) and voluminous curls
const char* const kHairBunD[kMapRows] = {
  nullptr,
  ".....43.....",
  "....34321...",
  "..23443332..",
  "..23333221..",
  "..2......1..",
  "..1......1..",
};
const char* const kHairBunU[kMapRows] = {
  nullptr,
  ".....43.....",
  "....34321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22221111..",
  "..12211110..",
  "...111110...",
};
const char* const kHairBunS[kMapRows] = {
  nullptr,
  ".342........",
  "23432332....",
  ".22344332...",
  "..23333321..",
  "..2332221...",
  "..2221......",
  "..221.......",
  "..21........",
  "...1........",
};
const char* const kHairCurlsD[kMapRows] = {
  nullptr,
  "....2343....",
  "..23434332..",
  ".2343443432.",
  ".2334333321.",
  ".232.2.212.1",
  ".22......11.",
  ".21......10.",
  "..1......1..",
};
const char* const kHairCurlsU[kMapRows] = {
  nullptr,
  "....2343....",
  "..23434332..",
  ".2343443432.",
  ".2334343321.",
  ".2343332321.",
  ".2323232211.",
  ".2232322110.",
  "..12212110..",
  "...111110...",
};
const char* const kHairCurlsS[kMapRows] = {
  nullptr,
  "...2343.....",
  ".2343433....",
  ".23434432...",
  "234343321...",
  "2323322.1...",
  "232321......",
  "22321.......",
  ".221........",
  "..1.........",
};

// cloth hood, face opening left free
const char* const kHoodD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  ".2334333221.",
  ".2310000011.",
  ".22......11.",
  ".22......11.",
  ".21......10.",
  ".221....110.",
  "..22111110..",
};
const char* const kHoodU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  ".2334332211.",
  ".2333222111.",
  ".2332222111.",
  ".2322221110.",
  ".2222211110.",
  ".1222111110.",
  "..12211110..",
  "...1111.....",
};
const char* const kHoodS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  ".233333321..",
  ".2333322001.",
  ".233321.....",
  ".23321......",
  ".22221......",
  ".122211.....",
  "..11111.....",
};

const char* const kBeardD[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "..2......1..",
  "..23322211..",
  "..23222111..",
  "...222110...",
  "....2110....",
};
const char* const kBeardS[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  ".....2......",
  ".....22232..",
  "......32221.",
  "......2211..",
  ".......10...",
};

const char* const kCapD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  ".1222222211.",
};
const char* const kCapU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23222211..",
  ".1222222211.",
};
const char* const kCapS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  ".122222221..",
};
const char* const kIronHelmD[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444332..",
  "..23333221..",
  ".1221221211.",
  "..1..21..1..",
  ".....21.....",
};
const char* const kIronHelmU[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  ".1222222111.",
  "..1......1..",
};
const char* const kIronHelmS[kMapRows] = {
  nullptr,
  "....233.....",
  "...23443....",
  "..2344332...",
  "..23333321..",
  ".122222221..",
  "..2211...2..",
  "..211....1..",
};
const char* const kGreatHelmD[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..33322221..",
  "..3vvvvvv1..",
  "..32212211..",
  "..32212211..",
  "..21111110..",
  "...111110...",
};
const char* const kGreatHelmU[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..33322221..",
  "..33222211..",
  "..32222211..",
  "..22222111..",
  "..21111110..",
  "...111110...",
};
const char* const kGreatHelmS[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..333222221.",
  "..3322vvvv1.",
  "..32222212..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};
const char* const kElvenHelmD[kMapRows] = {
  "1..........1",
  "21..2332..12",
  "321234432123",
  ".3234g4332..",
  "..23333221..",
  "..2.2221.1..",
  "..1......1..",
};
const char* const kElvenHelmU[kMapRows] = {
  "1..........1",
  "21..2332..12",
  "321234432123",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..1......1..",
};
const char* const kElvenHelmS[kMapRows] = {
  "1...........",
  "21..233.....",
  "3212344g....",
  ".32344332...",
  "..23333321..",
  "..2332221...",
  "..221.......",
  "..21........",
};
const char* const kEbonyHelmD[kMapRows] = {
  ".3........3.",
  ".2........2.",
  ".22.2332.21.",
  "..22344321..",
  "..23444332..",
  "..23333221..",
  "..2vgvvgv1..",
  "..22222211..",
  "..21122110..",
  "...111110...",
};
const char* const kEbonyHelmU[kMapRows] = {
  ".3........3.",
  ".2........2.",
  ".22.2332.21.",
  "..22344321..",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..21111110..",
  "...111110...",
};
const char* const kEbonyHelmS[kMapRows] = {
  "..3.........",
  "..2.........",
  "..22233.....",
  "...234432...",
  "..23444332..",
  "..233333221.",
  "..2322vvg1..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};
const char* const kKettleD[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444332..",
  "..23333221..",
  "122222222110",
};
const char* const kKettleU[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "122222221110",
};
const char* const kKettleS[kMapRows] = {
  nullptr,
  "....233.....",
  "...23443....",
  "..2344332...",
  "..23333321..",
  "12222222210.",
};

// M0 helmet bands. Row 0 of a head map lands above the cell on the bobbing walk frames, so these keep it empty
// (crests, wings and horns fit in rows 1-2 and never flicker). Gilded is winged with a green gem, jade crested with
// cheek guards and gold studs, obsidian horned with a slit visor, emberforged a horned crown with a glowing visor.
const char* const kGildHelmD[kMapRows] = {
  nullptr,
  "2...2332...2",
  "321234432123",
  ".3234g4332..",
  "..23333221..",
  "..2.2221.1..",
  "..1......1..",
};
const char* const kGildHelmU[kMapRows] = {
  nullptr,
  "2...2332...2",
  "321234432123",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..1......1..",
};
const char* const kGildHelmS[kMapRows] = {
  nullptr,
  "2...233.....",
  "3212344g....",
  ".32344332...",
  "..23333321..",
  "..2332221...",
  "..221.......",
  "..21........",
};
const char* const kJadeHelmD[kMapRows] = {
  nullptr,
  ".....gg.....",
  "...23gg32...",
  "..2344g332..",
  "..23333221..",
  ".1g222222g1.",
  "..21....12..",
  "..21....12..",
  "...1....1...",
};
const char* const kJadeHelmU[kMapRows] = {
  nullptr,
  ".....gg.....",
  "...23gg32...",
  "..23444322..",
  "..23333221..",
  ".1g222222g1.",
  "..22222211..",
  "..21111110..",
  "...1....1...",
};
const char* const kJadeHelmS[kMapRows] = {
  nullptr,
  "..gggg......",
  "..34gg332...",
  "..23444g32..",
  "..23333321..",
  ".1g22222g1..",
  "..2221...1..",
  "..221.......",
  "..21........",
};
const char* const kObsHelmD[kMapRows] = {
  nullptr,
  ".3........3.",
  ".22.2332.21.",
  "..22344321..",
  "..23444332..",
  "..23333221..",
  "..2vgvvgv1..",
  "..22222211..",
  "..21122110..",
  "...111110...",
};
const char* const kObsHelmU[kMapRows] = {
  nullptr,
  ".3........3.",
  ".22.2332.21.",
  "..22344321..",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..21111110..",
  "...111110...",
};
const char* const kObsHelmS[kMapRows] = {
  nullptr,
  "..3.........",
  "..22233.....",
  "...234432...",
  "..23444332..",
  "..233333221.",
  "..2322vvg1..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};
const char* const kEmberHelmD[kMapRows] = {
  nullptr,
  "3...4..4...3",
  "23223443322.",
  ".322344321..",
  "..23444332..",
  "..23333221..",
  "..2vgvvgv1..",
  "..22222211..",
  "..21122110..",
  "...111110...",
};
const char* const kEmberHelmU[kMapRows] = {
  nullptr,
  "3...4..4...3",
  "23223443322.",
  ".322344321..",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..21111110..",
  "...111110...",
};
const char* const kEmberHelmS[kMapRows] = {
  nullptr,
  "3...4.4.....",
  "2322343.....",
  ".3234432....",
  "..23444332..",
  "..233333221.",
  "..2322vvg1..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};

void drawMap(Canvas& c, Map m, int ox, int oy, const Ramp& r, uint32_t eye = kEye, uint32_t accent = 0, uint32_t visor = 0) {
  for (int row = 0; row < kMapRows; row++) {
    const char* s = m[row];
    if (!s) continue;
    for (int col = 0; s[col]; col++) {
      char ch = s[col];
      int x = ox + col, y = oy + row;
      if (ch >= '0' && ch <= '4') c.set(x, y, r[ch - '0']);
      else if (ch == 'e') c.set(x, y, eye);
      else if (ch == 'g' && accent) c.set(x, y, accent);
      else if (ch == 'v') c.set(x, y, visor ? visor : r[0]);
      else if (ch == 'b') c.set(x, y, kLeather[1]);
    }
  }
}

const Ramp kSteelArmor = ramp5(rgba(58, 64, 88), rgba(104, 116, 140), rgba(160, 172, 190), rgba(206, 216, 228), rgba(248, 252, 255));
const Ramp kElvenArmor = ramp5(rgba(92, 70, 40), rgba(150, 120, 46), rgba(204, 176, 70), rgba(232, 214, 112), rgba(252, 244, 184));
const Ramp kEbonyArmor = ramp5(rgba(22, 16, 30), rgba(38, 28, 50), rgba(58, 42, 74), rgba(92, 64, 112), rgba(146, 104, 168));
const Ramp kChainMail = ramp5(rgba(46, 48, 64), rgba(84, 88, 104), rgba(128, 132, 146), rgba(170, 174, 184), rgba(214, 218, 222));
const Ramp kRagCloth = ramp5(rgba(52, 44, 46), rgba(78, 70, 60), rgba(108, 98, 74), rgba(136, 126, 92), rgba(164, 154, 116));
// M0 bands that had no outfit of their own
const Ramp kJadeArmor = ramp5(rgba(20, 52, 50), rgba(34, 92, 74), rgba(58, 140, 98), rgba(108, 188, 132), rgba(184, 232, 186));
const Ramp kEmberArmor = ramp5(rgba(22, 12, 20), rgba(42, 20, 28), rgba(70, 30, 36), rgba(112, 48, 44), rgba(176, 86, 58));
const Ramp kJadeCloth = ramp5(rgba(22, 34, 40), rgba(34, 54, 56), rgba(52, 78, 72), rgba(78, 106, 92), rgba(112, 140, 116));
// a tan, oiled leather for the cap so it never reads as hair
const Ramp kCapLeather = ramp5(rgba(70, 42, 36), rgba(118, 76, 50), rgba(164, 116, 70), rgba(200, 156, 98), rgba(228, 196, 138));
const Ramp kFur = ramp5(rgba(84, 70, 70), rgba(134, 120, 112), rgba(180, 168, 152), rgba(214, 206, 188), rgba(242, 238, 224));
const uint32_t kEmberGlow = rgba(255, 168, 60);

// material of an armour band (HumanLook: 1 leather .. 7 emberforged)
const Ramp& bandRamp(int b) {
  switch (b) {
    case 1: return kLeather;
    case 2: return kIron;
    case 3: return kSteelArmor;
    case 4: return kElvenArmor;
    case 5: return kJadeArmor;
    case 6: return kEbonyArmor;
    case 7: return kEmberArmor;
    default: return kLeather;
  }
}
uint32_t bandAccent(int b) {
  switch (b) {
    case 4: return rgba(96, 210, 120);
    case 5: return kGold[3];
    case 6: return rgba(220, 60, 120);
    case 7: return kEmberGlow;
    default: return 0;
  }
}

struct Rig {
  Ramp skin, hair, top, sleeve, leg, boot, trim, metal, cape, hood, belt, glove, cloak;
  uint32_t accent = 0;
};

struct HumanPainter {
  Canvas& c;
  const HumanLook& L;
  Rig R;
  Pose P;
  int facing = kDown;
  int hy = 3, ty = 11, hip = 17;
  static constexpr int kGround = 22;
  Outfit O = Outfit::Tunic;   // the outfit whose silhouette is painted (armorStyle picks one for its band)
  int armor = 0;              // L.armorStyle (0 legacy)

  HumanPainter(Canvas& c_, const HumanLook& l, int f, const Pose& p) : c(c_), L(l), P(p), facing(f) {
    hy = 3 + P.bob;
    ty = hy + 8;
    hip = ty + 6;
    armor = L.armorStyle <= HumanLook::kBands ? L.armorStyle : 0;
    static const Outfit byBand[8] = {Outfit::Tunic, Outfit::Leather, Outfit::Chain, Outfit::Plate, Outfit::Elven, Outfit::Plate, Outfit::Ebony, Outfit::Ebony};
    O = armor ? byBand[armor] : L.outfit;
    buildRig();
  }

  bool skirt() const { return O == Outfit::Dress || O == Outfit::Robe; }
  bool metalBody() const { return O == Outfit::Plate || O == Outfit::Elven || O == Outfit::Ebony; }
  bool isStaff() const { return L.weapon == 4; }
  bool heavyHead() const { return L.weapon == 2 || L.weapon == 6; }
  bool idle() const { return !P.atk && !P.hurt && !P.bob && !P.liftA && !P.liftB && !P.stepA && !P.swingA; }

  void buildRig() {
    R.skin = ramp(L.skin, 0.8f);
    R.hair = ramp(L.hairColor);
    R.top = ramp(L.topColor);
    R.sleeve = R.top;
    R.leg = ramp(L.bottomColor);
    R.boot = ramp5(kLeather[0], kLeather[0], kLeather[1], kLeather[2], kLeather[3]);
    R.trim = ramp(L.tabardColor);
    R.cape = ramp(L.tabardColor);
    R.metal = ramp(L.weaponColor, 1.1f);
    R.belt = kLeather;
    R.hood = O == Outfit::Robe ? R.top : ramp(shade(L.bottomColor, 0.95f));
    switch (O) {
      case Outfit::Leather: R.top = kLeather; R.sleeve = R.skin; break;
      case Outfit::Chain: R.top = kChainMail; R.sleeve = kChainMail; R.leg = ramp(shade(L.bottomColor, 0.85f)); break;
      case Outfit::Plate:
        R.top = kSteelArmor; R.sleeve = kSteelArmor; R.leg = kSteelArmor;
        R.boot = ramp5(kSteelArmor[0], kSteelArmor[0], kSteelArmor[1], kSteelArmor[2], kSteelArmor[3]);
        break;
      case Outfit::Elven:
        R.top = kElvenArmor; R.sleeve = kElvenArmor;
        R.leg = ramp5(rgba(30, 56, 50), rgba(46, 86, 60), rgba(70, 120, 70), rgba(108, 156, 84), rgba(156, 192, 110));
        R.boot = ramp5(kElvenArmor[0], kElvenArmor[0], kElvenArmor[1], kElvenArmor[2], kElvenArmor[3]);
        R.accent = rgba(96, 210, 120);
        break;
      case Outfit::Ebony:
        R.top = kEbonyArmor; R.sleeve = kEbonyArmor; R.leg = kEbonyArmor; R.boot = kEbonyArmor;
        R.accent = rgba(220, 60, 120);
        break;
      case Outfit::Guard: R.top = R.trim; R.sleeve = kChainMail; R.leg = ramp(shade(L.bottomColor, 0.85f)); break;
      case Outfit::Rags: R.top = kRagCloth; R.sleeve = R.skin; R.leg = kRagCloth; R.boot = R.skin; break;
      default: break;
    }
    // M0 bands painted on a borrowed silhouette (jade on plate, emberforged on obsidian)
    if (armor == 5) {
      R.top = kJadeArmor; R.sleeve = kJadeArmor; R.leg = kJadeCloth;
      R.boot = ramp5(kJadeArmor[0], kJadeArmor[0], kJadeArmor[1], kJadeArmor[2], kJadeArmor[3]);
      R.accent = kGold[3];
    } else if (armor == 7) {
      R.top = kEmberArmor; R.sleeve = kEmberArmor; R.leg = kEmberArmor; R.boot = kEmberArmor;
      R.accent = kEmberGlow;
    }
    if (L.boots) {
      const Ramp& b = bandRamp(L.boots);
      R.boot = L.boots == 1 ? ramp5(b[0], b[0], b[1], b[2], b[3]) : b;
    }
    if (armor == 1) R.sleeve = ramp(L.topColor);   // the leather jerkin goes over the shirt: its sleeves show
    R.glove = L.gloves ? bandRamp(L.gloves) : R.skin;
    R.cloak = ramp(L.cloakColor);
  }

  // ---------------------------------------------------------------------- legs
  // extra boot rows above the legacy 2-row shoe: leather/iron boots reach the shin, metal greaves the knee
  int bootExtra() const { return L.boots ? (L.boots >= 3 ? 2 : 1) : 0; }
  void legsFront() {
    if (skirt()) return;
    const Ramp& lg = R.leg;
    for (int x = 5; x <= 10; x++) c.set(x, hip, lg[x == 5 ? 3 : (x == 10 ? 1 : 2)]);
    int lifts[2] = {P.liftA, P.liftB};
    bool bare = O == Outfit::Rags;
    for (int side = 0; side < 2; side++) {
      int x0 = side == 0 ? 5 : 8;
      int foot = kGround - lifts[side];
      int bootTop = foot - 1 - bootExtra();
      for (int y = hip + 1; y <= foot; y++) {
        bool boot = y >= bootTop;
        bool shin = bare && !boot && y > foot - 4;
        const Ramp& r = boot ? R.boot : (shin ? R.skin : lg);
        int kL = side == 0 ? 3 : 2;
        c.set(x0, y, r[kL]); c.set(x0 + 1, y, r[2]); c.set(x0 + 2, y, r[1]);
        if (boot && y == bootTop) {
          c.set(x0, y, r[kL + 1]);
          if (L.boots) c.set(x0 + 1, y, r[3]);   // turned-down cuff / greave rim catches the light
        }
        if (L.boots >= 3 && boot && y == foot) c.set(x0 + (side == 0 ? 0 : 1), y, r[4]);   // sabaton toe glint
      }
      if (side == 0) for (int y = hip + 2; y <= foot; y++) c.set(7, y, (y >= bootTop ? R.boot : lg)[0]);
      if (O == Outfit::Plate || O == Outfit::Elven) {   // knee plates
        int ky = hip + 2;
        if (ky < foot - 1) { c.set(x0, ky, R.leg[4]); c.set(x0 + 1, ky, R.leg[3]); }
      }
    }
  }

  void legsSide() {
    if (skirt()) return;
    struct Leg { int step, lift, bias; } legs[2] = {{P.stepB, P.liftB, -1}, {P.stepA, P.liftA, 0}};
    for (auto& g : legs) {
      int foot = kGround - g.lift;
      int hipX = 7 + P.lean;
      for (int y = hip; y <= foot; y++) {
        float t = (float)(y - hip) / std::max(1, kGround - hip);
        int cx = hipX + (int)std::lround(g.step * t) - (g.lift > 0 && y > hip + 2 ? 1 : 0);
        int bootTop = foot - 1 - bootExtra();
        bool boot = y >= bootTop;
        bool shin = O == Outfit::Rags && !boot && y > foot - 4;
        const Ramp& r = boot ? R.boot : (shin ? R.skin : R.leg);
        c.set(cx, y, r[3 + g.bias]); c.set(cx + 1, y, r[2 + g.bias]); c.set(cx + 2, y, r[1 + g.bias]);
        if (boot && y == foot) c.set(cx + 3, y, r[1 + g.bias]);
        if (L.boots && y == bootTop) { c.set(cx, y, r[4 + g.bias]); c.set(cx + 1, y, r[3 + g.bias]); }
      }
    }
  }

  // ---------------------------------------------------------------------- torso
  void torsoFront(bool back) {
    const Ramp& t = R.top;
    static const int colK[6] = {1, 3, 2, 2, 2, 1};
    for (int y = ty; y < hip; y++)
      for (int x = 5; x <= 10; x++) {
        int k = colK[x - 5];
        if (y == ty && x < 10) k = std::min(4, k + 1);
        c.set(x, y, t[k]);
      }
    switch (O) {
      case Outfit::Tunic:
        if (!back) { c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[1]); c.set(7, ty + 1, R.top[1]); }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Dress:
        if (!back) { c.set(6, ty, R.skin[2]); c.set(7, ty, R.skin[2]); c.set(8, ty, R.skin[1]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 3, R.trim[x < 8 ? 2 : 1]);
        break;
      case Outfit::Robe:
        if (!back) {
          for (int y = ty; y < hip; y++) { c.set(7, y, R.trim[3]); c.set(8, y, R.trim[2]); }
          c.set(7, ty, R.trim[4]);
        }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.trim[x < 8 ? 1 : 0]);
        break;
      case Outfit::Leather:
        for (int x = 5; x <= 10; x++) c.set(x, ty, kLeather[x < 9 ? 3 : 2]);
        if (!back) {
          line(c, 5, ty + 1, 9, ty + 4, kLeather[0]);
          c.set(6, ty + 2, kBrass[4]); c.set(9, ty + 1, kBrass[3]); c.set(8, ty + 3, kBrass[3]);
        } else {
          line(c, 9, ty + 1, 5, ty + 4, kLeather[0]);
        }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Chain:
        chainTexture(5, ty, 10, hip - 1);
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Plate:
        if (!back) { c.set(6, ty + 1, R.top[4]); c.set(6, ty + 2, R.top[4]); c.set(9, ty + 3, R.top[1]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.top[x < 8 ? 1 : 0]);
        break;
      case Outfit::Elven:
        if (!back) { c.set(7, ty + 1, R.accent); c.set(8, ty + 1, shade(R.accent, 0.7f)); c.set(7, ty + 2, shade(R.accent, 0.8f)); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, rgba(70, 120, 70));
        break;
      case Outfit::Ebony:
        if (!back) { c.set(7, ty + 2, R.accent); c.set(8, ty + 2, shade(R.accent, 0.6f)); }
        c.set(6, ty + 1, R.top[4]);
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.top[0]);
        break;
      case Outfit::Guard:
        chainTexture(5, ty, 10, ty);
        c.set(5, ty + 1, R.sleeve[1]); c.set(10, ty + 1, R.sleeve[0]);
        if (!back) { c.set(7, ty + 2, kGold[3]); c.set(8, ty + 2, kGold[2]); c.set(7, ty + 3, kGold[2]); c.set(8, ty + 1, kGold[3]); }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Rags:
        if (!back) { c.set(7, ty, R.skin[1]); c.set(8, ty + 1, R.top[0]); c.set(6, ty + 3, R.top[4]); }
        c.set(10, ty + 2, R.top[0]);
        break;
      default: break;
    }
    bandDetailFront(back);
  }
  // jade scale rows and a gilt belt; emberforged glowing seams. Painted over the borrowed outfit's torso.
  void bandDetailFront(bool back) {
    if (armor == 5) {
      static const int colK[6] = {1, 3, 2, 2, 2, 1};
      for (int y = ty + 1; y <= ty + 3; y++)
        for (int x = 5; x <= 10; x++)
          if (((x + (y & 1)) & 1) == 0) c.set(x, y, R.top[std::max(0, colK[x - 5] - 1)]);
      for (int x = 5; x <= 10; x++) c.set(x, ty + 4, kGold[x < 8 ? 2 : 1]);
      if (!back) { c.set(7, ty + 4, kGold[4]); c.set(7, ty + 1, R.top[4]); }
    } else if (armor == 7) {
      uint32_t dim = mix(R.top[1], kEmberGlow, 0.55f);
      if (!back) { c.set(6, ty + 3, dim); c.set(9, ty + 1, dim); c.set(8, ty + 3, dim); c.set(7, ty + 4, kEmberGlow); }
      else { c.set(7, ty + 2, dim); c.set(8, ty + 1, dim); }
      c.set(5, ty, R.top[3]); c.set(10, ty, R.top[2]);
    }
  }
  // build: slim pinches the waist, broad deepens the chest (front/back views; called before the arms)
  void buildFront() {
    if (skirt()) return;
    if (L.build == 1)
      for (int y = ty + 3; y < hip; y++) { c.set(5, y, 0); c.set(10, y, 0); }
  }
  void buildShoulders(bool lit, int x0) {   // broad: a deltoid bulge outside each arm (front/back views)
    if (L.build != 2 || P.atk) return;
    const Ramp& s = R.sleeve;
    int x = lit ? x0 - 1 : x0 + 2;
    c.set(x, ty + 1, s[lit ? 3 : 1]); c.set(x, ty + 2, s[lit ? 2 : 0]);
  }

  void torsoSide() {
    const Ramp& t = R.top;
    int ox = P.lean;
    static const int colK[5] = {3, 2, 2, 1, 1};
    for (int y = ty; y < hip; y++)
      for (int x = 6; x <= 10; x++) {
        int k = colK[x - 6];
        if (y == ty && x < 10) k = std::min(4, k + 1);
        c.set(x + ox, y, t[k]);
      }
    switch (O) {
      case Outfit::Tunic: c.set(10 + ox, ty, R.skin[1]); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case Outfit::Dress:
        c.set(10 + ox, ty, R.skin[2]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 3, R.trim[x < 9 ? 2 : 1]);
        break;
      case Outfit::Robe:
        for (int y = ty; y < hip; y++) c.set(10 + ox, y, R.trim[2]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, R.trim[1]);
        break;
      case Outfit::Leather:
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty, kLeather[3]);
        c.set(9 + ox, ty + 2, kBrass[4]);
        beltRow(6 + ox, 10 + ox, ty + 4);
        break;
      case Outfit::Chain: chainTexture(6 + ox, ty, 10 + ox, hip - 1); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case Outfit::Plate:
        c.set(9 + ox, ty + 1, R.top[4]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, R.top[1]);
        break;
      case Outfit::Elven:
        c.set(10 + ox, ty + 1, R.accent);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, rgba(70, 120, 70));
        break;
      case Outfit::Ebony: c.set(10 + ox, ty + 2, R.accent); c.set(8 + ox, ty + 1, R.top[4]); break;
      case Outfit::Guard:
        chainTexture(6 + ox, ty, 7 + ox, hip - 1);
        c.set(9 + ox, ty + 2, kGold[3]);
        beltRow(6 + ox, 10 + ox, ty + 4);
        break;
      case Outfit::Rags: c.set(10 + ox, ty, R.skin[1]); c.set(7 + ox, ty + 3, R.top[0]); break;
      default: break;
    }
    if (armor == 5) {
      for (int y = ty + 1; y <= ty + 3; y++)
        for (int x = 6; x <= 10; x++) if (((x + (y & 1)) & 1) == 0) c.set(x + ox, y, R.top[x < 8 ? 2 : 0]);
      for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, kGold[x < 9 ? 2 : 1]);
    } else if (armor == 7) {
      c.set(9 + ox, ty + 3, mix(R.top[1], kEmberGlow, 0.55f));
      c.set(10 + ox, ty + 4, kEmberGlow);
    }
    if (!skirt()) {
      if (L.build == 1) for (int y = ty + 3; y < hip; y++) c.set(6 + ox, y, 0);
      if (L.build == 2) for (int y = ty + 1; y <= ty + 3; y++) c.set(11 + ox, y, R.top[y == ty + 1 ? 2 : 1]);
    }
  }

  void beltRow(int x0, int x1, int y) {
    for (int x = x0; x <= x1; x++) c.set(x, y, R.belt[x < x1 - 1 ? 1 : 0]);
    if (facing == kDown) { c.set(7, y, kBrass[3]); c.set(8, y, kBrass[2]); }
    else if (facing == kSide) c.set(x1 - 1, y, kBrass[3]);
  }
  void chainTexture(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!solid(c, x, y)) continue;
        int k = (x == x0) ? 3 : (x == x1 ? 1 : 2);
        if (((x + y) & 1) == 0) k -= 1;
        c.set(x, y, kChainMail[std::max(0, k)]);
      }
  }

  // long skirt (dress / robe), the hem sways with the walk
  void skirtFront() {
    if (!skirt()) return;
    const Ramp& r = R.top;
    int sway = (P.liftA > 0) ? -1 : (P.liftB > 0 ? 1 : 0);
    bool robe = O == Outfit::Robe;
    int bottom = robe ? kGround : kGround - 1;
    for (int y = hip; y <= bottom; y++) {
      int grow = (y - hip + 1) / 2;
      int x0 = 5 - std::min(2, grow), x1 = 10 + std::min(2, grow);
      if (y >= bottom - 1) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = 2;
        if (x == x0) k = 3;
        else if (x >= x1 - 1) k = 1;
        if ((x == 7 || x == 8) && y > hip + 1 && ((y + x) & 1)) k = 1;
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
      if (robe && facing == kDown) { c.set(7, y, R.trim[3]); c.set(8, y, R.trim[2]); }
    }
    if (robe)
      for (int x = 0; x < 16; x++) if (solid(c, x, bottom)) c.set(x, bottom, R.trim[x < 8 ? 2 : 1]);
    if (!robe) {   // shoes peeking out
      c.set(6, kGround, R.boot[P.liftA ? 1 : 2]); c.set(7, kGround, R.boot[1]);
      c.set(8, kGround, R.boot[1]); c.set(9, kGround, R.boot[P.liftB ? 0 : 1]);
    }
  }
  void skirtSide() {
    if (!skirt()) return;
    const Ramp& r = R.top;
    int sway = P.stepA > 0 ? 1 : (P.stepA < 0 ? -1 : 0);
    bool robe = O == Outfit::Robe;
    int bottom = robe ? kGround : kGround - 1;
    for (int y = hip; y <= bottom; y++) {
      int grow = (y - hip + 1) / 2;
      int x0 = 6 - std::min(2, grow) + P.lean, x1 = 10 + std::min(2, grow) + P.lean;
      if (y >= bottom - 1) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = x == x0 + 1 ? 3 : (x >= x1 - 1 ? 1 : 2);
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
    }
    if (robe) for (int x = 0; x < 16; x++) if (solid(c, x, bottom)) c.set(x, bottom, R.trim[1]);
    if (!robe) { c.set(9 + sway, kGround, R.boot[1]); c.set(10 + sway, kGround, R.boot[0]); c.set(7 - sway, kGround, R.boot[0]); }
  }

  // ---------------------------------------------------------------------- cloaks (M0; replace the legacy cape)
  // The hem sways with the stride; from behind it covers the arms (the hands come out at the sides).
  int cloakBottom() const { return L.cloak == 2 ? hip + 2 : kGround - 1; }
  void cloakBack() {
    const Ramp& K = R.cloak;
    const int st = L.cloak, bottom = cloakBottom();
    int sway = P.liftA > 0 ? -1 : (P.liftB > 0 ? 1 : 0);
    for (int y = ty - 1; y <= bottom; y++) {
      int d = y - ty;
      int off = y > hip + 1 ? sway : 0;
      int x0 = (d < 0 ? 4 : 3 - std::min(1, d / 5)) + off, x1 = (d < 0 ? 11 : 12 + std::min(1, d / 5)) + off;
      for (int x = x0; x <= x1; x++) {
        int fx = x - off, k = 2;
        if (x == x0) k = 3;
        else if (x == x1) k = 1;
        if (d > 2 && (fx == 6 || fx == 10)) k = 1;                     // two deep folds
        if (d > 2 && (fx == 5 || fx == 9) && x != x1) k = 3;           // their lit ridges
        if (d <= 0) k = std::min(4, k + 1);                             // light across the shoulders
        if (y == bottom) k = std::max(0, k - 1);
        if (st == 3 && (x == x0 || x == x1 || y == bottom)) { c.set(x, y, kGold[x == x1 ? 1 : (y == bottom ? 2 : 3)]); continue; }
        if (st == 1 && y == bottom && ((fx + (sway & 1)) % 3 == 1)) continue;   // a worn, uneven hem
        c.set(x, y, K[k]);
      }
    }
  }
  // drawn after the head: the travel cloak's hood lies bunched at the nape; the mantle's fur collar wraps the neck
  void cloakCollarBack() {
    const Ramp& K = R.cloak;
    if (L.cloak == 1) {
      for (int x = 5; x <= 10; x++) c.set(x, ty - 1, K[x < 7 ? 4 : (x > 9 ? 2 : 3)]);
      for (int x = 4; x <= 11; x++) c.set(x, ty, K[x < 6 ? 3 : (x > 9 ? 1 : 2)]);
      for (int x = 5; x <= 10; x++) c.set(x, ty + 1, K[x == 7 || x == 8 ? 0 : 1]);
    } else if (L.cloak == 2) {
      furRow(4, 11, ty - 1, 3);
      furRow(3, 12, ty, 2);
      furRow(3, 12, ty + 1, 1);
    }
  }
  void furRow(int x0, int x1, int y, int k) {
    for (int x = x0; x <= x1; x++) {
      int kk = k + (((x + y) & 1) ? 1 : 0) - (x == x1 ? 1 : 0);
      c.set(x, y, kFur[std::clamp(kk, 0, 4)]);
    }
  }
  // front view, drawn first: the lining behind the legs and the cloak's sides past the arms
  void cloakFront() {
    const Ramp& K = R.cloak;
    const int st = L.cloak, bottom = cloakBottom();
    int sway = P.liftA > 0 ? -1 : (P.liftB > 0 ? 1 : 0);
    if (st != 2)
      for (int y = hip; y <= bottom; y++)
        for (int x = 4; x <= 11; x++) c.set(x + (y > hip + 2 ? sway : 0), y, K[y == bottom ? 0 : (x < 8 ? 1 : 0)]);
    for (int y = ty + 1; y <= bottom; y++) {
      int d = y - ty, off = y > hip + 1 ? sway : 0;
      bool wide = d > 4;
      uint32_t L0 = st == 3 ? kGold[3] : K[3], R0 = st == 3 ? kGold[1] : K[0];
      c.set(2 + off, y, wide ? K[2] : L0);
      if (wide && 1 + off >= 1) c.set(1 + off, y, L0);
      c.set(13 + off, y, wide ? K[1] : R0);
      if (wide && 14 + off <= 14) c.set(14 + off, y, R0);
      if (y == bottom) { c.set(2 + off, y, K[1]); c.set(13 + off, y, K[0]); }
    }
  }
  // front view, drawn last: the cloak over the shoulders and its clasps (or the fur collar)
  void cloakDrapeFront() {
    const Ramp& K = R.cloak;
    bool armUp = P.atk == 1;
    if (L.cloak == 2) {
      if (!armUp) { furRow(2, 5, ty, 3); c.set(3, ty - 1, kFur[4]); c.set(4, ty - 1, kFur[3]); }
      furRow(10, 13, ty, 2); c.set(11, ty - 1, kFur[3]); c.set(12, ty - 1, kFur[2]);
      return;
    }
    if (!armUp) { c.set(3, ty, K[4]); c.set(4, ty, K[3]); c.set(4, ty - 1, K[3]); c.set(2, ty + 1, K[3]); }
    c.set(11, ty, K[2]); c.set(12, ty, K[1]); c.set(11, ty - 1, K[2]); c.set(13, ty + 1, K[1]);
    const Ramp& clasp = L.cloak == 3 ? kGold : kBrass;
    c.set(5, ty, clasp[4]); c.set(10, ty, clasp[2]);
  }
  void cloakSide() {
    const Ramp& K = R.cloak;
    const int st = L.cloak, bottom = cloakBottom(), ox = P.lean;
    bool moving = P.stepA != 0 || P.liftA || P.liftB;
    for (int y = ty - 1; y <= bottom; y++) {
      int d = y - ty;
      int back = d <= 0 ? 0 : (d + 1) / 3 + (moving ? d / 5 : 0);
      if (moving && y >= bottom - 2 && P.stepA > 0) back++;   // the hem flicks back on the stride
      int xb = std::max(1, 5 + ox - back), xf = 7 + ox;
      for (int x = xb; x <= xf; x++) {
        int k = x == xb ? 1 : (x == xb + 1 ? 3 : 2);
        if (d > 3 && x == xb + 2 && x < xf) k = 1;   // a fold
        if (y == bottom) k = std::max(0, k - 1);
        if (st == 3 && (x == xb || y == bottom)) { c.set(x, y, kGold[x == xb ? 2 : 1]); continue; }
        c.set(x, y, K[k]);
      }
    }
  }
  // side view, after the torso: the cloak over the shoulder (or the fur collar)
  void cloakCollarSide() {
    const Ramp& K = R.cloak;
    int ox = P.lean;
    if (L.cloak == 2) { furRow(5 + ox, 9 + ox, ty - 1, 3); furRow(5 + ox, 10 + ox, ty, 2); return; }
    c.set(5 + ox, ty - 1, K[3]); c.set(6 + ox, ty - 1, K[3]);
    c.set(6 + ox, ty, K[4]); c.set(7 + ox, ty, K[3]);
    if (L.cloak == 3) c.set(9 + ox, ty, kGold[4]);
    else c.set(9 + ox, ty, kBrass[3]);
  }

  // ---------------------------------------------------------------------- carried on the back
  // backItem bits: 1 bow + quiver, 2 staff. Back view coordinates; the front view mirrors them and draws first,
  // so only the parts that stick out past the body show.
  void backItems() {
    if (!L.backItem) return;
    const bool mirror = facing == kDown;
    const int ox = facing == kSide ? P.lean : 0;
    auto put = [&](int x, int y, uint32_t col) { c.set(mirror ? 15 - x : x, y, col); };
    // quadratic curve, 1px, optionally with a dark underside one pixel down so it reads on any clothing
    auto stroke = [&](Vec2 a, Vec2 b, Vec2 ctl, const Ramp& r, int k0, int k1, bool under) {
      for (int pass = under ? 0 : 1; pass < 2; pass++)
        for (int i = 0; i <= 24; i++) {
          float t = i / 24.0f, u = 1 - t;
          Vec2 q = a * (u * u) + ctl * (2 * u * t) + b * (t * t);
          if (pass == 0) put((int)std::floor(q.x), (int)std::floor(q.y) + 1, r[0]);
          else put((int)std::floor(q.x), (int)std::floor(q.y), r[i < 3 || i > 21 ? k1 : k0]);
        }
    };
    if (L.backItem & 2) {   // staff: across the back, the gem above the shoulder
      Vec2 a, b;
      if (facing == kSide) { a = V(3.5f + ox, hip + 2.5f); b = V(6.5f + ox, ty - 6.5f); }
      else { a = V(3.5f, hip + 2.5f); b = V(12.5f, ty - 5.5f); }
      stroke(a, b, (a + b) * 0.5f, kWood, 3, 1, true);
      Ramp gr = ramp(mix(rgba(110, 196, 255), L.weaponColor, 0.2f));
      int gx = (int)std::floor(b.x), gy = (int)std::floor(b.y);
      put(gx, gy - 1, gr[4]); put(gx + 1, gy - 1, gr[2]); put(gx, gy - 2, gr[3]); put(gx + 1, gy, kGold[2]);
    }
    if (L.backItem & 1) {   // bow and quiver
      if (facing == kSide) {
        for (int y = ty - 2; y <= ty + 4; y++) { put(4 + ox, y, kLeather[y == ty - 2 ? 3 : 2]); put(5 + ox, y, kLeather[1]); }
        put(4 + ox, ty - 3, kRed[3]); put(5 + ox, ty - 3, kCloth[4]); put(4 + ox, ty - 4, kWood[3]);
        Vec2 a = V(5.5f + ox, ty - 3.5f), b = V(5.5f + ox, hip + 2.5f);
        stroke(a, b, V(1.0f + ox, (a.y + b.y) * 0.5f), kWood, 3, 1, false);
      } else {
        for (int y = ty - 1; y <= ty + 4; y++) { put(9, y, kLeather[y == ty - 1 ? 4 : 3]); put(10, y, kLeather[y == ty + 2 ? 4 : 1]); }
        put(9, ty - 2, kWood[3]); put(10, ty - 2, kWood[2]);
        put(9, ty - 3, kRed[3]); put(10, ty - 3, kCloth[4]); put(11, ty - 3, kRed[2]);
        Vec2 a = V(3.5f, ty - 3.5f), b = V(11.5f, hip + 1.5f);
        for (int i = 0; i <= 16; i++) {   // the string, under the stave
          Vec2 q = a + (b - a) * (i / 16.0f);
          put((int)std::floor(q.x), (int)std::floor(q.y), kCloth[2]);
        }
        stroke(a, b, (a + b) * 0.5f + V(-2.6f, 2.6f), kWood, 4, 2, true);
      }
    }
  }

  // ---------------------------------------------------------------------- jewellery
  void amuletFront() {
    if (!L.amulet) return;
    c.set(6, ty, kGold[2]); c.set(9, ty, kGold[1]);
    c.set(7, ty + 1, idle() ? kWhite : kCrystal[3]); c.set(8, ty + 1, kGold[2]);
  }
  void amuletSide() {
    if (!L.amulet) return;
    c.set(10 + P.lean, ty + 1, idle() ? kWhite : kCrystal[3]);
    c.set(9 + P.lean, ty, kGold[2]);
  }

  // ---------------------------------------------------------------------- capes
  void capeBack() {
    if (!L.cape || L.cloak) return;
    int sway = P.liftA > 0 ? -1 : (P.liftB > 0 ? 1 : 0);
    for (int y = ty; y <= kGround - 2; y++) {
      int grow = (y - ty) / 3;
      int x0 = 4 - std::min(1, grow), x1 = 11 + std::min(1, grow);
      if (y > hip) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = 2;
        if (x == x0) k = 3;
        if (x >= x1 - 1) k = 1;
        if ((x == 6 || x == 9) && y > ty + 2) k = 1;
        if (y == kGround - 2) k = std::max(0, k - 1);
        c.set(x, y, R.cape[k]);
      }
    }
    for (int x = 4; x <= 11; x++) c.set(x, ty, R.cape[3]);
  }
  void capeFrontSliver() {
    if (!L.cape || L.cloak) return;
    for (int y = ty + 1; y <= kGround - 3; y++) {
      c.set(2, y, R.cape[1]); c.set(13, y, R.cape[0]);
      if (y > ty + 3) { c.set(3, y, R.cape[2]); c.set(12, y, R.cape[1]); }
    }
    for (int x = 4; x <= 11; x++) for (int y = hip; y <= kGround - 3; y++) c.set(x, y, R.cape[0]);
  }
  void capeSide() {
    if (!L.cape || L.cloak) return;
    int flow = (P.stepA != 0 || P.liftA || P.liftB) ? 1 : 0;
    int ox = P.lean;
    for (int y = ty; y <= kGround - 2; y++) {
      int back = (y - ty) / 3 + flow * ((y - ty) / 4);
      int x0 = 5 - back + ox, x1 = 6 + ox;
      for (int x = x0; x <= x1; x++) c.set(x, y, R.cape[x == x0 ? 1 : 2]);
    }
  }

  // ---------------------------------------------------------------------- arms
  const Ramp& handRamp() const { return L.gloves ? R.glove : (metalBody() ? R.sleeve : R.skin); }

  // front / back arm hanging at column x0 (2 wide)
  void armFront(int x0, int swing, bool lit) {
    const Ramp& s = R.sleeve;
    int handY = ty + 5 + swing;
    int kA = lit ? 3 : 2, kB = lit ? 2 : 1;
    for (int y = ty + 1; y < handY; y++) { c.set(x0, y, s[kA]); c.set(x0 + 1, y, s[kB]); }
    c.set(lit ? x0 + 1 : x0, ty, s[kA]);
    if (metalBody()) pauldron(lit ? x0 - 1 : x0, lit);
    if (O == Outfit::Leather || O == Outfit::Robe) {
      const Ramp& b = O == Outfit::Leather ? kLeather : R.trim;
      c.set(x0, handY - 1, b[kA]); c.set(x0 + 1, handY - 1, b[kB]);
    }
    if (L.gloves) {   // the glove's cuff; gauntlets (steel and up) flare out past the sleeve
      c.set(x0, handY - 1, R.glove[kA + 1]); c.set(x0 + 1, handY - 1, R.glove[kB]);
      if (L.gloves >= 3) c.set(lit ? x0 - 1 : x0 + 2, handY - 1, R.glove[lit ? 3 : 1]);
    }
    buildShoulders(lit, x0);
    handPx(x0, handY, lit);
  }
  void pauldron(int px, bool lit) {
    const Ramp& s = R.sleeve;
    c.set(px, ty, s[4]); c.set(px + 1, ty, s[3]); c.set(px + 2, ty, s[2]);
    c.set(px, ty + 1, s[2]); c.set(px + 1, ty + 1, s[2]); c.set(px + 2, ty + 1, s[1]);
    if (O == Outfit::Ebony) c.set(lit ? px : px + 2, ty - 1, s[3]);
  }
  void handPx(int x0, int y, bool lit) {
    const Ramp& h = handRamp();
    c.set(x0, y, h[lit ? 3 : 2]); c.set(x0 + 1, y, h[lit ? 2 : 1]);
    if (lit && L.ring && idle()) c.set(x0 + 1, y, kGold[4]);   // the ring glints at rest
  }
  // arm as a 2px stroke from the shoulder to the hand (any view)
  void armLine(int x0, int y0, int x1, int y1, int bias = 0) {
    const Ramp& s = R.sleeve;
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= n; i++) {
      float t = n ? (float)i / n : 0;
      int x = x0 + (int)std::lround((x1 - x0) * t), y = y0 + (int)std::lround((y1 - y0) * t);
      c.set(x - 1, y, s[3 + bias]); c.set(x, y, s[2 + bias]);
    }
  }
  void handAt(int x, int y, int bias = 0) {
    const Ramp& h = handRamp();
    c.set(x, y, h[3 + bias]); c.set(x - 1, y, h[2 + bias]);
  }
  void armSide(bool nearArm, int swing) {
    int bias = nearArm ? 0 : -1;
    int sx = 8 + P.lean;
    armLine(sx, ty + 1, sx + swing, ty + 4, bias);
    if (nearArm && metalBody()) pauldron(sx - 2, true);
    if (L.gloves) { c.set(sx + swing - 1, ty + 4, R.glove[4 + bias]); c.set(sx + swing, ty + 4, R.glove[2 + bias]); }
    handAt(sx + swing, ty + 5, bias);
    if (nearArm && L.ring && idle()) c.set(sx + swing, ty + 5, kGold[4]);
  }

  // ---------------------------------------------------------------------- weapons
  // (hx,hy2) = hand pixel, (dx,dy) = 8-way direction the weapon points, side flips the perpendicular.
  void weapon(int hx, int hy2, int dx, int dy, int side) {
    int w = L.weapon;
    if (w == 0) return;
    const Ramp& m = R.metal;
    int px = -dy, py = dx;
    if (side < 0) { px = -px; py = -py; }
    bool diag = dx != 0 && dy != 0;
    auto put = [&](int t, int o, uint32_t col) { c.set(hx + dx * t + px * o, hy2 + dy * t + py * o, col); };
    // heads on a diagonal: also fill the neighbouring pixel so the shape stays solid (no checkerboard)
    auto head = [&](int t, int o, uint32_t col) {
      put(t, o, col);
      if (diag && o != 0) c.set(hx + dx * t + px * o, hy2 + dy * t + py * o - py * (o > 0 ? 1 : -1), col);
    };
    switch (w) {
      case 1:   // sword
        put(-1, 0, kBrass[3]);
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]); put(1, -1, kBrass[1]);
        for (int t = 2; t <= 7; t++) {
          put(t, 0, t == 7 ? m[4] : m[3]);
          if (!diag && t < 7) put(t, 1, m[1]);
        }
        if (diag) for (int t = 2; t <= 6; t++) c.set(hx + dx * t + (dx > 0 ? -1 : 1) * (dy > 0 ? 0 : 0), hy2 + dy * t + 1, m[1]);
        break;
      case 5:   // dagger
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]);
        for (int t = 2; t <= 4; t++) put(t, 0, t == 4 ? m[4] : m[3]);
        if (!diag) { put(2, 1, m[1]); put(3, 1, m[1]); }
        break;
      case 2:   // axe
        for (int t = -1; t <= 6; t++) put(t, 0, kWood[t < 3 ? 2 : 1]);
        head(4, 1, m[1]); head(5, 1, m[1]); head(6, 1, m[0]);
        head(3, 2, m[2]); head(4, 2, m[2]); head(5, 2, m[2]); head(6, 2, m[1]); head(7, 2, m[1]);
        head(3, 3, m[4]); head(4, 3, m[3]); head(5, 3, m[3]); head(6, 3, m[3]); head(7, 3, m[2]);
        put(5, -1, m[1]);
        break;
      case 6:   // hammer / pick
        for (int t = -1; t <= 5; t++) put(t, 0, kWood[t < 3 ? 2 : 1]);
        for (int o = -2; o <= 2; o++) { head(6, o, m[o < 0 ? 2 : 1]); head(7, o, m[o < 0 ? 1 : 0]); }
        head(6, -2, m[3]); head(6, -1, m[3]);
        break;
      case 4: {  // staff with a glowing gem
        for (int t = -7; t <= 6; t++) put(t, 0, kWood[(t & 3) == 0 ? 1 : 2]);
        put(6, 1, kWood[1]); put(6, -1, kWood[1]);
        Ramp gr = ramp(mix(rgba(110, 196, 255), L.weaponColor, 0.2f));
        put(7, 0, gr[3]); put(8, 0, gr[4]); put(7, 1, gr[2]); put(8, 1, gr[2]); put(7, -1, gr[3]); put(8, -1, gr[3]); put(9, 0, gr[2]);
        if (P.atk == 2) { put(10, 0, kWhite); put(8, 2, gr[4]); put(8, -2, gr[4]); }
        break;
      }
      case 3: {  // bow: limbs perpendicular to the aim, string behind
        bool drawn = P.atk == 1;
        Vec2 a = norm(Vec2{(float)dx, (float)dy}), pp = norm(Vec2{(float)-dy, (float)dx});
        Vec2 h{(float)hx, (float)hy2}, e0, e1;
        for (int s = -5; s <= 5; s++) {
          float bulge = 1.6f - s * s / 14.0f;
          Vec2 q = h + pp * (float)s + a * bulge;
          c.set((int)std::lround(q.x), (int)std::lround(q.y), kWood[std::abs(s) > 3 ? 1 : (s < 0 ? 3 : 2)]);
          if (s == -5) e0 = q;
          if (s == 5) e1 = q;
        }
        Vec2 back = h - a * (drawn ? 3.0f : 0.2f);
        int bx = (int)std::lround(back.x), by = (int)std::lround(back.y);
        line(c, (int)std::lround(e0.x), (int)std::lround(e0.y), bx, by, kCloth[3]);
        line(c, bx, by, (int)std::lround(e1.x), (int)std::lround(e1.y), kCloth[3]);
        if (drawn)
          for (int t = -3; t <= 3; t++) {
            Vec2 q = h + a * (float)t;
            c.set((int)std::lround(q.x), (int)std::lround(q.y), t == 3 ? m[4] : kWood[3]);
          }
        break;
      }
      default: break;
    }
  }

  bool hasShield() const { return L.shield || L.shieldStyle; }
  // M0 shield shapes (round, heater, kite, tower): r rim, f painted field (tabardColor), d gilt device, b boss, w planks
  void shieldShaped(int x0, int y0, bool edgeOn) {
    static const char* const kRound[] = {"..rrr..", ".rfffr.", "rfffwwr", "rffbwwr", "rfwwwwr", ".rwwwr.", "..rrr..", nullptr};
    static const char* const kHeater[] = {"rrrrrr", "rffffr", "rdffdr", "rfddfr", "rffffr", ".rffr.", ".rffr.", "..rr..", nullptr};
    static const char* const kKite[] = {".rrrr.", "rffffr", "rfddfr", "rddddr", "rfddfr", ".rffr.", ".rffr.", "..rr..", "..rr..", nullptr};
    static const char* const kTower[] = {"rrrrrrr", "rfffffr", "rffdffr", "rfdddfr", "rffdffr", "rffdffr", "rfffffr", "rfffffr", "rfffffr", "rrrrrrr", nullptr};
    static const char* const* const shapes[4] = {kRound, kHeater, kKite, kTower};
    int st = std::clamp((int)L.shieldStyle, 1, 4) - 1;
    const char* const* m = shapes[st];
    int h = 0, w = 0;
    while (m[h]) { w = std::max(w, (int)std::strlen(m[h])); h++; }
    Ramp M = L.trimColor ? ramp(L.trimColor, 1.1f) : R.metal;
    if (edgeOn) {   // seen edge-on from the side: rim + the field's edge, as tall as the shape
      for (int y = 0; y < h; y++) {
        int n = 0;
        for (const char* q = m[y]; *q; q++) n += *q != '.';
        uint32_t back = st == 0 ? kWood[y < h / 2 ? 2 : 1] : R.trim[y < h / 2 ? 1 : 0];
        c.set(x0, y0 + y, M[y == 0 ? 3 : (y == h - 1 ? 1 : 2)]);
        if (n > 2) c.set(x0 + 1, y0 + y, back);
        if (st == 3 && n > 2) c.set(x0 - 1, y0 + y, R.trim[2]);   // the tower's curved face shows
      }
      return;
    }
    if (facing == kDown) x0 = HUMAN_W - 1 - w;   // held on the off arm, as far out as the cell allows
    else if (w >= 7) x0--;
    for (int y = 0; y < h; y++)
      for (int x = 0; m[y][x]; x++) {
        char ch = m[y][x];
        if (ch == '.') continue;
        bool left = x < w / 2, low = y >= h - 3;
        uint32_t col = 0;
        if (ch == 'r') {
          bool lit = y == 0 || x == 0 || m[y][x - 1] == '.' || (y > 0 && m[y - 1][x] == '.');
          bool dark = x == w - 1 || m[y][x + 1] == '\0' || m[y][x + 1] == '.' || y == h - 1;
          col = M[lit && !dark ? (left && y < h / 2 ? 4 : 3) : (dark ? (low ? 0 : 1) : 2)];
        } else if (ch == 'f' || ch == 'w') {
          const Ramp& F = ch == 'w' ? kWood : R.trim;
          int k = left ? 3 : 2;
          if (x >= w - 2) k = 1;
          if (ch == 'w' && (x & 1)) k--;
          if (low) k--;
          if (x == 1 && y == 1) k = 4;
          col = F[k];
        } else if (ch == 'd') {
          col = kGold[low ? 1 : (left ? 4 : 2)];
        } else if (ch == 'b') {
          col = M[left && y < h / 2 ? 4 : 2];
        }
        c.set(x0 + x, y0 + y, col);
      }
  }
  void shield(int x0, int y0, bool edgeOn) {
    if (!hasShield()) return;
    if (L.shieldStyle) { shieldShaped(x0, y0, edgeOn); return; }
    const Ramp& rim = R.metal;
    if (edgeOn) {
      for (int y = 0; y < 7; y++) { c.set(x0, y0 + y, rim[2]); c.set(x0 + 1, y0 + y, kWood[y < 3 ? 2 : 1]); }
      c.set(x0, y0, rim[3]); c.set(x0 + 1, y0 + 6, rim[1]);
      return;
    }
    static const char* sh[7] = {".2332.", "234432", "234432", "233321", "233221", "122210", ".1110."};
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 6; x++) {
        char ch = sh[y][x];
        if (ch == '.') continue;
        int k = ch - '0';
        bool edge = y == 0 || y == 6 || x == 0 || x == 5 || sh[y][x - 1] == '.' || sh[y][x + 1] == '.';
        c.set(x0 + x, y0 + y, edge ? rim[std::max(0, k - 1)] : (x < 3 ? R.trim[k] : kWood[k]));
      }
    c.set(x0 + 2, y0 + 3, rim[4]); c.set(x0 + 3, y0 + 3, rim[2]);
  }

  // ---------------------------------------------------------------------- head
  Map hairMap() const {
    int f = facing;
    switch (L.hair) {
      case Hair::Short: return f == kDown ? kHairShortD : (f == kUp ? kHairShortU : kHairShortS);
      case Hair::Long: return f == kDown ? kHairLongD : (f == kUp ? kHairLongU : kHairLongS);
      case Hair::Ponytail: return f == kDown ? kHairPonyD : (f == kUp ? kHairPonyU : kHairPonyS);
      case Hair::Mohawk: return f == kDown ? kHairMohawkD : (f == kUp ? kHairMohawkU : kHairMohawkS);
      case Hair::Braids: return f == kDown ? kHairBraidD : (f == kUp ? kHairBraidU : kHairBraidS);
      case Hair::Bun: return f == kDown ? kHairBunD : (f == kUp ? kHairBunU : kHairBunS);
      case Hair::Curls: return f == kDown ? kHairCurlsD : (f == kUp ? kHairCurlsU : kHairCurlsS);
      default: return nullptr;
    }
  }
  Map helmetMap(uint32_t& accent, const Ramp*& rp, uint32_t& visor) const {
    int f = facing;
    visor = 0; accent = 0;
    switch (L.helmStyle) {   // M0 helmet bands
      case 1: rp = &kCapLeather; return f == kDown ? kCapD : (f == kUp ? kCapU : kCapS);
      case 2: rp = &kIron; return f == kDown ? kIronHelmD : (f == kUp ? kIronHelmU : kIronHelmS);
      case 3: rp = &kSteelArmor; visor = kInk; return f == kDown ? kGreatHelmD : (f == kUp ? kGreatHelmU : kGreatHelmS);
      case 4: rp = &kElvenArmor; accent = bandAccent(4); return f == kDown ? kGildHelmD : (f == kUp ? kGildHelmU : kGildHelmS);
      case 5: rp = &kJadeArmor; accent = bandAccent(5); return f == kDown ? kJadeHelmD : (f == kUp ? kJadeHelmU : kJadeHelmS);
      case 6: rp = &kEbonyArmor; visor = rgba(20, 12, 24); accent = rgba(240, 70, 120); return f == kDown ? kObsHelmD : (f == kUp ? kObsHelmU : kObsHelmS);
      case 7: rp = &kEmberArmor; visor = rgba(24, 8, 14); accent = kEmberGlow; return f == kDown ? kEmberHelmD : (f == kUp ? kEmberHelmU : kEmberHelmS);
      default: break;
    }
    switch (O) {
      case Outfit::Chain: rp = &kIron; return f == kDown ? kIronHelmD : (f == kUp ? kIronHelmU : kIronHelmS);
      case Outfit::Guard: rp = &kIron; return f == kDown ? kKettleD : (f == kUp ? kKettleU : kKettleS);
      case Outfit::Plate: rp = &kSteelArmor; visor = kInk; return f == kDown ? kGreatHelmD : (f == kUp ? kGreatHelmU : kGreatHelmS);
      case Outfit::Elven: rp = &kElvenArmor; accent = rgba(96, 210, 120); return f == kDown ? kElvenHelmD : (f == kUp ? kElvenHelmU : kElvenHelmS);
      case Outfit::Ebony: rp = &kEbonyArmor; visor = rgba(20, 12, 24); accent = rgba(240, 70, 120); return f == kDown ? kEbonyHelmD : (f == kUp ? kEbonyHelmU : kEbonyHelmS);
      default: rp = &kLeather; return f == kDown ? kCapD : (f == kUp ? kCapU : kCapS);
    }
  }

  void head() {
    int ox = 2 + (facing == kSide ? P.lean : 0), oy = hy - 3;
    Map skin = facing == kDown ? kSkinDown : (facing == kUp ? kSkinUp : kSkinSide);
    drawMap(c, skin, ox, oy, R.skin, L.eyeColor ? opaque(L.eyeColor) : kEye);
    if (P.hurt && facing != kUp) {   // squeezed eyes + open mouth
      uint32_t mouth = rgba(120, 40, 56);
      if (facing == kDown) {
        c.set(6, hy + 4, R.skin[2]); c.set(9, hy + 4, R.skin[2]);
        c.set(5, hy + 4, kEye); c.set(6, hy + 5, kEye); c.set(9, hy + 5, kEye); c.set(10, hy + 4, kEye);
        c.set(7, hy + 6, mouth); c.set(8, hy + 6, mouth);
      } else {
        c.set(10 + P.lean, hy + 4, R.skin[2]); c.set(11 + P.lean, hy + 4, kEye); c.set(10 + P.lean, hy + 5, kEye);
        c.set(11 + P.lean, hy + 6, mouth);
      }
    }
    const bool helm = L.helmet || L.helmStyle;
    bool covered = helm || L.hood;
    if (L.hair == Hair::Bald && !covered) { c.set(ox + 3, oy + 4, R.skin[4]); c.set(ox + 4, oy + 4, R.skin[4]); }
    if (L.hair == Hair::Mohawk && !covered && facing != kSide)
      for (int y = hy; y < hy + 3; y++) for (int x = 4; x <= 11; x++)
        if (((x + y) & 1) && solid(c, x, y) && (x < 6 || x > 9)) c.set(x, y, mix(c.get(x, y), R.hair[1], 0.4f));
    if (L.beard) {
      if (facing == kDown) drawMap(c, kBeardD, ox, oy, R.hair);
      else if (facing == kSide) drawMap(c, kBeardS, ox, oy, R.hair);
    }
    if (helm) {
      uint32_t accent, visor;
      const Ramp* rp;
      Map hm = helmetMap(accent, rp, visor);
      if (L.hair == Hair::Long || L.hair == Hair::Braids || L.hair == Hair::Ponytail || L.hair == Hair::Curls) {   // long hair flows out below
        Canvas tmp(c.w, c.h);
        drawMap(tmp, hairMap(), ox, oy, R.hair);
        for (int y = hy + 4; y < c.h; y++)
          for (int x = 0; x < c.w; x++)
            if (solid(tmp, x, y) && !(facing == kDown && x >= 5 && x <= 10)) c.set(x, y, tmp.get(x, y));
      }
      drawMap(c, hm, ox, oy, *rp, kEye, accent, visor);
    } else if (L.hood) {
      drawMap(c, facing == kDown ? kHoodD : (facing == kUp ? kHoodU : kHoodS), ox, oy, R.hood);
      if (facing == kDown) for (int x = 5; x <= 10; x++) c.set(x, hy + 3, R.skin[0]);
    } else if (Map hm = hairMap()) {
      drawMap(c, hm, ox, oy, R.hair);
    }
  }

  // ---------------------------------------------------------------------- composition
  void paint() {
    if (facing == kDown) paintDown();
    else if (facing == kUp) paintUp();
    else paintSide();
  }

  void paintDown() {
    backItems();
    if (L.cloak) cloakFront();
    capeFrontSliver();
    legsFront();
    torsoFront(false);
    buildFront();
    skirtFront();
    armFront(11, P.swingB, false);   // off hand (screen right)
    head();
    amuletFront();
    if (L.cloak) cloakDrapeFront();
    bool bow = L.weapon == 3;
    if (P.atk == 1) {   // weapon raised high beside the head
      int hx = 3, hy2 = ty - 3;
      armLine(4, ty + 1, 4, hy2 + 1);
      if (bow) weapon(7, ty + 3, 0, 1, 1);
      else weapon(hx, hy2, 0, -1, -1);
      handPx(3, hy2, true);
    } else if (P.atk == 2) {   // swung down across the body
      int hx = 6, hy2 = ty + 4;
      armLine(5, ty + 1, 6, ty + 3);
      if (bow) weapon(7, ty + 4, 0, 1, 1);
      else weapon(hx, hy2, 1, 1, 1);
      handPx(5, hy2, true);
    } else {
      armFront(3, P.swingA, true);
      int hy2 = ty + 5 + P.swingA;
      if (bow) weapon(3, hy2, -1, 0, 1);
      else if (isStaff()) weapon(3, hy2, 0, -1, -1);
      else weapon(3, hy2, 0, 1, heavyHead() ? 1 : -1);
      handPx(3, hy2, true);
    }
    shield(10, ty + 1, false);
  }

  void paintUp() {
    legsFront();
    torsoFront(true);
    skirtFront();
    int hx = 11, hy2 = ty + 5 + P.swingB, dx = 0, dy = isStaff() ? -1 : 1;
    if (L.weapon == 3) { dx = 1; dy = 0; }
    if (P.atk == 1) { hx = 12; hy2 = ty - 2; dx = 0; dy = -1; }
    if (P.atk == 2) { hx = 10; hy2 = ty - 3; dx = -1; dy = -1; if (L.weapon == 3) { hx = 9; dx = 0; } }
    if (P.atk) weapon(hx, hy2, dx, dy, 1);   // raised weapon is beyond the head
    armFront(3, P.swingA, true);
    if (L.cloak) {   // the cloak falls over both arms; the hands come out at its sides
      if (!P.atk) armFront(11, P.swingB, false);
      cloakBack();
      handPx(3, ty + 5 + P.swingA, true);
      if (!P.atk) handPx(11, hy2, false);
    }
    capeBack();
    backItems();
    if (hasShield()) shield(5, ty + 1, false);
    head();
    if (L.cloak) cloakCollarBack();
    if (P.atk) {
      armLine(12, ty + 1, hx + 1, hy2 + 1, -1);
      c.set(hx, hy2, handRamp()[2]); c.set(hx + 1, hy2, handRamp()[1]);
    } else if (L.cloak) {
      weapon(12, hy2, dx, dy, heavyHead() ? -1 : 1);
      handPx(11, hy2, false);
    } else {
      armFront(11, P.swingB, false);
      weapon(12, hy2, dx, dy, heavyHead() ? -1 : 1);
      handPx(11, hy2, false);
    }
  }

  void paintSide() {
    backItems();
    if (L.cloak) cloakSide();
    capeSide();
    armSide(false, P.atk ? 1 : P.swingB);   // far arm
    legsSide();
    torsoSide();
    skirtSide();
    if (L.cloak) cloakCollarSide();
    if (hasShield()) shield(11 + P.lean, ty + 1, true);
    head();
    amuletSide();
    int sx = 8 + P.lean;
    if (P.atk == 1) {
      if (L.weapon == 3) {   // bow drawn: bow arm forward, string hand back at the chest
        weapon(12 + P.lean, ty + 3, 1, 0, 1);
        armLine(sx, ty + 1, 11 + P.lean, ty + 3);
        handAt(9 + P.lean, ty + 3);
      } else {
        int hx = 5 + P.lean, hy2 = ty - 1;
        if (heavyHead()) weapon(hx, hy2, 0, -1, -1);   // axe / hammer hoisted behind the head
        else weapon(hx, hy2, -1, -1, 1);
        armLine(sx, ty + 1, hx + 1, hy2 + 1);
        handAt(hx, hy2);
      }
    } else if (P.atk == 2) {
      int hx = 12 + P.lean, hy2 = ty + 3;
      if (L.weapon == 3) weapon(hx, hy2, 1, 0, 1);
      else if (L.weapon == 5 || L.weapon == 4) weapon(hx, hy2, 1, 0, -1);
      else if (heavyHead()) weapon(hx, hy2, 0, 1, -1);   // chop finished low in front
      else weapon(hx, hy2, 1, 1, -1);
      armLine(sx, ty + 1, hx - 1, hy2);
      handAt(hx, hy2);
    } else {
      int hx = sx + P.swingA, hy2 = ty + 5;
      armSide(true, P.swingA);
      if (L.weapon == 3) weapon(hx, hy2, 1, 0, 1);
      else if (isStaff()) weapon(hx, hy2, 0, -1, -1);
      else if (heavyHead()) weapon(hx, hy2, 0, 1, 1);
      else weapon(hx, hy2, 1, -1, 1);   // blade raised in a ready stance
      handAt(hx, hy2);
    }
  }
};

}  // namespace

Canvas humanSheet(const HumanLook& look) {
  Canvas sheet(HUMAN_W * HUMAN_FRAMES, HUMAN_H * 3);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < HUMAN_FRAMES; f++) {
      Canvas cell(HUMAN_W, HUMAN_H);
      HumanPainter hp(cell, look, row, humanPose(row, f));
      hp.paint();
      outline(cell);
      place(sheet, cell, f, row);
    }
  return sheet;
}

Canvas humanCellRaw(const HumanLook& look, int row, int frame) {
  Canvas cell(HUMAN_W, HUMAN_H);
  HumanPainter hp(cell, look, row, humanPose(row, frame));
  hp.paint();
  return cell;
}

uint64_t HumanLook::key() const {
  uint64_t k = 1469598103934665603ull;
  auto mx = [&](uint64_t v) { k ^= v; k *= 1099511628211ull; };
  mx(skin); mx(hairColor); mx(topColor); mx(bottomColor); mx(tabardColor); mx(weaponColor);
  mx((uint64_t)hair | (uint64_t)outfit << 8 | (uint64_t)beard << 16 | (uint64_t)helmet << 17 | (uint64_t)hood << 18 |
     (uint64_t)cape << 19 | (uint64_t)shield << 20 | (uint64_t)weapon << 24);
  mx(eyeColor); mx(cloakColor); mx(trimColor);
  mx((uint64_t)build | (uint64_t)helmStyle << 8 | (uint64_t)armorStyle << 16 | (uint64_t)gloves << 24 | (uint64_t)boots << 32 |
     (uint64_t)cloak << 40 | (uint64_t)shieldStyle << 48 | (uint64_t)backItem << 56);
  mx((uint64_t)amulet | (uint64_t)ring << 1);
  return k;
}

Canvas humanFigureStill(const HumanLook& look, int facing) {
  Canvas fig(HUMAN_W, HUMAN_H);
  HumanPainter hp(fig, look, facing, Pose{});
  hp.paint();
  return fig;
}

// ------------------------------------------------------------------ humanoid monsters via the human rig
namespace {

// skeleton: draw the posed figure in bones (side view), reusing the human pose numbers
void skeletonFigure(Canvas& c, const Pose& P, int frame) {
  const Ramp& Bn = kBone;
  int hy = 3 + P.bob, ty = hy + 8, hip = ty + 6, ox = P.lean;
  const int ground = 22;
  // far leg + far arm (darker)
  auto leg = [&](int step, int lift, int bias) {
    int foot = ground - lift;
    Vec2 h{7.5f + ox, (float)hip + 0.5f}, f{7.5f + step + 0.0f, (float)foot + 0.5f};
    Vec2 k = (h + f) * 0.5f + Vec2{1.0f, 0};
    capsule(c, h, k, 0.7f, 0.6f, Bn, bias);
    capsule(c, k, f, 0.6f, 0.5f, Bn, bias);
    c.set((int)k.x, (int)k.y, Bn[3 + bias]);
    c.set((int)f.x, foot, Bn[2 + bias]); c.set((int)f.x + 1, foot, Bn[1 + bias]); c.set((int)f.x + 2, foot, Bn[1 + bias]);
  };
  auto arm = [&](Vec2 hand, int bias) {
    Vec2 s{8.0f + ox, (float)ty + 1.5f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-0.8f, 0.6f};
    capsule(c, s, e, 0.6f, 0.55f, Bn, bias);
    capsule(c, e, hand, 0.55f, 0.5f, Bn, bias);
    c.set((int)hand.x, (int)hand.y, Bn[3 + bias]);
  };
  arm(V(8.0f + ox + (P.atk ? 1 : P.swingB), (float)ty + 5.5f), -1);
  leg(P.stepB, P.liftB, -1);
  // spine, ribs, pelvis
  for (int y = ty; y <= hip; y++) c.set(7 + ox, y, Bn[1]);
  for (int r = 0; r < 4; r++) {
    int y = ty + 1 + r;
    int w = r < 3 ? 4 : 3;
    for (int x = 0; x < w; x++) c.set(8 + ox + x - (r == 0 ? 1 : 0), y, (r & 1) ? Bn[2] : Bn[3]);
    c.set(8 + ox + w - 1, y, Bn[1]);
  }
  for (int x = 6; x <= 9; x++) c.set(x + ox, hip, Bn[x < 8 ? 3 : 2]);
  leg(P.stepA, P.liftA, 0);
  // skull
  static const char* skull[8] = {
    "..2333..",
    ".233443.",
    "2333443.",
    "233300 2",
    "23330023",
    ".2233322",
    "..21212.",
    "..1222..",
  };
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      char ch = skull[y][x];
      if (ch >= '0' && ch <= '4') c.set(4 + ox + x, hy + y, ch == '0' ? kInk : Bn[ch - '0']);
    }
  if (frame != 6) c.set(9 + ox, hy + 4, rgba(255, 90, 60));   // ember in the eye socket
  (void)frame;
}


}  // namespace

// The HumanPainter gets a few species hooks via these helpers (kept outside the class for clarity).
Canvas humanoidCell(Species sp, int frame) {
  // the rig paints into 16x24; the monster cell is 24x24 with the figure centred
  HumanLook L;
  Pose P = frame == 7 ? Pose{} : humanPose(kSide, frame + 1);   // monster frames 0-6 = human frames 1-7
  Canvas fig(HUMAN_W, HUMAN_H);
  switch (sp) {
    case kSkeletonSp: {
      L.weapon = 1; L.weaponColor = rgba(150, 128, 104); L.shield = true; L.tabardColor = rgba(110, 80, 60);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.sleeve = kBone; hp.R.skin = kBone;
      skeletonFigure(fig, P, frame);
      if (L.shield) hp.shield(11 + P.lean, hp.ty + 1, true);
      // weapon hand
      int sx = 8 + P.lean;
      if (P.atk == 1) { hp.weapon(5 + P.lean, hp.ty - 1, -1, -1, 1); capsule(fig, V(sx, hp.ty + 1.5f), V(6.0f + P.lean, hp.ty + 0.0f), 0.6f, 0.5f, kBone); }
      else if (P.atk == 2) { hp.weapon(12 + P.lean, hp.ty + 3, 1, 1, -1); capsule(fig, V(sx, hp.ty + 1.5f), V(11.5f + P.lean, hp.ty + 3.5f), 0.6f, 0.5f, kBone); }
      else { hp.weapon(sx + P.swingA, hp.ty + 5, 1, -1, 1); capsule(fig, V(sx, hp.ty + 1.5f), V(sx + P.swingA + 0.5f, hp.ty + 5.5f), 0.6f, 0.5f, kBone); }
      break;
    }
    case kDraugrSp: {
      L.skin = rgba(120, 132, 118); L.hairColor = rgba(170, 170, 160); L.hair = Hair::Long; L.beard = true;
      L.outfit = Outfit::Chain; L.helmet = true; L.bottomColor = rgba(60, 54, 50); L.topColor = rgba(70, 70, 64);
      L.weapon = 2; L.weaponColor = rgba(118, 140, 120);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.top = ramp5(rgba(36, 36, 44), rgba(58, 58, 64), rgba(84, 82, 82), rgba(112, 108, 100), rgba(140, 136, 124));
      hp.R.sleeve = hp.R.top;
      hp.paint();
      // horns on the helm + glowing eyes
      int hy = hp.hy;
      fig.set(4 + P.lean, hy - 1, kBone[3]); fig.set(3 + P.lean, hy - 2, kBone[4]); fig.set(4 + P.lean, hy - 2, kBone[2]);
      fig.set(9 + P.lean, hy - 2, kBone[3]); fig.set(10 + P.lean, hy - 3, kBone[4]);
      fig.set(10 + P.lean, hy + 4, rgba(140, 230, 255)); fig.set(10 + P.lean, hy + 5, rgba(70, 150, 200));
      break;
    }
    case kGoblinSp: {
      L.skin = rgba(124, 160, 72); L.hair = Hair::Mohawk; L.hairColor = rgba(60, 40, 40);
      L.outfit = Outfit::Rags; L.weapon = 5; L.weaponColor = rgba(170, 160, 150);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.top = kLeather; hp.R.leg = kLeather;
      hp.paint();
      int hy = hp.hy, ox = P.lean;
      // big ears swept back, hooked nose, yellow eyes
      Ramp sk = ramp(L.skin, 0.8f);
      fig.set(5 + ox, hy + 3, sk[3]); fig.set(4 + ox, hy + 2, sk[3]); fig.set(3 + ox, hy + 1, sk[2]); fig.set(6 + ox, hy + 4, sk[2]);
      fig.set(5 + ox, hy + 4, sk[1]); fig.set(4 + ox, hy + 3, sk[1]);
      fig.set(12 + ox, hy + 5, sk[2]); fig.set(12 + ox, hy + 6, sk[1]);
      fig.set(10 + ox, hy + 4, rgba(250, 210, 60));
      // shorten: drop rows so the goblin stands ~4px shorter
      Canvas s(HUMAN_W, HUMAN_H);
      int drop[3] = {hp.ty + 2, hp.hip + 1, hp.hip + 3};
      int dy = 0;
      for (int y = HUMAN_H - 1; y >= 0; y--) {
        bool skip = false;
        for (int d : drop) if (y == d) skip = true;
        if (skip) { dy++; continue; }
        for (int x = 0; x < HUMAN_W; x++) s.set(x, y + dy, fig.get(x, y));
      }
      fig = s;
      break;
    }
    default: break;
  }
  Canvas cell(24, 24);
  if (frame == 7) {
    // dead: the figure lies on its back
    if (sp == kSkeletonSp) {
      // a scattered heap of bones
      const Ramp& Bn = kBone;
      capsule(cell, V(5, 21), V(12, 20), 0.7f, 0.7f, Bn);
      capsule(cell, V(9, 22), V(16, 22.5f), 0.7f, 0.7f, Bn);
      capsule(cell, V(14, 20), V(19, 18), 0.6f, 0.6f, Bn, -1);
      for (int r = 0; r < 3; r++) hline(cell, 10, 14, 17 + r * 1, Bn[r & 1 ? 2 : 3]);
      ball(cell, 6, 18, 3.0f, 2.6f, Bn);
      cell.set(6, 18, kInk); cell.set(7, 18, kInk); cell.set(5, 20, Bn[1]); cell.set(7, 20, Bn[1]);
      Canvas sw(HUMAN_W, HUMAN_H);
      HumanLook sl; sl.weapon = 1; sl.weaponColor = rgba(150, 128, 104);
      HumanPainter hp(sw, sl, kSide, Pose{});
      hp.weapon(14, 21, 1, 0, -1);
      blit(cell, sw, 2, 0);
    } else {
      Canvas r = rotCCW(fig);
      int x0, y0, x1, y1;
      bounds(r, x0, y0, x1, y1);
      Canvas t(x1 - x0 + 1, y1 - y0 + 1);
      for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) t.set(x - x0, y - y0, r.get(x, y));
      blit(cell, t, (24 - t.w) / 2, 22 - t.h + 1);
    }
  } else {
    blit(cell, fig, 4, 0);
  }
  return cell;
}

}  // namespace art
