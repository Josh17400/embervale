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

// ================================================================== M3 peoples, dress and culture arms (VISION_PLAN
// 5.5, 15.6, 15.11). Everything below is reached only through HumanLook fields that are 0 on every pre-M3 look, so the
// M2 pixels never move (art_hash "humans").
//
// Extra map letters (drawMapX): p/P/q plume mid/light/dark, m mail (checkered), k/K trim dark/light (gilt when the
// arms are gilded, else the metal's own highlights), h/H horn mid/light, r lacquer red, f fur (noisy), w white cloth.

// elves: a finer face with a narrow, tapering jaw
const char* const kElfSkinDown[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333332...",
  "..33333322..",
  "..33222221..",
  "..32222221..",
  "..32e22e21..",
  "..22e22e21..",
  "...222221...",
  "....1111....",
};
const char* const kElfSkinUp[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333221...",
  "..33222211..",
  "..32222211..",
  "..22222211..",
  "..22222211..",
  "..22222111..",
  "...222111...",
  "....1111....",
};
const char* const kElfSkinSide[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333322...",
  "..33333222..",
  "..33322222..",
  "..32222222..",
  "..322122e2..",
  "..222122e22.",
  "...222222...",
  "....12221...",
};

// ---- headwear (cult::Headwear): 2 cap, 3 turban, 4 fur hat, 5 veil, 6 circlet, 7 conical hat, 8 headscarf
// (1 hood reuses kHood*). Brims and crowns are seen from the high 3/4 camera: the top surface shows.
const char* const kBeretD[kMapRows] = {
  nullptr, nullptr,
  "..2333332...",
  ".234443332..",
  ".2344333221.",
  "..11111111..",
};
const char* const kBeretU[kMapRows] = {
  nullptr, nullptr,
  "...2333321..",
  "..234433321.",
  ".2333332221.",
  "..11111111..",
};
const char* const kBeretS[kMapRows] = {
  nullptr, nullptr,
  ".233333.....",
  "2344433332..",
  ".233333321..",
  "..1111111...",
};
const char* const kTurbanD[kMapRows] = {
  nullptr,
  "....3443....",
  "..23444332..",
  ".2343332342.",
  ".2433423321.",
  ".122g222211.",
  "..1......1..",
};
const char* const kTurbanU[kMapRows] = {
  nullptr,
  "....3443....",
  "..23444332..",
  ".2343332221.",
  ".2334222321.",
  ".1222232211.",
  "..12222111..",
  "....211.....",
};
const char* const kTurbanS[kMapRows] = {
  nullptr,
  "...34432....",
  ".23444433...",
  ".234333423..",
  ".2433423321.",
  ".12222g221..",
  "..221.......",
  "..21........",
};
const char* const kFurHatD[kMapRows] = {
  nullptr,
  "...f3f3f2...",
  "..ff4f4f3f..",
  "..f4f3f3f2..",
  ".f3f3f3f2f1.",
  ".1f2f2f2f11.",
};
const char* const kFurHatU[kMapRows] = {
  nullptr,
  "...f3f3f2...",
  "..ff4f4f3f..",
  "..f3f3f2f2..",
  ".f3f3f2f2f1.",
  ".1f2f2f1f11.",
  "..1f1f1f1...",
};
const char* const kFurHatS[kMapRows] = {
  nullptr,
  "..f3f3f2....",
  ".ff4f4f3f...",
  ".f4f3f3f2f..",
  "f3f3f3f2f1..",
  ".1f2f2f211..",
  "..1f1.......",
};
const char* const kVeilD[kMapRows] = {
  nullptr, nullptr,
  "...www332...",
  "..ww443332..",
  ".2w3433322..",
  ".2310000011.",
  ".22......11.",
  ".22......11.",
  ".2133333310.",
  ".2233333210.",
  "..22332110..",
  "...22111....",
};
const char* const kVeilU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  ".2334332211.",
  ".2333222111.",
  ".2332222111.",
  ".2322221110.",
  ".2222211110.",
  ".1222111110.",
  ".122211110..",
  "..1221110...",
  "...11.10....",
};
const char* const kVeilS[kMapRows] = {
  nullptr, nullptr,
  "...ww332....",
  "..w344332...",
  ".233333321..",
  ".2333322001.",
  ".233321.....",
  ".23321......",
  ".2332133332.",
  ".223213332..",
  ".1222133....",
  "..112.......",
};
const char* const kCircletD[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr,
  "..........",
  "..kKKgKKk1..",
};
const char* const kCircletU[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr,
  "..kKKKkkk1..",
};
const char* const kCircletS[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr,
  "..kkKKKKg...",
};
const char* const kConicalD[kMapRows] = {
  nullptr,
  ".....43.....",
  "...344332...",
  ".2344433332.",
  "234433333221",
  ".1122222110.",
};
const char* const kConicalU[kMapRows] = {
  nullptr,
  ".....43.....",
  "...343332...",
  ".2344333322.",
  "234333332221",
  ".1122222110.",
};
const char* const kConicalS[kMapRows] = {
  nullptr,
  "....43......",
  "..344332....",
  "2344433332..",
  "3443333322..",
  ".11222221...",
};
const char* const kScarfD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23433322..",
  "..21111112..",
  "..2......1..",
  "..2......1..",
  "..1......0..",
};
const char* const kScarfU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22231111..",
  "...12321....",
  "....2.21....",
  "....1..1....",
};
const char* const kScarfS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  "..2333211...",
  "..2332......",
  ".2321.......",
  "2.21........",
  "1..1........",
};

// ---- culture helm forms (cult::HelmForm): those the M0 bands lacked. The bands' maps serve Nasal (kIronHelm), Kettle,
// GreatHelm, Horned (kObsHelm) and Winged (kGildHelm); Crested gets its own crest in the plume colour.
const char* const kSpangenD[kMapRows] = {
  nullptr,   // (M6 fixer r3) a narrower, spired crown: the ribs meet at a point over the dome
  ".....k3.....",
  "...23k432...",
  "..2k44k332..",
  "..2k333k21..",
  ".1KKkkkkKk1.",
  "..1v1KK1v1..",
  "..1.1kk1.1..",
  "...1....1...",
};
const char* const kSpangenU[kMapRows] = {
  nullptr,
  ".....k3.....",
  "...23k432...",
  "..2k44k322..",
  "..2k33k221..",
  "..2k322k11..",
  ".1KKkkkkKk1.",
  "..1......1..",
};
const char* const kSpangenS[kMapRows] = {
  nullptr,
  "....k3......",
  "...23k43....",
  "..2k443k2...",
  "..2k3333k1..",
  ".1kkkkkKK1..",
  "..2211.1v1..",
  "..211...k1..",
};
const char* const kHornedD[kMapRows] = {
  nullptr,
  ".H........H.",
  ".hh.2332.hh.",
  "..h234432h..",
  "..23444332..",
  "..23333221..",
  ".1222k22211.",
  "..1..21..1..",
  ".....21.....",
};
const char* const kHornedU[kMapRows] = {
  nullptr,
  ".H........H.",
  ".hh.2332.hh.",
  "..h234432h..",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  ".1222k22111.",
  "..1......1..",
};
const char* const kHornedS[kMapRows] = {
  nullptr,
  "H...........",
  ".hh.233.....",
  "..h23443....",
  "..2344332...",
  "..23333321..",
  ".122k22221..",
  "..2211...2..",
  "..211....1..",
};
// (M6 fixer round 2, review: "the sun-temple plume reads as a flat teal cap band") the plume is an upright fan over
// the crown, narrow at the top and spreading into the helm, not a band across the whole brow
const char* const kPlumedD[kMapRows] = {
  nullptr,
  "....qPPq....",
  "...qpPPpq...",
  "...234432...",
  "..23444332..",
  ".1kkKkkkk11.",
  "..1......1..",
};
const char* const kPlumedU[kMapRows] = {
  nullptr,
  "....qPPq....",
  "...qpPPpq...",
  "...234432...",
  "..23444322..",
  "..23333221..",
  ".1kkKkkkk11.",
  "..1......1..",
};
const char* const kPlumedS[kMapRows] = {
  nullptr,
  "qpPpP.......",
  ".qpPPp......",
  "..qp4432....",
  "..2344332...",
  "..23333321..",
  ".1kkKkkk1...",
  "..21........",
};
const char* const kAventailD[kMapRows] = {
  nullptr,
  ".....43.....",
  "....3432....",
  "...234432...",
  "..23444332..",
  ".1kkkKkkkk1.",
  ".mm......mm.",
  ".mm......mm.",
  ".mm......mm.",
  ".mmm....mmm.",
  "..mmmmmmmm..",
  "...mmmmmm...",
};
const char* const kAventailU[kMapRows] = {
  nullptr,
  ".....43.....",
  "....3432....",
  "...234432...",
  "..23444322..",
  ".1kkkKkkkk1.",
  ".mmmmmmmmmm.",
  ".mmmmmmmmmm.",
  ".mmmmmmmmmm.",
  ".mmmmmmmmmm.",
  "..mmmmmmmm..",
  "...mmmmmm...",
};
const char* const kAventailS[kMapRows] = {
  nullptr,
  "....43......",
  "...3432.....",
  "..234432....",
  "..2344332...",
  ".1kkkKkkk1..",
  ".mmmm.......",
  ".mmmm.......",
  ".mmmmm......",
  ".mmmmm.mmm..",
  "..mmmmmmm...",
  "...mmmmm....",
};
const char* const kMaskedD[kMapRows] = {
  nullptr,
  "...kK..Kk...",
  "....kKKk....",
  "...234432...",
  "..23444332..",
  ".2344433322.",
  "122222222211",
  "1r1......1r0",
  ".........1..",
  ".........1..",
  "...vvrrvv...",
  "....vvvv....",
};
const char* const kMaskedU[kMapRows] = {
  nullptr,
  "...kK..Kk...",
  "....kKKk....",
  "...234432...",
  "..23444322..",
  ".2343332221.",
  "122222222211",
  "1r2222222r10",
  "1r1111111r10",
  ".r1111111r0.",
};
const char* const kMaskedS[kMapRows] = {
  nullptr,
  "......Kk....",
  ".....kK.....",
  "...23442....",
  "..2344332...",
  ".23444332...",
  "12222222221.",
  "1r22r.......",
  "1r11r.......",
  ".r1r....vv..",
  "........vv1.",
  ".......vv...",
};
const char* const kCrestedD[kMapRows] = {
  nullptr,
  "....pPPp....",
  "...qpPPpq...",
  "..23pPp332..",
  "..23444332..",
  ".1k2222k211.",
  "..21....12..",
  "..21....12..",
  "...1....1...",
};
const char* const kCrestedU[kMapRows] = {
  nullptr,
  "....pPPp....",
  "...qpPPpq...",
  "..23pPp322..",
  "..233pp221..",
  "..23322211..",
  ".1k2222k111.",
  "..22222211..",
  "..21111110..",
};
const char* const kCrestedS[kMapRows] = {
  nullptr,
  "..qpPPPPp...",
  ".qpPPPPPpq..",
  "..2344332...",
  "..23333321..",
  ".1k22222k1..",
  "..2211..12..",
  "..211...1...",
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

struct MapInk {
  const Ramp* r = nullptr;            // the material
  uint32_t eye = kEye, accent = 0, visor = 0;
  const Ramp* plume = nullptr;        // p P q
  const Ramp* trim = nullptr;         // k K (null: the material's highlights)
  const Ramp* mail = nullptr;         // m
  const Ramp* fur = nullptr;          // f
};
void drawMapX(Canvas& c, Map m, int ox, int oy, const MapInk& in) {
  const Ramp& r = *in.r;
  for (int row = 0; row < kMapRows; row++) {
    const char* s = m[row];
    if (!s) continue;
    for (int col = 0; s[col]; col++) {
      const char ch = s[col];
      const int x = ox + col, y = oy + row;
      uint32_t v = 0;
      if (ch >= '0' && ch <= '4') v = r[ch - '0'];
      else if (ch == 'e') v = in.eye;
      else if (ch == 'g') v = in.accent ? in.accent : r[4];
      else if (ch == 'v') v = in.visor ? in.visor : r[0];
      else if (ch == 'b') v = kLeather[1];
      else if (ch == 'p' || ch == 'P' || ch == 'q') {
        const Ramp& p = in.plume ? *in.plume : kRed;
        v = ch == 'P' ? p[3 + ((x + y) & 1)] : (ch == 'p' ? p[2] : p[1]);
      } else if (ch == 'k' || ch == 'K') v = in.trim ? (*in.trim)[ch == 'K' ? 4 : 2] : r[ch == 'K' ? 4 : 3];
      else if (ch == 'm') { const Ramp& M = in.mail ? *in.mail : kChainMail; const int base = x < 8 ? 3 : 2, q = (x + 2 * y) & 3; v = M[q == 0 ? base - 1 : (q == 2 ? base + 1 : base)]; }
      else if (ch == 'h' || ch == 'H') v = ch == 'H' ? kBone[4] : kBone[2 + ((x + y) & 1)];
      else if (ch == 'r') v = kRed[1 + ((y & 1) ? 1 : 0)];
      else if (ch == 'w') v = mix(r[3], kWhite, 0.35f);
      else if (ch == 'f') { const Ramp& F = in.fur ? *in.fur : kFur; v = F[1 + (int)(hash3(x, y, 77) % 3u)]; }
      if (v) c.set(x, y, v);
    }
  }
}
int topRow(Map m) {
  if (!m) return kMapRows;
  for (int r = 0; r < kMapRows; r++)
    if (m[r]) for (const char* q = m[r]; *q; q++) if (*q != '.' && *q != ' ') return r;
  return kMapRows;
}


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
  // M3: headwear cloth, cloth pattern, plume / crest, the arms' trim (gilt or the metal), the body form's metal
  Ramp head, pat, plume, gilt, mat;
  // M6: the armour's metal when HumanLook::armourTint is set (bronze, a culture alloy) and its glowing seam colour
  Ramp alloy;
  bool tinted = false;
  uint32_t seam = 0;
};
bool sameRamp(const Ramp& a, const Ramp& b) { return std::memcmp(a.c, b.c, sizeof a.c) == 0; }
bool isBandMetal(const Ramp& r) {
  return sameRamp(r, kIron) || sameRamp(r, kSteelArmor) || sameRamp(r, kElvenArmor) || sameRamp(r, kJadeArmor) ||
         sameRamp(r, kEbonyArmor) || sameRamp(r, kEmberArmor) || sameRamp(r, kChainMail);
}

// M3 garment cuts (HumanLook::cut = cult::Cut + 1) and body forms (bodyForm = cult::BodyArm + 1)
enum { kCutTunic = 1, kCutRobe, kCutKaftan, kCutWrap, kCutCoat, kCutKilt, kCutPoncho, kCutGown };
enum { kFormPadded = 1, kFormLeather, kFormMail, kFormScale, kFormLamellar, kFormBrigandine, kFormPlate, kFormLeaf };
// headwear (HumanLook::headwear = cult::Headwear)
enum { kHwHood = 1, kHwCap, kHwTurban, kHwFurHat, kHwVeil, kHwCirclet, kHwConical, kHwScarf };

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
  // ---- M3 (all 0 on a pre-M3 look)
  int lift = 0;               // elves stand a pixel taller (longer legs) when nothing tall sits on the head
  int bld = 0;                // the build painted: L.build, slimmed for elves (1) and half-breeds (3: a touch slimmer)
  int cutK = 0;               // the garment cut when the body is cloth (armour hides it)
  int form = 0;               // the armour's body form (bodyForm) when a torso armour is painted
  int hw = 0;                 // culture headwear actually painted (helmets and the legacy hood win)
  // ---- M6
  int frameNo = 0;            // the sheet column (the legendary glint travels with it)
  const Ramp* helmRamp = nullptr;   // the helm's material as painted
  Ramp shieldRamp{};          // the shield rim's metal as painted

  HumanPainter(Canvas& c_, const HumanLook& l, int f, const Pose& p) : c(c_), L(l), P(p), facing(f) {
    armor = L.armorStyle <= HumanLook::kBands ? L.armorStyle : 0;
    static const Outfit byBand[8] = {Outfit::Tunic, Outfit::Leather, Outfit::Chain, Outfit::Plate, Outfit::Elven, Outfit::Plate, Outfit::Ebony, Outfit::Ebony};
    O = armor ? byBand[armor] : L.outfit;
    bld = L.build;
    if (L.people == 1) bld = L.build == 0 ? 3 : L.build;
    if (L.people == 2) bld = L.build == 2 ? 0 : 1;
    const bool clothBody = !armor && (L.outfit == Outfit::Tunic || L.outfit == Outfit::Dress || L.outfit == Outfit::Robe);
    cutK = clothBody && L.cut <= kCutGown ? L.cut : 0;
    const bool armouredBody = armor || L.outfit == Outfit::Guard || L.outfit == Outfit::Chain || L.outfit == Outfit::Plate ||
                              L.outfit == Outfit::Leather || L.outfit == Outfit::Elven || L.outfit == Outfit::Ebony;
    form = armouredBody && L.bodyForm <= kFormLeaf ? L.bodyForm : 0;
    hw = (L.helmet || L.helmStyle || L.hood) ? 0 : (L.headwear <= kHwScarf ? L.headwear : 0);
    if (L.people == 2) {   // taller only if every facing's head stays below map row 2 (row 1 would leave the cell)
      int top = kMapRows;
      for (int fc = 0; fc < 3; fc++) top = std::min(top, headTopRow(fc));
      lift = top >= 2 ? 1 : 0;
    }
    hy = 3 + P.bob - lift;
    ty = hy + 8;
    hip = ty + 6;
    buildRig();
  }
  // the highest head-map row anything on the head uses in facing fc (hair, helmet, headwear)
  int headTopRow(int fc) const {
    if (L.helmet || L.helmStyle) return 0;   // every helmet uses row 1
    int t = kMapRows;
    if (L.hood) t = std::min(t, topRow(kHoodD));
    else if (hw) { Map m = headwearMap(fc); t = std::min(t, topRow(m)); }
    if (!L.hood && (!hw || hw == kHwCirclet)) t = std::min(t, topRow(hairMapF(fc)));
    return t;
  }

  bool skirt() const {
    if (cutK) return cutK == kCutRobe || cutK == kCutGown || cutK == kCutWrap;
    return O == Outfit::Dress || O == Outfit::Robe;
  }
  bool metalBody() const { return O == Outfit::Plate || O == Outfit::Elven || O == Outfit::Ebony; }
  bool isStaff() const { return L.weapon == 4 || L.weapon == 8 || L.weapon == 9; }   // (M4: a spear, a walking stick)
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
    if (L.people || L.cut || L.headwear || L.pattern || L.bodyForm || L.helmForm || L.armsOrnament || L.pauldron || L.skirt || L.crest) buildRigM3();
    if (L.armourTint) buildAlloy();
  }
  // M6: every metal armour ramp (the band's, chain mail, the body form's) becomes the alloy's; leather stays leather
  void buildAlloy() {
    uint32_t k[5];
    metalRampKeys(L.armourTint, L.armourTint2, L.sheen, k);
    R.alloy = ramp5(k[0], k[1], k[2], k[3], k[4]);
    R.tinted = true;
    R.seam = lighten(opaque(L.armourTint), 0.55f);
    const Ramp& A = R.alloy;
    const bool bootSet = L.boots >= 2;
    const bool legMetal = isBandMetal(R.leg);
    for (Ramp* r : {&R.top, &R.sleeve, &R.leg, &R.mat, &R.glove})
      if (isBandMetal(*r)) *r = A;
    if (bootSet) R.boot = A;
    else if (!L.boots && (legMetal || (form == kFormPlate || form == kFormLeaf))) R.boot = ramp5(A[0], A[0], A[1], A[2], A[3]);
  }
  const Ramp& chainRamp() const { return R.tinted ? R.alloy : kChainMail; }
  void buildRigM3() {
    R.head = ramp(L.headColor ? L.headColor : L.topColor);
    R.pat = ramp(L.patternColor ? L.patternColor : shade(L.topColor, 0.62f));
    R.plume = ramp(L.plumeColor ? L.plumeColor : L.tabardColor);
    R.gilt = (L.armsOrnament & 4) ? kGold : (L.trimColor ? ramp(L.trimColor, 1.1f) : kBrass);
    if (cutK) {   // a culture garment: the cloth is the top colour; long cuts tuck no shirt over trousers
      R.top = ramp(L.topColor);
      R.sleeve = R.top;
      R.hood = R.top;
    }
    if (form) {
      // the body form over the band's material: padded and leather are not metal (the band shows in studs and trim)
      R.mat = armor ? bandRamp(armor) : (O == Outfit::Plate ? kSteelArmor : kIron);
      if (armor == 1 && form >= kFormMail) R.mat = kIron;   // a leather band cannot be mail: plain iron
      switch (form) {
        case kFormPadded: R.top = ramp(L.outfit == Outfit::Guard ? L.tabardColor : L.topColor); R.sleeve = R.top; break;
        case kFormLeather: R.top = kLeather; R.sleeve = L.outfit == Outfit::Guard ? kChainMail : ramp(L.topColor); break;
        // (M6 fixer r3, review: "the imperial watch is a flat lemon-yellow shape") a guard's brigandine is the kingdom's
        // cloth dyed deep (a bright livery colour would flatten the whole body): its plates' rivets and the lit and
        // shaded sides then read on it
        case kFormBrigandine: R.top = L.outfit == Outfit::Guard ? ramp(darken(L.tabardColor, 0.42f)) : ramp(L.topColor); R.sleeve = R.mat; break;
        default: R.top = R.mat; R.sleeve = form == kFormLamellar || form == kFormScale ? R.mat : (form == kFormMail ? R.mat : R.mat); break;
      }
      if (form == kFormPlate || form == kFormLeaf) {
        R.leg = R.mat;
        if (!L.boots) R.boot = ramp5(R.mat[0], R.mat[0], R.mat[1], R.mat[2], R.mat[3]);
      }
      if (form == kFormLeaf) R.accent = rgba(110, 200, 120);
      // (M6) only plate and leaf armour carry plate legs: under mail, scale, lamellar, brigandine or padding the
      // trousers show (the boots stay the band's)
      if (form != kFormPlate && form != kFormLeaf && isBandMetal(R.leg)) R.leg = ramp(shade(L.bottomColor, 0.85f));
    }
  }
  // the body form's silhouette rules (front views use them for pauldrons and knee plates)
  bool formMetal() const { return form >= kFormMail; }

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
      if ((O == Outfit::Plate || O == Outfit::Elven) && (!form || form == kFormPlate || form == kFormLeaf)) {   // knee plates
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
    if (cutK) { cutTorsoFront(back); return; }
    if (form) { formTorsoFront(back); return; }
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
    if (bld == 1 || bld == 3)
      for (int y = ty + (bld == 3 ? 4 : 3); y < hip; y++) { c.set(5, y, 0); c.set(10, y, 0); }
  }
  void buildShoulders(bool lit, int x0) {   // broad: a deltoid bulge outside each arm (front/back views)
    if (bld != 2 || P.atk) return;
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
    if (cutK || form) {
      if (cutK) cutTorsoSide();
      else formTorsoSide();
      if (!skirt()) {
        if (bld == 1 || bld == 3) for (int y = ty + (bld == 3 ? 4 : 3); y < hip; y++) c.set(6 + ox, y, 0);
        if (bld == 2) for (int y = ty + 1; y <= ty + 3; y++) c.set(11 + ox, y, R.top[y == ty + 1 ? 2 : 1]);
      }
      return;
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
      if (bld == 1 || bld == 3) for (int y = ty + (bld == 3 ? 4 : 3); y < hip; y++) c.set(6 + ox, y, 0);
      if (bld == 2) for (int y = ty + 1; y <= ty + 3; y++) c.set(11 + ox, y, R.top[y == ty + 1 ? 2 : 1]);
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
        c.set(x, y, chainRamp()[std::max(0, k)]);
      }
  }

  // long skirt (dress / robe), the hem sways with the walk
  void skirtFront() {
    if (cutK) { cutSkirtFront(); return; }
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
    if (cutK) { cutSkirtSide(); return; }
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
    const int bf = (L.bowForm >= 1 && L.bowForm <= 5) ? L.bowForm : 0;
    if ((L.backItem & 1) && bf) {   // M6: the culture's bow (and quiver) on the back
      static const Ramp kHorn = ramp5(rgba(60, 36, 30), rgba(104, 70, 48), rgba(150, 112, 72), rgba(196, 166, 120), rgba(232, 214, 176));
      const Ramp& W = bf == 3 ? kHorn : (bf == 2 ? kWoodDark : kWood);
      auto flick = [&](Vec2 tip, float sx, float sy) {   // a recurved tip turned back past the string line
        put((int)std::floor(tip.x + sx), (int)std::floor(tip.y + sy), W[1]);
      };
      if (facing == kSide) {
        for (int y = ty - 2; y <= ty + 4; y++) { put(4 + ox, y, kLeather[y == ty - 2 ? 3 : 2]); put(5 + ox, y, kLeather[1]); }
        if (bf == 4) {   // a crossbow slung by its stock, the prod across the shoulder blades
          put(4 + ox, ty - 3, kRed[3]); put(5 + ox, ty - 3, kCloth[4]);
          for (int y = ty - 3; y <= hip; y++) put(6 + ox, y, kWood[y < ty ? 3 : 2]);
          for (int x = 3; x <= 8; x++) put(x + ox, ty - 2 + (x == 3 || x == 8 ? 1 : 0), x < 6 ? kIron[3] : kIron[1]);
          return;
        }
        put(4 + ox, ty - 3, kRed[3]); put(5 + ox, ty - 3, kCloth[4]); put(4 + ox, ty - 4, kWood[3]);
        const float ext = bf == 5 ? 2.0f : (bf == 3 ? -1.0f : 0.0f);
        Vec2 a = V(5.5f + ox, ty - 3.5f - ext), b = V(5.5f + ox, hip + 2.5f + std::min(ext, 1.0f));
        stroke(a, b, V((bf == 5 ? 2.5f : 1.0f) + ox, (a.y + b.y) * 0.5f), W, 3, 1, false);
        if (bf == 2 || bf == 3) { flick(a, 1.0f, -0.2f); flick(b, 1.0f, 0.2f); }
      } else {
        for (int y = ty - 1; y <= ty + 4; y++) { put(9, y, kLeather[y == ty - 1 ? 4 : 3]); put(10, y, kLeather[y == ty + 2 ? 4 : 1]); }
        put(9, ty - 2, kWood[3]); put(10, ty - 2, kWood[2]);
        put(9, ty - 3, kRed[3]); put(10, ty - 3, kCloth[4]); put(11, ty - 3, kRed[2]);
        if (bf == 4) {   // the crossbow: the stock down the back, the steel prod across the shoulders
          for (int y = ty - 3; y <= hip + 1; y++) { put(6, y, kWood[3]); put(7, y, kWood[1]); }
          for (int x = 3; x <= 10; x++) {
            const int yy = ty - 2 + (x == 3 || x == 10 ? 1 : 0);
            put(x, yy, kIron[x < 6 ? 4 : (x < 9 ? 3 : 1)]);
            put(x, yy + 1, kIron[0]);
          }
          for (int x = 4; x <= 9; x++) put(x, ty, kCloth[2]);   // the string
          return;
        }
        const float ext = bf == 5 ? 1.6f : (bf == 3 ? -1.0f : 0.0f);
        Vec2 a = V(3.5f - ext * 0.7f, ty - 3.5f - ext), b = V(11.5f + ext * 0.4f, hip + 1.5f + ext);
        for (int i = 0; i <= 16; i++) {   // the string, under the stave
          Vec2 q = a + (b - a) * (i / 16.0f);
          put((int)std::floor(q.x), (int)std::floor(q.y), kCloth[2]);
        }
        stroke(a, b, (a + b) * 0.5f + V(bf == 5 ? -2.0f : -2.6f, bf == 5 ? 2.0f : 2.6f), W, 4, 2, true);
        if (bf == 2 || bf == 3) { flick(a, 1.0f, 0.0f); flick(b, 0.0f, 1.0f); }
        if (bf == 3) put((int)std::floor((a.x + b.x) * 0.5f - 1.3f), (int)std::floor((a.y + b.y) * 0.5f + 1.3f), R.plume[2]);   // the sinew grip
      }
    } else if (L.backItem & 1) {   // bow and quiver
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
  const Ramp& handRamp() const {
    if (L.gloves) return R.glove;
    if (form) return form == kFormPlate || form == kFormLeaf ? R.mat : R.skin;
    return metalBody() ? R.sleeve : R.skin;
  }

  // front / back arm hanging at column x0 (2 wide)
  void armFront(int x0, int swing, bool lit) {
    const Ramp& s = R.sleeve;
    int handY = ty + 5 + swing;
    int kA = lit ? 3 : 2, kB = lit ? 2 : 1;
    if (poncho() && !P.atk) { handPx(x0, handY, lit); return; }   // the poncho covers the arm
    for (int y = ty + 1; y < handY; y++) { c.set(x0, y, s[kA]); c.set(x0 + 1, y, s[kB]); }
    c.set(lit ? x0 + 1 : x0, ty, s[kA]);
    if (wideSleeve()) {   // a wide cuff flares past the wrist
      c.set(lit ? x0 - 1 : x0 + 2, handY - 1, s[lit ? 3 : 1]);
      if (cutK == kCutGown) c.set(lit ? x0 - 1 : x0 + 2, handY, s[lit ? 2 : 0]);
    }
    if (m3Pauldron()) pauldronM3(x0, lit);
    else if (metalBody()) pauldron(lit ? x0 - 1 : x0, lit);
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
    if (poncho() && !P.atk) { handAt(sx + swing, ty + 5, bias); return; }
    armLine(sx, ty + 1, sx + swing, ty + 4, bias);
    if (wideSleeve()) c.set(sx + swing + 1, ty + 4, R.sleeve[1 + bias + 1]);
    if (nearArm && m3Pauldron()) pauldronM3(sx - 1, true);
    else if (nearArm && metalBody()) pauldron(sx - 2, true);
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
    const bool m6Pole = w == 8 && L.polearmForm >= 1 && L.polearmForm <= 3;
    const bool m6Bow = w == 3 && L.bowForm >= 1 && L.bowForm <= 5;
    if (m6Pole || m6Bow) {   // M6 culture polearms and bows, painted on a scratch canvas kept off the cell's edges
      Canvas tmp(c.w, c.h);
      auto put2 = [&](int t, int o, uint32_t col) { tmp.set(hx + dx * t + px * o, hy2 + dy * t + py * o, col); };
      auto head2 = [&](int t, int o, uint32_t col) {
        put2(t, o, col);
        if (diag && o != 0) tmp.set(hx + dx * t + px * o, hy2 + dy * t + py * o - py * (o > 0 ? 1 : -1), col);
      };
      if (m6Pole) polearmM6(put2, head2, m);
      else bowM6(tmp, hx, hy2, dx, dy, m);
      blitWeapon(tmp, c.h - 1);
      return;
    }
    if (w == 1 && L.bladeForm > 1 && L.bladeForm <= 9) {   // M3 culture blades (the straight blade is the legacy sword)
      // painted on a scratch canvas and kept off the cell's edges (the 1px outline must fit)
      Canvas tmp(c.w, c.h);
      auto put2 = [&](int t, int o, uint32_t col) { tmp.set(hx + dx * t + px * o, hy2 + dy * t + py * o, col); };
      auto head2 = [&](int t, int o, uint32_t col) {
        put2(t, o, col);
        if (diag && o != 0) tmp.set(hx + dx * t + px * o, hy2 + dy * t + py * o - py * (o > 0 ? 1 : -1), col);
      };
      cultureBlade(put2, head2, diag, m);
      blitWeapon(tmp, c.h);
      return;
    }
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
      case 7: {  // (M4) a herald's horn: a brass coil from the mouthpiece in the hand to a flared bell
        put(0, 0, kBrass[1]); put(1, 0, kBrass[2]); put(2, 0, kBrass[3]); put(3, 1, kBrass[3]); put(4, 1, kBrass[2]);
        put(5, 1, kBrass[2]); put(6, 0, kBrass[3]);
        head(6, 1, kBrass[4]); head(7, 0, kBrass[3]); head(7, 1, kBrass[2]); head(7, 2, kBrass[1]); head(7, -1, kBrass[4]);
        put(2, 1, kBrass[1]);   // the coil's shade below
        break;
      }
      case 8: {  // (M4) a soldier's spear: an ash shaft, an iron leaf head, a socket ring
        for (int t = -7; t <= 6; t++) put(t, 0, kWood[(t & 3) == 0 ? 1 : (t < 2 ? 2 : 3)]);
        put(7, 0, kIron[1]);
        head(8, 0, m[3]); head(9, 0, m[4]); head(8, 1, m[1]); head(9, 1, m[2]);
        if (!diag) put(10, 0, m[3]);
        break;
      }
      case 9: {  // (M4) a refugee's walking stick, a knot at its head
        for (int t = -5; t <= 4; t++) put(t, 0, kWoodDark[(t & 1) ? 2 : 3]);
        put(5, 0, kWoodDark[1]); put(5, 1, kWoodDark[2]);
        break;
      }
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

  // a weapon from its scratch canvas onto the figure, kept off the cell's edges. Where its metal lies over the figure
  // (a spearhead against a helmet, a blade across a mail shirt) a dark rim parts the two, as a selective outline would.
  void blitWeapon(const Canvas& tmp, int yEnd) {
    auto metal = [&](int x, int y) {
      const uint32_t v = tmp.get(x, y);
      return (v >> 24) && rampIndex(kWood, v) < 0 && rampIndex(kWoodDark, v) < 0 && rampIndex(kLeather, v) < 0;
    };
    const Canvas under = c;
    for (int y = 1; y < yEnd; y++)
      for (int x = 1; x < c.w - 1; x++) {
        if (solid(tmp, x, y)) { c.set(x, y, tmp.get(x, y)); continue; }
        if (!solid(under, x, y)) continue;
        const bool below = metal(x, y - 1) || metal(x - 1, y);   // the rim falls on the lower-right side of the metal
        const bool above = metal(x, y + 1) || metal(x + 1, y);
        if (below) c.set(x, y, mix(under.get(x, y), kInk, 0.72f));
        else if (above) c.set(x, y, mix(under.get(x, y), kInk, 0.45f));
      }
  }
  // M6: the culture's polearm (cult::Polearm + 1) on an ash shaft: spear, glaive, halberd. put(t, o, col) as the blades
  template <class Put, class Head>
  void polearmM6(Put& put, Head& head, const Ramp& m) {
    const uint32_t ring = (L.armsOrnament & 4) ? kGold[3] : kIron[3];
    for (int t = -7; t <= 6; t++) put(t, 0, kWood[(t & 3) == 0 ? 1 : (t < 2 ? 2 : 3)]);
    put(7, 0, ring);
    switch (L.polearmForm) {
      case 1:   // spear: a broad leaf head swelling to a long point
        head(8, 1, m[1]); head(8, 0, m[3]); head(8, -1, m[4]);
        head(9, 1, m[2]); head(9, 0, m[4]); head(9, -1, m[3]);
        put(10, 0, m[3]); put(11, 0, m[4]);
        break;
      case 2:   // glaive: a long single edge that swells out and sweeps back to the point, a spur behind
        put(8, 0, m[3]); put(9, 0, m[3]); put(10, 0, m[4]); put(11, 0, m[4]);
        head(8, 1, m[2]); head(9, 1, m[1]); head(10, 1, m[1]); head(11, 1, m[2]); put(12, 0, m[4]);
        head(8, -1, m[2]);
        break;
      default:   // halberd: a broad axe blade, the top spike, a back hook
        put(8, 0, m[3]); put(9, 0, m[3]); put(10, 0, m[4]); put(11, 0, m[4]);
        head(7, 1, m[2]); head(8, 1, m[2]); head(9, 1, m[1]);
        head(6, 2, m[3]); head(7, 2, m[4]); head(8, 2, m[3]); head(9, 2, m[2]); head(10, 2, m[1]);
        head(8, -1, m[2]); head(9, -2, m[1]);
        break;
    }
    if (L.armsOrnament & (64 | 8 | 1)) {   // a tassel under the head
      put(6, 1, R.plume[3]); put(5, 1, R.plume[2]);
    }
  }
  // M6: the culture's bow (cult::BowKind + 1): self, recurve, composite, crossbow, longbow. Same aim rules as the M0 bow.
  void bowM6(Canvas& t, int hx, int hy2, int dx, int dy, const Ramp& m) {
    static const Ramp kHorn = ramp5(rgba(60, 36, 30), rgba(104, 70, 48), rgba(150, 112, 72), rgba(196, 166, 120), rgba(232, 214, 176));
    const int f = L.bowForm;
    const bool drawn = P.atk == 1;
    Vec2 a = norm(Vec2{(float)dx, (float)dy}), pp = norm(Vec2{(float)-dy, (float)dx});
    Vec2 h{(float)hx, (float)hy2};
    auto at = [&](Vec2 q, uint32_t col) { t.set((int)std::lround(q.x), (int)std::lround(q.y), col); };
    if (f == 4) {   // crossbow: the stock along the aim, the steel prod across its nose, a bolt when spanned
      for (int s = -3; s <= 2; s++) at(h + a * (float)s, kWood[s < 0 ? 2 : 3]);
      const Vec2 nose = h + a * 2.0f;
      Vec2 e0 = nose, e1 = nose;
      for (int s = -3; s <= 3; s++) {
        Vec2 q = nose + pp * (float)s - a * (s * s / 9.0f);
        at(q, kIron[std::abs(s) == 3 ? 1 : (s < 0 ? 4 : 2)]);
        if (s == -3) e0 = q;
        if (s == 3) e1 = q;
      }
      const Vec2 back = h + a * (drawn ? -1.0f : 0.6f);
      line(t, (int)std::lround(e0.x), (int)std::lround(e0.y), (int)std::lround(back.x), (int)std::lround(back.y), kCloth[3]);
      line(t, (int)std::lround(back.x), (int)std::lround(back.y), (int)std::lround(e1.x), (int)std::lround(e1.y), kCloth[3]);
      for (int s = -3; s <= 2; s++) at(h + a * (float)s, kWood[s < 0 ? 2 : 3]);   // the stock over the string
      if (drawn) at(h + a * 3.0f, m[4]);
      return;
    }
    const int n = f == 5 ? 7 : (f == 3 ? 4 : 5);
    const Ramp& W = f == 3 ? kHorn : (f == 2 ? kWoodDark : kWood);
    Vec2 e0 = h, e1 = h;
    for (int s = -n; s <= n; s++) {
      const float u = (float)s / n;
      float bulge = (f == 5 ? 1.3f : 1.7f) * (1 - u * u);
      if ((f == 2 || f == 3) && std::abs(s) >= n - 1) bulge += std::abs(s) == n ? 1.5f : 0.6f;   // the tips recurve forward
      const Vec2 q = h + pp * (float)s + a * bulge;
      uint32_t col = W[std::abs(s) >= n - 1 ? 1 : (s < 0 ? 3 : 2)];
      if (std::abs(s) <= 1) col = (L.armsOrnament & 4) ? kGold[s < 0 ? 3 : 2] : kLeather[s < 0 ? 3 : 1];   // the grip
      if (f == 3 && std::abs(s) == 2) col = R.plume[2];   // sinew bindings
      at(q, col);
      if (s == -n) e0 = q;
      if (s == n) e1 = q;
    }
    const Vec2 back = h - a * (drawn ? (f == 5 ? 4.0f : 3.0f) : 0.2f);
    line(t, (int)std::lround(e0.x), (int)std::lround(e0.y), (int)std::lround(back.x), (int)std::lround(back.y), kCloth[3]);
    line(t, (int)std::lround(back.x), (int)std::lround(back.y), (int)std::lround(e1.x), (int)std::lround(e1.y), kCloth[3]);
    // the stave over the string at the grip
    for (int s = -1; s <= 1; s++) at(h + pp * (float)s + a * (f == 5 ? 1.3f : 1.7f), (L.armsOrnament & 4) ? kGold[s < 0 ? 3 : 2] : kLeather[s < 0 ? 3 : 1]);
    if (drawn)
      for (int k = -3; k <= 3; k++) at(h + a * (float)k, k == 3 ? m[4] : kWood[3]);
  }

  // M3: the culture's blade silhouette (cult::Blade + 1). put(t, o, col): t along the blade from the hand, o across it
  template <class Put, class Head>
  void cultureBlade(Put& put, Head& head, bool diag, const Ramp& m) {
    const uint32_t gold = (L.armsOrnament & 4) ? kGold[3] : kBrass[3];
    put(-1, 0, gold);
    switch (L.bladeForm) {
      case 2:   // leaf: swells past the middle, then a long point
        put(1, 1, kBrass[3]); put(1, -1, kBrass[1]); put(1, 0, kBrass[2]);
        for (int t = 2; t <= 7; t++) put(t, 0, t == 7 ? m[4] : m[3]);
        for (int t = 3; t <= 6; t++) head(t, 1, m[1]);
        head(4, -1, m[4]); head(5, -1, m[3]);
        break;
      case 3:   // falchion: a heavy blade widening to a clipped tip
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]); put(1, -1, kBrass[1]);
        for (int t = 2; t <= 6; t++) { put(t, 0, m[3]); head(t, 1, m[1]); }
        head(5, 2, m[1]); head(6, 2, m[2]); put(7, 1, m[4]); put(7, 0, m[3]);
        break;
      case 4:   // scimitar: a deep curve, the edge on the outside
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]);
        put(2, 0, m[3]); put(3, 0, m[3]); put(4, 1, m[3]); put(5, 1, m[3]); put(6, 2, m[3]); put(7, 2, m[4]);
        head(2, 1, m[1]); head(3, 1, m[1]); head(4, 2, m[1]); head(5, 2, m[1]);
        break;
      case 5:   // khopesh: a short shaft, then the sickle hook
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]);
        for (int t = 2; t <= 3; t++) put(t, 0, m[2]);
        head(4, 0, m[3]); head(4, 1, m[3]); head(5, 1, m[3]); head(5, 2, m[2]); head(6, 2, m[2]); head(6, 1, m[4]); head(7, 0, m[4]);
        break;
      case 6:   // wavy (a kris): the blade snakes
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]); put(1, -1, kBrass[1]);
        for (int t = 2; t <= 7; t++) { int o = (t & 1) ? 1 : 0; put(t, o, t == 7 ? m[4] : m[3]); if (!diag) put(t, o == 1 ? 0 : 1, m[1]); }
        break;
      case 7:   // broad: a short, very wide blade
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]); put(1, -1, kBrass[1]); put(1, 2, kBrass[3]); put(1, -2, kBrass[1]);
        for (int t = 2; t <= 6; t++) { put(t, 0, m[3]); head(t, 1, m[1]); head(t, -1, m[4]); }
        put(7, 0, m[4]);
        break;
      case 8:   // curved (a long, slim single edge with a gentle curve and a long grip)
        put(-2, 0, kLeather[1]); put(0, 0, kLeather[2]);
        put(1, 0, gold); put(1, 1, kBrass[2]);
        for (int t = 2; t <= 6; t++) put(t, 0, m[3]);
        put(7, -1, m[4]);
        for (int t = 2; t <= 5; t++) if (!diag) put(t, 1, m[1]);
        break;
      default: {   // glaive: a blade on a long shaft
        for (int t = -6; t <= 3; t++) put(t, 0, kWood[(t & 3) == 0 ? 1 : 2]);
        put(3, 1, gold); put(3, -1, gold);
        for (int t = 4; t <= 7; t++) { put(t, 0, t == 7 ? m[4] : m[3]); if (t < 7) head(t, 1, m[1]); }
        head(5, 2, m[1]);
        break;
      }
    }
  }

  bool hasShield() const { return L.shieldForm != 9 && (L.shield || L.shieldStyle || L.shieldForm); }
  // M0 shield shapes (round, heater, kite, tower): r rim, f painted field (tabardColor), d gilt device, b boss, w planks
  void shieldShaped(int x0, int y0, bool edgeOn) {
    static const char* const kRound[] = {"..rrr..", ".rfffr.", "rfffwwr", "rffbwwr", "rfwwwwr", ".rwwwr.", "..rrr..", nullptr};
    static const char* const kHeater[] = {"rrrrrr", "rffffr", "rdffdr", "rfddfr", "rffffr", ".rffr.", ".rffr.", "..rr..", nullptr};
    static const char* const kKite[] = {".rrrr.", "rffffr", "rfddfr", "rddddr", "rfddfr", ".rffr.", ".rffr.", "..rr..", "..rr..", nullptr};
    static const char* const kTower[] = {"rrrrrrr", "rfffffr", "rffdffr", "rfdddfr", "rffdffr", "rffdffr", "rfffffr", "rfffffr", "rfffffr", "rrrrrrr", nullptr};
    static const char* const* const shapes[4] = {kRound, kHeater, kKite, kTower};
    // M3 culture shields (cult::ShieldForm + 1): round, kite, heater, tower, crescent, oval, buckler, leaf
    static const char* const kCrescent[] = {"rr...rr", "rfr.rfr", "rfrrrfr", "rffdffr", "rfdddfr", ".rffffr", "..rrrr.", nullptr};
    static const char* const kOval[] = {".rrrr.", "rffffr", "rffffr", "rfdffr", "rddbdr", "rffdfr", "rffffr", "rffffr", ".rrrr.", nullptr};
    static const char* const kBuckler[] = {".rrr.", "rfffr", "rfbfr", "rfffr", ".rrr.", nullptr};
    static const char* const kLeafSh[] = {"..rr..", ".rffr.", "rfdffr", "rffdfr", "rfdffr", "rffdfr", "rfdffr", ".rfdr.", ".rffr.", "..rr..", nullptr};
    // (M6 fixer r3, review: "the imperial watch is a flat yellow robe": a tower shield in the kingdom's colour hid the
    // whole body) the culture tower shield is a curved scutum: a row shorter, a boss on its spine
    static const char* const kScutum[] = {"rrrrrrr", "rfffffr", "rffdffr", "rfdbdfr", "rffdffr", "rffdffr", "rfffffr", "rrrrrrr", nullptr};
    static const char* const* const forms[8] = {kRound, kKite, kHeater, kScutum, kCrescent, kOval, kBuckler, kLeafSh};
    int st = std::clamp((int)L.shieldStyle, 1, 4) - 1;
    const char* const* m = shapes[st];
    if (L.shieldForm >= 1 && L.shieldForm <= 8) {
      m = forms[L.shieldForm - 1];
      static const int asStyle[8] = {0, 2, 1, 3, 0, 1, 0, 1};   // the edge-on look of the nearest M0 shape
      st = asStyle[L.shieldForm - 1];
    }
    int h = 0, w = 0;
    while (m[h]) { w = std::max(w, (int)std::strlen(m[h])); h++; }
    Ramp M = L.trimColor ? ramp(L.trimColor, 1.1f) : R.metal;
    if (R.tinted) M = R.alloy;   // M6: the rim in the armour's metal
    shieldRamp = M;
    Ramp field = L.shieldField ? ramp(L.shieldField) : R.trim;      // M6 fixer: the maker culture's paint
    // (M6 fixer r3) a culture tower shield (the scutum) in a bright livery colour covers half the figure: its painted
    // leather face is the colour dyed deep, so the shield, the harness and the livery read apart
    if (L.shieldForm == 4 && !L.shieldField && luma(field[2]) > 0.5f) field = ramp(darken(field[2], 0.5f));
    Ramp device = L.shieldDevice ? ramp(L.shieldDevice, 1.1f) : kGold;
    // (M6 fixer r3) a device that would vanish into its field (gold on a yellow field) is painted dark instead
    if (L.shieldForm && std::fabs(luma(device[2]) - luma(field[2])) < 0.16f) device = ramp(darken(field[2], 0.62f));
    if (edgeOn) {   // seen edge-on from the side: rim + the field's edge, as tall as the shape
      for (int y = 0; y < h; y++) {
        int n = 0;
        for (const char* q = m[y]; *q; q++) n += *q != '.';
        uint32_t back = st == 0 ? kWood[y < h / 2 ? 2 : 1] : field[y < h / 2 ? 1 : 0];
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
          const Ramp& F = ch == 'w' ? kWood : field;
          int k = left ? 3 : 2;
          if (x >= w - 2) k = 1;
          if (ch == 'w' && (x & 1)) k--;
          if (low) k--;
          if (x == 1 && y == 1) k = 4;
          col = F[k];
        } else if (ch == 'd') {
          col = device[low ? 1 : (left ? 4 : 2)];
        } else if (ch == 'b') {
          col = M[left && y < h / 2 ? 4 : 2];
        }
        c.set(x0 + x, y0 + y, col);
      }
  }
  void shield(int x0, int y0, bool edgeOn) {
    if (!hasShield()) return;
    if (L.shieldStyle || L.shieldForm) { shieldShaped(x0, y0, edgeOn); return; }
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
  Map hairMap() const { return hairMapF(facing); }
  Map hairMapF(int f) const {
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
    if (L.people == 2) skin = facing == kDown ? kElfSkinDown : (facing == kUp ? kElfSkinUp : kElfSkinSide);
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
    bool covered = helm || L.hood || hwHidesHair();
    if (L.hair == Hair::Bald && !covered) { c.set(ox + 3, oy + 4, R.skin[4]); c.set(ox + 4, oy + 4, R.skin[4]); }
    if (L.hair == Hair::Mohawk && !covered && facing != kSide)
      for (int y = hy; y < hy + 3; y++) for (int x = 4; x <= 11; x++)
        if (((x + y) & 1) && solid(c, x, y) && (x < 6 || x > 9)) c.set(x, y, mix(c.get(x, y), R.hair[1], 0.4f));
    facePaint();
    if (L.beard) {
      if (facing == kDown) drawMap(c, kBeardD, ox, oy, R.hair);
      else if (facing == kSide) drawMap(c, kBeardS, ox, oy, R.hair);
    }
    if (helm) {
      uint32_t accent, visor;
      const Ramp* rp;
      Map hm = helmetMap(accent, rp, visor);
      if (R.tinted && L.helmStyle >= 2) rp = &R.alloy;   // M6: the helm in the armour's metal
      helmRamp = rp;
      if (L.hair == Hair::Long || L.hair == Hair::Braids || L.hair == Hair::Ponytail || L.hair == Hair::Curls) {   // long hair flows out below
        Canvas tmp(c.w, c.h);
        drawMap(tmp, hairMap(), ox, oy, R.hair);
        for (int y = hy + 4; y < c.h; y++)
          for (int x = 0; x < c.w; x++)
            if (solid(tmp, x, y) && !(facing == kDown && x >= 5 && x <= 10)) c.set(x, y, tmp.get(x, y));
      }
      ears();
      if (L.helmForm) helmForm(ox, oy, *rp);
      else drawMap(c, hm, ox, oy, *rp, kEye, accent, visor);
      if (!L.helmForm && (L.armsOrnament || L.crest)) helmExtras(ox, *rp);
      helmFace();
    } else if (L.hood) {
      drawMap(c, facing == kDown ? kHoodD : (facing == kUp ? kHoodU : kHoodS), ox, oy, R.hood);
      if (facing == kDown) for (int x = 5; x <= 10; x++) c.set(x, hy + 3, R.skin[0]);
    } else if (hw) {
      if (hw == kHwCirclet) { if (Map hm = hairMap()) drawMap(c, hm, ox, oy, R.hair); }
      else if (hw == kHwCap || hw == kHwTurban || hw == kHwFurHat || hw == kHwConical) {
        // the hair shows below the hat's edge (never above it: no mohawk through a turban)
        if (Map hm = hairMap()) {
          Canvas tmp(c.w, c.h);
          drawMap(tmp, hm, ox, oy, R.hair);
          for (int y = hy + 2; y < c.h; y++)
            for (int x = 0; x < c.w; x++) if (solid(tmp, x, y)) c.set(x, y, tmp.get(x, y));
        }
      } else flowingHair(ox, oy);
      ears();
      headwear(ox, oy);
    } else if (Map hm = hairMap()) {
      drawMap(c, hm, ox, oy, R.hair);
      ears();
    } else {
      ears();
    }
    earrings();
  }

  // ====================================================================== M3: peoples, dress, culture arms
  static int rampIndex(const Ramp& r, uint32_t c) {
    for (int k = 0; k < 5; k++) if (r[k] == c) return k;
    return -1;
  }
  bool poncho() const { return cutK == kCutPoncho; }
  bool wideSleeve() const { return cutK == kCutRobe || cutK == kCutGown || cutK == kCutKaftan; }

  // ---- ears: drawn after the hair (they poke through it), before helmets and headwear (which cover their base)
  void ears() {
    if (!L.people || L.hood || hw == kHwHood || hw == kHwVeil || hw == kHwScarf) return;
    const Ramp& s = R.skin;
    const bool elf = L.people == 2;
    if (facing == kSide) {
      const int ox = P.lean;
      c.set(7 + ox, hy + 4, s[2]); c.set(7 + ox, hy + 5, s[1]);
      c.set(6 + ox, hy + 4, s[3]); c.set(6 + ox, hy + 3, s[3]);
      if (elf) { c.set(5 + ox, hy + 3, s[2]); c.set(5 + ox, hy + 2, s[3]); c.set(4 + ox, hy + 2, s[3]); c.set(3 + ox, hy + 1, s[4]); }
      else c.set(5 + ox, hy + 2, s[3]);
      return;
    }
    const bool back = facing == kUp;
    const int kL = back ? 2 : 3, kR = back ? 1 : 2;
    // screen-left ear (lit) and screen-right ear (shaded), each swept up and out
    c.set(3, hy + 4, s[kL - 1]); c.set(3, hy + 5, s[kL - 2]); c.set(2, hy + 3, s[kL]);
    c.set(12, hy + 4, s[kR - 1]); c.set(12, hy + 5, s[0]); c.set(13, hy + 3, s[kR]);
    if (elf) {
      c.set(2, hy + 4, s[kL - 1]); c.set(1, hy + 2, s[kL + 1]); c.set(2, hy + 2, s[kL]);
      c.set(13, hy + 4, s[kR - 1]); c.set(14, hy + 2, s[kR]); c.set(13, hy + 2, s[kR]);
    }
  }

  // (fixer M4 r3, review: "guards are faceless: the helmet leaves a dark blob") under a helmet's brim the face stays a
  // face: the eyes get a pale white on their outer side (on a dark skin a dark eye alone vanishes), and the cheeks
  // under them catch the light from the top-left
  void helmFace() {
    if (facing == kUp || P.hurt) return;
    const uint32_t eye = L.eyeColor ? opaque(L.eyeColor) : kEye, white = rgba(226, 218, 204);
    auto isSkin = [&](int x, int y) { return solid(c, x, y) && rampIndex(R.skin, c.get(x, y)) >= 0; };
    const bool dark = luma(R.skin[2]) < 0.42f;
    if (facing == kDown) {
      if (c.get(6, hy + 4) != eye || c.get(9, hy + 4) != eye) return;   // the helm covers the eyes (a great-helm)
      if (dark) {
        if (isSkin(5, hy + 4)) c.set(5, hy + 4, white);
        if (isSkin(10, hy + 4)) c.set(10, hy + 4, shade(white, 0.9f));
      }
      if (isSkin(7, hy + 5)) c.set(7, hy + 5, R.skin[3]);                  // the nose's lit side
      if (isSkin(5, hy + 5)) c.set(5, hy + 5, R.skin[dark ? 4 : 3]);       // the lit cheek
    } else {
      const int ex = 10 + P.lean;
      if (c.get(ex, hy + 4) != eye) return;
      if (dark && isSkin(ex - 1, hy + 4)) c.set(ex - 1, hy + 4, white);
      if (isSkin(ex - 2, hy + 5)) c.set(ex - 2, hy + 5, R.skin[dark ? 4 : 3]);
    }
  }

  // ---- face paint (1 stripes, 2 dots, 3 mask band, 4 sun mark) and jewellery (1 earrings, 2 torc, 3 both)
  void facePaint() {
    if (!L.facePaint || facing == kUp) return;
    static const uint32_t ochre = rgba(176, 58, 40), chalk = rgba(240, 236, 220), woad = rgba(44, 60, 120);
    const bool side = facing == kSide;
    const int ox = side ? P.lean : 0;
    auto on = [&](int x, int y, uint32_t col) { if (solid(c, x, y)) c.set(x, y, col); };
    switch (L.facePaint) {
      case 1:   // two war stripes under each eye
        if (side) { on(9 + ox, hy + 6, ochre); on(10 + ox, hy + 6, ochre); on(9 + ox, hy + 7, shade(ochre, 0.8f)); }
        else { on(5, hy + 6, ochre); on(6, hy + 6, ochre); on(9, hy + 6, shade(ochre, 0.85f)); on(10, hy + 6, shade(ochre, 0.85f));
               on(5, hy + 7, shade(ochre, 0.8f)); on(10, hy + 7, shade(ochre, 0.7f)); }
        break;
      case 2:   // chalk dots on the cheeks and brow
        if (side) { on(9 + ox, hy + 6, chalk); on(10 + ox, hy + 2, chalk); }
        else { on(5, hy + 6, chalk); on(10, hy + 6, shade(chalk, 0.85f)); on(7, hy + 2, chalk); on(8, hy + 2, shade(chalk, 0.9f)); }
        break;
      case 3:   // a dark band across the eyes
        if (side) { for (int x = 8; x <= 11; x++) if (x != 10) on(x + ox, hy + 4, woad); on(9 + ox, hy + 5, shade(woad, 0.8f)); }
        else { for (int x = 4; x <= 11; x++) if (x != 6 && x != 9) on(x, hy + 4, x < 8 ? woad : shade(woad, 0.8f));
               on(5, hy + 5, shade(woad, 0.85f)); on(10, hy + 5, shade(woad, 0.7f)); on(7, hy + 5, shade(woad, 0.9f)); on(8, hy + 5, shade(woad, 0.9f)); }
        break;
      default:  // a sun mark on the brow
        if (side) { on(10 + ox, hy + 2, kGold[3]); on(11 + ox, hy + 3, kGold[2]); }
        else { on(7, hy + 2, kGold[4]); on(8, hy + 2, kGold[3]); on(7, hy + 1, kGold[2]); on(8, hy + 3, kGold[2]); on(6, hy + 2, kGold[2]); on(9, hy + 2, kGold[1]); }
        break;
    }
  }
  void earrings() {
    if (!(L.jewellery & 1)) return;
    if (facing == kSide) { c.set(7 + P.lean, hy + 6, kGold[3]); return; }
    if (!solid(c, 3, hy + 6)) c.set(3, hy + 6, kGold[facing == kUp ? 2 : 4]);
    if (!solid(c, 12, hy + 6)) c.set(12, hy + 6, kGold[facing == kUp ? 1 : 2]);
  }
  void torc() {
    if (!(L.jewellery & 2)) return;
    if (facing == kSide) { c.set(9 + P.lean, ty, kGold[3]); c.set(10 + P.lean, ty, kGold[2]); return; }
    const bool back = facing == kUp;
    c.set(6, ty, kGold[back ? 2 : 4]); c.set(7, ty, kGold[3]); c.set(8, ty, kGold[back ? 1 : 3]); c.set(9, ty, kGold[back ? 1 : 2]);
    if (!back) c.set(7, ty + 1, kGold[2]);
  }

  // ---- headwear
  Map headwearMap(int fc) const {
    switch (hw) {
      case kHwHood: return fc == kDown ? kHoodD : (fc == kUp ? kHoodU : kHoodS);
      case kHwCap: return fc == kDown ? kBeretD : (fc == kUp ? kBeretU : kBeretS);
      case kHwTurban: return fc == kDown ? kTurbanD : (fc == kUp ? kTurbanU : kTurbanS);
      case kHwFurHat: return fc == kDown ? kFurHatD : (fc == kUp ? kFurHatU : kFurHatS);
      case kHwVeil: return fc == kDown ? kVeilD : (fc == kUp ? kVeilU : kVeilS);
      case kHwCirclet: return fc == kDown ? kCircletD : (fc == kUp ? kCircletU : kCircletS);
      case kHwConical: return fc == kDown ? kConicalD : (fc == kUp ? kConicalU : kConicalS);
      case kHwScarf: return fc == kDown ? kScarfD : (fc == kUp ? kScarfU : kScarfS);
      default: return nullptr;
    }
  }
  bool hwHidesHair() const { return hw && hw != kHwCirclet; }
  // long hair flows out below a hat or helmet (the face stays clear)
  void flowingHair(int ox, int oy) {
    if (L.hair != Hair::Long && L.hair != Hair::Braids && L.hair != Hair::Ponytail && L.hair != Hair::Curls) return;
    Canvas tmp(c.w, c.h);
    drawMap(tmp, hairMap(), ox, oy, R.hair);
    for (int y = hy + 4; y < c.h; y++)
      for (int x = 0; x < c.w; x++)
        if (solid(tmp, x, y) && !(facing == kDown && x >= 5 && x <= 10)) c.set(x, y, tmp.get(x, y));
  }
  void headwear(int ox, int oy) {
    MapInk in;
    in.r = &R.head;
    in.accent = hw == kHwTurban ? kGold[4] : (hw == kHwCirclet ? rgba(120, 210, 230) : 0);
    in.trim = &kGold;
    Ramp fur = ramp(L.headColor ? L.headColor : rgba(120, 92, 70));
    in.fur = &fur;
    if (hw == kHwConical) { static const Ramp straw = ramp5(rgba(96, 70, 44), rgba(150, 112, 60), rgba(196, 160, 90), rgba(226, 198, 126), rgba(244, 228, 170));
                            in.r = L.headColor ? &R.head : &straw; }
    drawMapX(c, headwearMap(facing), ox, oy, in);
    if (hw == kHwConical && facing != kUp) {   // the brim's shadow falls across the brow
      for (int x = 4; x <= 11; x++) if (solid(c, x, hy + 3) && rampIndex(R.skin, c.get(x, hy + 3)) >= 0) c.set(x, hy + 3, R.skin[1]);
    }
    if (hw == kHwHood && facing == kDown) for (int x = 5; x <= 10; x++) c.set(x, hy + 3, R.skin[0]);
  }

  // ---- helm forms (cult::HelmForm + 1)
  Map helmFormMap(int fc, uint32_t& visor, uint32_t& accent) const {
    visor = kInk; accent = 0;
    switch (L.helmForm) {
      case 1: return fc == kDown ? kIronHelmD : (fc == kUp ? kIronHelmU : kIronHelmS);
      case 2: return fc == kDown ? kKettleD : (fc == kUp ? kKettleU : kKettleS);
      case 3: return fc == kDown ? kGreatHelmD : (fc == kUp ? kGreatHelmU : kGreatHelmS);
      case 4: return fc == kDown ? kSpangenD : (fc == kUp ? kSpangenU : kSpangenS);
      case 5: return fc == kDown ? kHornedD : (fc == kUp ? kHornedU : kHornedS);
      case 6: return fc == kDown ? kPlumedD : (fc == kUp ? kPlumedU : kPlumedS);
      case 7: return fc == kDown ? kAventailD : (fc == kUp ? kAventailU : kAventailS);
      case 8: visor = rgba(40, 20, 30); return fc == kDown ? kMaskedD : (fc == kUp ? kMaskedU : kMaskedS);
      case 9: return fc == kDown ? kCrestedD : (fc == kUp ? kCrestedU : kCrestedS);
      case 10: accent = rgba(120, 210, 230); return fc == kDown ? kGildHelmD : (fc == kUp ? kGildHelmU : kGildHelmS);
      default: return nullptr;
    }
  }
  void helmForm(int ox, int oy, const Ramp& mat) {
    uint32_t visor, accent;
    Map m = helmFormMap(facing, visor, accent);
    if (!m) return;
    MapInk in;
    in.r = &mat; in.visor = visor; in.accent = accent;
    in.plume = &R.plume;
    in.trim = (L.armsOrnament & 4) ? &kGold : nullptr;
    in.mail = &mat;
    drawMapX(c, m, ox, oy, in);
    helmExtras(ox, mat);
  }
  // crest dial, plumes and horse-tail on helms without a crest of their own
  void helmExtras(int ox, const Ramp& mat) {
    const int f = L.helmForm;
    const bool crested = f == 6 || f == 9 || f == 8;
    const Ramp& p = R.plume;
    const int cx = facing == kSide ? 6 + ox - 2 : 7;
    if (!crested && (L.crest >= 3 || (L.armsOrnament & 1))) {   // a plume tuft at the crown
      if (facing == kSide) { c.set(cx, hy - 2, p[3]); c.set(cx - 1, hy - 2, p[2]); c.set(cx - 2, hy - 1, p[1]); c.set(cx + 1, hy - 1, p[2]); }
      else { c.set(7, hy - 2, p[3]); c.set(8, hy - 2, p[2]); c.set(6, hy - 1, p[2]); c.set(9, hy - 1, p[1]); }
    }
    if ((L.armsOrnament & 8) || L.crest >= 4) {   // a horse-tail down the back
      if (facing == kUp) for (int y = hy; y <= ty + 3; y++) { c.set(7, y, p[y & 1 ? 1 : 2]); c.set(8, y, p[1]); }
      else if (facing == kSide) for (int y = hy; y <= ty + 2; y++) c.set(3 + ox - (y > hy + 4 ? 1 : 0), y, p[y & 1 ? 1 : 2]);
      else { c.set(3, hy + 4, p[2]); c.set(2, hy + 5, p[1]); c.set(2, hy + 6, p[1]); }
    }
    (void)mat;
  }

  // ---- garment cuts (front / back view): the torso detail, then the hem below the waist
  void cutTorsoFront(bool back) {
    const Ramp& T = R.top;
    const Ramp& Pt = R.pat;
    switch (cutK) {
      case kCutTunic:
        if (!back) { c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[1]); c.set(7, ty + 1, T[1]); }
        beltRow(5, 10, ty + 4);
        break;
      case kCutRobe:   // a crossed collar and a broad sash
        if (!back) { c.set(6, ty, Pt[3]); c.set(7, ty + 1, Pt[3]); c.set(8, ty + 2, Pt[2]); c.set(9, ty, Pt[2]); c.set(8, ty + 1, Pt[2]); c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[0]); }
        for (int x = 5; x <= 10; x++) { c.set(x, ty + 3, Pt[x < 8 ? 3 : 2]); c.set(x, ty + 4, Pt[x < 8 ? 2 : 1]); }
        break;
      case kCutKaftan:   // open down the front over the inner garment, a sash
        if (!back) {
          for (int y = ty + 1; y < hip; y++) { c.set(7, y, R.leg[2]); c.set(8, y, R.leg[1]); c.set(6, y, Pt[3]); c.set(9, y, Pt[2]); }
          c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[0]);
        }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, Pt[x < 8 ? 2 : 1]);
        break;
      case kCutWrap:   // cloth wrapped over one shoulder, the other bare
        if (!back) {
          c.set(5, ty, R.skin[3]); c.set(6, ty, R.skin[2]); c.set(5, ty + 1, R.skin[2]);
          for (int i = 0; i < 4; i++) c.set(6 + i, ty + 1 + i, Pt[3 - (i >> 1)]);
        } else { c.set(9, ty, R.skin[2]); c.set(10, ty, R.skin[1]); for (int i = 0; i < 4; i++) c.set(9 - i, ty + 1 + i, Pt[2]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, Pt[x < 8 ? 2 : 1]);
        break;
      case kCutCoat:   // lapels and a button line
        if (!back) {
          c.set(6, ty, T[4]); c.set(7, ty, R.leg[2]); c.set(8, ty, R.leg[1]); c.set(9, ty, T[1]);
          c.set(7, ty + 1, T[4]); c.set(8, ty + 1, T[1]);
          for (int y = ty + 2; y < hip; y += 2) c.set(8, y, kBrass[3]);
          for (int y = ty + 2; y < hip; y++) c.set(7, y, T[std::min(4, 3)]);
        } else for (int y = ty; y < hip; y++) c.set(7, y, T[1]);
        for (int x = 5; x <= 10; x++) c.set(x, ty, T[x < 8 ? 4 : 2]);   // a raised collar
        break;
      case kCutKilt:   // a plaid sash over the shoulder, a belt with a buckle
        for (int i = 0; i < 5; i++) {
          int x = back ? 10 - i : 5 + i;
          c.set(x, ty + i, Pt[3]);
          if (x + 1 <= 10) c.set(back ? x - 1 : x + 1, ty + i, Pt[2]);
        }
        beltRow(5, 10, ty + 4);
        break;
      case kCutPoncho: break;   // ponchoFront
      case kCutGown:   // a fitted bodice under a high waist band, a low neckline
        if (!back) { c.set(7, ty, R.skin[2]); c.set(8, ty, R.skin[1]); c.set(6, ty, R.skin[2]); c.set(7, ty + 1, T[1]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 2, Pt[x < 8 ? 3 : 2]);
        if (!back) c.set(7, ty + 2, Pt[4]);
        break;
      default: break;
    }
  }
  // a hem layer from row y0 to y1 over (or instead of) the legs, flaring by 'flare' px per 2 rows (cap)
  void hemFront(int y0, int y1, int cap, int split, bool sway, int baseX0 = 5, int baseX1 = 10) {
    const Ramp& r = R.top;
    const int sw = sway ? ((P.liftA > 0) ? -1 : (P.liftB > 0 ? 1 : 0)) : 0;
    for (int y = y0; y <= y1; y++) {
      const int grow = std::min(cap, (y - y0 + 1) / 2);
      int x0 = baseX0 - grow, x1 = baseX1 + grow;
      if (y >= y1 - 1) { x0 += sw; x1 += sw; }
      x0 = std::max(x0, 1); x1 = std::min(x1, 14);
      for (int x = x0; x <= x1; x++) {
        if (split == 1 && (x == 7 || x == 8)) continue;   // open front: the legs show
        int k = 2;
        if (x == x0) k = 3;
        else if (x >= x1 - 1) k = 1;
        if (split == 2 && x == 8 && y > y0) k = 0;          // a centre vent
        if ((x == 6 || x == 9) && y > y0 + 1 && ((y + x) & 1)) k = std::max(0, k - 1);   // folds
        if (y == y1) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
      if (split == 1) { c.set(6 + (y >= y1 - 1 ? sw : 0), y, R.pat[3]); c.set(9 + (y >= y1 - 1 ? sw : 0), y, R.pat[2]); }
    }
  }
  void cutSkirtFront() {
    switch (cutK) {
      case kCutTunic: hemFront(hip, hip + 1, 1, 0, false); break;
      case kCutCoat: hemFront(hip, hip + 3, 2, facing == kDown ? 1 : 2, true); break;
      case kCutKaftan: hemFront(hip, kGround - 1, 2, facing == kDown ? 1 : 0, true); break;
      case kCutKilt: {
        const Ramp& r = R.top;
        for (int y = hip; y <= hip + 2; y++) {
          int x0 = 4 + (y == hip ? 1 : 0), x1 = 11 - (y == hip ? 1 : 0);
          for (int x = x0; x <= x1; x++) {
            int k = x == x0 ? 3 : (x >= x1 - 1 ? 1 : 2);
            if ((x & 1) && y > hip) k = std::max(0, k - 1);   // pleats
            if (y == hip + 2) k = std::max(0, k - 1);
            c.set(x, y, r[k]);
          }
        }
        if (facing == kDown) { c.set(7, hip + 1, kLeather[2]); c.set(8, hip + 1, kLeather[1]); c.set(7, hip, kFur[3]); c.set(8, hip, kFur[2]); }
        for (int side = 0; side < 2; side++) {   // bare knees above the hose
          int x0 = side == 0 ? 5 : 8, lift = side == 0 ? P.liftA : P.liftB;
          int ky = hip + 3;
          if (ky <= kGround - lift - 2) { c.set(x0, ky, R.skin[side ? 2 : 3]); c.set(x0 + 1, ky, R.skin[2]); c.set(x0 + 2, ky, R.skin[1]); }
        }
        break;
      }
      case kCutWrap: {
        hemFront(hip, kGround - 2, 1, 0, true);
        for (int x = 6; x <= 9; x++) if (solid(c, x, hip + 2 + (x - 6))) c.set(x, hip + 2 + (x - 6), R.top[1]);   // the wrap's edge
        // bare shins and sandals
        c.set(6, kGround - 1, R.skin[P.liftA ? 2 : 3]); c.set(9, kGround - 1, R.skin[P.liftB ? 1 : 2]);
        c.set(6, kGround, kLeather[P.liftA ? 1 : 2]); c.set(7, kGround, kLeather[1]); c.set(8, kGround, kLeather[1]); c.set(9, kGround, kLeather[P.liftB ? 0 : 1]);
        break;
      }
      case kCutRobe: {
        hemFront(hip, kGround, 2, 0, true);
        for (int x = 0; x < 16; x++) if (solid(c, x, kGround) && rampIndex(R.top, c.get(x, kGround)) >= 0) c.set(x, kGround, R.pat[x < 8 ? 2 : 1]);
        break;
      }
      case kCutGown: {
        const Ramp& r = R.top;
        const int sw = (P.liftA > 0) ? -1 : (P.liftB > 0 ? 1 : 0);
        for (int y = hip; y <= kGround; y++) {
          int grow = std::min(3, (y - hip + 2) / 2);
          int x0 = 5 - grow, x1 = 10 + grow;
          if (y >= kGround - 1) { x0 += sw; x1 += sw; }
          x0 = std::max(x0, 1); x1 = std::min(x1, 14);
          for (int x = x0; x <= x1; x++) {
            int k = x == x0 ? 3 : (x >= x1 - 1 ? 1 : 2);
            if (((x - x0) % 3 == 2) && y > hip + 1) k = std::max(0, k - 1);   // long falling folds
            if (((x - x0) % 3 == 1) && y > hip + 1 && x < x1 - 1) k = std::min(4, k + 1);
            if (y == kGround) k = std::max(0, k - 1);
            c.set(x, y, r[k]);
          }
        }
        for (int x = 0; x < 16; x++) if (solid(c, x, kGround) && rampIndex(R.top, c.get(x, kGround)) >= 0) c.set(x, kGround, R.pat[x < 8 ? 3 : 2]);
        break;
      }
      default: break;
    }
  }
  // the poncho: a wide triangle over the shoulders and arms, the hands below its edge
  void ponchoFront() {
    if (!poncho() || P.atk) {
      if (poncho()) for (int y = ty; y <= ty + 4; y++) for (int x = 5; x <= 10; x++) c.set(x, y, R.top[x < 8 ? 3 : 2]);
      return;
    }
    const Ramp& r = R.top;
    for (int x = 2; x <= 13; x++) {
      const int d = x < 8 ? 7 - x : x - 8;   // distance from the centre
      const int bottom = ty + 6 - (d + 1) / 2;
      for (int y = ty - 1 + (d >= 5 ? 1 : 0); y <= bottom; y++) {
        int k = x < 6 ? 3 : (x > 10 ? 1 : 2);
        if (y == ty - 1 || (y == ty && d >= 5)) k = std::min(4, k + 1);   // the shoulders catch the light
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
      // a woven band near the hem
      const int by = bottom - 1;
      if (by > ty) c.set(x, by, R.pat[x < 8 ? 3 : 2]);
    }
    if (facing == kDown) { c.set(7, ty - 1, R.skin[1]); c.set(8, ty - 1, R.skin[0]); }
  }
  void ponchoSide() {
    if (!poncho() || P.atk) return;
    const Ramp& r = R.top;
    const int ox = P.lean;
    for (int x = 4; x <= 12; x++) {
      const int d = std::abs(x - 8);
      const int bottom = ty + 5 - d / 2;
      for (int y = ty - 1 + (d >= 4 ? 1 : 0); y <= bottom; y++) {
        int k = x < 7 ? 3 : (x > 10 ? 1 : 2);
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x + ox, y, r[k]);
      }
      if (bottom - 1 > ty) c.set(x + ox, bottom - 1, R.pat[x < 8 ? 3 : 2]);
    }
  }
  void cutTorsoSide() {
    const int ox = P.lean;
    const Ramp& Pt = R.pat;
    switch (cutK) {
      case kCutTunic: c.set(10 + ox, ty, R.skin[1]); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case kCutRobe: c.set(10 + ox, ty, Pt[3]); c.set(9 + ox, ty + 1, Pt[3]); for (int x = 6; x <= 10; x++) { c.set(x + ox, ty + 3, Pt[3]); c.set(x + ox, ty + 4, Pt[2]); } break;
      case kCutKaftan: for (int y = ty; y < hip; y++) c.set(10 + ox, y, Pt[2]); for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, Pt[2]); break;
      case kCutWrap: c.set(9 + ox, ty, R.skin[2]); for (int i = 0; i < 4; i++) c.set(10 + ox - i / 2, ty + 1 + i, Pt[3]); for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, Pt[2]); break;
      case kCutCoat: for (int x = 6; x <= 10; x++) c.set(x + ox, ty, R.top[x < 9 ? 4 : 2]); c.set(10 + ox, ty + 2, kBrass[3]); c.set(10 + ox, ty + 4, kBrass[3]); break;
      case kCutKilt: for (int i = 0; i < 5; i++) c.set(7 + ox + (i >> 1), ty + i, Pt[3]); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case kCutGown: c.set(10 + ox, ty, R.skin[2]); for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 2, Pt[x < 9 ? 3 : 2]); break;
      default: break;
    }
  }
  void hemSide(int y0, int y1, int cap, bool sway, int vent) {
    const Ramp& r = R.top;
    const int sw = sway ? (P.stepA > 0 ? 1 : (P.stepA < 0 ? -1 : 0)) : 0;
    for (int y = y0; y <= y1; y++) {
      int grow = std::min(cap, (y - y0 + 1) / 2);
      int x0 = 6 - grow + P.lean, x1 = 10 + grow + P.lean;
      if (y >= y1 - 1) { x0 += sw; x1 += sw; }
      x0 = std::max(x0, 1); x1 = std::min(x1, 14);
      for (int x = x0; x <= x1; x++) {
        int k = x == x0 + 1 ? 3 : (x >= x1 - 1 ? 1 : 2);
        if (vent && x == x1 - 1 - vent && y > y0 + 1) k = 0;
        if (y == y1) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
    }
  }
  void cutSkirtSide() {
    switch (cutK) {
      case kCutTunic: hemSide(hip, hip + 1, 1, false, 0); break;
      case kCutCoat: hemSide(hip, hip + 3, 1, true, 0); break;
      case kCutKaftan: hemSide(hip, kGround - 1, 2, true, 1); break;
      case kCutKilt: {
        for (int y = hip; y <= hip + 2; y++)
          for (int x = 5 + P.lean; x <= 11 + P.lean; x++) {
            int k = ((x & 1) && y > hip) ? 1 : 2;
            if (x == 5 + P.lean) k = 3;
            if (y == hip + 2) k = std::max(0, k - 1);
            c.set(x, y, R.top[k]);
          }
        break;
      }
      case kCutWrap: {
        hemSide(hip, kGround - 2, 1, true, 0);
        const int sw = P.stepA > 0 ? 1 : (P.stepA < 0 ? -1 : 0);
        c.set(8 + sw, kGround - 1, R.skin[2]); c.set(9 + sw, kGround - 1, R.skin[1]);
        c.set(8 + sw, kGround, kLeather[2]); c.set(9 + sw, kGround, kLeather[1]); c.set(10 + sw, kGround, kLeather[0]);
        break;
      }
      case kCutRobe:
        hemSide(hip, kGround, 2, true, 0);
        for (int x = 0; x < 16; x++) if (solid(c, x, kGround) && rampIndex(R.top, c.get(x, kGround)) >= 0) c.set(x, kGround, R.pat[1]);
        break;
      case kCutGown:
        hemSide(hip, kGround, 3, true, 0);
        for (int y = kGround - 2; y <= kGround; y++) c.set(3 + P.lean, y, R.top[y == kGround ? 0 : 1]);   // the train behind
        for (int x = 0; x < 16; x++) if (solid(c, x, kGround) && rampIndex(R.top, c.get(x, kGround)) >= 0) c.set(x, kGround, R.pat[2]);
        break;
      default: break;
    }
  }

  // ---- body forms (cult::BodyArm + 1): the torso's construction over the band's material
  void formTorsoFront(bool back) {
    const Ramp& T = R.top;
    switch (form) {
      case kFormPadded:   // a quilted gambeson with a thick collar
        for (int x = 5; x <= 10; x++) c.set(x, ty, T[x < 8 ? 4 : 3]);
        beltRow(5, 10, ty + 4);
        break;
      case kFormLeather:
        for (int x = 5; x <= 10; x++) c.set(x, ty, kLeather[x < 9 ? 3 : 2]);
        if (!back) line(c, 5, ty + 1, 9, ty + 4, kLeather[0]);
        beltRow(5, 10, ty + 4);
        break;
      case kFormBrigandine:
        for (int x = 5; x <= 10; x++) c.set(x, ty, R.mat[x < 8 ? 3 : 2]);
        beltRow(5, 10, ty + 4);
        break;
      case kFormPlate:
        if (!back) { c.set(6, ty + 1, T[4]); c.set(6, ty + 2, T[4]); c.set(9, ty + 3, T[1]); for (int y = ty + 1; y <= ty + 3; y++) c.set(7, y, T[3]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, T[x < 8 ? 1 : 0]);
        break;
      default:
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.belt[x < 9 ? 1 : 0]);
        if (facing == kDown) c.set(7, ty + 4, R.gilt[3]);
        break;
    }
    if (L.outfit == Outfit::Guard && (form == kFormMail || form == kFormScale || form == kFormLamellar || form == kFormPlate || form == kFormLeaf || form == kFormLeather)) {
      // the guard's tabard in the kingdom's colours, over the armour
      for (int y = ty + 1; y <= hip; y++)
        for (int x = 6; x <= 9; x++) c.set(x, y, R.trim[y == hip ? 1 : (x < 8 ? 3 : 2)]);
      if (!back) { c.set(7, ty + 2, kGold[3]); c.set(8, ty + 2, kGold[2]); c.set(7, ty + 3, kGold[2]); }
    }
  }
  void formTorsoSide() {
    const int ox = P.lean;
    const Ramp& T = R.top;
    switch (form) {
      case kFormPadded: case kFormBrigandine: case kFormLeather:
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty, (form == kFormBrigandine ? R.mat : (form == kFormLeather ? kLeather : T))[3]);
        beltRow(6 + ox, 10 + ox, ty + 4);
        break;
      case kFormPlate: c.set(9 + ox, ty + 1, T[4]); for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, T[1]); break;
      default: for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, R.belt[1]); break;
    }
    if (L.outfit == Outfit::Guard && (form == kFormMail || form == kFormScale || form == kFormLamellar || form == kFormPlate || form == kFormLeaf || form == kFormLeather))
      for (int y = ty + 1; y <= hip; y++) { c.set(9 + ox, y, R.trim[2]); c.set(10 + ox, y, R.trim[1]); }
  }
  // the armour skirt (tassets, a mail or lamellar skirt): L.skirt - 1 rows below the waist
  int tassetRows() const { return form && L.skirt > 1 ? std::min(3, L.skirt - 1) : 0; }
  void tassets() {
    const int n = tassetRows();
    if (!n) return;
    const Ramp& T = R.top;
    const bool side = facing == kSide;
    const int ox = side ? P.lean : 0;
    for (int i = 0; i < n; i++) {
      const int y = hip + i;
      int x0 = side ? 5 : 4, x1 = side ? 11 : 11;
      if (i == 0) { x0++; x1--; }
      for (int x = x0; x <= x1; x++) {
        int k = x == x0 ? 3 : (x >= x1 - 1 ? 1 : 2);
        if (i == n - 1) k = std::max(0, k - 1);
        if (!side && (x == 7 || x == 8) && i > 0 && form != kFormMail) k = 0;   // split at the front for the stride
        if ((L.armsOrnament & 128) && i == n - 1 && ((x + (int)P.liftA) & 1)) continue;   // scalloped hem
        c.set(x + ox, y, T[k]);
      }
    }
    if (L.armsOrnament & 64) {   // tassels at the hem
      const int y = hip + n;
      if (side) c.set(10 + ox, y, R.plume[2]);
      else { c.set(5, y, R.plume[3]); c.set(10, y, R.plume[1]); }
    }
  }
  // the texture of the body form, over every pixel of its material in the torso, sleeves and tassets
  void formTexture() {
    if (!form) return;
    const Ramp& T = R.top;
    const int y1 = hip - 1 + tassetRows();
    for (int y = ty; y <= y1; y++)
      for (int x = 0; x < 16; x++) {
        const int k = rampIndex(T, c.get(x, y));
        if (k < 0) continue;
        const int ry = y - ty;
        int nk = k;
        switch (form) {
          // (M6 fixer round 2, review: "a worn padded gambeson is a harsh single-pixel checker") on a torso six pixels
          // wide the quilting is vertical stitched channels (every third column, one step down and never into the
          // ramp's near-black), with one stitched row at the waist: a grid (or every other row) made a checker at 1x
          case kFormPadded: if (ry > 0 && ((x % 3) == 0 || ry == 3)) nk = std::max(1, k - 1); break;   // quilting
          case kFormMail: { const int q = (x + 2 * y) & 3; if (q == 0) nk = k - 1; else if (q == 2 && k < 3) nk = k + 1; break; }   // ring rows
          case kFormScale: {   // overlapping scales, lit along their upper edges, a dark crescent under each
            const int ph = (ry >> 1) & 1, col = (x + ph) & 1;
            if ((ry & 1) == 0) nk = col == 0 ? k + 1 : k;
            else nk = col == 1 ? k - 2 : k - 1;
            break;
          }
          case kFormLamellar:   // rows of small plates laced in the cloth colour
            if (ry > 0 && (ry % 2) == 0) { if (x & 1) { c.set(x, y, L.outfit == Outfit::Guard ? R.trim[2] : R.plume[2]); continue; } nk = k - 1; }
            else if (x & 1) nk = k - 1;
            break;
          case kFormBrigandine: if (ry > 0 && (ry & 1) && ((x + (ry >> 1)) & 1)) { c.set(x, y, R.mat[4]); continue; } break;   // rivets
          case kFormLeaf: {   // overlapping leaf plates pointing down: a lit midrib, dark edges closing to the tip
            const int ph = (ry / 2) & 1;
            const int m = (x + ph * 2) % 3;
            if ((ry & 1) == 0) nk = m == 1 ? k + 1 : (m == 2 ? k - 1 : k);
            else nk = m == 1 ? k : k - 2;
            if (ry == 2 && (x == 7 || x == 8) && facing == kDown) { c.set(x, y, R.accent); continue; }
            break;
          }
          case kFormPlate:   // a smooth breastplate over two lames of faulds
            if (y == hip - 2) nk = k - 2;
            else if (y == hip - 1) nk = (x & 1) ? k : k + 1;
            else if (y >= hip) nk = (x & 1) ? k - 1 : k;
            break;
          default: break;
        }
        c.set(x, y, T[std::clamp(nk, 0, 4)]);
      }
    ornamentsBody();
  }
  void ornamentsBody() {
    const uint16_t o = L.armsOrnament;
    if (!o) return;
    const bool side = facing == kSide;
    const int ox = side ? P.lean : 0;
    auto onBody = [&](int x, int y, uint32_t col) { if (solid(c, x, y)) c.set(x, y, col); };
    if ((o & 16) && form != kFormPadded) {   // studs (not on quilted cloth: they read as yellow flecks there)
      static const int sx[5] = {6, 9, 6, 9, 7}, sy[5] = {1, 1, 3, 3, 2};
      for (int i = 0; i < 5; i++) onBody(side ? 7 + ox + (i & 1) * 2 : sx[i], ty + sy[i], kBrass[4]);
    }
    if ((o & 32) && !side && facing == kDown) { onBody(6, ty + 2, R.top[4]); onBody(9, ty + 2, R.top[3]); onBody(7, ty + 3, R.top[4]); onBody(8, ty + 1, R.top[4]); }   // etching
    if ((o & 512) && facing == kDown) { onBody(6, ty + 1, kGold[3]); onBody(7, ty + 2, kGold[4]); onBody(8, ty + 2, kGold[3]); onBody(9, ty + 1, kGold[2]); }   // filigree
    if (o & 4) {   // gilding: the waist line
      const int y = ty + 4;
      for (int x = 0; x < 16; x++) if (solid(c, x, y) && (x >= (side ? 6 + ox : 5)) && (x <= (side ? 10 + ox : 10))) c.set(x, y, kGold[x < 8 ? 3 : 2]);
    }
    if (o & 2) {   // fur trim at the collar
      if (side) furRow(6 + ox, 10 + ox, ty);
      else furRow(5, 10, ty);
    }
  }
  void furRow(int x0, int x1, int y) { furRow(x0, x1, y, 3); }

  // ---- pauldrons by the culture's dial (L.pauldron - 1: 0 none .. 3 huge)
  bool m3Pauldron() const { return form && L.pauldron; }
  void pauldronM3(int x0, bool lit) {
    const int d = L.pauldron - 1;
    if (d <= 0) return;
    const Ramp& s = form >= kFormMail ? R.mat : (form == kFormLeather ? kLeather : R.top);
    const bool gilt = (L.armsOrnament & 4) != 0;
    if (d == 1) {
      const int px = lit ? x0 - 1 : x0 + 1;
      c.set(px, ty, s[lit ? 4 : 2]); c.set(px + 1, ty, s[lit ? 3 : 1]);
      c.set(px, ty + 1, s[lit ? 2 : 1]); c.set(px + 1, ty + 1, s[lit ? 2 : 0]);
    } else if (d == 2) {
      const int px = lit ? x0 - 1 : x0;
      for (int i = 0; i < 3; i++) { c.set(px + i, ty, s[lit ? 4 - i : 3 - i]); c.set(px + i, ty + 1, (gilt ? kGold : s)[lit ? 2 : 1]); }
      c.set(px + 1, ty + 2, s[1]);
    } else {
      const int px = lit ? x0 - 2 : x0;
      for (int i = 1; i < 4; i++) c.set(px + i - (lit ? 0 : 1), ty - 1, s[lit ? 4 : 3]);
      for (int i = 0; i < 4; i++) { c.set(px + i, ty, s[lit ? 4 - (i >> 1) : 3 - (i >> 1)]); c.set(px + i, ty + 1, s[lit ? 2 : 1]); c.set(px + i, ty + 2, (gilt ? kGold : s)[lit ? 2 : 0]); }
      if (L.armsOrnament & 256) c.set(px + (lit ? 1 : 2), ty, kWhite);   // a rivet
    }
    if (L.armsOrnament & 64) c.set(lit ? x0 - 1 : x0 + 2, ty + d + 1, R.plume[lit ? 3 : 1]);   // tassels
  }

  // ---- cloth patterns over every garment pixel (cult::Pattern: 1 stripes, 2 checks, 3 border trim, 4 dots,
  //      5 embroidery, 6 vines); subtle at this scale
  void clothPattern() {
    if (!L.pattern || L.pattern > 6) return;
    const Ramp& T = R.top;
    const Ramp& Pt = R.pat;
    int hem[16];   // the lowest garment pixel per column (border trim, embroidery, vines)
    for (int x = 0; x < 16; x++) {
      hem[x] = -1;
      for (int y = kGround; y >= ty; y--) if (rampIndex(T, c.get(x, y)) >= 0) { hem[x] = y; break; }
    }
    for (int y = ty; y <= kGround; y++)
      for (int x = 0; x < 16; x++) {
        const int k = rampIndex(T, c.get(x, y));
        if (k < 0) continue;
        const int ry = y - ty;
        uint32_t v = 0;
        switch (L.pattern) {
          case 1: if (ry % 3 == 1) v = Pt[k]; break;
          case 2: {
            const bool a = x % 3 == 0, b = ry % 3 == 0;
            if (a && b) v = Pt[k];
            else if (a || b) v = mix(T[k], Pt[k], 0.5f);
            break;
          }
          case 3: if (y == hem[x] || y == hem[x] - 1 || ry == 0) v = Pt[std::min(4, k + (y == hem[x] ? 0 : 1))]; break;
          case 4: if ((ry & 1) == 1 && ((x + ry) % 3) == 0) v = Pt[std::min(4, k + 1)]; break;
          case 5:
            if (y == hem[x] - 1 && (x & 1)) v = Pt[std::min(4, k + 1)];
            else if (y == hem[x]) v = Pt[k];
            else if (facing == kDown && ry >= 1 && ry <= 2 && (x == 7 || x == 8)) v = Pt[std::min(4, k + 1)];
            break;
          case 6: {
            const int wave = ((x >> 1) & 1);
            if (y == hem[x] - 1 - wave) v = Pt[std::min(4, k + 1)];
            else if (y == hem[x] - 2 - wave && (x % 4) == 1) v = mix(Pt[k], rgba(110, 170, 90), 0.5f);
            break;
          }
          default: break;
        }
        if (v) c.set(x, y, v);
      }
  }

  // ---------------------------------------------------------------------- composition
  void paint() {
    if (facing == kDown) paintDown();
    else if (facing == kUp) paintUp();
    else paintSide();
    if (form) formTexture();
    if (cutK) clothPattern();
    if (L.jewellery) torc();
    finishM6();
  }

  // ====================================================================== M6 Steel: the metal's sheen, legendary glow
  void finishM6() {
    if (R.tinted) sheenPass();
    if (L.glow) glowPass();
  }
  // the sheen's pattern over every pixel of the alloy (the ramp itself carries matte, dark, iridescent and pale)
  void sheenPass() {
    const int sh = L.sheen;
    if (sh != 2 && sh != 4 && sh != 6 && sh != 8) return;
    const Canvas src = c;
    const Ramp& A = R.alloy;
    auto isA = [&](int x, int y) { return rampIndex(A, src.get(x, y)) >= 0; };
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const int k = rampIndex(A, src.get(x, y));
        if (k < 0) continue;
        int nk = k;
        switch (sh) {
          case 2:   // bright: the lit rims flash white
            if (k >= 3 && (!isA(x - 1, y) || !isA(x, y - 1)) && ((x + y) & 1)) { c.set(x, y, mix(A[4], kWhite, 0.6f)); continue; }
            break;
          case 4: {   // banded (pattern-welded): wavy diagonal bands of dark and light
            const int b = (x + 2 * y + ((x >> 2) & 1)) % 5;
            if (b == 0 && k >= 2) nk = k - 1;
            else if (b == 2 && k == 2) nk = 3;
            break;
          }
          case 6:   // glowing: the seams (the darkest steps) are lit from within
            if (k == 0 && (y & 1) == 0) { c.set(x, y, R.seam); continue; }
            if (k == 0) { c.set(x, y, mix(R.seam, A[0], 0.6f)); continue; }
            break;
          case 8:   // burnished: polish streaks on the lit faces
            if (k == 3 && ((x + y) & 3) == 0) nk = 4;
            break;
          default: break;
        }
        if (nk != k) c.set(x, y, A[std::clamp(nk, 0, 4)]);
      }
  }
  // which glowing piece a pixel belongs to (HumanLook::glow bits; 0 none), judged by the ramp it was painted in
  int pieceAt(const Canvas& src, int x, int y) const {
    const uint32_t v = src.get(x, y);
    if (!(v >> 24)) return 0;
    const int g = L.glow;
    if ((g & 1) && L.weapon && rampIndex(R.metal, v) >= 0) return 1;
    if ((g & 8) && hasShield() && rampIndex(shieldRamp, v) >= 0) return 8;
    if ((g & 16) && L.cloak && rampIndex(R.cloak, v) >= 0) return 16;
    const bool helmPx = helmRamp && rampIndex(*helmRamp, v) >= 0;
    const bool armourPx = helmPx || (R.tinted && rampIndex(R.alloy, v) >= 0) ||
                          ((armor || form) && (rampIndex(R.top, v) >= 0 || rampIndex(R.mat, v) >= 0));
    if (!armourPx) return 0;
    const bool headZone = y < ty - 1 || (y <= hy + 7 && helmPx);
    if (headZone) return (g & 4) ? 4 : 0;
    return (g & 2) ? 2 : 0;
  }
  uint32_t glowCol() const { return L.glowColor ? opaque(L.glowColor) : rgba(255, 190, 80); }
  // a legendary piece's glint: one bright point travelling over the piece frame by frame (never a blob)
  void glowPass() {
    const Canvas src = c;
    const uint32_t gc = glowCol();
    for (int bit = 1; bit <= 16; bit <<= 1) {
      if (!(L.glow & bit)) continue;
      int pts[96][2], n = 0;
      for (int y = 0; y < c.h && n < 96; y++)
        for (int x = 1; x < c.w - 1 && n < 96; x++)
          if (pieceAt(src, x, y) == bit && luma(src.get(x, y)) >= 0.35f) { pts[n][0] = x; pts[n][1] = y; n++; }
      if (!n) continue;
      const int i = (frameNo * 7 + bit * 3) % n;
      const int x = pts[i][0], y = pts[i][1];
      c.set(x, y, mix(gc, kWhite, 0.55f));
      if (frameNo & 1) {   // the pulse: on alternate frames the glint spreads to a soft cross
        for (int d = 0; d < 4; d++) {
          const int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
          if (pieceAt(src, nx, ny) == bit) c.set(nx, ny, mix(src.get(nx, ny), gc, 0.5f));
        }
      }
    }
  }
  // after the outline (humanSheet): the outline around a glowing piece takes its colour, brighter on alternate frames
  void glowRim(Canvas& out, const Canvas& raw) const {
    if (!L.glow) return;
    const uint32_t gc = glowCol();
    const float a = (frameNo & 1) ? 0.62f : 0.38f;
    for (int y = 0; y < out.h; y++)
      for (int x = 0; x < out.w; x++) {
        if ((raw.get(x, y) >> 24) || !(out.get(x, y) >> 24)) continue;
        bool near = false;
        for (int d = 0; d < 4 && !near; d++) near = pieceAt(raw, x + (d == 0) - (d == 1), y + (d == 2) - (d == 3)) != 0;
        if (near) out.set(x, y, mix(out.get(x, y), gc, a));
      }
  }

  void paintDown() {
    backItems();
    if (L.cloak) cloakFront();
    capeFrontSliver();
    legsFront();
    torsoFront(false);
    buildFront();
    skirtFront();
    if (form) tassets();
    if (cutK) ponchoFront();
    armFront(11, P.swingB, false);   // off hand (screen right)
    // (M6 fixer r4, review: "the spear head covers the hero's face") a polearm at rest stands behind the head: its
    // shaft is painted before the head (the hand still closes over it below), so the face and helm stay clear
    const bool poleBack = !P.atk && L.weapon == 8;
    if (poleBack) weapon(3, ty + 5 + P.swingA, 0, -1, -1);
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
      if (L.weapon == 8 && L.polearmForm) handPx(3, ty + 1, false);   // M6: the polearm's second hand up the shaft
    } else {
      armFront(3, P.swingA, true);
      int hy2 = ty + 5 + P.swingA;
      if (bow && L.bowForm == 4) weapon(3, hy2, 0, 1, 1);   // M6: a crossbow is carried nose down
      else if (bow) weapon(3, hy2, -1, 0, 1);
      else if (poleBack) {}   // (painted behind the head above)
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
    if (form) tassets();
    if (cutK) ponchoFront();
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
    if (form) tassets();
    if (cutK) ponchoSide();
    if (L.cloak) cloakCollarSide();
    if (hasShield()) shield(11 + P.lean, ty + 1, true);
    // (M6 fixer r4) a polearm at rest stands behind the head (see paintDown)
    const bool poleBack = !P.atk && L.weapon == 8;
    if (poleBack) weapon(8 + P.lean + P.swingA, ty + 5, 0, -1, -1);
    head();
    amuletSide();
    int sx = 8 + P.lean;
    if (P.atk && L.weapon == 8 && L.polearmForm == 1) {   // M6: the spear is couched two-handed and thrust level
      const int hx = (P.atk == 1 ? 2 : 3) + P.lean, hy2 = ty + 3;
      weapon(hx, hy2, 1, 0, -1);
      armLine(sx, ty + 1, hx + 3, hy2);
      handAt(hx + 3, hy2);
      handAt(hx + 1, hy2, -1);
      return;
    }
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
      else if (poleBack) {}   // (painted behind the head above)
      else if (isStaff()) weapon(hx, hy2, 0, -1, -1);
      else if (heavyHead()) weapon(hx, hy2, 0, 1, 1);
      else weapon(hx, hy2, 1, -1, 1);   // blade raised in a ready stance
      handAt(hx, hy2);
    }
  }
};

// ====================================================================== M5 "Hearth and Hall": postures (rpg/art/art_life.h)
// What townsfolk do with their bodies, painted on the same rig as the standing sheet so every look (outfits, culture
// cuts, headwear, peoples, armour) carries over: the head, the torso and the garment are the rig's own; the legs (seated
// on a chair, cross-legged on the floor) and the arms (with the tool, the cup, the instrument) are the posture's. Same
// camera, light and palette; the cell is outlined by the caller like the standing sheet.
enum PzSeat { kPzStand = 0, kPzChair, kPzFloor };
int pzSeat(Posture p) {
  switch (p) {
    case Posture::Sit: case Posture::SitEat: case Posture::SitDrink: return kPzChair;
    case Posture::SitFloor: case Posture::SitFloorEat: case Posture::SitFloorDrink: case Posture::Beg: return kPzFloor;
    default: return kPzStand;
  }
}
// the body's pose for a posture frame (legs and bob; the arms are the posture's own)
Pose posturePose(Posture p, int facing, int frame) {
  Pose q;
  switch (p) {
    case Posture::Carry: q = humanPose(facing, 1 + (frame & 3)); break;
    case Posture::Play: q = humanPose(facing, 1 + (frame & 3)); q.bob = (frame & 1) ? 0 : -1; break;   // a hop
    case Posture::Dance: {
      static const int lA[4] = {2, 0, 0, 0}, lB[4] = {0, 0, 2, 0}, bob[4] = {-1, 0, -1, 0};
      q.liftA = lA[frame & 3]; q.liftB = lB[frame & 3]; q.bob = bob[frame & 3];
      if (facing == kSide) { q.stepA = (frame & 3) == 0 ? 2 : ((frame & 3) == 2 ? -2 : 0); q.stepB = -q.stepA; q.liftA = lA[frame & 3]; q.liftB = lB[frame & 3]; }
      break;
    }
    case Posture::Pray: q.bob = frame & 1; break;
    case Posture::Hammer: case Posture::Chop: if (facing == kSide) q.lean = (frame & 1) ? 1 : 0; break;
    case Posture::Hoe: if (facing == kSide && (frame & 1)) q.lean = 1; break;
    case Posture::SitFloor: case Posture::SitFloorEat: case Posture::SitFloorDrink: case Posture::Beg: q.bob = 3; break;
    default: break;
  }
  q.swingA = q.swingB = 0;
  return q;
}

struct PostureRig : HumanPainter {
  Posture pz;
  int fr;
  uint8_t var;
  int seat;
  PostureRig(Canvas& c_, const HumanLook& l, int f, const Pose& p, Posture z, int frame, uint8_t v)
      : HumanPainter(c_, l, f, p), pz(z), fr(frame), var(v), seat(pzSeat(z)) {
    frameNo = frame;
    // a hop never lifts the hat (or the hair) out of the cell: the head's top row keeps a pixel for its outline
    const int minHy = std::max(2, 4 - headTopRow(facing));
    if (hy < minHy) { hy = minHy; ty = hy + 8; hip = ty + 6; }
    // busy arms: the poncho shows only its yoke and the cloak's drape keeps clear of a raised arm
    if (pz != Posture::Sit && pz != Posture::SitFloor) P.atk = litArmHigh() ? 1 : 2;
  }
  bool litArmHigh() const {
    switch (pz) {
      case Posture::Cheer: case Posture::Wave: case Posture::Lamp: case Posture::Play: case Posture::Carry: return true;
      case Posture::Hammer: case Posture::Chop: case Posture::Hoe: return !(fr & 1);
      case Posture::Dance: return (fr & 3) == 0;
      default: return false;
    }
  }

  // ------------------------------------------------------------------ small helpers
  // a 2-px sleeve from the shoulder through the elbow to the wrist and the hand (bias -1: the far / shaded arm)
  void arm(int sx, int sy, int ex, int ey, int hx, int hy2, int bias, bool showHand = true) {
    ex = std::clamp(ex, 2, 14); hx = std::clamp(hx, 2, 14);   // the sleeve and the hand keep a pixel for the outline
    if (poncho() && !P.atk) { if (showHand) handAt(hx, hy2, bias); return; }   // the poncho covers resting arms
    armLine(sx, sy, ex, ey, bias);
    armLine(ex, ey, hx, hy2 + (showHand ? -1 : 0), bias);
    if (wideSleeve() && showHand) {   // the wide cuff flares past the wrist (kept a pixel inside the cell)
      const int fx = hx + (hx < 8 ? -2 : 1);
      if (fx >= 1 && fx <= 14) c.set(fx, hy2 - 1, R.sleeve[std::max(0, 2 + bias)]);
    }
    if (showHand) handAt(hx, hy2, bias);
  }
  // front / back view arms from the shoulders: the lit one screen-left, the other screen-right
  void armL(int ex, int ey, int hx, int hy2, bool hand = true) { c.set(4, ty, R.sleeve[4]); arm(4, ty + 1, ex, ey, hx, hy2, 0, hand); }
  void armR(int ex, int ey, int hx, int hy2, bool hand = true) { c.set(11, ty, R.sleeve[2]); arm(12, ty + 1, ex, ey, hx, hy2, -1, hand); }
  // side view: the near arm (after the body) and the far one (before it)
  int sxS() const { return 8 + P.lean; }
  void armNear(int ex, int ey, int hx, int hy2, bool hand = true) { arm(sxS(), ty + 1, ex, ey, hx, hy2, 0, hand); }
  void armFar(int ex, int ey, int hx, int hy2, bool hand = true) { arm(sxS() - 1, ty + 1, ex, ey, hx, hy2, -1, hand); }
  void restArmsFront() {   // hanging at the sides (the standing sheet's idle arms)
    armFront(3, 0, true);
  }
  // a shaded straight stick (hafts, poles, rods, necks): lit on its upper-left pixel
  void stick(float x0, float y0, float x1, float y1, const Ramp& r, int k = 2) {
    const int n = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)));
    for (int i = 0; i <= n; i++) {
      const float t = n ? (float)i / n : 0;
      const int x = (int)std::floor(x0 + (x1 - x0) * t + 0.5f), y = (int)std::floor(y0 + (y1 - y0) * t + 0.5f);
      c.set(x, y, r[i < n / 3 ? k + 1 : k]);
    }
  }
  void mug(int x, int y, bool up) {   // a wooden tankard, 3x3 (x, y top-left), handle on the right; up: raised to drink
    const Ramp& W = kWood;
    if (!up) { c.set(x, y, kWhite); c.set(x + 1, y, kCloth[4]); }
    else { c.set(x, y, W[3]); c.set(x + 1, y, W[2]); }
    c.set(x, y + 1, W[4]); c.set(x + 1, y + 1, W[2]);
    c.set(x, y + 2, kIron[3]); c.set(x + 1, y + 2, kIron[1]);
    c.set(x + 2, y + 1, W[1]); c.set(x + 2, y, W[2]);
  }
  void bread(int x, int y) {   // a heel of bread 3x2
    c.set(x, y, kSand[4]); c.set(x + 1, y, kSand[3]); c.set(x + 2, y, kSand[2]);
    c.set(x, y + 1, kSand[2]); c.set(x + 1, y + 1, kSand[1]); c.set(x + 2, y + 1, kLeather[2]);
  }
  void bowl(int x, int y, bool coin) {   // a wooden begging bowl 4x2 seen from above
    c.set(x, y, kWood[4]); c.set(x + 1, y, kWoodDark[0]); c.set(x + 2, y, kWoodDark[0]); c.set(x + 3, y, kWood[2]);
    c.set(x, y + 1, kWood[2]); c.set(x + 1, y + 1, kWood[3]); c.set(x + 2, y + 1, kWood[1]); c.set(x + 3, y + 1, kWood[0]);
    if (coin) c.set(x + 1, y, kGold[4]);
  }
  void book(int x0, int y0, bool turning) {   // an open book 6x3 held in both hands (x0, y0 top-left), its cover below
    for (int x = x0; x <= x0 + 5; x++) {
      const bool leftPage = x < x0 + 3;
      c.set(x, y0, leftPage ? kWhite : kCloth[4]);
      c.set(x, y0 + 1, leftPage ? kCloth[4] : kCloth[3]);
      c.set(x, y0 + 2, kRed[x == x0 ? 3 : 2]);
    }
    c.set(x0 + 2, y0, kCloth[2]); c.set(x0 + 3, y0 + 1, kCloth[1]);   // the gutter
    c.set(x0 + 1, y0 + 1, kCloth[2]); c.set(x0 + 4, y0, kCloth[2]);   // lines of writing
    if (turning) { c.set(x0 + 3, y0 - 1, kWhite); c.set(x0 + 4, y0 - 1, kCloth[4]); c.set(x0 + 4, y0 - 2, kWhite); }
  }
  void flame(int x, int y) {   // a taper's flame: two pixels, the core bright
    c.set(x, y, kFire[4]); c.set(x, y - 1, kFire[3]);
  }
  // tool heads
  void hammerHead(int x, int y, bool down) {   // 3x2 iron block centred on (x, y); down: the face toward the ground
    for (int i = -1; i <= 1; i++) { c.set(x + i, y, kIron[i < 0 ? 4 : (i == 0 ? 3 : 2)]); c.set(x + i, y + 1, kIron[i < 1 ? 2 : 1]); }
    if (down) c.set(x, y + 1, kIron[3]);
  }
  void axeHead(int x, int y, int dir) {   // the bit beside the haft end (dir +1: the edge to the right, -1 left)
    c.set(x + dir, y, kIron[3]); c.set(x + dir, y + 1, kIron[2]); c.set(x + 2 * dir, y - 1, kIron[4]); c.set(x + 2 * dir, y, kIron[4]);
    c.set(x + 2 * dir, y + 1, kIron[3]); c.set(x + 2 * dir, y + 2, kIron[2]);
  }
  void broomHead(int x, int y, int dir) {   // a besom of twigs fanned out at the haft's foot (x, y), dir: the lean
    const Ramp& S = kThatch;
    for (int k = 0; k < 3; k++) {
      const int yy = y + k;
      for (int i = -1 - k / 2; i <= 1 + k / 2; i++) c.set(x + i + (k == 2 ? dir : 0), yy, S[(i + k) & 1 ? 2 : (i < 0 ? 4 : 3)]);
    }
    c.set(x, y - 1, kLeather[2]);   // the binding
  }

  // ------------------------------------------------------------------ the lower body when seated
  bool longSkirt() const { return skirt() || cutK == kCutKaftan || cutK == kCutRobe || cutK == kCutGown || cutK == kCutWrap; }
  int hemRows() const {   // a short garment's hem over the thighs
    if (cutK == kCutTunic) return 2;
    if (cutK == kCutCoat) return 3;
    if (cutK == kCutKilt) return 2;
    return tassetRows();
  }
  const Ramp& shinRamp(int y, int foot) const {
    if (O == Outfit::Rags) return R.skin;
    return y >= foot - bootExtra() ? R.boot : R.leg;
  }
  // chair height, facing down: the lap toward the camera, the knees apart, the shins down to the feet on the floor
  void chairLegsFront() {
    // the lap is wider than the hips (the thighs come toward the camera and part), the knees apart, the shins straight
    // down under them in the lap's shade, the feet planted; between the shins the seat's front and the chair's legs show
    const Ramp& lg = R.leg;
    if (longSkirt()) {
      const Ramp& T = R.top;
      for (int y = hip; y <= hip + 4; y++)
        for (int x = 3; x <= 12; x++) {
          int k = x <= 4 ? 3 : (x >= 11 ? 1 : 2);
          if (y == hip) k = std::min(4, k + 1);                            // the lap catches the light
          if (y >= hip + 2 && (x == 7 || x == 8)) k = 1;                     // the cloth dips between the knees
          if (y >= hip + 3 && (x == 5 || x == 10) && ((x + y) & 1)) k = std::max(0, k - 1);   // folds of the drape
          if (y == hip + 4) k = std::max(0, k - 1);
          c.set(x, y, T[k]);
        }
      for (int x : {4, 5, 10, 11}) c.set(x, kGround, R.boot[x == 4 || x == 10 ? 2 : 1]);
      if (O == Outfit::Robe || cutK == kCutRobe) for (int x = 3; x <= 12; x++) c.set(x, hip + 4, R.trim[x < 8 ? 2 : 1]);
      return;
    }
    static const int kTop[10] = {3, 4, 3, 3, 2, 3, 3, 2, 2, 1}, kBot[10] = {2, 3, 2, 2, 0, 0, 2, 2, 1, 0};
    for (int i = 0; i < 10; i++) { c.set(3 + i, hip, lg[kTop[i]]); c.set(3 + i, hip + 1, lg[kBot[i]]); }
    static const int kKneeL[3] = {3, 2, 1}, kKneeR[3] = {2, 1, 0};
    for (int i = 0; i < 3; i++) { c.set(3 + i, hip + 2, lg[kKneeL[i]]); c.set(10 + i, hip + 2, lg[kKneeR[i]]); }
    for (int y = hip + 3; y < kGround; y++) {
      const Ramp& r = shinRamp(y, kGround);
      c.set(4, y, r[2]); c.set(5, y, r[1]);
      c.set(10, y, r[1]); c.set(11, y, r[0]);
    }
    const Ramp& ft = O == Outfit::Rags ? R.skin : R.boot;   // the feet, toes toward the camera
    c.set(3, kGround, ft[3]); c.set(4, kGround, ft[2]); c.set(5, kGround, ft[1]);
    c.set(10, kGround, ft[2]); c.set(11, kGround, ft[1]); c.set(12, kGround, ft[0]);
    if (const int n = hemRows()) {   // a tunic's, coat's or kilt's hem over the lap
      const Ramp& T = R.top;
      for (int y = hip; y < hip + n && y <= hip + 2; y++)
        for (int x = 3; x <= 12; x++) {
          if (y == hip + 2 && x >= 6 && x <= 9) continue;
          int k = x <= 4 ? 3 : (x >= 11 ? 1 : 2);
          if (y == hip + n - 1) k = std::max(0, k - 1);
          if (cutK == kCutCoat && (x == 7 || x == 8) && y > hip) k = 0;   // the coat parts over the knees
          c.set(x, y, T[k]);
        }
      if (cutK == kCutKilt) { c.set(4, hip + 2, R.skin[3]); c.set(5, hip + 2, R.skin[2]); c.set(10, hip + 2, R.skin[2]); c.set(11, hip + 2, R.skin[1]); }
    }
  }
  // chair height, side view (facing right): the thigh level from the hip to the knee, the shin down to the foot
  void chairLegsSide() {
    const Ramp& lg = R.leg;
    const int ox = P.lean;
    if (longSkirt()) {
      const Ramp& T = R.top;
      for (int y = hip; y <= kGround - 1; y++) {
        const int x0 = (y <= hip + 2 ? 5 : 10) + ox, x1 = (y <= hip + 1 ? 13 : 13) + ox;
        for (int x = x0; x <= x1; x++) {
          int k = x == x0 ? 3 : (x >= x1 - 1 ? 1 : 2);
          if (y == hip) k = std::min(4, k + 1);
          if (y == kGround - 1) k = std::max(0, k - 1);
          c.set(x, y, T[k]);
        }
      }
      c.set(12 + ox, kGround, R.boot[2]); c.set(13 + ox, kGround, R.boot[1]); c.set(14 + ox, kGround, R.boot[0]);
      return;
    }
    // the far leg (a step behind, in shade), then the near one
    for (int y = hip + 2; y < kGround; y++) c.set(10 + ox, y, shinRamp(y, kGround)[0]);
    c.set(10 + ox, kGround, R.boot[0]); c.set(11 + ox, kGround, R.boot[0]);
    for (int x = 6; x <= 12; x++) {
      c.set(x + ox, hip, lg[x == 12 ? 2 : 3]);
      c.set(x + ox, hip + 1, lg[x == 6 ? 1 : (x == 12 ? 1 : 2)]);
    }
    c.set(13 + ox, hip, lg[2]); c.set(13 + ox, hip + 1, lg[1]);   // the knee
    for (int y = hip + 2; y < kGround; y++) { const Ramp& r = shinRamp(y, kGround); c.set(11 + ox, y, r[3]); c.set(12 + ox, y, r[1]); }
    const Ramp& f = O == Outfit::Rags ? R.skin : R.boot;
    c.set(11 + ox, kGround, f[2]); c.set(12 + ox, kGround, f[2]); c.set(13 + ox, kGround, f[1]); c.set(14 + ox, kGround, f[0]);
    if (const int n = hemRows())
      for (int y = hip; y < hip + std::min(n, 2); y++)
        for (int x = 6; x <= 11; x++) c.set(x + ox, y, R.top[x == 6 ? 3 : (y == hip + n - 1 ? 1 : 2)]);
  }
  // chair height, facing up: the seat of the trousers (or the skirt) on the seat; the legs are under the table
  void chairLegsBack() {
    const Ramp& r = longSkirt() || hemRows() >= 2 ? R.top : R.leg;
    for (int y = hip; y <= hip + 1; y++)
      for (int x = 4; x <= 11; x++) {
        int k = x <= 5 ? 3 : (x >= 10 ? 1 : 2);
        if (y == hip + 1) k = std::max(0, k - 1);
        if (!longSkirt() && (x == 7 || x == 8) && y == hip + 1) k = 0;
        c.set(x, y, r[k]);
      }
    if (longSkirt()) for (int x = 3; x <= 12; x++) c.set(x, hip + 2, R.top[x <= 4 ? 2 : (x >= 11 ? 0 : 1)]);   // the hem spills over the seat
  }
  // cross-legged on the floor (hip = ground - 2): the knees out to the sides, the crossed shins in front
  void floorLegsFront() {
    const Ramp& lg = longSkirt() ? R.top : R.leg;
    for (int y = hip; y <= kGround; y++)
      for (int x = 2; x <= 13; x++) {
        const int d = y - hip;
        if (d == 0 && (x == 2 || x == 13)) continue;
        int k = x <= 4 ? 3 : (x >= 11 ? 1 : 2);
        if (d == 0) k = std::min(4, k + 1);
        if (d == 2) k = std::max(0, k - 1);
        if (!longSkirt() && d >= 1 && (x == 7 || x == 8)) k = 1;   // the crossed shins
        c.set(x, y, lg[k]);
      }
    if (!longSkirt()) {   // the feet tucked in under the opposite knee
      const Ramp& f = O == Outfit::Rags ? R.skin : R.boot;
      c.set(5, kGround, f[2]); c.set(6, kGround, f[1]); c.set(9, kGround, f[2]); c.set(10, kGround, f[1]);
      if (const int n = hemRows()) for (int x = 4; x <= 11; x++) c.set(x, hip, R.top[x <= 5 ? 3 : (x >= 10 ? 1 : 2)]);
    } else {
      c.set(1, kGround, R.top[2]); c.set(14, kGround, R.top[0]);
    }
  }
  void floorLegsSide() {
    const Ramp& lg = longSkirt() ? R.top : R.leg;
    const int ox = P.lean;
    for (int y = hip; y <= kGround; y++)
      for (int x = 5; x <= 13; x++) {
        const int d = y - hip;
        if (d == 0 && x > 12) continue;
        if (d == 0 && x == 5) continue;
        int k = x <= 6 ? 3 : (x >= 12 ? 1 : 2);
        if (d == 0) k = std::min(4, k + 1);
        if (d == 2) k = std::max(0, k - 1);
        c.set(x + ox, y, lg[k]);
      }
    c.set(12 + ox, hip - 1, lg[3]); c.set(13 + ox, hip - 1, lg[2]);   // the near knee up
    if (!longSkirt()) { const Ramp& f = O == Outfit::Rags ? R.skin : R.boot; c.set(9 + ox, kGround, f[2]); c.set(10 + ox, kGround, f[1]); c.set(11 + ox, kGround, f[0]); }
  }
  void floorLegsBack() {
    const Ramp& lg = longSkirt() ? R.top : R.leg;
    for (int y = hip; y <= kGround; y++)
      for (int x = 3; x <= 12; x++) {
        int k = x <= 4 ? 3 : (x >= 11 ? 1 : 2);
        if (y == kGround) k = std::max(0, k - 1);
        c.set(x, y, lg[k]);
      }
    c.set(2, hip + 1, lg[3]); c.set(2, hip + 2, lg[2]); c.set(13, hip + 1, lg[1]); c.set(13, hip + 2, lg[0]);   // knees at the sides
  }

  // ------------------------------------------------------------------ eyes shut (prayer, sleep)
  void eyesShut() {
    const uint32_t eye = L.eyeColor ? opaque(L.eyeColor) : kEye;
    for (int y = hy + 2; y <= hy + 7; y++)
      for (int x = 0; x < 16; x++)
        if (c.get(x, y) == eye) c.set(x, y, c.get(x, y + 1) == eye ? R.skin[2] : R.skin[0]);
  }
  // the mouth open in song (front / side)
  void singing() {
    if (facing == kDown) { c.set(7, hy + 6, rgba(120, 40, 56)); c.set(8, hy + 6, rgba(96, 30, 48)); }
    else if (facing == kSide) c.set(11 + P.lean, hy + 6, rgba(110, 36, 52));
  }

  // ------------------------------------------------------------------ composition
  void paintPosture() {
    if (facing == kDown) paintPD();
    else if (facing == kUp) paintPU();
    else paintPS();
    if (form) formTexture();
    if (cutK) clothPattern();
    if (L.jewellery) torc();
    finishM6();
  }
  void lowerFront() {
    if (seat == kPzChair) { chairLegsFront(); return; }
    if (seat == kPzFloor) { floorLegsFront(); return; }
    legsFront();
  }
  void paintPD() {
    backItems();
    if (L.cloak) cloakFront();
    capeFrontSliver();
    if (seat == kPzStand) {
      legsFront();
      torsoFront(false);
      buildFront();
      skirtFront();
      if (form) tassets();
    } else {
      torsoFront(false);
      buildFront();
      lowerFront();
    }
    if (cutK) ponchoFront();
    armsD(0);
    head();
    if (pz == Posture::Pray) eyesShut();
    amuletFront();
    if (L.cloak) cloakDrapeFront();
    armsD(1);
  }
  void paintPU() {
    if (seat == kPzStand) {
      legsFront();
      torsoFront(true);
      skirtFront();
      if (form) tassets();
    } else {
      torsoFront(true);
      if (seat == kPzChair) chairLegsBack();
      else floorLegsBack();
    }
    if (cutK) ponchoFront();
    armsU(0);
    if (L.cloak) cloakBack();
    capeBack();
    backItems();
    head();
    if (L.cloak) cloakCollarBack();
    armsU(1);
  }
  void paintPS() {
    backItems();
    if (L.cloak) cloakSide();
    capeSide();
    armsS(0);
    if (seat == kPzStand) {
      legsSide();
      torsoSide();
      skirtSide();
      if (form) tassets();
    } else {
      torsoSide();
      if (seat == kPzChair) chairLegsSide();
      else floorLegsSide();
    }
    if (cutK) ponchoSide();
    if (L.cloak) cloakCollarSide();
    armsS(1);
    head();
    if (pz == Posture::Pray) eyesShut();
    amuletSide();
    armsS(2);
  }

  // ------------------------------------------------------------------ the arms and what they hold: facing down
  // phase 0: before the head (the off arm, things behind the head); 1: after it (the lit arm, things in front)
  void armsD(int ph) {
    const int f = fr;
    const bool odd = f & 1;
    switch (pz) {
      case Posture::Sit: case Posture::SitFloor:
        if (ph == 0) armR(12, ty + 4, 11, seat == kPzFloor ? hip : ty + 6);
        else armL(4, ty + 4, 5, seat == kPzFloor ? hip : ty + 6);
        break;
      case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
        if (ph == 0) { if (seat == kPzStand) armFront(11, 0, false); else armR(12, ty + 4, 11, seat == kPzFloor ? hip : ty + 6); }
        else if (!odd) { armL(4, ty + 4, 6, ty + 4); bread(5, ty + 2); }
        else { armL(4, ty + 4, 7, ty + 1); bread(7, hy + 6); }
        break;
      case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
        if (ph == 0) { if (seat == kPzStand) armFront(11, 0, false); else armR(12, ty + 4, 11, seat == kPzFloor ? hip : ty + 6); }
        else if (!odd) { mug(5, ty + 1, false); armL(4, ty + 4, 6, ty + 4); }
        else { mug(6, hy + 5, true); armL(4, ty + 3, 7, ty + 1); }
        break;
      case Posture::Cheer:
        if (ph == 0) { if (odd) armR(13, ty - 1, 13, hy + 3); else armFront(11, 0, false); }
        else { armL(2, ty - 1, 3, hy + 5); mug(1, hy + 1 - (odd ? 1 : 0), false); if (odd) c.set(2, hy - 1, kWhite); }
        break;
      case Posture::Wave:
        if (ph == 0) armFront(11, 0, false);
        else { armL(3, ty - 1, odd ? 3 : 2, hy + 2); c.set(odd ? 2 : 1, hy + 1, handRamp()[4]); }
        break;
      case Posture::Hammer:
        if (ph == 0) armR(12, ty + 4, 11, ty + 5);   // the off hand at the work (tongs out of frame)
        else if (!odd) { stick(3, hy + 6, 3, hy + 2, kWood); hammerHead(3, hy + 1, false); armL(2, ty, 3, hy + 6); }
        else { armL(4, ty + 3, 7, ty + 5); stick(7, ty + 5, 9, ty + 7, kWood); hammerHead(10, ty + 7, true); }
        break;
      case Posture::Chop:
        if (!odd) {
          if (ph == 0) armR(9, ty + 2, 5, ty);
          else { stick(4, ty, 3, hy + 1, kWood); axeHead(3, hy + 1, -1); armL(2, ty, 3, ty - 1); }
        } else {
          if (ph == 0) armR(12, ty + 3, 9, ty + 5);
          else { stick(8, ty + 5, 8, ty + 9, kWood); axeHead(8, ty + 9, 1); armL(4, ty + 3, 7, ty + 5); }
        }
        break;
      case Posture::Hoe:
        if (!odd) {
          if (ph == 0) armR(9, ty + 2, 5, ty + 1);
          else { stick(4, ty + 1, 3, hy + 1, kWood); c.set(3, hy, kIron[4]); c.set(2, hy + 1, kIron[3]); c.set(2, hy + 2, kIron[2]); c.set(1, hy + 2, kIron[2]); armL(2, ty, 3, ty - 1); }
        } else {
          if (ph == 0) armR(12, ty + 3, 10, ty + 4);
          else { stick(6, ty + 3, 11, kGround, kWood); for (int x = 10; x <= 13; x++) c.set(x, kGround + 1, kIron[x == 10 ? 4 : 2]); armL(4, ty + 3, 7, ty + 3); }
        }
        break;
      case Posture::Sweep: {
        const int dir = odd ? -1 : 1;   // the broom's foot swings left and right
        const int topX = dir > 0 ? 5 : 10, footX = dir > 0 ? 11 : 4;
        if (ph == 0) { armR(12, ty + 3, dir > 0 ? 10 : 11, dir > 0 ? ty + 5 : ty + 1); }
        else {
          stick(topX, ty - 1, footX, kGround - 2, kWood);
          broomHead(footX, kGround - 1, dir);
          armL(4, ty + 3, dir > 0 ? 6 : 5, dir > 0 ? ty + 1 : ty + 5);
        }
        break;
      }
      case Posture::Stir: {
        static const int dx[4] = {-1, 0, 1, 0}, dy[4] = {0, 1, 0, -1};
        const int lx = 8 + dx[f & 3], ly = ty + 8 + dy[f & 3];
        if (ph == 0) armR(12, ty + 4, 10, ty + 5);
        else { stick(8, ty + 5, lx, ly, kWood, 3); c.set(lx, ly + 1, kIron[3]); c.set(lx + 1, ly + 1, kIron[2]); armL(4, ty + 4, 7, ty + 5); }
        break;
      }
      case Posture::Carry: {
        const Pose wp = humanPose(facing, 1 + (f & 3));
        if (ph == 0) armFront(11, wp.swingB, false);
        else {
          const Ramp S = ramp(rgba(184, 150, 104));
          ball(c, 4.5, ty - 2.0, 3.6, 2.8, S, 0.06f);
          c.set(4, ty - 5, kLeather[2]); c.set(5, ty - 5, kLeather[1]); c.set(3, ty - 3, S[4]);
          armL(2, ty + 1, 2, ty - 1);
        }
        break;
      }
      case Posture::Fish: {
        const int tipY = hy - 1 + (odd ? 1 : 0);
        if (ph == 0) {
          for (int y = tipY + 1; y <= kGround + 1; y++) if ((y + (odd ? 1 : 0)) % 5 != 0) c.set(14, y, withA(kWhite, 150));
          armR(12, ty + 4, 10, ty + 4);
        } else { stick(8, ty + 5, 14, tipY, kWood, 3); armL(4, ty + 4, 7, ty + 4); }
        break;
      }
      case Posture::Lute: instrumentD(ph, f); break;
      case Posture::Flute: windD(ph, f); break;
      case Posture::Drum: drumD(ph, f); break;
      case Posture::Pray:
        if (ph == 0) armR(12, ty + 4, 9, ty + 3);
        else { armL(4, ty + 4, 7, ty + 3); c.set(7, ty + 2, handRamp()[4]); c.set(8, ty + 2, handRamp()[2]); }
        break;
      case Posture::Lamp: {
        const int up = odd ? 1 : 0;
        if (ph == 0) armR(12, ty + 4, 9, ty + 4);
        else {
          stick(8, ty + 5, 3, 4 - up, kWood, 2);
          c.set(2, 4 - up, kBrass[4]); c.set(3, 3 - up, kBrass[2]);
          flame(2, 3 - up);
          armL(3, ty, 4, hy + 4 - up);
        }
        break;
      }
      case Posture::Read:
        if (ph == 0) armR(12, ty + 4, 11, ty + 4);
        else { book(5, ty + 2, odd); armL(4, ty + 4, 5, ty + 4); }
        break;
      case Posture::Play: {
        const int a = odd ? 0 : 2;
        if (ph == 0) armR(13, ty - 1, 13, hy + 1 + (2 - a));
        else armL(3, ty - 1, 2, hy + 1 + a);
        break;
      }
      case Posture::Dance: {
        const int s = f & 3;
        if (ph == 0) {
          if (s == 2) armR(13, ty - 1, 13, hy + 2);
          else if (s == 0) armR(12, ty + 3, 10, ty + 4);   // a hand on the hip
          else armR(13, ty + 1, 14, ty);
        } else {
          if (s == 0) armL(3, ty - 1, 2, hy + 2);
          else if (s == 2) armL(4, ty + 3, 6, ty + 4);
          else armL(2, ty + 1, 1, ty);
        }
        break;
      }
      case Posture::Beg:
        if (ph == 0) armR(12, ty + 4, 12, hip);
        else { armL(4, ty + 4, 7, ty + 5 - (odd ? 1 : 0)); bowl(5, ty + 4 - (odd ? 1 : 0), odd); }
        break;
      default:
        if (ph == 0) armFront(11, 0, false);
        else restArmsFront();
        break;
    }
  }
  // the bard's string instrument, facing down
  void instrumentD(int ph, int f) {
    const bool odd = f & 1;
    const LeadInst li = (LeadInst)var;
    if (li == LeadInst::Fiddle) {   // under the chin on the left shoulder (screen right), the bow in the right hand
      if (ph == 0) armR(13, ty + 2, 12, ty + 3);
      else {
        ball(c, 10.5, ty + 1.5, 1.9, 2.3, kWood, 0.0f);
        c.set(10, ty + 1, kWoodDark[0]); c.set(11, ty + 2, kWoodDark[0]);
        stick(12, ty + 3, 13, ty + 5, kWoodDark);
        const int hx = odd ? 7 : 5;
        stick(hx - 1, ty + 4, hx + 6, ty + 0, kCloth, 3);   // the bow across the strings
        armL(4, ty + 4, hx, ty + 4);
      }
      return;
    }
    if (li == LeadInst::Harp) {   // a small frame harp held upright on the left, plucked with both hands
      if (ph == 0) armR(13, ty + 3, 12, ty + 2);
      else {
        stick(12, ty - 3, 12, ty + 5, kWood, 2);                  // the pillar
        stick(8, ty - 2, 12, ty - 3, kWood, 3);                   // the neck
        stick(8, ty - 2, 11, ty + 5, kWoodDark, 2);               // the sound box
        for (int k = 0; k < 3; k++) stick(9 + k, ty - 2, 9 + k, ty + 2 + k, kCloth, 3);   // strings
        armL(4, ty + 4, odd ? 9 : 10, ty + 2);
      }
      return;
    }
    const bool oud = li == LeadInst::Oud;
    if (ph == 0) armR(13, ty + 2, 13, ty - 1);   // the fretting hand up the neck
    else {
      // (M5 fixer: "a few orange pixels at the chest") a lute that reads at 1x: a big pear-shaped bowl across the
      // belly, rimmed dark, a pale soundboard lit from the top-left, the rose, the bridge, a long neck slanting up to
      // the right shoulder and the pegbox bent back; the strumming hand moves between frames
      const float bx = 6.0f, by = ty + 5.0f;
      ball(c, bx, by, oud ? 4.2 : 3.8, oud ? 3.4 : 3.1, kWoodDark, 0.0f);            // the bowl's rim
      ball(c, bx - 0.3, by - 0.4, oud ? 3.4 : 3.0, oud ? 2.7 : 2.4, kWood, 0.04f);  // the soundboard
      c.set(6, ty + 4, kWoodDark[0]); c.set(7, ty + 4, kWoodDark[0]); c.set(6, ty + 5, kWoodDark[1]);   // the rose
      hline(c, 4, 6, ty + 7, kWoodDark[0]);                                              // the bridge
      if (oud) {   // the oud: a short neck bent sharply back
        stick(9, ty + 3, 12, ty + 0, kWoodDark, 3); stick(9, ty + 4, 12, ty + 1, kWoodDark, 2);
        c.set(13, ty + 1, kWoodDark[2]); c.set(13, ty + 2, kWoodDark[1]); c.set(12, ty + 1, kWoodDark[3]);
      } else {
        stick(9, ty + 3, 12, ty - 1, kWoodDark, 3); stick(9, ty + 4, 13, ty, kWoodDark, 2);
        c.set(13, ty - 2, kWoodDark[3]); c.set(13, ty - 1, kWoodDark[2]); c.set(14, ty - 1, kWoodDark[1]);   // the pegbox
      }
      for (int k = 0; k < 3; k++) c.set(8 + k, ty + 4 - k, mix(kCloth[4], kWood[3], 0.4f));   // the strings
      armL(3, ty + 4, odd ? 5 : 6, ty + 5 + (odd ? 1 : 0));
    }
  }
  void windD(int ph, int f) {
    const bool odd = f & 1;
    const LeadInst li = (LeadInst)var;
    switch (li) {
      case LeadInst::Pipes:
        if (ph == 1) {
          for (int k = 0; k < 4; k++) stick(6 + k, hy + 6, 6 + k, hy + 6 + 3 - k, kThatch, 3);
          armL(4, ty + 2, 6, ty);
        } else armR(12, ty + 2, 10, ty);
        break;
      case LeadInst::Reed:
        if (ph == 1) {
          stick(7, hy + 6, 7, ty + 4, kWoodDark, 2); c.set(8, ty + 2, kWoodDark[1]); c.set(8, ty + 3, kWoodDark[1]); c.set(8, ty + 4, kWoodDark[1]);
          c.set(6, ty + 5, kWoodDark[3]); c.set(7, ty + 5, kWoodDark[2]); c.set(8, ty + 5, kWoodDark[2]); c.set(9, ty + 5, kWoodDark[1]);
          c.set(7, ty + 1 + (odd ? 1 : 0), kBrass[4]);
          armL(4, ty + 3, 6, ty + 2);
        } else armR(12, ty + 3, 9, ty + 3);
        break;
      case LeadInst::Horn: case LeadInst::Brass:
        if (ph == 1) {
          stick(7, hy + 6, 4, hy + 4, kBrass, 2);
          ball(c, 2.5, hy + 2.5, 1.8, 2.2, kBrass, 0.0f);
          c.set(2, hy + 2, kBrass[0]);
          armL(3, ty + 1, 5, hy + 7);
        } else armR(12, ty + 3, 9, ty + 1);
        break;
      case LeadInst::Bells:
        if (ph == 0) { armR(13, ty, 13, hy + 4 - (odd ? 0 : 1)); c.set(14, hy + 2 - (odd ? 0 : 1), kGold[3]); c.set(14, hy + 1 - (odd ? 0 : 1), kGold[2]); }
        else { armL(2, ty, 2, hy + 4 - (odd ? 1 : 0)); c.set(1, hy + 2 - (odd ? 1 : 0), kGold[4]); c.set(1, hy + 1 - (odd ? 1 : 0), kGold[3]); }
        break;
      case LeadInst::Voice:
        if (ph == 0) armR(13, ty + 1, 13, ty - (odd ? 1 : 0));
        else { singing(); armL(4, ty + 4, 7, ty + 2); }
        break;
      default:   // the transverse flute, held out to the player's right (screen left)
        if (ph == 1) {
          stick(1, hy + 6, 7, hy + 6, kWood, 3);
          c.set(3, hy + 6, kWoodDark[0]); c.set(5, hy + 6, kWoodDark[0]);
          if (odd) c.set(4, hy + 6, kWoodDark[1]);
          armL(3, ty + 1, 3, hy + 7);
        } else armR(12, ty + 2, 7, hy + 8);
        break;
    }
  }
  void drumD(int ph, int f) {
    const bool odd = f & 1;
    const PercKind pk = (PercKind)var;
    const Ramp Skin = ramp5(rgba(140, 116, 92), rgba(196, 172, 138), rgba(226, 208, 172), rgba(242, 230, 204), rgba(252, 246, 228));
    switch (pk) {
      case PercKind::Taiko: case PercKind::Kettle: {   // a barrel drum on a strap at the belly, two sticks
        if (ph == 1) {
          for (int y = ty + 4; y <= hip + 1; y++)
            for (int x = 5; x <= 10; x++) c.set(x, y, kRed[x == 5 ? 3 : (x >= 9 ? 1 : 2)]);
          for (int x = 5; x <= 10; x++) { c.set(x, ty + 3, Skin[x < 8 ? 4 : 3]); c.set(x, hip + 1, kWoodDark[1]); }
          c.set(5, ty + 5, kBrass[4]); c.set(10, ty + 5, kBrass[2]);
          stick(4, ty + (odd ? 1 : 3), 6, ty + 3, kWood, 3);
          stick(11, ty + (odd ? 3 : 1), 9, ty + 3, kWood, 3);
          armL(4, ty + 3, 4, ty + (odd ? 1 : 3));
          armR(12, ty + 3, 12, ty + (odd ? 3 : 1));
        }
        break;
      }
      case PercKind::Hand: case PercKind::Tabla: {   // a pair of small drums at the belly
        if (ph == 1) {
          for (int s = 0; s < 2; s++) {
            const int x0 = s ? 9 : 4;
            for (int y = ty + 5; y <= ty + 7; y++) for (int x = x0; x <= x0 + 2; x++) c.set(x, y, (s ? kWoodDark : kWood)[x == x0 ? 3 : (x == x0 + 2 ? 1 : 2)]);
            c.set(x0, ty + 4, Skin[4]); c.set(x0 + 1, ty + 4, Skin[3]); c.set(x0 + 2, ty + 4, Skin[2]);
          }
          armL(4, ty + 3, 5, ty + 4 - (odd ? 0 : 1));
          armR(12, ty + 3, 11, ty + 4 - (odd ? 1 : 0));
        }
        break;
      }
      case PercKind::Gong: {
        if (ph == 0) { ball(c, 11.5, ty + 1.5, 2.6, 2.6, kBrass, 0.0f); c.set(11, ty + 1, kBrass[4]); armR(13, ty + 3, 13, ty - 1); }
        else { stick(odd ? 6 : 8, ty + 1, odd ? 5 : 9, ty + 3, kWood, 3); c.set(odd ? 6 : 9, ty + 0, kCloth[3]); armL(4, ty + 3, odd ? 5 : 8, ty + 3); }
        break;
      }
      case PercKind::Wood: {   // clappers
        if (ph == 0) { armR(13, ty + 1, odd ? 10 : 12, ty); stick(odd ? 9 : 12, ty - 1, odd ? 9 : 13, ty - 4, kWood, 3); }
        else { armL(3, ty + 1, odd ? 6 : 4, ty); stick(odd ? 7 : 3, ty - 1, odd ? 7 : 2, ty - 4, kWood, 3); }
        break;
      }
      case PercKind::Bells: {   // a jingle ring shaken above the head
        if (ph == 0) armFront(11, 0, false);
        else {
          armL(2, ty, 3, ty - 1);
          for (int k = 0; k < 6; k++) {
            const float a = k / 6.0f * 6.2832f + (odd ? 0.5f : 0.0f);
            c.set((int)std::lround(3 + std::cos(a) * 1.6f), (int)std::lround(ty - 3 + std::sin(a) * 1.6f), k & 1 ? kGold[4] : kWood[3]);
          }
        }
        break;
      }
      default: {   // a frame drum (Bodhran: larger, struck with a beater) held up in the left hand (screen right)
        const bool bod = pk == PercKind::Bodhran;
        const float rr = bod ? 3.5f : 3.0f;
        if (ph == 0) {
          armR(13, ty + 3, 12, ty + 4);
        } else {
          ellipse(c, 10.5, ty + 2.0, rr + 0.6, rr + 0.6, kWood[1]);
          ball(c, 10.5, ty + 2.0, rr, rr, Skin, 0.04f);
          if (!bod) for (int k = 0; k < 4; k++) c.set(k < 2 ? 7 : 14, ty + (k & 1 ? 3 : 0), kBrass[4 - (k & 1)]);
          if (bod) { stick(odd ? 5 : 7, ty - 1, odd ? 4 : 8, ty + 3, kWood, 3); armL(4, ty + 3, odd ? 4 : 6, ty + 2); }
          else armL(4, ty + 3, odd ? 5 : 7, ty + (odd ? 2 : 1));
        }
        break;
      }
    }
  }

  // ------------------------------------------------------------------ facing up (the back to the camera)
  void armsU(int ph) {
    const int f = fr;
    const bool odd = f & 1;
    auto elbowsIn = [&]() { if (ph == 0) { armL(3, ty + 3, 4, ty + 4, false); armR(12, ty + 3, 12, ty + 4, false); } };
    switch (pz) {
      case Posture::Sit: case Posture::SitFloor: case Posture::Stir: case Posture::Read: case Posture::Beg:
        elbowsIn();
        break;
      case Posture::Pray:
        if (ph == 0) { armL(4, ty + 3, 5, ty + 4, false); armR(11, ty + 3, 11, ty + 4, false); }
        break;
      case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
        if (ph == 0) { armL(3, ty + 3, 4, ty + 4, false); if (!odd) armR(12, ty + 3, 12, ty + 4, false); else armR(13, ty + 2, 12, ty, false); }
        break;
      case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
        if (ph == 0) { armL(3, ty + 3, 4, ty + 4, false); if (!odd) armR(12, ty + 3, 12, ty + 4, false); else armR(13, ty + 2, 12, ty, false); }
        if (ph == 1 && odd) { c.set(13, hy + 3, kWood[2]); c.set(13, hy + 4, kIron[1]); c.set(14, hy + 3, kWood[1]); }   // the tankard's foot over the head
        break;
      case Posture::Cheer:
        if (ph == 1) { armR(13, ty - 1, 13, hy + 5); mug(11, hy + 1 - (odd ? 1 : 0), false); if (odd) armL(3, ty - 1, 3, hy + 4); }
        else if (!odd) armFront(3, 0, true);
        break;
      case Posture::Wave:
        if (ph == 0) armFront(3, 0, true);
        else { armR(13, ty - 1, odd ? 13 : 14, hy + 2); }
        break;
      case Posture::Hammer:
        if (!odd) { if (ph == 1) { stick(13, hy + 6, 13, hy + 2, kWood); hammerHead(13, hy + 1, false); armR(14, ty, 13, hy + 6); } else armL(3, ty + 3, 4, ty + 4, false); }
        else elbowsIn();
        break;
      case Posture::Chop:
        if (!odd) { if (ph == 1) { stick(11, ty, 12, hy + 1, kWood); axeHead(12, hy + 1, 1); armL(6, ty + 2, 10, ty); armR(14, ty, 13, ty - 1); } }
        else elbowsIn();
        break;
      case Posture::Hoe:
        if (!odd) { if (ph == 1) { stick(9, ty + 2, 13, hy - 1, kWood); c.set(13, hy - 1, kIron[3]); c.set(14, hy, kIron[2]); armR(13, ty + 2, 11, ty + 2); } else armL(3, ty + 3, 4, ty + 4, false); }
        else elbowsIn();
        break;
      case Posture::Sweep: {
        const int dir = odd ? -1 : 1;
        if (ph == 0) {
          const int footX = dir > 0 ? 11 : 4;
          stick(dir > 0 ? 9 : 6, ty + 3, footX, kGround - 2, kWood);
          broomHead(footX, kGround - 1, dir);
          armL(3, ty + 3, 4, ty + 4, false); armR(12, ty + 3, 12, ty + 4, false);
        }
        break;
      }
      case Posture::Carry: {
        const Pose wp = humanPose(facing, 1 + (f & 3));
        if (ph == 0) armFront(3, wp.swingA, true);
        else {
          const Ramp S = ramp(rgba(184, 150, 104));
          ball(c, 11.5, ty - 2.0, 3.6, 2.8, S, 0.06f);
          c.set(11, ty - 5, kLeather[2]); c.set(12, ty - 5, kLeather[1]);
          armR(14, ty + 1, 14, ty - 1);
        }
        break;
      }
      case Posture::Fish:
        if (ph == 0) { stick(9, ty + 3, 13, 2 + (odd ? 1 : 0), kWood, 3); elbowsIn(); }
        break;
      case Posture::Lute:
        if (ph == 0) { stick(5, ty + 3, 2, ty - 2, kWoodDark, 3); c.set(1, ty - 3, kWoodDark[2]); armL(3, ty + 2, 3, ty + 1, false); armR(13, ty + 3, 12, ty + 4 - (odd ? 1 : 0), false); }
        break;
      case Posture::Drum:
        if (ph == 0) { ellipse(c, 2.0, ty + 2.0, 1.2, 3.4, kWood[1]); c.set(2, ty, kWood[3]); armL(3, ty + 3, 3, ty + 3, false); armR(13, ty + 2, 12, ty + 3 - (odd ? 1 : 0), false); }
        break;
      case Posture::Flute:
        if (ph == 1 && (LeadInst)var == LeadInst::Flute) stick(12, hy + 6, 14, hy + 6, kWood, 3);
        if (ph == 0) { armL(3, ty + 2, 4, ty + 1, false); armR(13, ty + 2, 12, ty + 1, false); }
        break;
      case Posture::Lamp:
        if (ph == 1) { stick(10, ty + 4, 13, 4 - (odd ? 1 : 0), kWood); c.set(13, 4 - (odd ? 1 : 0), kBrass[3]); flame(13, 3 - (odd ? 1 : 0)); armR(13, ty, 12, hy + 4 - (odd ? 1 : 0)); }
        else armL(3, ty + 3, 4, ty + 4, false);
        break;
      case Posture::Play: {
        const int a = odd ? 0 : 2;
        if (ph == 1) { armL(3, ty - 1, 2, hy + 1 + a); armR(13, ty - 1, 13, hy + 1 + (2 - a)); }
        break;
      }
      case Posture::Dance: {
        const int s = f & 3;
        if (ph == 1) {
          if (s == 0) { armR(13, ty - 1, 13, hy + 2); armL(4, ty + 3, 5, ty + 4); }
          else if (s == 2) { armL(3, ty - 1, 2, hy + 2); armR(12, ty + 3, 10, ty + 4); }
          else { armL(2, ty + 1, 1, ty); armR(13, ty + 1, 14, ty); }
        }
        break;
      }
      default:
        if (ph == 0) armFront(3, 0, true);
        else armFront(11, 0, false);
        break;
    }
  }

  // ------------------------------------------------------------------ side view (facing right)
  // phase 0: before the body (the far arm); 1: after the body, before the head; 2: after the head (the near arm)
  void armsS(int ph) {
    const int f = fr;
    const bool odd = f & 1;
    const int o = P.lean;
    switch (pz) {
      case Posture::Sit: case Posture::SitFloor:
        if (ph == 0) armFar(8 + o, ty + 4, 10 + o, seat == kPzFloor ? hip : ty + 5);
        if (ph == 2) armNear(8 + o, ty + 4, 11 + o, seat == kPzFloor ? hip : ty + 5);
        break;
      case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
        if (ph == 0) armFar(8 + o, ty + 4, 10 + o, ty + 5);
        if (ph == 2) {
          if (!odd) { armNear(8 + o, ty + 4, 11 + o, ty + 3); bread(11 + o, ty + 1); }
          else { armNear(9 + o, ty + 3, 11 + o, hy + 8); bread(11 + o, hy + 5); }
        }
        break;
      case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
        if (ph == 0) armFar(8 + o, ty + 4, 10 + o, ty + 5);
        if (ph == 2) {
          if (!odd) { mug(10 + o, ty + 1, false); armNear(8 + o, ty + 4, 11 + o, ty + 4); }
          else { mug(11 + o, hy + 4, true); armNear(9 + o, ty + 3, 12 + o, hy + 8); }
        }
        break;
      case Posture::Cheer:
        if (ph == 0 && odd) armFar(8 + o, ty - 2, 9 + o, hy + 1);
        if (ph == 0 && !odd) armFar(8 + o, ty + 3, 8 + o, ty + 5);
        if (ph == 2) { armNear(10 + o, ty - 1, 12 + o, hy + 5); mug(11 + o, hy + 1 - (odd ? 1 : 0), false); }
        break;
      case Posture::Wave:
        if (ph == 0) armFar(8 + o, ty + 3, 8 + o, ty + 5);
        if (ph == 2) armNear(10 + o, ty - 1, odd ? 11 + o : 10 + o, hy + 2);
        break;
      case Posture::Hammer:
        if (ph == 0) armFar(9 + o, ty + 3, 11 + o, ty + 4);
        if (ph == 2) {
          if (!odd) { stick(4 + o, hy + 6, 3 + o, hy + 2, kWood); hammerHead(3 + o, hy + 1, false); armNear(6 + o, ty, 4 + o, hy + 6); }
          else { armNear(10 + o, ty + 2, 12 + o, ty + 3); stick(12 + o, ty + 3, 12 + o, ty + 6, kWood); hammerHead(12 + o, ty + 7, true); }
        }
        break;
      case Posture::Chop:
        if (!odd) {
          if (ph == 0) armFar(7 + o, ty, 5 + o, hy + 6);
          if (ph == 2) { stick(5 + o, hy + 5, 3 + o, hy + 1, kWood); axeHead(3 + o, hy + 1, -1); armNear(7 + o, ty, 6 + o, hy + 6); }
        } else {
          if (ph == 0) armFar(10 + o, ty + 3, 11 + o, ty + 5);
          if (ph == 2) { stick(11 + o, ty + 5, 11 + o, ty + 8, kWood); axeHead(11 + o, ty + 8, 1); armNear(10 + o, ty + 3, 11 + o, ty + 5); }
        }
        break;
      case Posture::Hoe:
        if (!odd) {
          if (ph == 0) { stick(9 + o, ty + 4, 13 + o, hy + 1, kWood); c.set(13 + o, hy + 2, kIron[3]); c.set(14 + o, hy + 2, kIron[2]); c.set(14 + o, hy + 3, kIron[1]); armFar(8 + o, ty + 3, 10 + o, ty + 2); }
          if (ph == 2) armNear(8 + o, ty + 4, 10 + o, ty + 4);
        } else {
          if (ph == 0) armFar(10 + o, ty + 3, 11 + o, ty + 3);
          if (ph == 2) { stick(10 + o, ty + 3, 13, kGround, kWood); c.set(13, kGround + 1, kIron[3]); c.set(14, kGround + 1, kIron[2]); c.set(12, kGround + 1, kIron[4]); armNear(9 + o, ty + 4, 11 + o, ty + 4); }
        }
        break;
      case Posture::Sweep: {
        const int footX = odd ? 9 : 11;
        if (ph == 0) armFar(9 + o, ty + 3, 10 + o, ty + 2);
        if (ph == 2) { stick(10 + o, ty, footX + o, kGround - 2, kWood); broomHead(footX + o, kGround - 1, odd ? -1 : 1); armNear(9 + o, ty + 4, odd ? 10 + o : 11 + o, ty + 4); }
        break;
      }
      case Posture::Stir: {
        static const int dx[4] = {-1, 0, 1, 0}, dy[4] = {0, 1, 0, -1};
        const int lx = 12 + o + dx[f & 3], ly = ty + 8 + dy[f & 3];
        if (ph == 0) armFar(9 + o, ty + 4, 11 + o, ty + 5);
        if (ph == 2) { stick(12 + o, ty + 4, lx, ly, kWood, 3); c.set(lx, ly + 1, kIron[3]); c.set(lx + 1, ly + 1, kIron[2]); armNear(9 + o, ty + 4, 12 + o, ty + 5); }
        break;
      }
      case Posture::Carry: {
        const Pose wp = humanPose(facing, 1 + (f & 3));
        if (ph == 0) armSide(false, wp.swingB);
        if (ph == 2) {
          const Ramp S = ramp(rgba(184, 150, 104));
          ball(c, 6.5 + o, ty - 2.0, 3.4, 2.7, S, 0.06f);
          c.set(4 + o, ty - 4, kLeather[2]); c.set(9 + o, ty - 3, S[1]);
          armNear(9 + o, ty + 1, 9 + o, ty - 1);
        }
        break;
      }
      case Posture::Fish: {
        const int tipY = hy - 1 + (odd ? 1 : 0);
        if (ph == 0) { for (int y = tipY + 1; y <= kGround + 1; y++) if ((y + (odd ? 1 : 0)) % 5 != 0) c.set(14, y, withA(kWhite, 150)); armFar(9 + o, ty + 4, 11 + o, ty + 4); }
        if (ph == 2) { stick(11 + o, ty + 4, 14, tipY, kWood, 3); armNear(9 + o, ty + 4, 11 + o, ty + 5); }
        break;
      }
      case Posture::Lute: {
        const LeadInst li = (LeadInst)var;
        if (ph == 0) {
          if (li == LeadInst::Fiddle) armFar(9 + o, ty, 11 + o, ty);
          else armFar(10 + o, ty + 1, 13 + o, ty);
        }
        if (ph == 2) {
          if (li == LeadInst::Fiddle) {
            ball(c, 10.5 + o, ty + 1.0, 1.8, 2.0, kWood, 0.0f);
            stick(11 + o, ty + 1, 14 + o, ty + 2, kWoodDark);
            const int hx = odd ? 9 : 11;
            stick(hx + o, ty + 4, hx + 3 + o, ty - 1, kCloth, 3);
            armNear(8 + o, ty + 4, hx + o, ty + 4);
          } else if (li == LeadInst::Harp) {
            stick(14 + o, ty - 3, 14 + o, ty + 5, kWood, 2);
            stick(11 + o, ty - 2, 14 + o, ty - 3, kWood, 3);
            stick(11 + o, ty - 2, 13 + o, ty + 5, kWoodDark, 2);
            stick(12 + o, ty - 1, 12 + o, ty + 3, kCloth, 3);
            armNear(9 + o, ty + 4, odd ? 12 + o : 13 + o, ty + 2);
          } else {
            const bool oud = li == LeadInst::Oud;
            ball(c, 11.0 + o, ty + 4.0, oud ? 2.8 : 2.4, oud ? 2.6 : 2.2, kWood, 0.04f);
            c.set(11 + o, ty + 4, kWoodDark[0]);
            stick(12 + o, ty + 2, 13 + o, ty - 1 + (oud ? 1 : 0), kWoodDark, 3);
            c.set(14 + o, ty - 2 + (oud ? 1 : 0), kWoodDark[2]);
            armNear(9 + o, ty + 4, 11 + o, ty + 3 + (odd ? 1 : 0));
          }
        }
        break;
      }
      case Posture::Flute: {
        const LeadInst li = (LeadInst)var;
        if (ph == 0) armFar(10 + o, ty + 2, 12 + o, hy + 8);
        if (ph == 2) {
          if (li == LeadInst::Voice) { singing(); armNear(9 + o, ty + 2, 12 + o, ty - (odd ? 1 : 0)); }
          else if (li == LeadInst::Reed) { stick(11 + o, hy + 6, 13 + o, ty + 4, kWoodDark, 2); c.set(13 + o, ty + 5, kWoodDark[3]); c.set(14 + o, ty + 5, kWoodDark[1]); armNear(9 + o, ty + 3, 12 + o, ty + 2); }
          else if (li == LeadInst::Horn || li == LeadInst::Brass) { stick(11 + o, hy + 6, 13 + o, hy + 3, kBrass, 2); ball(c, 14.0 + o, hy + 2.0, 1.4, 2.0, kBrass, 0.0f); armNear(9 + o, ty + 2, 12 + o, hy + 7); }
          else if (li == LeadInst::Pipes) { for (int k = 0; k < 3; k++) stick(11 + o + k, hy + 6, 11 + o + k, hy + 9 - k, kThatch, 3); armNear(9 + o, ty + 2, 12 + o, hy + 8); }
          else if (li == LeadInst::Bells) { armNear(10 + o, ty, 11 + o, hy + 4 - (odd ? 1 : 0)); c.set(12 + o, hy + 2 - (odd ? 1 : 0), kGold[4]); c.set(12 + o, hy + 3 - (odd ? 1 : 0), kGold[2]); }
          else { stick(11 + o, hy + 6, 14 + o, hy + 8, kWood, 3); if (odd) c.set(13 + o, hy + 7, kWoodDark[0]); armNear(9 + o, ty + 2, 12 + o, hy + 8); }
        }
        break;
      }
      case Posture::Drum: {
        const PercKind pk = (PercKind)var;
        if (pk == PercKind::Taiko || pk == PercKind::Kettle || pk == PercKind::Hand || pk == PercKind::Tabla) {
          if (ph == 0) armFar(9 + o, ty + 3, 11 + o, ty + (odd ? 2 : 4));
          if (ph == 2) {
            for (int y = ty + 4; y <= hip + 1; y++) for (int x = 10; x <= 13; x++) c.set(x + o, y, kRed[x == 10 ? 3 : (x == 13 ? 1 : 2)]);
            for (int x = 10; x <= 13; x++) c.set(x + o, ty + 3, kCloth[4]);
            armNear(9 + o, ty + 3, 12 + o, ty + (odd ? 4 : 2));
          }
        } else {
          if (ph == 0) { armFar(10 + o, ty + 2, 12 + o, ty + 2); }
          if (ph == 2) {
            ellipse(c, 13.0 + o, ty + 1.5, 1.4, 3.6, kWood[1]);
            ellipse(c, 12.6 + o, ty + 1.5, 0.9, 3.0, kCloth[3]);
            armNear(9 + o, ty + 4, odd ? 10 + o : 11 + o, ty + 2);
          }
        }
        break;
      }
      case Posture::Pray:
        if (ph == 0) armFar(9 + o, ty + 4, 11 + o, ty + 3);
        if (ph == 2) { armNear(9 + o, ty + 4, 11 + o, ty + 3); c.set(11 + o, ty + 2, handRamp()[3]); c.set(12 + o, ty + 2, handRamp()[2]); }
        break;
      case Posture::Lamp: {
        const int up = odd ? 1 : 0;
        if (ph == 0) armFar(9 + o, ty + 3, 10 + o, ty + 4);
        if (ph == 2) { stick(9 + o, ty + 5, 13, 4 - up, kWood); c.set(13, 4 - up, kBrass[3]); flame(13, 3 - up); armNear(10 + o, ty - 1, 11 + o, hy + 3 - up); }
        break;
      }
      case Posture::Read:
        if (ph == 0) armFar(9 + o, ty + 3, 11 + o, ty + 3);
        if (ph == 2) {
          for (int x = 10; x <= 13; x++) { c.set(x + o, ty + 1, x < 12 ? kWhite : kCloth[4]); c.set(x + o, ty + 2, kRed[x == 10 ? 3 : 2]); }
          if (odd) c.set(12 + o, ty, kWhite);
          armNear(9 + o, ty + 4, 11 + o, ty + 3);
        }
        break;
      case Posture::Play: {
        const int a = odd ? 0 : 2;
        if (ph == 0) armFar(7 + o, ty - 1, 6 + o, hy + 1 + (2 - a));
        if (ph == 2) armNear(10 + o, ty - 1, 11 + o, hy + 1 + a);
        break;
      }
      case Posture::Dance: {
        const int s = f & 3;
        if (ph == 0) { if (s == 2) armFar(7 + o, ty - 1, 6 + o, hy + 2); else armFar(6 + o, ty + 1, 4 + o, ty); }
        if (ph == 2) { if (s == 0) armNear(10 + o, ty - 1, 11 + o, hy + 2); else armNear(11 + o, ty + 1, 13 + o, ty); }
        break;
      }
      case Posture::Beg:
        if (ph == 0) armFar(9 + o, ty + 4, 11 + o, hip);
        if (ph == 2) { armNear(9 + o, ty + 4, 11 + o, ty + 4 - (odd ? 1 : 0)); bowl(11 + o, ty + 3 - (odd ? 1 : 0), odd); }
        break;
      default:
        if (ph == 0) armSide(false, 0);
        if (ph == 2) armSide(true, 0);
        break;
    }
  }
};

// ---- the sleeper (rpg/art/art_life.cpp sleeperSprite): the head on the pillow, face up (eyes shut, hats and helmets
//      off), the shoulders and the hands on the turned-down blanket. Painted in a 16-wide column at (ox, 0); hy is the
//      face's top row. The body under the blanket is the caller's.
struct SleeperRig : HumanPainter {
  SleeperRig(Canvas& c_, const HumanLook& l) : HumanPainter(c_, l, kDown, Pose{}) {}
  void paintAt(int headTop, int frame) {
    hy = headTop; ty = hy + 8; hip = ty + 6;
    // the shoulders and the top of the night shirt just above the blanket's edge
    for (int x = 4; x <= 11; x++) c.set(x, ty, R.top[x <= 5 ? 3 : (x >= 10 ? 1 : 2)]);
    c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[1]);
    head();
    const uint32_t eye = L.eyeColor ? opaque(L.eyeColor) : kEye;
    for (int y = hy + 2; y <= hy + 7; y++)
      for (int x = 0; x < 16; x++)
        if (c.get(x, y) == eye) c.set(x, y, c.get(x, y + 1) == eye ? R.skin[2] : R.skin[0]);
    if (frame & 1) { c.set(7, hy + 6, R.skin[1]); c.set(8, hy + 6, R.skin[0]); }   // breathing out: the mouth slack
    // the hands folded on the blanket
    const Ramp& h = handRamp();
    c.set(5, ty + 3, R.sleeve[3]); c.set(6, ty + 3, h[3]); c.set(7, ty + 3, h[2]);
    c.set(10, ty + 3, R.sleeve[1]); c.set(9, ty + 3, h[2]); c.set(8, ty + 3, h[1]);
  }
};

}  // namespace

Canvas humanSheet(const HumanLook& look) {
  Canvas sheet(HUMAN_W * HUMAN_FRAMES, HUMAN_H * 3);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < HUMAN_FRAMES; f++) {
      Canvas cell(HUMAN_W, HUMAN_H);
      HumanPainter hp(cell, look, row, humanPose(row, f));
      hp.frameNo = f;
      hp.paint();
      if (look.glow) {
        const Canvas raw = cell;
        outline(cell);
        hp.glowRim(cell, raw);
      } else {
        outline(cell);
      }
      place(sheet, cell, f, row);
    }
  return sheet;
}

Canvas humanCellRaw(const HumanLook& look, int row, int frame) {
  Canvas cell(HUMAN_W, HUMAN_H);
  HumanPainter hp(cell, look, row, humanPose(row, frame));
  hp.frameNo = frame;
  hp.paint();
  return cell;
}

void metalRampKeys(uint32_t light, uint32_t dark, uint8_t sheen, uint32_t out[5]) {
  light = opaque(light);
  dark = dark ? opaque(dark) : darken(light, 0.8f);
  const uint32_t mid = mix(dark, light, 0.55f);
  uint32_t k[5] = {darken(dark, 0.55f), dark, mid, light, lighten(light, 0.75f)};
  switch (sheen) {
    case 1:   // matte: soft highlights, shallow shadows
      k[0] = darken(dark, 0.35f); k[2] = mix(dark, light, 0.6f); k[3] = mix(mid, light, 0.7f); k[4] = lighten(light, 0.2f);
      break;
    case 2:   // bright: polished, near-white highlights
      k[3] = lighten(light, 0.2f); k[4] = mix(light, kWhite, 0.78f);
      break;
    case 3:   // dark: blackened metal with a cold rim
      k[0] = darken(dark, 0.85f); k[1] = darken(dark, 0.3f); k[2] = mix(dark, light, 0.32f); k[3] = mix(dark, light, 0.68f);
      k[4] = mix(light, rgba(176, 200, 240), 0.45f);
      break;
    case 5:   // iridescent: the hue turns across the ramp (violet shadows, teal body, gold-green light, rose highlights)
      k[0] = mix(k[0], rgba(70, 30, 110), 0.5f); k[1] = mix(k[1], rgba(40, 70, 150), 0.45f); k[2] = mix(k[2], rgba(40, 150, 150), 0.4f);
      k[3] = mix(k[3], rgba(180, 226, 140), 0.38f); k[4] = mix(k[4], rgba(255, 214, 240), 0.45f);
      break;
    case 6:   // glowing: a dark body (the seams carry the light key)
      k[0] = darken(dark, 0.75f); k[1] = darken(dark, 0.3f); k[2] = dark; k[3] = mix(dark, light, 0.4f); k[4] = mix(dark, light, 0.7f);
      break;
    case 7:   // pale: whitened, low contrast
      k[0] = darken(mix(dark, light, 0.2f), 0.5f); k[1] = mix(dark, light, 0.45f); k[2] = mix(light, kWhite, 0.12f);
      k[3] = mix(light, kWhite, 0.45f); k[4] = mix(light, kWhite, 0.8f);
      break;
    case 8:   // burnished: warm, deep shadows, hot highlights
      k[0] = darken(dark, 0.75f); k[3] = lighten(light, 0.2f); k[4] = mix(light, rgba(255, 246, 214), 0.8f);
      break;
    default: break;
  }
  for (int i = 0; i < 5; i++) out[i] = opaque(k[i]);
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
  // M3 peoples and cultures (mixed only when set, so every pre-M3 look keeps its key)
  const uint64_t m3a = (uint64_t)people | (uint64_t)cut << 8 | (uint64_t)headwear << 16 | (uint64_t)pattern << 24 |
                       (uint64_t)facePaint << 32 | (uint64_t)jewellery << 40 | (uint64_t)helmForm << 48 | (uint64_t)bodyForm << 56;
  const uint64_t m3b = (uint64_t)shieldForm | (uint64_t)bladeForm << 8 | (uint64_t)armsOrnament << 16 | (uint64_t)pauldron << 32 |
                       (uint64_t)skirt << 40 | (uint64_t)crest << 48;
  if (m3a || m3b || headColor || patternColor || plumeColor) {
    mx(m3a); mx(m3b); mx(headColor); mx(patternColor); mx(plumeColor);
  }
  // M6 Steel (mixed only when set, so every pre-M6 look keeps its key)
  const uint64_t m6 = (uint64_t)polearmForm | (uint64_t)bowForm << 8 | (uint64_t)sheen << 16 | (uint64_t)glow << 24;
  if (m6 || armourTint || armourTint2 || glowColor) {
    mx(m6); mx((uint64_t)armourTint << 32 | armourTint2); mx(glowColor);
  }
  if (shieldField || shieldDevice) mx((uint64_t)shieldField << 32 | shieldDevice);
  return k;
}

Canvas humanFigureStill(const HumanLook& look, int facing) {
  Canvas fig(HUMAN_W, HUMAN_H);
  HumanPainter hp(fig, look, facing, Pose{});
  hp.paint();
  return fig;
}

// ------------------------------------------------------------------ M5 postures and the sleeper (rpg/art/art_life.h)
Canvas humanPostureCellRaw(const HumanLook& look, Posture p, uint8_t variant, int row, int frame) {
  Canvas cell(HUMAN_W, HUMAN_H);
  if (p == Posture::None || p == Posture::Sleep || (int)p >= (int)Posture::COUNT) {
    if (p == Posture::None) { HumanPainter hp(cell, look, row, humanPose(row, 0)); hp.paint(); }
    return cell;
  }
  PostureRig pr(cell, look, row, posturePose(p, row, frame), p, frame, variant);
  pr.paintPosture();
  return cell;
}
// (art_life.cpp) the sleeper's head and shoulders in a 16-wide column of c at x offset ox, the face's top row headTop
void paintSleeperHead(Canvas& c, const HumanLook& look0, int ox, int headTop, int frame) {
  HumanLook look = look0;
  look.helmet = false; look.helmStyle = 0; look.hood = false; look.helmForm = 0; look.crest = 0;
  look.headwear = 0; look.cloak = 0; look.cape = false; look.backItem = 0; look.shield = false; look.shieldStyle = 0;
  look.shieldForm = 0; look.weapon = 0;
  Canvas col(HUMAN_W, std::max(HUMAN_H, headTop + 14));
  SleeperRig sr(col, look);
  sr.paintAt(headTop, frame);
  for (int y = 0; y < col.h; y++)
    for (int x = 0; x < col.w; x++) if (chA(col.get(x, y))) c.set(ox + x, y, col.get(x, y));
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
