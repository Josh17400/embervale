// Settlement homes (M7c split from town_build.cpp; EDGE lane): what a home looks like in each part of town, and the homes
// themselves: farmhouses at the edge, the frontage sweep from the heart outward (a plot on the street's north side
// first, the door right on it, at an uneven setback; then behind it, then beside it), the top-up rounds when the land
// left a settlement short of its scale. A planned city's own homes (compounds, row houses) and its share of the rounds
// go through the CITY lane's hooks (cityHomesWant, plannedHomes, cityFront, cityHomesRound, cityHomesLast; town_urban.cpp).
// See rpg/world/town_gen.h.
#include <algorithm>
#include <cmath>
#include "rpg/world/town_edge.h"
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

using art::Building;
using art::Prop;

namespace {
// (M7c, the village character) a home placed with its door row at ay and centred on ax would stand shoulder to
// shoulder with another home on the same front row (a gap of a tile or less, or overlapping in x)
bool villageRowmate(const Gen& g, int ax, int w, int ay) {
  const int x0 = ax - w / 2, x1 = x0 + w;
  for (const Bldg& c : g.M.bldgs) {
    if (!townIsHome(c.type) || c.r.y + c.r.h != ay) continue;
    const int gap = c.r.x >= x1 ? c.r.x - x1 : x0 - (c.r.x + c.r.w);
    if (gap >= -8 && gap <= 1) return true;
  }
  return false;
}
}  // namespace

// what a home looks like in each part of town
void Gen::homeShape(District d, Building& t, int& bw, int& bh) {
  if (village) {
    float hutP = arch == Archetype::Fishing ? 0.55f : 0.4f;
    if (rng.f() < hutP) { t = Building::Hut; bw = 3; bh = 2; return; }
    t = (arch == Archetype::Mining && rng.f() < 0.35f) ? Building::StoneHouse : Building::House;
    bw = 3 + rng.irange(2); bh = 2 + rng.irange(2);
    return;
  }
  if (town) {
    float stoneP = arch == Archetype::Mining || arch == Archetype::HillFort ? 0.5f : 0.3f;
    t = rng.f() < stoneP ? Building::StoneHouse : Building::House;
    bw = 3 + rng.irange(3); bh = 2 + rng.irange(2);
    if (rng.f() < 0.08f) { t = Building::Hut; bw = 3; bh = 2; }
    return;
  }
  switch (d) {
    case District::Noble:
      t = rng.f() < 0.8f ? Building::StoneHouse : Building::House;
      bw = 4 + rng.irange(3); bh = 3 + rng.irange(2);
      break;
    case District::Poor:
      if (rng.f() < (capital ? 0.08f : 0.35f)) { t = Building::Hut; bw = 3; bh = 2; }   // (M1: a royal seat's poor quarter is still a town)
      else { t = Building::House; bw = 3 + rng.irange(2); bh = 2; }
      break;
    case District::Crafts:
      t = rng.f() < 0.3f ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(3); bh = rng.f() < 0.75f ? 2 : 3;
      break;
    case District::Temple:
      t = rng.f() < 0.55f ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(3); bh = rng.f() < 0.7f ? 2 : 3;
      break;
    default:   // the old centre: tall narrow town houses (in a capital, mostly stone)
      t = rng.f() < (capital ? 0.8f : 0.55f) ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(2); bh = rng.f() < 0.75f ? 2 : 3;
      break;
  }
}

// (M3) homes() is built over several generator phases (homesBegin, homesSweep until it is done, homesFinish), so a
// city's two hundred homes never cost one web frame their whole build: the sweep places the homes of a few hundred
// street tiles per call. The sequence is the same however it is cut (run() and the web's phase-at-a-time build agree).
void Gen::homes() {
  homesBegin();
  while (homesSweep()) {}
  homesFinish();
}

void Gen::homesBegin() {
  const TownScale sc = townScale(P.type);
  homesWant = village ? 11 + rng.irange(4) : (town ? 44 + rng.irange(12) : sc.homesMin + 6 + rng.irange(24));
  hHave = 0;
  for (const Bldg& b : M.bldgs) if (townIsHome(b.type)) hHave++;
  if (planned) cityHomesWant();   // (M7b: a planned city aims a little higher; CITY lane hook)
  // farmhouses at the edge, among their fields (villages and towns)
  if (!city) {
    int farms = village ? (arch == Archetype::Farming ? 3 : 1 + rng.irange(2)) : (arch == Archetype::Farming ? 4 : 2);
    for (int k = 0; k < farms && hHave < homesWant; k++)
      if (placeBuilding(Want{Building::Farmhouse, Role::Farmer, 5, 3, 1.2f, false, false, District::COUNT, -1, -1})) hHave++;
  }
  // M3 compound (dune courtyards, highland clans): most homes stand in walled family compounds along the lanes
  if (style == cult::Layout::Compound) hHave += compoundHomes(homesWant - hHave);
  // (M7b) the planned city's own homes first (CITY lane hook: compounds, row houses along every front); the frontage
  // sweep fills in after them (the outskirts, the lanes, the odd corners)
  if (planned) hHave += plannedHomes();
  pathMax = city ? 5 : (town ? 7 : 9);   // homes front the streets: short footpaths, not trails across the gardens
  // the frontage sweep: every street tile once, from the heart outward (a little noise so the edge of the built-up
  // area is ragged, not a circle), a plot on its north side first (the door right on the street), then behind it,
  // then beside it; a second pass with smaller plots fills what is left
  std::vector<uint8_t> seen((size_t)W * H, 0);
  hOrder.clear();
  for (auto& s : allStreet) {
    const size_t i = I(s.first, s.second);
    if (seen[i] || M.at(s.first, s.second) == Ground::Bridge || M.wall[i]) continue;
    seen[i] = 1;
    float d = dist(s.first, s.second);
    if (!walled && d > 1.0f) continue;
    if (city && !inside[i]) continue;   // (M7c: outside the wall is the faubourgs' (outskirts); nothing is built there here)
    float dk = 1.0f;   // (M7b: a planned city says which street tiles may front a house, and in what order; CITY lane hook)
    if (planned && !cityFront(s.first, s.second, dk)) continue;
    hOrder.push_back({d * dk * (city ? 0.7f : 1.0f) + hfAt(s.first, s.second, 19u) * (city ? 0.25f : 0.35f), (int)i});
  }
  std::stable_sort(hOrder.begin(), hOrder.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first < b.first; });
  hPass = 0;
  hIdx = 0;
}

// a slice of the frontage sweep; false once it is done
bool Gen::homesSweep() {
  const size_t slice = planned ? 480 : 160;   // (M7b: a planned city's sweep has fewer, cheaper tiles left to try)
  const int want = homesWant - faubourgWant;  // (M7c: a city's faubourgs' share is built outside the walls, outskirts)
  for (size_t n = 0; n < slice; n++) {
    if (hPass >= 4 || hHave >= want) return false;
    if (hIdx >= hOrder.size()) { hIdx = 0; hPass++; continue; }
    const auto& o = hOrder[hIdx++];
    const int pass = hPass;
    const int sx = o.second % W, sy = o.second / W;
    if (!isStreet(sx, sy)) continue;
    const float d = dist(sx, sy);
    // villages and towns: denser at the heart, gardens and orchards between the houses toward the edge
    if (!city && pass < 3 && rng.f() > (village ? 1.35f : 1.3f) - d * 0.7f / densityF) continue;   // (M3: a dense people packs closer)
    Building t;
    int bw, bh;
    homeShape(districtAt(sx, sy), t, bw, bh);
    if (pass >= 2) { bw = std::max(3, bw - 1); bh = 2; if (t == Building::StoneHouse && rng.f() < 0.5f) t = Building::House; }
    if (pass == 3) bw = 3;   // the last pass squeezes cottages into what is left
    int setback = (!city && rng.irange(3) == 0) || (city && rng.irange(5) == 0) ? 1 : 0;
    // (M3) a dense people builds right on the street; a loose one stands back more often
    if (setback && densityF > 1.25f && hfAt(sx, sy, 29u) < densityF - 1.25f) setback = 0;
    if (!setback && densityF < 0.75f && hfAt(sx, sy, 31u) < 0.75f - densityF) setback = 1;
    const Role owner = t == Building::Farmhouse ? Role::Farmer : Role::Villager;
    const int j0 = rng.irange(3) - 1;
    bool done = false;
    // north of the street (the door on it), at an uneven setback and a little to either side; then a narrower plot
    // (M7c, the village character: a village's cottages stand loose, never shoulder to shoulder on one front row, and
    // now and then one stands back behind a front garden)
    auto rowmate = [&](int ax, int w, int ay) { return village && villageRowmate(*this, ax, w, ay); };
    const int deep = village && hfAt(sx, sy, 37u) < (style == cult::Layout::Terraced ? 0.45f : 0.36f) ? 1 : 0;   // ((M7c fixer: 0.28 left a village in four with its cottages at one setback, "houses in a line") a terrace village's ledges line its cottages up: more of them stand back)
    const bool swapBack = village && !deep && hfAt(sx, sy, 41u) < 0.3f;
    // (a terrace village: now and then a cottage stands below the lane, its back to it, its path round the end)
    if (village && terraced && hfAt(sx, sy, 43u) < 0.3f)
      done = !rowmate(sx + j0, bw, sy + 2 + bh) && tryPlace(t, owner, bw, bh, sx + j0, sy + 2 + bh, false);
    for (int k = 0; k < 9 && !done; k++) {
      int wv = k < 3 ? bw : std::max(3, bw - 1);
      int hv = k < 6 ? bh : 2;
      int jx = j0 + (k % 3 == 1 ? 1 : (k % 3 == 2 ? -1 : 0));
      int ya = sy - 1 - setback - (k < 3 ? deep : 0), yb = sy - 2 + setback;
      if (swapBack) std::swap(ya, yb);   // (a village cottage at the other setback first: its garden in front, or none)
      done = (!(k < 6 && rowmate(sx + jx, wv, ya)) && tryPlace(t, owner, wv, hv, sx + jx, ya, false)) ||
             (!(k < 6 && rowmate(sx + jx, wv, yb)) && tryPlace(t, owner, wv, hv, sx + jx, yb, false));
    }
    // behind it (the back to the street, a path round), beside it
    if (!done && pass >= 1)
      done = (!rowmate(sx + j0, bw, sy + 1 + bh) && tryPlace(t, owner, bw, bh, sx + j0, sy + 1 + bh, false)) ||
             (!rowmate(sx + j0, bw, sy + 2 + bh) && tryPlace(t, owner, bw, bh, sx + j0, sy + 2 + bh, false));
    for (int k = 0; k < 3 && !done; k++)
      done = (!rowmate(sx + 1 + bw / 2, bw, sy + j0 + k - 1) && tryPlace(t, owner, bw, bh, sx + 1 + bw / 2, sy + j0 + k - 1, false)) ||
             (!rowmate(sx - (bw - bw / 2), bw, sy + j0 + k - 1) && tryPlace(t, owner, bw, bh, sx - (bw - bw / 2), sy + j0 + k - 1, false));
    if (done) hHave++;
  }
  return hPass < 4 && hHave < want;
}

// (M7c fixer) a part a call, as the sweep is: the web builds a phase a frame, and a planned linear capital's six
// top-up rounds cost one frame 13 ms native; now one round a call, then the rest. The sequence is the same however it
// is cut (homesFinish runs the parts back to back).
void Gen::homesFinish() {
  while (homesFinishStep()) {}
}

bool Gen::homesFinishStep() {
  if (hfRound == 0) {
    hfHave = hHave;
    hOrder.clear();
    hOrder.shrink_to_fit();
  }
  int& have = hfHave;
  // still short of the scale (a river or the sea took the land, a hillside's terraces): every open tile near a street
  // becomes a candidate doorstep for a cottage, the footpaths may run a little longer
  const TownScale need = townScale(P.type);
  // (M1 economy: a second, wider round when the first leaves a hillside city short: the footpaths may run to 15)
  // (M7b) a planned city still short after both: its row houses give up their back yards for cottages, and two more
  // rounds look again
  // (fixer M7b r1, review: "steppe and fjord cities 15% under the housing floor") and two last, wider rounds (a street
  // within twelve tiles, footpaths to twenty) for a planned city whose wide plan left its open ground far from a door
  // ((M7c fixer) and a village still short of its ten homes after both (seed 21's mining village on a river: 8) a
  // third, wider round: a street within twelve tiles, out to 1.3 of its radius, footpaths to twenty; only while it
  // is under the ten)
  const bool villageLast = village && hfRound == 2;
  if (hfRound < (planned ? 6 : village ? 3 : 2) && have < need.homesMin + (villageLast ? 0 : 2)) {
    const int round = hfRound++;
    if (planned) cityHomesRound(round);   // (M7b: round 2 gives up the row houses' back yards; CITY lane hook)
    pathMax = round >= 4 || villageLast ? 20 : (round % 2 == 0 ? 12 : 15);
    const int reach = round >= 4 || villageLast ? 12 : (round % 2 == 0 ? 5 : 8);
    // (M3) the tiles near a street, from a distance map (it was a square search round every tile: a city paid
    // several ms for it in one web frame)
    std::vector<uint8_t> near((size_t)W * H, 0);
    {
      // a street within the square of `reach` round the tile: the street mask dilated along the rows, then the columns
      // (running counts over a sliding window)
      std::vector<uint8_t> row((size_t)W * H, 0);
      for (int y = 0; y < H; y++) {
        int cnt = 0;
        for (int x = -reach; x < W; x++) {
          if (x + reach < W && isStreet(x + reach, y)) cnt++;
          if (x - reach - 1 >= 0 && isStreet(x - reach - 1, y)) cnt--;
          if (x >= 0) row[I(x, y)] = cnt > 0 ? 1 : 0;
        }
      }
      for (int x = 0; x < W; x++) {
        int cnt = 0;
        for (int y = -reach; y < H; y++) {
          if (y + reach < H && row[I(x, y + reach)]) cnt++;
          if (y - reach - 1 >= 0 && row[I(x, y - reach - 1)]) cnt--;
          if (y >= 0) near[I(x, y)] = cnt > 0 ? 1 : 0;
        }
      }
    }
    std::vector<int> spots;
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        uint8_t k = get(x, y);
        if ((k != K_NONE && k != K_YARD) || M.bldgAt[I(x, y)] >= 0 || water[I(x, y)]) continue;
        if (!walled && dist(x, y) > (villageLast ? 1.3f : 1.15f)) continue;
        if (near[I(x, y)]) spots.push_back(y * W + x);
      }
    for (size_t i = spots.size(); i > 1; i--) std::swap(spots[i - 1], spots[(size_t)rng.irange((int)i)]);
    for (int s : spots) {
      if (have >= need.homesMin + (villageLast ? 0 : 2)) break;
      const Building t = village && rng.f() < 0.5f ? Building::Hut : Building::House;
      // (M7c, the village character: a village's top-up cottages stand loose too, unless the last round is short)
      if (village && round == 0 && villageRowmate(*this, s % W, 3, s / W)) continue;
      if (tryPlace(t, Role::Villager, 3, 2, s % W, s / W, false)) have++;
    }
    return true;
  }
  hfRound = 0;
  // (M7c) a city still short: its faubourgs ribbon further out along the roads (town_edge.cpp outskirts, again)
  if (city && edge && edge->belt > 0 && have < need.homesMin + 2) {
    edge->topUp = need.homesMin + 4 - have;
    hHave = have;
    outskirts();
    have = hHave;
    edge->topUp = 0;
  }
  // (its own count of the homes, as rpg_test --towns counts them: the rounds' tally runs a few ahead of it)
  int homesNow = 0;
  for (const Bldg& b : M.bldgs) if (townIsHome(b.type)) homesNow++;
  if (planned) have += cityHomesLast(homesNow);   // (fixer M7b r2: wide row houses let as two; CITY lane hook)
  pathMax = 12;
  // (M1 economy) the optional trades where the homes left room
  for (const Want& w : lateWants) placeWant(w);
  lateWants.clear();
  return false;
}

}  // namespace town
}  // namespace ew
