// M7c "Order of Streets" checks (VISION_PLAN 15.M7c, rpg/world/town_plan.h). SURVEY lane (phase B).
//   rpg_test --structure [--seeds A..B] [--strict] [--real R] [--verbose]
//       the structure score of every synthetic settlement (each settlement archetype on plains, river, hill and coast;
//       every culture on its own land; every layout forced; a capital a seed) and of the real settlements of the
//       endless world within R regions of the origin (default 2: the 5 x 5 regions). Prints the mean, min and max per
//       type and per culture's cities, with the score's four parts. --strict fails unless cities >> towns >> villages
//       (STRUCT_GAP), every city >= STRUCT_CITY_FLOOR, the cities' mean >= STRUCT_CITY_MEAN, the towns' mean within
//       STRUCT_TOWN_MIN..MAX, every village <= STRUCT_VILLAGE_MAX and the villages' mean <= STRUCT_VILLAGE_MEAN. (Phase A: --strict is the phase B target; the
//       plain run reports.)
//   rpg_test --city-unique [--seeds A..B] [--strict] [--real R] [--top N]
//       the plan signature of every city (synthetic and real) and every pair's similarity: the most alike pairs, the
//       mean; --strict fails a pair at or above CITY_DUP_SIM or a mean above CITY_MEAN_SIM_MAX.
//   rpg_test --plan [--seeds A..B] [--type village|town|city] [--real R] [--png DIR]
//       one line per settlement: its order, age, layout, traits and structure score (the reviews' index).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"
#include "rpg/world/town_gen.h"
#include "rpg/world/town_plan.h"
#include "tools/tests/tests.h"

namespace {

enum class Land { Plains, River, Hill, Coast };
const char* landName(Land l) { static const char* n[] = {"plains", "river", "hill", "coast"}; return n[(int)l]; }
const char* typeName(SiteType t) { return t == SiteType::City ? "city" : (t == SiteType::Town ? "town" : "village"); }

struct PCase {
  SiteType type = SiteType::Village;
  ew::Archetype arch = ew::Archetype::Plain;
  Land land = Land::Plains;
  bool capital = false;
  std::vector<float> roads;
  uint32_t seed = 0;
  int culture = -1, forceLayout = -1;
  Biome bio = Biome::Plains;
};

// the --towns case list (test_towns.cpp's standard cases, seeded the same way)
std::vector<PCase> casesFor(uint64_t s) {
  std::vector<PCase> cases;
  Rng r(s * 977 + 13);
  static const ew::Archetype archs[] = {ew::Archetype::Plain, ew::Archetype::Farming, ew::Archetype::Fishing, ew::Archetype::Port,
                                        ew::Archetype::Mining, ew::Archetype::RiverCrossing, ew::Archetype::HillFort, ew::Archetype::Market};
  for (int li = 0; li < 3; li++)
    for (SiteType t : {SiteType::Village, SiteType::Town, SiteType::City}) {
      PCase c;
      c.type = t;
      c.arch = archs[(s * 3 + (uint64_t)li + (uint64_t)t * 2) % 8];
      c.land = (Land)li;
      if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
      if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
      c.capital = t == SiteType::City && (s + (uint64_t)li) % 3 == 0;
      if ((s + (uint64_t)li + (uint64_t)t) % 2 == 0) {
        int nr = t == SiteType::Village ? 1 + r.irange(2) : 2 + r.irange(t == SiteType::City ? 3 : 2);
        float a0 = r.f() * ew::D_TAU;
        for (int k = 0; k < nr; k++) c.roads.push_back(ew::dwrap(a0 + k * ew::D_TAU / nr + r.range(-0.4f, 0.4f)));
      }
      c.seed = (uint32_t)ew::mix64(s * 31 + (uint64_t)t * 7 + (uint64_t)li * 1013);
      cases.push_back(c);
    }
  {
    PCase c;
    c.type = SiteType::City; c.arch = ew::Archetype::Plain; c.land = Land::Plains; c.capital = true;
    float a0 = r.f() * ew::D_TAU;
    for (int k = 0; k < 3; k++) c.roads.push_back(ew::dwrap(a0 + k * 2.1f));
    c.seed = (uint32_t)ew::mix64(s * 7919 + 5);
    cases.push_back(c);
  }
  static const Land landOf[12] = {Land::Coast, Land::Plains, Land::Plains, Land::Plains, Land::Plains, Land::Plains,
                                  Land::Plains, Land::Plains, Land::River, Land::Plains, Land::Plains, Land::Plains};
  static const Biome bioOf[12] = {Biome::Taiga, Biome::Forest, Biome::Plains, Biome::Plains, Biome::Desert, Biome::Plains,
                                  Biome::Swamp, Biome::Forest, Biome::Plains, Biome::Forest, Biome::Forest, Biome::Snow};
  static const int byLayout[7] = {2, 3, 5, 8, 4, 7, 6};
  for (int pass = 0; pass < 2; pass++)
    for (int a = 0; a < (pass == 0 ? 12 : 7); a++)
      for (SiteType t : {SiteType::Village, SiteType::Town, SiteType::City}) {
        PCase c;
        c.type = t;
        c.culture = pass == 0 ? a : byLayout[a];
        c.forceLayout = pass == 0 ? -1 : a;
        c.arch = archs[(s + (uint64_t)a * 3 + (uint64_t)t + (uint64_t)pass * 5) % 8];
        c.land = landOf[c.culture];
        c.bio = bioOf[c.culture];
        if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
        if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
        if (c.land == Land::Coast && c.bio == Biome::Desert) c.bio = Biome::Plains;
        c.capital = t == SiteType::City && (s + (uint64_t)a + (uint64_t)pass) % 3 == 0;
        if ((s + (uint64_t)a + (uint64_t)t + (uint64_t)pass) % 2 == 0) {
          int nr = t == SiteType::Village ? 1 + r.irange(2) : 2 + r.irange(t == SiteType::City ? 3 : 2);
          float a0 = r.f() * ew::D_TAU;
          for (int k = 0; k < nr; k++) c.roads.push_back(ew::dwrap(a0 + k * ew::D_TAU / nr + r.range(-0.4f, 0.4f)));
        }
        c.seed = (uint32_t)ew::mix64(s * 131 + (uint64_t)a * 17 + (uint64_t)t * 7 + (uint64_t)pass * 7777 + 99991);
        cases.push_back(c);
      }
  return cases;
}

void buildCase(const PCase& c, ew::SettlementOut& so, bool variant = false) {
  ew::SitePlan p;
  p.type = c.type;
  p.archetype = c.arch;
  p.seed = c.seed;
  ew::settlementFootprint(c.type, c.arch, p.seed, p.w, p.h);
  p.gx = 1000 - p.w / 2; p.gy = -2000 - p.h / 2; p.ex = 1000; p.ey = -2000;
  p.bldgCap = 400;
  p.flags = c.capital ? ew::SPF_CAPITAL : 0;
  p.name = "TESTHOLD";
  ew::KingdomPlan k;
  k.id = 77; k.name = "TESTMARK"; k.color = rgba(40, 70, 160); k.color2 = rgba(230, 200, 80); k.emblem = (uint8_t)(c.seed % 8);
  if (variant) {   // (the metric controls: every non-plan choice changed)
    p.name = "OTHERSTEAD";
    k.id = 91; k.name = "ELSEWHERE"; k.color = rgba(150, 40, 50); k.color2 = rgba(240, 240, 230); k.emblem = (uint8_t)((c.seed + 3) % 8);
  }
  ew::SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = &k;
  cult::Culture K;
  if (c.culture >= 0) {
    K = cult::Atlas::make((cult::Archetype)c.culture, (uint32_t)ew::mix64(c.seed ^ 0xC17u), (int)c.bio);
    if (c.forceLayout >= 0) K.town.layout = K.town.altLayout = (cult::Layout)c.forceLayout;
    ctx.culture = &K;
  }
  ctx.rx = 3; ctx.ry = -8;
  ctx.roadBearings = c.roads;
  const int cxg = 1000, cyg = -2000, hw = p.w / 2;
  const Land land = c.land;
  const Biome open = c.bio;
  ctx.base = [land, cxg, cyg, hw, open](int32_t gx, int32_t gy, Ground& g, Biome& bi, uint8_t& h) {
    g = open == Biome::Desert ? Ground::Sand : (open == Biome::Snow ? Ground::Snow : Ground::Grass); bi = open; h = 1;
    int dx = gx - cxg, dy = gy - cyg;
    switch (land) {
      case Land::River: {
        int rxv = -hw / 3 + (int)(ew::dsin(dy * 0.07f) * 4.0f) + dy / 6;
        if (std::abs(dx - rxv) <= 1) g = Ground::Water;
        else if (std::abs(dx - rxv) == 2) g = Ground::Dirt;
        break;
      }
      case Land::Hill:
        h = (uint8_t)std::clamp(3 - (dy + dx / 2) / 13, 0, 6);
        bi = Biome::Forest;
        if ((gx * 7 + gy * 13) % 17 == 0) g = Ground::ForestFloor;
        break;
      case Land::Coast:
        if (dx > hw * 2 / 3 + (int)(ew::dsin(dy * 0.11f) * 3.0f)) { g = Ground::Water; bi = Biome::Ocean; }
        else if (dx > hw * 2 / 3 - 3) { g = Ground::Sand; bi = Biome::Beach; }
        break;
      default: break;
    }
  };
  so = ew::SettlementOut();
  so.gx = p.gx - ew::town::MARGIN;
  so.gy = p.gy - ew::town::MARGIN;
  ew::buildSettlement(ctx, so);
}

// one settlement measured: what it is and its build
struct Built {
  std::string name;      // e.g. "s1_c12 city heartland plains" or "real_s1_<id> town"
  SiteType type = SiteType::Village;
  bool capital = false;
  int culture = -1;
  std::shared_ptr<const ew::SettlementOut> out;
  int pcase = -1;        // a synthetic case: its index and its recipe (the controls rebuild it)
  PCase pc;
};

const char* cultName(int c) { return c >= 0 && c < (int)cult::Archetype::COUNT ? cult::archetypeName((cult::Archetype)c) : "CLASSIC"; }

// every synthetic case of the seeds, and the real settlements within R regions of the origin
std::vector<Built> gather(uint64_t a, uint64_t b, int R, int typeMask) {
  std::vector<Built> all;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    const std::vector<PCase> cs = casesFor(s);
    for (size_t ci = 0; ci < cs.size(); ci++) {
      if (!(typeMask & (1 << (int)cs[ci].type))) continue;
      auto so = std::make_shared<ew::SettlementOut>();
      buildCase(cs[ci], *so);
      Built B;
      char nm[128];
      std::snprintf(nm, sizeof nm, "s%llu_c%zu %s%s %s %s", (unsigned long long)s, ci, typeName(cs[ci].type), cs[ci].capital ? "(capital)" : "",
                    cultName(cs[ci].culture), landName(cs[ci].land));
      B.name = nm;
      B.type = cs[ci].type;
      B.capital = cs[ci].capital;
      B.culture = cs[ci].culture;
      B.out = so;
      B.pcase = (int)ci;
      B.pc = cs[ci];
      all.push_back(B);
    }
    if (R < 0) continue;
    ew::EndlessSource src(s);
    for (int ry = -R; ry <= R; ry++)
      for (int rx = -R; rx <= R; rx++) {
        const std::vector<ew::SitePlan> sites = src.region(rx, ry).sites;   // (a copy: settlement() may plan regions)
        for (const ew::SitePlan& p : sites) {
          if (p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) continue;
          if (!(typeMask & (1 << (int)p.type))) continue;
          std::shared_ptr<const ew::SettlementOut> o = src.settlement(p.id);
          if (!o) { out("FAIL: plan: real site %016llx has no settlement build\n", (unsigned long long)p.id); continue; }
          Built B;
          char nm[128];
          const cult::Culture* K = p.culture ? &src.culture(p.culture) : nullptr;
          std::snprintf(nm, sizeof nm, "real_s%llu_%016llx %s%s %s", (unsigned long long)s, (unsigned long long)p.id, typeName(p.type),
                        (p.flags & ew::SPF_CAPITAL) ? "(capital)" : "", K ? cult::archetypeName(K->archetype) : "CLASSIC");
          B.name = nm;
          B.type = p.type;
          B.capital = (p.flags & ew::SPF_CAPITAL) != 0;
          B.culture = K ? (int)K->archetype : -1;
          B.out = o;
          all.push_back(B);
        }
      }
  }
  return all;
}

struct Agg {
  int n = 0;
  double sum = 0, f = 0, st = 0, rg = 0, de = 0, cn = 0;
  float mn = 1e9f, mx = -1e9f;
  std::string worst;
  void add(const ew::StructureScore& S, const std::string& name) {
    n++; sum += S.total; f += S.frontage; st += S.straightness; rg += S.regularity; de += S.deadEnds; cn += S.connectivity;
    if (S.total < mn) { mn = S.total; worst = name; }
    mx = std::max(mx, S.total);
  }
  double mean() const { return n ? sum / n : 0; }
};

void parseCommon(int argc, char** argv, uint64_t& a, uint64_t& b, int& R, bool& strict, bool& verbose) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--strict")) strict = true;
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
    else if (!strcmp(argv[i], "--real") && i + 1 < argc) R = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--no-real")) R = -1;
  }
}

// the metric controls (below): run by --structure and --city-unique on a sample of their own settlements, failing
// whatever the gates (a check that cannot tell a duplicate or a trick is broken, not merely unmet)
std::vector<Built> controlSample(const std::vector<Built>& all);
int simControls(const std::vector<Built>& cities);
int structControls(const std::vector<Built>& places);
extern bool g_ctrlQuiet;   // (defined below)

// (SURVEY) the default runs: seeds 1..6 and the real settlements of the 7 x 7 regions round the origin (about 20 s
// for --structure, CI's budget is a minute); --seeds / --real narrow or widen it
constexpr uint64_t DEF_SEED_B = 6;
constexpr int DEF_REAL = 3;

int cmdStructure(int argc, char** argv) {
  uint64_t a = 1, b = DEF_SEED_B;
  int R = DEF_REAL;
  bool strict = false, verbose = false;
  parseCommon(argc, argv, a, b, R, strict, verbose);
  const std::vector<Built> all = gather(a, b, R, 7);
  Agg byType[3], capitals;
  std::map<std::string, Agg> byCulture;
  int bad = 0;
  std::vector<std::string> villagesOver, citiesUnder;
  for (const Built& B : all) {
    const ew::StructureScore S = ew::structureScore(*B.out);
    const int ti = B.type == SiteType::City ? 2 : (B.type == SiteType::Town ? 1 : 0);
    byType[ti].add(S, B.name);
    if (B.capital) capitals.add(S, B.name);
    if (ti == 2) byCulture[cultName(B.culture)].add(S, B.name);
    if (verbose)
      printf("structure: %-56s %5.1f  front %.2f (%d/%d) straight %.2f (%d) reg %.2f blocks %d ends %d dead %.2f conn %.2f\n", B.name.c_str(), S.total,
             S.frontage, S.fronting, S.bldgs, S.straightness, S.streetTiles, S.regularity, S.blocks, S.ends, S.deadEnds, S.connectivity);
    if (ti == 0 && S.total > ew::STRUCT_VILLAGE_MAX) villagesOver.push_back(B.name);
    if (ti == 2 && S.total < ew::STRUCT_CITY_FLOOR) citiesUnder.push_back(B.name);
  }
  static const char* tn[3] = {"villages", "towns", "cities"};
  for (int t = 0; t < 3; t++) {
    const Agg& A = byType[t];
    const double k = A.n ? 1.0 / A.n : 0;
    printf("structure: %-9s %4d  mean %5.1f  min %5.1f  max %5.1f | frontage %.2f straight %.2f regularity %.2f dead-ends %.2f connectivity %.2f | lowest %s\n",
           tn[t], A.n, A.mean(), A.n ? A.mn : 0, A.n ? A.mx : 0, A.f * k, A.st * k, A.rg * k, A.de * k, A.cn * k, A.worst.c_str());
  }
  if (capitals.n) printf("structure: capitals  %4d  mean %5.1f  min %5.1f  max %5.1f\n", capitals.n, capitals.mean(), capitals.mn, capitals.mx);
  for (auto& kv : byCulture)
    printf("structure:   city %-11s %3d  mean %5.1f  min %5.1f | frontage %.2f straight %.2f regularity %.2f dead-ends %.2f connectivity %.2f\n",
           kv.first.c_str(), kv.second.n, kv.second.mean(), kv.second.mn, kv.second.f / kv.second.n, kv.second.st / kv.second.n, kv.second.rg / kv.second.n,
           kv.second.de / kv.second.n, kv.second.cn / kv.second.n);
  // the gates (phase B's targets: --strict)
  auto gate = [&](bool ok, const char* fmt, double v1, double v2) {
    if (ok) return;
    char msg[256];
    std::snprintf(msg, sizeof msg, fmt, v1, v2);
    if (strict) { printf("FAIL: structure: %s\n", msg); bad++; }
    else printf("structure: (target, --strict) %s\n", msg);
  };
  const double vm = byType[0].mean(), tm = byType[1].mean(), cm = byType[2].mean();
  gate(cm >= ew::STRUCT_CITY_MEAN, "cities' mean %.1f under %.1f", cm, ew::STRUCT_CITY_MEAN);
  gate(citiesUnder.empty(), "%.0f cities under the floor %.1f", (double)citiesUnder.size(), ew::STRUCT_CITY_FLOOR);
  gate(tm >= ew::STRUCT_TOWN_MIN && tm <= ew::STRUCT_TOWN_MAX, "towns' mean %.1f outside the town band (max %.1f)", tm, ew::STRUCT_TOWN_MAX);
  gate(villagesOver.empty(), "%.0f villages over %.1f", (double)villagesOver.size(), ew::STRUCT_VILLAGE_MAX);
  gate(vm <= ew::STRUCT_VILLAGE_MEAN, "villages' mean %.1f over %.1f", vm, ew::STRUCT_VILLAGE_MEAN);
  gate(cm - tm >= ew::STRUCT_GAP, "cities %.1f points over towns (wanted a wider gap)", cm - tm, 0);
  gate(tm - vm >= ew::STRUCT_GAP, "towns %.1f points over villages (wanted a wider gap)", tm - vm, 0);
  for (size_t i = 0; i < citiesUnder.size() && i < 8; i++) printf("structure:   city under the floor: %s\n", citiesUnder[i].c_str());
  for (size_t i = 0; i < villagesOver.size() && i < 8; i++) printf("structure:   village over the cap: %s\n", villagesOver[i].c_str());
  {
    g_ctrlQuiet = !verbose;
    const int cb = structControls(controlSample(all));
    printf("structure: the metric's controls (rpg_test --plan-controls): %s\n", cb ? "FAILED" : "ok (a grid scores as a city, a scatter as a village, no trick gains)");
    bad += cb;
  }
  printf("structure: %zu settlements (seeds %llu..%llu, real regions %d): %s\n", all.size(), (unsigned long long)a, (unsigned long long)b, R,
         bad ? "FAILED" : (strict ? "ok" : "reported (phase B gates under --strict)"));
  return bad ? 1 : 0;
}

int cmdCityUnique(int argc, char** argv) {
  uint64_t a = 1, b = DEF_SEED_B;
  int R = DEF_REAL, top = 8;
  bool strict = false, verbose = false;
  parseCommon(argc, argv, a, b, R, strict, verbose);
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--top") && i + 1 < argc) top = atoi(argv[++i]);
  const std::vector<Built> all = gather(a, b, R, 1 << (int)SiteType::City);
  std::vector<ew::PlanSig> sig;
  sig.reserve(all.size());
  for (const Built& B : all) sig.push_back(ew::planSignature(*B.out));
  struct Pair { float s; size_t i, j; };
  std::vector<Pair> pairs;
  double sum = 0;
  for (size_t i = 0; i < all.size(); i++)
    for (size_t j = i + 1; j < all.size(); j++) {
      const float s = ew::planSimilarity(sig[i], sig[j]);
      pairs.push_back({s, i, j});
      sum += s;
    }
  std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.s > y.s; });
  const double mean = pairs.empty() ? 0 : sum / pairs.size();
  int dups = 0, hist[10] = {};
  for (const Pair& p : pairs) {
    if (p.s >= ew::CITY_DUP_SIM) dups++;
    hist[std::clamp((int)(p.s * 10), 0, 9)]++;
  }
  printf("city-unique: %zu cities, %zu pairs: mean similarity %.3f, most alike %.3f, %d pairs at or above %.2f\n", all.size(), pairs.size(), mean,
         pairs.empty() ? 0.0 : pairs[0].s, dups, ew::CITY_DUP_SIM);
  printf("city-unique: similarity histogram (0.0 .. 1.0 in tenths):");
  for (int k = 0; k < 10; k++) printf(" %d", hist[k]);
  printf("\n");
  for (int k = 0; k < top && k < (int)pairs.size(); k++)
    printf("city-unique:   %.3f  %s  ~  %s\n", pairs[(size_t)k].s, all[pairs[(size_t)k].i].name.c_str(), all[pairs[(size_t)k].j].name.c_str());
  int bad = 0;
  if (dups) {
    if (strict) { printf("FAIL: city-unique: %d near-duplicate city plans (similarity >= %.2f)\n", dups, ew::CITY_DUP_SIM); bad++; }
    else printf("city-unique: (target, --strict) %d near-duplicate pairs\n", dups);
  }
  if (mean > ew::CITY_MEAN_SIM_MAX) {
    if (strict) { printf("FAIL: city-unique: mean similarity %.3f over %.2f\n", mean, ew::CITY_MEAN_SIM_MAX); bad++; }
    else printf("city-unique: (target, --strict) mean similarity %.3f over %.2f\n", mean, ew::CITY_MEAN_SIM_MAX);
  }
  {
    g_ctrlQuiet = !verbose;
    std::vector<Built> sample;
    for (const Built& B : controlSample(all)) if (B.type == SiteType::City) sample.push_back(B);
    const int cb = simControls(sample);
    printf("city-unique: the metric's controls (rpg_test --plan-controls): %s\n",
           cb ? "FAILED" : "ok (shifted, cropped, mirrored, turned, rebuilt and offset-grid copies caught; other plans apart)");
    bad += cb;
  }
  printf("city-unique: %s\n", bad ? "FAILED" : (strict ? "ok" : "reported (phase B gates under --strict)"));
  return bad ? 1 : 0;
}

// (--plan --png DIR) a settlement's two planner layers side by side, 2 pixels a tile, for the reviews: on the left the
// road hierarchy (main roads red, streets orange, lanes yellow, footpaths tan, squares white) over the buildings
// (grey), the wall (black) and water (blue); on the right the block roles in their own colours (homes pale, seat
// purple, temple violet, market gold, park green, orchard olive, kitchen gardens lime, craft yards brown, compounds
// teal, barracks maroon, harbour navy, faubourg orange, farm wheat) over the same ground
std::string fileSafe(const std::string& s) {
  std::string r;
  for (char c : s) r += (std::isalnum((unsigned char)c) ? c : '_');
  return r;
}
void planPng(const ew::SettlementOut& o, const std::string& path) {
  const Map& m = o.buf;
  const int w = m.w, h = m.h, SC = 2, W = (w * 2 + 4) * SC, H = h * SC;
  if (w <= 0 || h <= 0 || o.rank.size() != (size_t)w * h) return;
  auto C = [](int r, int g, int b) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; };
  static const uint32_t rankC[6] = {0, C(206, 52, 40), C(236, 140, 40), C(236, 214, 80), C(196, 170, 120), C(250, 250, 244)};
  static const uint32_t roleC[(int)ew::BlockRole::COUNT] = {0, C(220, 206, 180), C(130, 60, 170), C(170, 110, 220), C(232, 186, 50), C(70, 160, 70),
                                                            C(130, 140, 50), C(170, 220, 80), C(140, 90, 50), C(40, 150, 150), C(130, 30, 40),
                                                            C(40, 60, 140), C(240, 130, 50), C(220, 200, 120)};
  std::vector<uint32_t> px((size_t)W * H, C(30, 30, 34));
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t i = (size_t)y * w + x;
      uint32_t base = groundWater(m.at(x, y)) ? C(60, 100, 170) : C(70, 84, 64);
      if (m.bldgAt[i] >= 0) base = C(150, 150, 150);
      if (m.wall[i]) base = C(10, 10, 10);
      const int rk = o.rank[i], rl = o.role.size() == o.rank.size() ? o.role[i] : 0;
      const uint32_t left = rk > 0 && rk < 6 && m.bldgAt[i] < 0 ? rankC[rk] : base;
      uint32_t right = base;
      if (rl > 0 && rl < (int)ew::BlockRole::COUNT) {
        const uint32_t c = roleC[rl];
        auto mixch = [](uint32_t a, uint32_t b, int s) { return (((a >> s) & 255) + ((b >> s) & 255) * 3) / 4; };
        right = m.bldgAt[i] >= 0 || m.wall[i] ? base : C((int)mixch(base, c, 0), (int)mixch(base, c, 8), (int)mixch(base, c, 16));
      }
      for (int sy = 0; sy < SC; sy++)
        for (int sx = 0; sx < SC; sx++) {
          px[(size_t)(y * SC + sy) * W + x * SC + sx] = left;
          px[(size_t)(y * SC + sy) * W + (w + 4 + x) * SC + sx] = right;
        }
    }
  writePng(path.c_str(), W, H, px);
}

int cmdPlan(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  int R = 2;
  bool strict = false, verbose = false;
  int mask = 7;
  parseCommon(argc, argv, a, b, R, strict, verbose);
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--type") && i + 1 < argc) {
      const char* t = argv[++i];
      mask = !strcmp(t, "city") ? 1 << (int)SiteType::City : (!strcmp(t, "town") ? 1 << (int)SiteType::Town : 1 << (int)SiteType::Village);
    }
  std::string pngDir;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--png") && i + 1 < argc) pngDir = argv[++i];
  const std::vector<Built> all = gather(a, b, R, mask);
  for (const Built& B : all) {
    const ew::SettlementOut& o = *B.out;
    const ew::PlanTraits& T = o.traits;
    const ew::StructureScore S = ew::structureScore(o);
    if (!pngDir.empty()) planPng(o, pngDir + "/plan_" + fileSafe(B.name) + ".png");
    int ranks[(int)ew::Rank::COUNT] = {}, roles = 0;
    for (uint8_t r : o.rank) if (r < (int)ew::Rank::COUNT) ranks[r]++;
    for (uint8_t r : o.role) if (r) roles++;
    printf("plan: %-56s at %d,%d order %3d age %d layout %d plan %d wall %d water %d gates %d avenues %d blocks %dx%d sections %d plazas %d faubourgs %d (%d bldgs) | "
           "main %d street %d lane %d path %d square %d role-tiles %d | homes %d bldgs %zu | structure %.1f\n",
           B.name.c_str(), (int)o.ex, (int)o.ey, T.order, T.age, T.layout, T.planKind, (int)T.wall, (int)T.water, T.gates, T.avenues, T.blockW, T.blockH, T.gridSections, T.plazas,
           T.faubourgs, T.faubourgBldgs, ranks[1], ranks[2], ranks[3], ranks[4], ranks[5], roles, o.homes, o.buf.bldgs.size(), S.total);
  }
  return 0;
}

// ---------------------------------------------------------------- the metric controls (SURVEY: the checks checked)
// The checks are only worth what they catch. Each control builds a settlement (or a synthetic plan), changes it in a
// known way and asserts what the metric must say:
//   similarity  a city against itself shifted 2 tiles, cropped by 2 tiles, mirrored, turned a quarter, and rebuilt with
//               every non-plan choice changed (its kingdom, names, colours) is a near-duplicate (>= CITY_DUP_SIM); a
//               pure grid against the same grid offset half a block is a near-duplicate (offsetting a grid is no new
//               plan); a grid against a ring plan and against an organic scatter is not
//   structure   a pure grid of blocks scores like a planned city (>= STRUCT_CITY_FLOOR), an organic scatter of houses
//               on wandering paths like a village (<= STRUCT_VILLAGE_MEAN); and tricks earn nothing: stubs at every
//               dead end and loose scraps of street, loose 1-tile street slivers laid along the house fronts, 1-tile sliver
//               buildings strung along the streets each move a real settlement's score by at most CTRL_TRICK points
constexpr float CTRL_TRICK = 1.0f;

ew::SettlementOut blankOut(int w, int h) {
  ew::SettlementOut o;
  o.gx = 0; o.gy = 0;
  o.buf.alloc(w, h, Ground::Grass);
  o.buf.urb.assign((size_t)w * h, 0);
  o.used.assign((size_t)w * h, 0);
  o.rank.assign((size_t)w * h, 0);
  o.role.assign((size_t)w * h, 0);
  o.ex = w / 2; o.ey = h / 2;
  return o;
}
bool addBldg(ew::SettlementOut& o, int x, int y, int w, int h) {
  Map& m = o.buf;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      if (xx < 0 || yy < 0 || xx >= m.w || yy >= m.h) return false;
      const size_t i = (size_t)yy * m.w + xx;
      if (m.bldgAt[i] >= 0 || o.rank[i] || m.wall[i]) return false;
    }
  Bldg b;
  b.type = art::Building::House;
  b.r = IRect{x, y, w, h};
  b.id = (ew::Gid)(m.bldgs.size() + 1);
  m.bldgs.push_back(b);
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) m.bldgAt[(size_t)yy * m.w + xx] = (int32_t)m.bldgs.size() - 1;
  return true;
}
void setRankAt(ew::SettlementOut& o, int x, int y, ew::Rank r) {
  if (x < 0 || y < 0 || x >= o.buf.w || y >= o.buf.h) return;
  const size_t i = (size_t)y * o.buf.w + x;
  if (o.buf.bldgAt[i] >= 0 || o.buf.wall[i]) return;
  o.rank[i] = (uint8_t)r;
  o.buf.ground[i] = (uint8_t)(r == ew::Rank::Square ? Ground::Plaza : Ground::Road);
}
// a walled grid city: the ring at [m, size - m), avenues across the middle, streets every `block` tiles (offset by
// `phase`), row houses along every block's south and north edges
ew::SettlementOut gridCity(int size, int block, int phase) {
  ew::SettlementOut o = blankOut(size, size);
  const int m = 16, a = m + 1, b = size - m - 1;
  for (int k = m; k < size - m; k++) {
    o.buf.wall[(size_t)m * size + k] = 1; o.buf.wall[(size_t)(size - m - 1) * size + k] = 1;
    o.buf.wall[(size_t)k * size + m] = 1; o.buf.wall[(size_t)k * size + size - m - 1] = 1;
  }
  for (int y = a; y < b; y++)
    for (int x = a; x < b; x++) {
      o.used[(size_t)y * size + x] = 1;
      const bool av = std::abs(x - size / 2) <= 1 || std::abs(y - size / 2) <= 1;
      const bool st = ((x - a - phase) % block + block) % block < 2 || ((y - a - phase) % block + block) % block < 2;
      if (av) setRankAt(o, x, y, ew::Rank::Main);
      else if (st) setRankAt(o, x, y, ew::Rank::Street);
    }
  for (int y = a; y < b; y++)
    for (int x = a; x < b; x++) {
      const size_t i = (size_t)y * size + x;
      if (o.rank[i] || o.buf.bldgAt[i] >= 0) continue;
      if (y + 3 < size && o.rank[(size_t)(y + 3) * size + x]) addBldg(o, x, y, 4, 3);
      else if (y > 0 && o.rank[(size_t)(y - 1) * size + x]) addBldg(o, x, y, 4, 3);
    }
  return o;
}
// rings round a court, joined by eight radial spokes (a steppe-like plan)
ew::SettlementOut ringCity(int size) {
  ew::SettlementOut o = blankOut(size, size);
  const int c = size / 2, R = size / 2 - 18;
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      const float dx = (float)(x - c), dy = (float)(y - c), r = std::sqrt(dx * dx + dy * dy);
      if (std::fabs(r - (float)R) < 0.8f) { o.buf.wall[(size_t)y * size + x] = 1; continue; }
      if (r >= (float)R) continue;
      o.used[(size_t)y * size + x] = 1;
      const float sec = (ew::datan2(dy, dx) + ew::D_PI) / ew::D_TAU * 8.0f;
      const float off = std::fabs(sec - std::floor(sec + 0.5f)) * r * ew::D_TAU / 8.0f;
      if ((int)r % 14 < 2) setRankAt(o, x, y, ew::Rank::Street);
      else if (off < 1.2f) setRankAt(o, x, y, ew::Rank::Main);
    }
  Rng rr(77);
  for (int k = 0; k < 900; k++) {
    const int x = 20 + rr.irange(size - 40), y = 20 + rr.irange(size - 40);
    const int bw = 3 + rr.irange(2);
    if (o.used[(size_t)y * size + x]) addBldg(o, x, y, bw, 3);
  }
  return o;
}
// an organic scatter: houses at random round one wandering spine and paths wandering off it (a village's look)
ew::SettlementOut scatterVillage(int size, uint32_t seed) {
  ew::SettlementOut o = blankOut(size, size);
  Rng r(seed);
  float x = 4, y = size * 0.5f, a = 0;
  while (x < size - 4) {
    for (int oy = 0; oy <= 1; oy++) setRankAt(o, (int)x, (int)y + oy, ew::Rank::Main);
    a += r.range(-0.35f, 0.35f);
    a = std::clamp(a, -0.6f, 0.6f);
    x += ew::dcos(a); y += ew::dsin(a);
  }
  for (int k = 0; k < 6; k++) {
    float px = (float)(10 + r.irange(size - 20)), py = size * 0.5f, pa = r.f() < 0.5f ? -1.4f : 1.4f;
    const int len = 18 + r.irange(14);
    for (int s = 0; s < len; s++) {
      setRankAt(o, (int)px, (int)py, ew::Rank::Footpath);
      pa += r.range(-0.5f, 0.5f);
      px += ew::dcos(pa); py += ew::dsin(pa);
    }
  }
  for (int k = 0; k < 40; k++) {
    const int bx = 6 + r.irange(size - 14), by = 6 + r.irange(size - 14);
    const int bw = 3 + r.irange(3);
    addBldg(o, bx, by, bw, 3);
  }
  for (size_t i = 0; i < o.used.size(); i++) o.used[i] = 1;
  return o;
}

// the transforms
ew::SettlementOut shifted(const ew::SettlementOut& a, int dx, int dy) {
  ew::SettlementOut o = blankOut(a.buf.w, a.buf.h);
  const int w = a.buf.w, h = a.buf.h;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const int sx = x - dx, sy = y - dy;
      if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
      const size_t i = (size_t)y * w + x, j = (size_t)sy * w + sx;
      o.buf.ground[i] = a.buf.ground[j]; o.buf.wall[i] = a.buf.wall[j]; o.buf.bldgAt[i] = a.buf.bldgAt[j]; o.buf.prop[i] = a.buf.prop[j];
      if (a.buf.urb.size() == a.buf.ground.size()) o.buf.urb[i] = a.buf.urb[j];
      o.rank[i] = a.rank[j]; o.role[i] = a.role[j]; o.used[i] = a.used.size() == a.buf.ground.size() ? a.used[j] : 0;
    }
  o.buf.bldgs = a.buf.bldgs;
  for (Bldg& b : o.buf.bldgs) { b.r.x += dx; b.r.y += dy; }
  o.ex = a.ex + dx; o.ey = a.ey + dy; o.gx = a.gx; o.gy = a.gy;
  return o;
}
// mirrored east-west, or transposed (a quarter turn and a mirror)
ew::SettlementOut flipped(const ew::SettlementOut& a, bool transpose) {
  const int w = a.buf.w, h = a.buf.h, W = transpose ? h : w, H = transpose ? w : h;
  ew::SettlementOut o = blankOut(W, H);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const int nx = transpose ? y : w - 1 - x, ny = transpose ? x : y;
      const size_t i = (size_t)ny * W + nx, j = (size_t)y * w + x;
      o.buf.ground[i] = a.buf.ground[j]; o.buf.wall[i] = a.buf.wall[j]; o.buf.bldgAt[i] = a.buf.bldgAt[j];
      o.rank[i] = a.rank[j]; o.role[i] = a.role[j]; o.used[i] = a.used.size() == a.buf.ground.size() ? a.used[j] : 0;
    }
  o.buf.bldgs = a.buf.bldgs;
  for (Bldg& b : o.buf.bldgs) b.r = transpose ? IRect{b.r.y, b.r.x, b.r.h, b.r.w} : IRect{w - b.r.x - b.r.w, b.r.y, b.r.w, b.r.h};
  o.gx = a.gx; o.gy = a.gy;
  o.ex = o.gx + (transpose ? a.ey - a.gy : w - 1 - (a.ex - a.gx));
  o.ey = o.gy + (transpose ? a.ex - a.gx : a.ey - a.gy);
  return o;
}
// cropped: the core's west-most n columns lose everything, the ring closing n tiles further east
ew::SettlementOut cropped(const ew::SettlementOut& a, int n) {
  ew::SettlementOut o = a;
  const int w = o.buf.w;
  int x0 = w;
  for (size_t i = 0; i < o.buf.wall.size(); i++) if (o.buf.wall[i]) x0 = std::min(x0, (int)(i % (size_t)w));
  if (x0 >= w - n) return o;
  for (int y = 0; y < o.buf.h; y++) {
    bool wl = false;
    for (int x = x0; x < x0 + n; x++) {
      const size_t i = (size_t)y * w + x;
      wl = wl || o.buf.wall[i];
      o.buf.wall[i] = 0; o.rank[i] = 0; o.buf.ground[i] = (uint8_t)Ground::Grass;
      const int bi = o.buf.bldgAt[i];
      if (bi >= 0) o.buf.bldgs[(size_t)bi].r.w = 0;   // (a building cut by the crop goes)
    }
    if (wl) o.buf.wall[(size_t)y * w + x0 + n] = 1;
  }
  for (size_t i = 0; i < o.buf.bldgAt.size(); i++) {
    const int bi = o.buf.bldgAt[i];
    if (bi >= 0 && o.buf.bldgs[(size_t)bi].r.w == 0) o.buf.bldgAt[i] = -1;
  }
  // (the removed buildings stay in the list as empty records: keep them out of the counts)
  std::vector<Bldg> keep;
  std::vector<int> remap(o.buf.bldgs.size(), -1);
  for (size_t k = 0; k < o.buf.bldgs.size(); k++)
    if (o.buf.bldgs[k].r.w > 0) { remap[k] = (int)keep.size(); keep.push_back(o.buf.bldgs[k]); }
  for (int32_t& bi : o.buf.bldgAt) if (bi >= 0) bi = remap[(size_t)bi];
  o.buf.bldgs = keep;
  return o;
}
// is any of the tiles [x0, x1] x y or their 8 neighbours a way (or off the buffer)?
bool nearWay(const ew::SettlementOut& o, int x0, int x1, int y) {
  for (int yy = y - 1; yy <= y + 1; yy++)
    for (int xx = x0 - 1; xx <= x1 + 1; xx++) {
      if (xx < 0 || yy < 0 || xx >= o.buf.w || yy >= o.buf.h) return true;
      if (ew::rankWay((ew::Rank)o.rank[(size_t)yy * o.buf.w + xx])) return true;
    }
  return false;
}
// the dead-end trick (stubs): a 3-tile stub across every dead-end street tip (each tip a T, two new tips)
ew::SettlementOut withStubs(const ew::SettlementOut& a) {
  ew::SettlementOut o = a;
  const int w = o.buf.w, h = o.buf.h;
  auto st = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && ew::rankStreet((ew::Rank)a.rank[(size_t)y * w + x]); };
  for (int y = 1; y < h - 1; y++)
    for (int x = 1; x < w - 1; x++) {
      if (!st(x, y)) continue;
      const int n = (int)st(x + 1, y) + (int)st(x - 1, y) + (int)st(x, y + 1) + (int)st(x, y - 1);
      if (n != 1) continue;
      const bool horiz = st(x + 1, y) || st(x - 1, y);
      for (int k = -1; k <= 1; k++) setRankAt(o, horiz ? x : x + k, horiz ? y + k : y, ew::Rank::Lane);
    }
  return o;
}
// the dead-end trick (scraps): loose 2 x 2 scraps of street in open land, touching no street
ew::SettlementOut withScraps(const ew::SettlementOut& a, uint32_t seed) {
  ew::SettlementOut o = a;
  const int w = o.buf.w, h = o.buf.h;
  Rng r(seed);
  for (int k = 0; k < 80; k++) {
    const int x = 2 + r.irange(w - 5), y = 2 + r.irange(h - 5);
    if (nearWay(o, x, x + 1, y) || nearWay(o, x, x + 1, y + 1)) continue;
    for (int oy = 0; oy < 2; oy++)
      for (int ox = 0; ox < 2; ox++) setRankAt(o, x + ox, y + oy, ew::Rank::Lane);
  }
  return o;
}
// the frontage trick (streets): a loose 1-tile sliver of street (joining no street) along the front of every
// building not on a street
ew::SettlementOut withStreetSlivers(const ew::SettlementOut& a) {
  ew::SettlementOut o = a;
  for (const Bldg& b : a.buf.bldgs) {
    const int y = b.r.y + b.r.h;
    if (y >= a.buf.h - 1) continue;
    const int x0 = b.r.x - 3, x1 = b.r.x + b.r.w + 2;
    if (nearWay(o, x0, x1, y)) continue;
    bool free = true;
    for (int x = x0; x <= x1; x++)
      if (x < 0 || x >= a.buf.w || o.buf.bldgAt[(size_t)y * a.buf.w + x] >= 0 || o.buf.wall[(size_t)y * a.buf.w + x]) free = false;
    if (!free) continue;
    for (int x = x0; x <= x1; x++) setRankAt(o, x, y, ew::Rank::Lane);
  }
  return o;
}
// the frontage trick (buildings): a 1 x 1 sliver building on every third free tile along every street's north kerb
ew::SettlementOut withBuildingSlivers(const ew::SettlementOut& a) {
  ew::SettlementOut o = a;
  const int w = a.buf.w, h = a.buf.h;
  int k = 0;
  for (int y = 1; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t i = (size_t)y * w + x, up = (size_t)(y - 1) * w + x;
      if (!ew::rankStreet((ew::Rank)a.rank[i]) || a.rank[up] || a.buf.bldgAt[up] >= 0 || a.buf.wall[up]) continue;
      if (k++ % 3) continue;
      addBldg(o, x, y - 1, 1, 1);
    }
  return o;
}

int g_ctrlBad = 0;
bool g_ctrlVerbose = false, g_ctrlQuiet = false;   // (quiet: only the failures, inside --structure / --city-unique)
void ctrl(bool ok, const char* what, double v, double want) {
  if (!ok || !g_ctrlQuiet) printf("controls: %-78s %7.3f (%s %.3f)%s\n", what, v, ok ? "ok vs" : "WANTED", want, ok ? "" : "  <-- FAIL");
  if (!ok) { printf("FAIL: controls: %s %.3f\n", what, v); g_ctrlBad++; }
}

// the similarity controls; returns the failures
int simControls(const std::vector<Built>& cities) {
  g_ctrlBad = 0;
  const ew::SettlementOut G = gridCity(180, 12, 0), G6 = gridCity(180, 12, 6), Rg = ringCity(180), V = scatterVillage(180, 5);
  const ew::PlanSig sG = ew::planSignature(G);
  const float gOff = ew::planSimilarity(sG, ew::planSignature(G6)), gRing = ew::planSimilarity(sG, ew::planSignature(Rg));
  const float gScat = ew::planSimilarity(sG, ew::planSignature(V));
  ctrl(gOff >= ew::CITY_DUP_SIM, "a grid ~ the same grid offset half a block (a duplicate)", gOff, ew::CITY_DUP_SIM);
  ctrl(gRing < ew::CITY_DUP_SIM, "a grid ~ a ring plan (different)", gRing, ew::CITY_DUP_SIM);
  ctrl(gScat < ew::CITY_DUP_SIM, "a grid ~ an organic scatter (different)", gScat, ew::CITY_DUP_SIM);
  for (const Built& C : cities) {
    const ew::SettlementOut& A = *C.out;
    const ew::PlanSig sA = ew::planSignature(A);
    auto sim = [&](const ew::SettlementOut& B) { return ew::planSimilarity(sA, ew::planSignature(B)); };
    char nm[200];
    const std::string n = C.name.substr(0, 44);
    std::snprintf(nm, sizeof nm, "%s ~ itself", n.c_str()); ctrl(sim(A) >= 0.99f, nm, sim(A), 0.99);
    const float sh = sim(shifted(A, 2, 2)), cr = sim(cropped(A, 2)), mi = sim(flipped(A, false)), tr = sim(flipped(A, true));
    std::snprintf(nm, sizeof nm, "%s ~ shifted 2 tiles", n.c_str()); ctrl(sh >= ew::CITY_DUP_SIM, nm, sh, ew::CITY_DUP_SIM);
    std::snprintf(nm, sizeof nm, "%s ~ cropped 2 tiles", n.c_str()); ctrl(cr >= ew::CITY_DUP_SIM, nm, cr, ew::CITY_DUP_SIM);
    std::snprintf(nm, sizeof nm, "%s ~ mirrored", n.c_str()); ctrl(mi >= ew::CITY_DUP_SIM, nm, mi, ew::CITY_DUP_SIM);
    std::snprintf(nm, sizeof nm, "%s ~ a quarter turn (transposed)", n.c_str()); ctrl(tr >= ew::CITY_DUP_SIM, nm, tr, ew::CITY_DUP_SIM);
    if (C.pcase >= 0) {
      ew::SettlementOut B;
      buildCase(C.pc, B, true);
      const float nv = sim(B);
      std::snprintf(nm, sizeof nm, "%s ~ rebuilt, non-plan choices changed", n.c_str()); ctrl(nv >= ew::CITY_DUP_SIM, nm, nv, ew::CITY_DUP_SIM);
    }
  }
  return g_ctrlBad;
}

// the structure controls; returns the failures
int structControls(const std::vector<Built>& places) {
  g_ctrlBad = 0;
  const ew::SettlementOut G = gridCity(180, 12, 0), V = scatterVillage(140, 9);
  const float gS = ew::structureScore(G).total, vS = ew::structureScore(V).total;
  ctrl(gS >= ew::STRUCT_CITY_FLOOR, "a pure grid of blocks with row houses (>= the city floor)", gS, ew::STRUCT_CITY_FLOOR);
  ctrl(vS <= ew::STRUCT_VILLAGE_MEAN, "an organic scatter on wandering paths (<= the villages' mean cap)", vS, ew::STRUCT_VILLAGE_MEAN);
  for (size_t c = 0; c < places.size(); c++) {
    const ew::SettlementOut& A = *places[c].out;
    const float s0 = ew::structureScore(A).total;
    const float s1 = ew::structureScore(withScraps(A, 31 + (uint32_t)c)).total;
    const float s2 = ew::structureScore(withStreetSlivers(A)).total;
    const float s3 = ew::structureScore(withBuildingSlivers(A)).total;
    const float s4 = ew::structureScore(withStubs(A)).total;
    if (g_ctrlVerbose) {
      auto parts = [](const char* tag, const ew::StructureScore& S) {
        printf("controls:     %-8s %5.1f front %.2f (%d/%d) straight %.2f reg %.2f blocks %d ends %d dead %.2f conn %.2f\n", tag, S.total, S.frontage, S.fronting, S.bldgs,
               S.straightness, S.regularity, S.blocks, S.ends, S.deadEnds, S.connectivity);
      };
      printf("controls:   %s\n", places[c].name.c_str());
      parts("plain", ew::structureScore(A));
      parts("scraps", ew::structureScore(withScraps(A, 31 + (uint32_t)c)));
      parts("slivers", ew::structureScore(withStreetSlivers(A)));
      parts("bslivers", ew::structureScore(withBuildingSlivers(A)));
      parts("stubs", ew::structureScore(withStubs(A)));
    }
    char nm[200];
    const std::string n = places[c].name.substr(0, 40);
    std::snprintf(nm, sizeof nm, "%s %.1f: + loose street scraps (gain)", n.c_str(), s0); ctrl(s1 - s0 <= CTRL_TRICK, nm, s1 - s0, CTRL_TRICK);
    std::snprintf(nm, sizeof nm, "%s %.1f: + stubs across the dead ends (gain)", n.c_str(), s0); ctrl(s4 - s0 <= CTRL_TRICK, nm, s4 - s0, CTRL_TRICK);
    std::snprintf(nm, sizeof nm, "%s %.1f: + street slivers (gain)", n.c_str(), s0); ctrl(s2 - s0 <= CTRL_TRICK, nm, s2 - s0, CTRL_TRICK);
    std::snprintf(nm, sizeof nm, "%s %.1f: + sliver buildings (gain)", n.c_str(), s0); ctrl(s3 - s0 <= CTRL_TRICK, nm, s3 - s0, CTRL_TRICK);
  }
  return g_ctrlBad;
}

// a sample of a run's settlements for the controls: a capital, three cities, two towns, two villages (synthetic)
std::vector<Built> controlSample(const std::vector<Built>& all) {
  std::vector<Built> pick;
  int nc = 0, nt = 0, nv = 0, ncap = 0;
  for (const Built& B : all) {
    if (B.pcase < 0) continue;
    if (B.type == SiteType::City && B.capital && ncap < 1) { pick.push_back(B); ncap++; }
    else if (B.type == SiteType::City && !B.capital && nc < 3) { pick.push_back(B); nc++; }
    else if (B.type == SiteType::Town && nt < 2) { pick.push_back(B); nt++; }
    else if (B.type == SiteType::Village && nv < 2) { pick.push_back(B); nv++; }
  }
  return pick;
}

int cmdControls(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  int R = -1;
  bool strict = false, verbose = false;
  parseCommon(argc, argv, a, b, R, strict, verbose);
  g_ctrlVerbose = verbose;
  const std::vector<Built> pick = controlSample(gather(a, b, -1, 7));
  std::vector<Built> cities;
  for (const Built& B : pick) if (B.type == SiteType::City) cities.push_back(B);
  const int bs = simControls(cities);
  const int bt = structControls(pick);
  printf("controls: %s (%d similarity, %d structure failures)\n", bs + bt ? "FAILED" : "ok", bs, bt);
  return bs + bt ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--plan-controls", "M7c: the metrics checked: duplicates caught (shift, crop, mirror, offset grid, non-plan rebuild), tricks earn nothing [--seeds A..B]", cmdControls);
RPG_TEST_CMD("--structure", "M7c: structure score per settlement (city >> town >> village, city floor) [--seeds A..B] [--real R] [--strict] [--verbose]", cmdStructure);
RPG_TEST_CMD("--city-unique", "M7c: city plan signatures, no near-duplicates across seeds [--seeds A..B] [--real R] [--strict] [--top N]", cmdCityUnique);
RPG_TEST_CMD("--plan", "M7c: one line per settlement: order, age, traits, ranks, structure [--seeds A..B] [--type village|town|city] [--real R] [--png DIR: rank and role layers]", cmdPlan);
