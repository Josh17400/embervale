// render_internal.h - PRIVATE to rpg/view/render.cpp and rpg/view/render_sorted.cpp (SOLID 3.4a): what the world's
// sorted pass (render_sorted.cpp) shares with the rest of the world draw (render.cpp). No other file includes it.
//   beasts::   the ranked foes' view (glows, plates, x-ray ghosts, the per-foe memo; render.cpp defines the data)
//   rview::    the sprite painters behind View::cachedTex keys, the market / stall / cart helpers, the frame's shared
//              records (g_occRecs, g_bldgColTop, g_beaconFires, g_drawT0); all defined in render.cpp
#pragma once
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "rpg/sim/foes.h"
#include "rpg/view/view.h"

// ---- (M6 BEASTS) the view of ranked foes: elites, champions, named uniques, bosses and world bosses (render.cpp)
// While the world is drawn, the monster branch queues what must show over the night's darkness (the glow sheets of
// glowing eyes, ember cracks, crystal glints and a golem's rune; the name plates; the world boss's bar); drawWeather's
// tail draws them after the lighting. `memo` is what the view remembers of each foe near the player between frames (its
// last state, for the shriek and the slam; its boss phase, for the flash; whether its sting or roar has played).
namespace beasts {
struct Glow { const Tex* t; int sx, sw, sh; float x, y; bool flip; float a; };
struct Plate { int idx; float x, y, d2, feet; };
struct Memo { uint8_t st = 255, phase = 0; float flashT = 0, shriekT = 0, lastT = 0; bool seen = false, roared = false; };
extern std::vector<Glow> glows;
// (P2c ladder, contracts 6b) the living finish queued by the sorted pass: glow-plane tiles (t set) and light points (the
// spark's star and the motes: t null, a w x h block), drawn additively after the lighting
struct Finish { const Tex* t; int sx, sy, sw, sh; float x, y, w, h; Color c; };
extern std::vector<Finish> finish;
extern int finishFigures;
// (M6 fixer) the ranked foes and the flyers, drawn again as a faint ghost over everything once the world is down, so a
// named unique in its bamboo, a world boss under the canopy or a harpy behind a ruin's wall still shows where it is
extern std::vector<Glow> xray;
extern std::vector<Plate> plates;
extern std::unordered_map<int, Memo> memo;
extern float now, stingT;
bool isRank(const Actor& a, foes::Rank r);
bool marked(const Actor& a);              // the ranks that wear an aura and a name plate
uint32_t auraOf(const Actor& a);          // the aura's colour (0: none)
int drawScale(const Actor&);              // always 1: every 16-bit sprite on screen shares one pixel grid
art::MonsterLook lookOf(const Actor& a);
Color runeLight(const Actor& a);          // the light a golem's rune casts (its material's colour)
template <class F> struct OnExit { F f; ~OnExit() { f(); } };
// what the last frame drew (for the scripts' `expect beasts` and the perf budget: auras + plates + glows <= 0.3 ms)
struct Stats { int auras = 0, plates = 0, glows = 0, bossBar = 0; double ms = 0; };
extern Stats cur, last;
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0);
}  // namespace beasts

bool boardwalkCoversTile(const Map& m, int tx, int ty);   // terrain.cpp (M4)
// (M10 ART) render_sea.cpp: the hero's lift onto the deck of the boat aboard (px); whether the boat's rig stood over the
// hero this frame (the ghost silhouette then shows him through the sail); a sea creature drawn half under the swell
float seaHeroLift(View& v, const Game& g);
bool seaHeroUnderRig(View& v);
void seaDrawUnderlay(View& v, const Game& g, Vec2 cam);
bool rvSwingFxBehind(const Game& g, const Actor& a);   // render_anim.cpp: the swing effect goes behind the body this frame
bool seaDrawSwimmer(View& v, const Game& g, const Actor& a, const Tex& sheet, int fr, int cw, int chh, bool flip, float alpha, float flash, Vec2 cam);

namespace rview {
// (P1b ENGINE lane) a story's hand cart: rpg/art/art_handcart.cpp
using art::handcartDir;
constexpr int HC_OX = art::HANDCART_OX, HC_OY = art::HANDCART_OY;
Canvas paintHandcart(uint64_t key);
// (P1b ENGINE lane) a beacon a story lit on a watchtower's broken top (its fire's sheet) and this frame's lit beacons
constexpr int BF_W = 22, BF_H = 28;
extern std::vector<Vec2> g_beaconFires;
Canvas paintBeaconFlame(uint64_t key);
bool bathing(const Game& g, const Map& m, const Actor& a);
// the cachedTex painters of the props' variants (render.cpp)
Canvas paintInteriorPiece(uint64_t key);
Canvas paintStall(uint64_t key);
Canvas paintTradeStall(uint64_t key);
Canvas paintStallShadow(uint64_t key);
Canvas paintTableShadow(uint64_t key);
Canvas paintClothShadow(uint64_t);
Canvas paintCart(uint64_t key);
bool snowyTile(const Map& m, int tx, int ty);
Canvas paintCartShadow(uint64_t key);
int stallFacingOn(const Game& g, const Map& m, int tx, int ty);
int stallFormOn(const Game& g, int tx, int ty, int facing);
int cartLook(const Game& g, int tx, int ty);
Canvas paintMarketTable(uint64_t key);
Canvas paintGroundCloth(uint64_t key);
Canvas paintMineRail(uint64_t key);
Canvas paintMineHill(uint64_t key);
Canvas paintRuin(uint64_t key);
Canvas paintTallGrass(uint64_t key);
Canvas paintBoulder(uint64_t key);
Canvas paintFlora(uint64_t key);
Canvas paintWinterTree(uint64_t key);
Canvas paintStandingStone(uint64_t key);
Canvas paintPeak(uint64_t key);
bool tallWild(art::Prop p);
uint32_t interiorPropTexKey(const Game& g, const Map& m, int tx, int ty, art::Prop p, int frame = 0);
// (P2a LENS) the frame's records: every building sprite's column profile, the sorted pass's plain blits (the night
// lift's occluders) and when this frame's world draw began (the ahead-of-time building painter's budget)
extern std::unordered_map<uint64_t, std::vector<int16_t>> g_bldgColTop;
extern std::vector<Pix::BlitRec> g_occRecs;
extern std::chrono::steady_clock::time_point g_drawT0;
uint64_t rigKey(const art::HumanLook& L, art::RigPart part, bool child);
}  // namespace rview
