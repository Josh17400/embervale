// builder_gallery (M3b "Builders & Societies", VISION_PLAN 15.14): the builder's per-culture galleries and the art side
// of the "no bypass" acceptance.
//
//   builder_gallery <outdir> [--culture NAME|all] [--wealth]    per-culture boards: every purpose in the culture's
//                                                                 parts (row per purpose; --wealth: wealth 0..3 side by
//                                                                 side), builder_<culture>.png (3x) and _1x.png
//   builder_gallery --check [--props-strict] [--budget MS]      checks:
//     1. every art::Building x 13 styles x wealth 0 and 3 x two footprints paints from its blueprint: the sprite is not
//        empty and BuildingInfo::planKey == the blueprint's key (the painter drew THAT blueprint); the incremental job
//        (art::beginBuilding) gives the same pixels as buildingSprite for the seat of power of every culture; the
//        worst single paint is reported against the iPhone web budget (--budget, default 60 ms desktop for one
//        building in one go: the view spreads big ones over frames with BuildingJob);
//     2. every BUILT prop (bld::kindOfProp: wells, fences, lamps, benches, monuments, shrines, signs, banners, tents,
//        stalls, work yards) is drawn from the culture's parts: the number of distinct looks across the 12 archetypes'
//        PropStyles. Reported; --props-strict fails any built prop with fewer than 3 looks (the FORTIFICATIONS &
//        GROUND lane's acceptance).
//     3. (fixer r2) no see-through holes: no transparent pixel enclosed by the sprite that would show the ground under
//        a volume's footprint (a roof left unpainted under another's eave); EMB_HOLE_DIR=<dir> saves the offenders.
// Ownership (M3b): written by the lead in phase A; the BUILDER lane owns it (the props check's thresholds stay).
#include "tools/preview/preview_util.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/build/blueprint.h"
#include "rpg/build/registry.h"
#include "rpg/culture/society.h"
#include "rpg/sim/world.h"

using art::Building;

namespace {

const char* kNames[] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple", "sylvan", "starspire"};

uint64_t canvasHash(const Canvas& c) {
  uint64_t h = 1469598103934665603ull ^ (uint64_t)c.w << 32 ^ (uint64_t)c.h;
  for (uint32_t p : c.px) { h ^= p; h *= 1099511628211ull; }
  return h;
}

// (fixer r2) see-through holes: transparent pixels inside a sprite that the outside cannot reach (4-connected) and that
// would show the ground UNDER a volume's footprint (no ground is seen there: a roof or a wall was left unpainted, like
// the stave church's upper stage or a wing's eave over the body's roof). Ground seen between volumes (beside a porch,
// in a court or a yard) is fine. Returns the biggest such hole's size in px.
int biggestHole(const Canvas& c, const bld::Blueprint& bp) {
  const int W = c.w, H = c.h;
  const int D = std::max(1, (int)bp.req.hTiles) * 16, top = H - D - art::BLDG_PAD_B;
  std::vector<uint8_t> seen((size_t)W * H, 0);
  std::vector<int> st;
  auto clear = [&](int i) { return (c.px[(size_t)i] >> 24) < 8; };
  auto seed = [&](int x, int y) { const int i = y * W + x; if (!seen[(size_t)i] && clear(i)) { seen[(size_t)i] = 1; st.push_back(i); } };
  auto under = [&](int i) {   // the ground point this pixel shows lies inside a volume's footprint
    const float gx = (float)(i % W - art::BLDG_PAD_X) + 0.5f, gy = (float)(i / W - top) + 0.5f;
    for (const bld::Volume& v : bp.vols) {
      if (v.role == bld::VolRole::Platform || v.role == bld::VolRole::Enclosure || v.role == bld::VolRole::Tree || v.role == bld::VolRole::Chimney) continue;
      if (v.shape != bld::VolShape::Box) {
        const float rx = (v.x1 - v.x0) * 0.5f - 3, ry = (v.y1 - v.y0) * 0.5f - 3;
        if (rx <= 0 || ry <= 0) continue;
        const float dx = (gx - (v.x0 + v.x1) * 0.5f) / rx, dy = (gy - (v.y0 + v.y1) * 0.5f) / ry;
        if (dx * dx + dy * dy < 1) return true;
      } else if (gx > v.x0 + 3 && gx < v.x1 - 3 && gy > v.y0 + 3 && gy < v.y1 - 3) return true;
    }
    return false;
  };
  for (int x = 0; x < W; x++) { seed(x, 0); seed(x, H - 1); }
  for (int y = 0; y < H; y++) { seed(0, y); seed(W - 1, y); }
  auto flood = [&]() {
    int n = 0;
    while (!st.empty()) {
      const int i = st.back(); st.pop_back();
      if (under(i)) n++;
      const int x = i % W, y = i / W;
      if (x > 0) seed(x - 1, y);
      if (x + 1 < W) seed(x + 1, y);
      if (y > 0) seed(x, y - 1);
      if (y + 1 < H) seed(x, y + 1);
    }
    return n;
  };
  flood();
  int best = 0;
  for (int i = 0; i < W * H; i++)
    if (!seen[(size_t)i] && clear(i)) { seen[(size_t)i] = 1; st.push_back(i); best = std::max(best, flood()); }
  return best;
}

int check(bool propsStrict, double budget) {
  int holes = 0;
  int bad = 0, n = 0, keyBad = 0, empty = 0;
  double worst = 0;
  std::string worstDesc;
  static const int sizes[2][2] = {{4, 3}, {7, 4}};
  for (int t = 0; t < (int)Building::COUNT; t++)
    for (int c = -1; c < (int)cult::Archetype::COUNT; c++) {
      const cult::Culture K = cult::Atlas::make((cult::Archetype)(c < 0 ? 0 : c), 4242u + (uint32_t)c, 2);
      for (int w = 0; w <= 3; w += 3)
        for (int s = 0; s < 2; s++) {
          const art::ArchStyle st = c < 0 ? art::archForBiome(2, 5u + (uint32_t)t) : cult::buildingArch(K, 2, w == 3 ? 3 : 1, w, 77u + (uint32_t)t * 13u + (uint32_t)s);
          bld::Request r = bld::simpleRequest((Building)t, sizes[s][0], sizes[s][1], st, 1000u + (uint32_t)t * 31u + (uint32_t)s);
          r.wealth = (uint8_t)w; r.urban = (uint8_t)(w == 3 ? 3 : 1);
          const bld::Blueprint bp = bld::design(r);
          art::BuildingInfo info;
          const auto t0 = std::chrono::steady_clock::now();
          const Canvas cv = art::buildingSprite(bp, &info);
          const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
          n++;
          if (ms > worst) { worst = ms; worstDesc = std::string(bldgTypeName((Building)t)) + " " + (c < 0 ? "classic" : kNames[c]); }
          if (ms > budget * 0.5) std::printf("  slow paint: %s %s wealth %d %dx%d: %.1f ms (%zu volumes)\n", bldgTypeName((Building)t), c < 0 ? "classic" : kNames[c], w, sizes[s][0], sizes[s][1], ms, bp.vols.size());
          bool any = false;
          for (uint32_t p : cv.px) if (p >> 24) { any = true; break; }
          if (!any) { if (empty < 6) std::printf("FAIL: %s (style %d, wealth %d): empty sprite\n", bldgTypeName((Building)t), c, w); empty++; }
          if (const int hb = biggestHole(cv, bp); hb >= 6) {
            if (holes < 12) std::printf("FAIL: hole: %s %s wealth %d %dx%d: a see-through hole of %d px\n", bldgTypeName((Building)t), c < 0 ? "classic" : kNames[c], w, sizes[s][0], sizes[s][1], hb);
            if (const char* hd = std::getenv("EMB_HOLE_DIR"); hd && holes < 60) {   // the sprite, its holes in magenta
              Canvas hc = cv;
              for (uint32_t& q : hc.px) if ((q >> 24) < 8) q = 0;
              char fn[256];
              std::snprintf(fn, sizeof fn, "%s/hole_%02d_%s_%s_w%d_%dx%d.png", hd, holes, bldgTypeName((Building)t), c < 0 ? "classic" : kNames[c], w, sizes[s][0], sizes[s][1]);
              savePng(hc, fn, 3);
            }
            holes++;
          }
          if (info.planKey != bp.key) { if (keyBad < 6) std::printf("FAIL: %s (style %d): painted planKey %016llx != blueprint %016llx\n", bldgTypeName((Building)t), c, (unsigned long long)info.planKey, (unsigned long long)bp.key); keyBad++; }
        }
    }
  bad += keyBad + empty + holes;
  std::printf("see-through holes (>= 6 px of ground under a volume; EMB_HOLE_DIR=<dir> saves them): %d designs\n", holes);
  // the incremental paint of each culture's seat of power equals the one-go paint
  int jobBad = 0;
  for (int c = 0; c < (int)cult::Archetype::COUNT; c++) {
    const cult::Culture K = cult::Atlas::make((cult::Archetype)c, 9000u + (uint32_t)c, 2);
    const cult::Society S = cult::societyOf(K);
    bld::Request r = bld::simpleRequest(cult::seatPurpose(S.seat, false), 15, 7, cult::buildingArch(K, 2, 3, 3, 4711u), 4711u);
    r.wealth = 3; r.urban = 3; r.form = (bld::Form)cult::seatForm(S.seat, false); r.civic = bld::CIVIC_SEAT; r.seat = (uint8_t)((int)S.seat + 1);
    const bld::Blueprint bp = bld::design(r);
    art::BuildingInfo i1, i2;
    const Canvas whole = art::buildingSprite(bp, &i1);
    auto job = art::beginBuilding(bp);
    while (!art::stepBuilding(*job, 0.0)) {}
    const Canvas inc = art::finishBuilding(*job, &i2);
    if (canvasHash(whole) != canvasHash(inc) || i1.planKey != i2.planKey) { std::printf("FAIL: the %s seat's incremental paint differs\n", kNames[c]); jobBad++; }
  }
  bad += jobBad;
  // (owner 2026-10-06) open fronts: every culture x purpose x wealth x form x footprint design with an open face (a
  // colonnade, an arcade, a veranda, an iwan) anywhere is painted; an open entrance shows NO door, its pillars are
  // painted exactly where the walking puts them (bld::openFront), and no door or gateway anywhere stands behind a pillar
  int openN = 0, openPaints = 0, openDoor = 0, openPillarBad = 0, doorOverPillar = 0;
  {
    static const int fsz[3][2] = {{3, 2}, {5, 3}, {9, 5}};
    for (int t = 0; t < (int)Building::COUNT; t++)
      for (int c = -1; c < (int)cult::Archetype::COUNT; c++) {
        const art::ArchStyle st = c < 0 ? art::archForBiome(2, 7u + (uint32_t)t) : cult::buildingArch(cult::Atlas::make((cult::Archetype)c, 1000u + (uint32_t)c, 2), 2, 1, 1, 99u + (uint32_t)t);
        for (int w = 0; w < 4; w++)
          for (int f = 0; f < (int)bld::Form::COUNT; f++)
            for (int s = 0; s < 3; s++) {
              bld::Request r = bld::simpleRequest((Building)t, fsz[s][0], fsz[s][1], st, 31u * (uint32_t)t + 7u * (uint32_t)s + 1u);
              r.wealth = (uint8_t)w; r.form = (bld::Form)f; r.urban = (uint8_t)(w > 2 ? 3 : w);
              const bld::Blueprint bp = bld::design(r);
              bool any = false;
              for (const bld::Volume& v : bp.vols) if (bld::openFaceKind(v.face)) any = true;
              if (!any) continue;
              openPaints++;
              art::BuildingInfo info;
              (void)art::buildingSprite(bp, &info);
              const bld::OpenFront of = bld::openFront(bp);
              const char* who = c < 0 ? "classic" : kNames[c];
              if (of.open()) {
                openN++;
                if (!info.doors.empty()) {
                  if (openDoor < 6) std::printf("FAIL: open front %s %s %dx%d wealth %d form %s: a door is painted\n", bldgTypeName((Building)t), who, fsz[s][0], fsz[s][1], w, bld::formName((bld::Form)f));
                  openDoor++;
                }
                for (const bld::Pillar& q : of.pillars) {
                  bool found = false;
                  for (const auto& pp : info.pillars) if (pp[0] == q.x0 && pp[1] == q.x1) found = true;
                  if (!found) {
                    if (openPillarBad < 6) std::printf("FAIL: open front %s %s %dx%d wealth %d form %s: the pillar at %d..%d the walking blocks is not painted\n", bldgTypeName((Building)t), who, fsz[s][0], fsz[s][1], w, bld::formName((bld::Form)f), q.x0, q.x1);
                    openPillarBad++;
                    break;
                  }
                }
              }
              for (const auto& d : info.doors)
                for (const auto& pp : info.pillars)
                  if (pp[2] >= d[2] && pp[0] < d[1] && pp[1] > d[0]) {
                    if (doorOverPillar < 6) std::printf("FAIL: %s %s %dx%d wealth %d form %s: a door at %d..%d stands behind a pillar at %d..%d\n", bldgTypeName((Building)t), who, fsz[s][0], fsz[s][1], w, bld::formName((bld::Form)f), d[0], d[1], pp[0], pp[1]);
                    doorOverPillar++;
                  }
            }
      }
  }
  bad += openDoor + openPillarBad + doorOverPillar;
  std::printf("open fronts: %d designs with an open face painted, %d open entrances: %d with a door, %d with pillars off the walking's; "
              "%d doors behind a pillar\n", openPaints, openN, openDoor, openPillarBad, doorOverPillar);
  std::printf("builder_gallery --check: %d building paints from blueprints, %d planKey mismatches, %d empty; seats' incremental paints %d bad; "
              "worst paint %.1f ms (%s), budget %.0f ms%s\n", n, keyBad, empty, jobBad, worst, worstDesc.c_str(), budget, worst > budget ? " OVER" : "");
  if (worst > budget) { std::printf("FAIL: worst building paint %.1f ms over the %.0f ms budget\n", worst, budget); bad++; }
  // built props: distinct looks across the cultures
  int weak = 0, builtN = 0;
  std::string weakList;
  for (int p = 0; p < (int)art::Prop::COUNT; p++) {
    const bld::Kind k = bld::kindOfProp((art::Prop)p);
    if (k == bld::Kind::None || k == bld::Kind::Furniture || k == bld::Kind::Building || k == bld::Kind::COUNT) continue;
    builtN++;
    std::set<uint64_t> looks;
    for (int c = 0; c < (int)cult::Archetype::COUNT; c++) {
      const cult::Culture K = cult::Atlas::make((cult::Archetype)c, 300u + (uint32_t)c, 2);
      looks.insert(canvasHash(art::propSprite((art::Prop)p, K.props)));
    }
    if (looks.size() < 3) {
      weak++;
      weakList += std::string(" ") + std::to_string(p) + "(" + bld::kindName(k) + ":" + std::to_string(looks.size()) + ")";
    }
  }
  std::printf("built props: %d, %d with fewer than 3 culture looks:%s%s\n", builtN, weak, weakList.c_str(), propsStrict ? " (gated)" : " (--props-strict gates)");
  if (propsStrict && weak) bad++;
  std::printf("builder_gallery --check: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

namespace {
// the footprint each purpose usually gets in a settlement (rpg/world/town_build.cpp), so the boards judge the real sizes
void boardSize(Building t, int& w, int& h) {
  switch (t) {
    case Building::House: case Building::Shop: case Building::Bakery: case Building::Butcher: case Building::Tanner: case Building::Fishmonger:
    case Building::Weaver: case Building::Granary: w = 4; h = 3; break;
    case Building::StoneHouse: case Building::Smithy: case Building::Farmhouse: case Building::Watermill: case Building::Smelter:
    case Building::Sawmill: w = 5; h = 3; break;
    case Building::Inn: w = 6; h = 3; break;
    case Building::Temple: w = 7; h = 5; break;
    case Building::Keep: w = 9; h = 4; break;
    case Building::Tower: w = 3; h = 3; break;
    case Building::Hut: w = 3; h = 2; break;
    case Building::Palace: w = 15; h = 7; break;
    case Building::Barracks: case Building::Guildhall: case Building::Lodge: case Building::CouncilHall: w = 7; h = 4; break;
    case Building::Windmill: w = 4; h = 3; break;
    case Building::MeadHall: w = 8; h = 4; break;
    default: w = 6; h = 4; break;
  }
}

// rows: every purpose, then the lord's seat (the society's lord seat purpose as CIVIC_SEAT); rows.txt records each
// row's y range for the review crops
Canvas cultureBoard(int a, bool wealthRow, std::string& rowsTxt) {
  const cult::Culture K = cult::Atlas::make((cult::Archetype)a, 2024u + (uint32_t)a * 7u, 2);
  const cult::Society S = cult::societyOf(K);
  const int N = (int)Building::COUNT + 1;
  const int cols = wealthRow ? 4 : 1, cellW = 15 * 16 + 2 * art::BLDG_PAD_X + 24;
  struct Cell { Canvas c; std::string label; };
  std::vector<std::vector<Cell>> rows((size_t)N);
  std::vector<std::string> names((size_t)N);
  std::vector<int> rowH((size_t)N, 0);
  for (int t = 0; t < N; t++) {
    const bool lordRow = t == (int)Building::COUNT;
    const Building purpose = lordRow ? cult::seatPurpose(S.seat, true) : (Building)t;
    names[(size_t)t] = lordRow ? std::string("LORD SEAT: ") + bldgTypeName(purpose) : std::string(bldgTypeName(purpose));
    for (int w = 0; w < cols; w++) {
      const int wealth = wealthRow ? w : 2;
      int wT, hT;
      boardSize(purpose, wT, hT);
      if (lordRow) { wT = 9; hT = purpose == Building::Temple ? 5 : 4; }
      art::BuildingFacts f;
      f.storeys = art::defaultStoreys(purpose);
      if (purpose == Building::House || purpose == Building::StoneHouse || purpose == Building::Shop || purpose == Building::Exchange) f.storeys = ((w + t) & 1) ? 2 : 1;
      if (purpose == Building::Palace || purpose == Building::Barracks) f.storeys = 2;
      f.banner = K.heraldry.field; f.banner2 = K.heraldry.charge; f.emblem = K.heraldry.emblem;
      const uint32_t seed = 31u + (uint32_t)t * 7u + (uint32_t)w;
      bld::Request r = bld::simpleRequest(purpose, wT, hT, cult::buildingArch(K, 2, 2, wealth, seed), seed, f);
      r.wealth = (uint8_t)wealth; r.urban = 2;
      if (purpose == Building::Palace) { r.form = (bld::Form)cult::seatForm(S.seat, false); r.civic = bld::CIVIC_SEAT; r.seat = (uint8_t)((int)S.seat + 1); r.urban = 3; }
      if (lordRow) { r.form = (bld::Form)cult::seatForm(S.seat, true); r.civic = bld::CIVIC_SEAT; r.seat = (uint8_t)((int)S.seat + 1); }
      const bld::Blueprint bp = bld::design(r);
      art::BuildingInfo info;
      Cell cell;
      cell.c = art::buildingSprite(bp, &info);
      cell.label = std::string("WEALTH ") + std::to_string(wealth) + " " + bld::formName(bp.form) + " " + std::to_string(bp.facts.storeys) + "F";
      rowH[(size_t)t] = std::max(rowH[(size_t)t], cell.c.h + 34);
      rows[(size_t)t].push_back(std::move(cell));
    }
  }
  int H = 40;
  for (int h : rowH) H += h;
  const int W = 160 + cols * cellW;
  Board b(W, H);
  b.text(6, 6, std::string(cult::archetypeName((cult::Archetype)a)) + "  " + cult::governmentName(S.government) + "  SEAT: " + cult::seatName(S.seat) +
                   "  GATHERING: " + cult::gatheringName(S.gathering));
  int y = 40;
  for (int t = 0; t < N; t++) {
    const int gy = y + rowH[(size_t)t] - 20;
    b.text(6, gy - 40, names[(size_t)t]);
    for (int w = 0; w < cols; w++) {
      const Cell& cell = rows[(size_t)t][(size_t)w];
      const int fx = 160 + w * cellW;
      b.put(cell.c, fx - art::BLDG_PAD_X, gy + art::BLDG_PAD_B - cell.c.h);
      if (wealthRow) b.text(fx, gy + 8, cell.label);
    }
    rowsTxt += std::to_string(t) + " " + std::to_string(y) + " " + std::to_string(y + rowH[(size_t)t]) + "\n";
    y += rowH[(size_t)t];
  }
  return b.c;
}
}  // namespace

// builder_gallery --dump <purpose#> <culture#> <wealth> <w> <h> [storeys] [seed]: print a blueprint's volumes (debugging)
int dump(int argc, char** argv) {
  if (argc < 7) { std::printf("usage: builder_gallery --dump <purpose#> <culture#> <wealth> <w> <h> [storeys] [seed]\n"); return 2; }
  const int t = std::atoi(argv[2]), a = std::atoi(argv[3]), wealth = std::atoi(argv[4]), w = std::atoi(argv[5]), h = std::atoi(argv[6]);
  const int storeys = argc > 7 ? std::atoi(argv[7]) : 0;
  const uint32_t seed = argc > 8 ? (uint32_t)std::atoi(argv[8]) : 31u + (uint32_t)t * 7u + (uint32_t)wealth;
  const cult::Culture K = cult::Atlas::make((cult::Archetype)a, 2024u + (uint32_t)a * 7u, 2);
  art::BuildingFacts f;
  f.storeys = storeys;
  bld::Request r = bld::simpleRequest((Building)t, w, h, cult::buildingArch(K, 2, 2, wealth, seed), seed, f);
  r.wealth = (uint8_t)wealth; r.urban = 2;
  const bld::Blueprint bp = bld::design(r);
  std::printf("%s %s wealth %d %dx%d: form %s, %d storeys, %zu volumes, seat %d, sign %d icon %d\n", bldgTypeName((Building)t), kNames[a], wealth, w, h,
              bld::formName(bp.form), bp.facts.storeys, bp.vols.size(), bp.seat, (int)bp.sign.kind, bp.sign.icon);
  for (const bld::Volume& v : bp.vols)
    std::printf("  role %d shape %d box %d,%d..%d,%d z0 %d wallH %d st %d roof %d mat %d wall %d pitch %d ns %d eave %d face %d feat %x door %d\n", (int)v.role,
                (int)v.shape, v.x0, v.y0, v.x1, v.y1, v.z0, v.wallH, v.storeys, (int)v.roof, (int)v.roofMat, (int)v.wall, v.pitch, v.ridgeNS ? 1 : 0, v.eave,
                (int)v.face, v.feat, v.doorHere ? 1 : 0);
  if (const char* out = std::getenv("EMB_DUMP_PNG")) savePng(art::buildingSprite(bp), out, 4);
  return 0;
}

int main(int argc, char** argv) {
  if (argc >= 2 && !std::strcmp(argv[1], "--dump")) return dump(argc, argv);
  if (argc >= 2 && !std::strcmp(argv[1], "--check")) {
    bool strict = false;
    double budget = 60;
    for (int i = 2; i < argc; i++) {
      if (!std::strcmp(argv[i], "--props-strict")) strict = true;
      else if (!std::strcmp(argv[i], "--budget") && i + 1 < argc) budget = std::atof(argv[++i]);
    }
    return check(strict, budget);
  }
  if (argc < 2) { std::printf("usage: builder_gallery <outdir> [--culture NAME|all] [--wealth] | --check [--props-strict] [--budget MS]\n"); return 2; }
  const std::string dir = argv[1];
  std::string only = "all";
  bool wealth = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--culture") && i + 1 < argc) only = argv[++i];
    else if (!std::strcmp(argv[i], "--wealth")) wealth = true;
  }
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    if (only != "all" && only != kNames[a]) continue;
    std::string rowsTxt;
    const Canvas c = cultureBoard(a, wealth, rowsTxt);
    if (std::FILE* f = std::fopen((dir + "/builder_" + kNames[a] + "_rows.txt").c_str(), "w")) { std::fputs(rowsTxt.c_str(), f); std::fclose(f); }
    savePng(c, dir + "/builder_" + kNames[a] + "_1x.png", 1);
    savePng(c, dir + "/builder_" + kNames[a] + ".png", 3);
    std::printf("wrote %s/builder_%s.png\n", dir.c_str(), kNames[a]);
  }
  return 0;
}
