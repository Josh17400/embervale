// rpg_test --builder (M3b "Builders & Societies", VISION_PLAN 15.14): the builder and society generators' contracts.
//
//   rpg_test --builder [--seeds A..B] [--strict] [--society-strict] [--verbose]
//
// Always gated (phase A on):
//   1. the registry: every art::Prop is classified (bld::kindOfProp), every art::Building has a name;
//   2. the design matrix: every art::Building x 13 styles (no culture + the 12 archetypes) x wealth 0..3 x every form x
//      three footprints designs a blueprint that passes bld::validate, deterministically (designing twice gives the
//      same key and volumes) and with a resolved form;
//   3. the society: every archetype (several seeds each) has a complete society, and its requirements for every tier
//      hold the essentials (an inn and a smith everywhere (15.11), a temple from towns up, the seat of power in cities
//      and capitals, each required purpose a real building or a known space);
//   4. placed buildings: a capital, a city, a town and a village of every archetype are built (synthetic plains sites,
//      like rpg_test --towns), and every building in them designs a valid blueprint from its Bldg (bldgBlueprint).
// Reported, gated with --strict (the BUILDER lane's acceptance):
//   5. the capital repetition audit per culture: buildings whose massing + palette signature (bld::Blueprint volumes and
//      style tints, not the seed) equals a neighbour's within 12 tiles ("neighbours never clones"), and the largest group
//      of one signature in the capital.
// Reported, gated with --society-strict (the SOCIETY/TOWNS lane's acceptance):
//   6. every required building of the society's requirements is placed in each built settlement (by purpose, with the
//      seat of power marked CIVIC_SEAT in cities and capitals).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/build/blueprint.h"
#include "rpg/build/registry.h"
#include "rpg/culture/society.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_gen.h"
#include "tools/tests/tests.h"

namespace {

using art::Building;

uint64_t mixk(uint64_t k, uint64_t v) {
  k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2);
  k *= 0xBF58476D1CE4E5B9ull;
  return k ^ (k >> 31);
}

// the look of a blueprint without its seed: massing, materials and palette as the eye reads them (tints quantised to
// 3 bits a channel, so the culture's +-6 % jitter does not make two copies "different")
uint64_t q3(uint32_t c) { return c ? (uint64_t)((c >> 5) & 7u) | (uint64_t)((c >> 13) & 7u) << 3 | (uint64_t)((c >> 21) & 7u) << 6 | 512u : 0u; }
uint64_t lookSignature(const bld::Blueprint& b) {
  uint64_t k = 0xC10E5u;
  k = mixk(k, (uint64_t)b.req.purpose | (uint64_t)b.form << 8 | (uint64_t)b.req.wTiles << 16 | (uint64_t)b.req.hTiles << 24);
  for (const bld::Volume& v : b.vols) {
    k = mixk(k, (uint64_t)v.shape | (uint64_t)v.role << 8 | (uint64_t)(uint16_t)v.x0 << 16 | (uint64_t)(uint16_t)v.y0 << 32 | (uint64_t)(uint16_t)v.x1 << 48);
    k = mixk(k, (uint64_t)(uint16_t)v.y1 | (uint64_t)(uint16_t)v.z0 << 16 | (uint64_t)(uint16_t)v.wallH << 32 | (uint64_t)v.storeys << 48);
    k = mixk(k, (uint64_t)v.roof | (uint64_t)v.roofMat << 8 | (uint64_t)v.wall << 16 | (uint64_t)v.pitch << 24 | (uint64_t)v.ridgeNS << 32 |
                    (uint64_t)v.window << 40 | (uint64_t)v.door << 48);
    k = mixk(k, (uint64_t)v.ornament | q3(v.wallTint) << 16 | q3(v.roofTint) << 32 | q3(v.trimTint) << 48);
  }
  const art::ArchStyle& s = b.style;
  k = mixk(k, (uint64_t)s.roof | (uint64_t)s.roofMat << 8 | (uint64_t)s.wall << 16 | (uint64_t)s.window << 24 | (uint64_t)s.door << 32 |
                  (uint64_t)s.ornament << 40);
  k = mixk(k, q3(s.roofTint) | q3(s.wallTint) << 12 | q3(s.trimTint) << 24 | q3(s.accentTint) << 36);
  k = mixk(k, (uint64_t)b.sign.kind | (uint64_t)b.sign.icon << 8 | (uint64_t)b.yard << 16);
  return k;
}

// a synthetic settlement on open plains (as rpg_test --towns builds its cases)
void buildSite(SiteType type, bool capital, int archetype, uint32_t seed, ew::SettlementOut& so, cult::Culture& K) {
  static ew::SitePlan p;
  static ew::KingdomPlan k;
  p = ew::SitePlan();
  p.type = type;
  p.archetype = ew::Archetype::Plain;
  p.seed = seed;
  ew::settlementFootprint(type, p.archetype, p.seed, p.w, p.h);
  p.gx = 1000 - p.w / 2; p.gy = -2000 - p.h / 2; p.ex = 1000; p.ey = -2000;
  p.bldgCap = 400;
  p.flags = capital ? ew::SPF_CAPITAL : 0;
  p.name = "TESTHOLD";
  k = ew::KingdomPlan();
  k.id = 77; k.name = "TESTMARK"; k.color = 0xFFA04628u; k.color2 = 0xFF50C8E6u; k.emblem = (uint8_t)(seed % 8);
  K = cult::Atlas::make((cult::Archetype)archetype, (uint32_t)ew::mix64(seed ^ 0xC17u), 2);
  ew::SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = &k;
  ctx.culture = &K;
  ctx.rx = 3; ctx.ry = -8;
  ctx.roadBearings = {0.0f, 3.14159f, 1.5708f};
  ctx.base = [](int32_t, int32_t, Ground& g, Biome& bi, uint8_t& h) { g = Ground::Grass; bi = Biome::Plains; h = 1; };
  so = ew::SettlementOut();
  ew::buildSettlement(ctx, so);
}

// (owner 2026-10-06) an open front's geometry: the door column is a bay, every entry tile has a spot where a person
// (radius 4.5 px) stands clear of the pillars and inside the open span, and no pillar stands in the door column's
// middle. Returns "" or what is wrong.
std::string openFrontWhy(const bld::Blueprint& bp, const bld::OpenFront& of) {
  const int wt = bp.req.wTiles, dcol = wt / 2;
  if (of.open() && !(of.gaps & (1u << dcol))) return "the door column is no walk-in bay";
  if (of.raised) return "";
  if (of.open() && of.bays < 1) return "no walk-in bay";
  for (int t = 0; t < std::min(32, wt); t++) {
    if (!((of.gaps >> t) & 1u)) continue;
    // a person (9 px wide) stands within the tile clear of every blocking px column
    int run = 0, best = 0;
    for (int px = 0; px < 16; px++) { run = ((of.solid[(size_t)t] >> px) & 1u) ? 0 : run + 1; best = std::max(best, run); }
    if (best < bld::OPEN_MIN_BAY) return "entry tile " + std::to_string(t) + " has no room to walk in between the pillars";
  }
  if (of.open() && (of.solid[(size_t)dcol] & 0x0FF0u)) return "a pillar stands in the door column";
  return "";
}

cult::SettleTier tierOf(SiteType t, bool capital) {
  if (capital) return cult::SettleTier::Capital;
  return t == SiteType::City ? cult::SettleTier::City : (t == SiteType::Town ? cult::SettleTier::Town : cult::SettleTier::Village);
}

int cmdBuilder(int argc, char** argv) {
  uint64_t a = 1, b = 2;
  bool strict = false, socStrict = false, verbose = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--strict")) strict = true;
    else if (!strcmp(argv[i], "--society-strict")) socStrict = true;
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
  }
  int bad = 0;
  const int NA = (int)cult::Archetype::COUNT;

  // ---- 1. the registry
  int unclassified = 0;
  std::map<int, int> kinds;
  for (int p = 0; p < (int)art::Prop::COUNT; p++) {
    const bld::Kind k = bld::kindOfProp((art::Prop)p);
    if (k == bld::Kind::COUNT) { out("FAIL: builder registry: prop %d is not classified (rpg/build/registry.cpp)\n", p); unclassified++; }
    kinds[(int)k]++;
  }
  bad += unclassified;
  for (int t = 0; t < (int)Building::COUNT; t++)
    if (!bldgTypeName((Building)t) || !*bldgTypeName((Building)t)) { out("FAIL: building %d has no name\n", t); bad++; }
  std::string kl;
  for (auto& kv : kinds) kl += std::string(" ") + bld::kindName((bld::Kind)kv.first) + " " + std::to_string(kv.second);
  printf("builder registry: %d props classified:%s\n", (int)art::Prop::COUNT - unclassified, kl.c_str());

  // ---- 2. the design matrix
  int designs = 0, invalid = 0, nondet = 0;
  int openDesigns = 0, openBad = 0, openRaised = 0, openOther = 0, openGalleries = 0;
  std::map<int, int> openByFace;
  static const int sizes[3][2] = {{3, 2}, {5, 3}, {9, 5}};
  for (int t = 0; t < (int)Building::COUNT; t++)
    for (int c = -1; c < NA; c++) {
      art::ArchStyle st = c < 0 ? art::archForBiome(2, 7u + (uint32_t)t) : cult::buildingArch(cult::Atlas::make((cult::Archetype)c, 1000u + (uint32_t)c, 2), 2, 1, 1, 99u + (uint32_t)t);
      for (int w = 0; w < 4; w++)
        for (int f = 0; f < (int)bld::Form::COUNT; f++)
          for (int s = 0; s < 3; s++) {
            bld::Request r = bld::simpleRequest((Building)t, sizes[s][0], sizes[s][1], st, 31u * (uint32_t)t + 7u * (uint32_t)s + 1u);
            r.wealth = (uint8_t)w; r.form = (bld::Form)f; r.urban = (uint8_t)(w > 2 ? 3 : w);
            const bld::Blueprint b1 = bld::design(r), b2 = bld::design(r);
            designs++;
            const char* why = bld::validate(b1);
            if (*why) {
              if (invalid < 8) out("FAIL: builder: %s %dx%d style %d wealth %d form %s: %s\n", bldgTypeName((Building)t), sizes[s][0], sizes[s][1], c, w, bld::formName((bld::Form)f), why);
              invalid++;
            }
            if (b1.key != b2.key || lookSignature(b1) != lookSignature(b2) || b1.vols.size() != b2.vols.size()) nondet++;
            // (owner) open fronts: walked into between the pillars
            const bld::OpenFront of = bld::openFront(b1);
            if (of.gaps && !of.open()) { openGalleries++; const std::string ow = openFrontWhy(b1, of); if (!ow.empty()) { if (openBad < 8) out("FAIL: open gallery: %s style %d: %s\n", bldgTypeName((Building)t), c, ow.c_str()); openBad++; } }
            if (of.open()) {
              openDesigns++;
              openByFace[(int)of.face]++;
              if (of.raised) openRaised++;
              const std::string ow = openFrontWhy(b1, of);
              if (!ow.empty()) {
                if (openBad < 8) out("FAIL: open front: %s %dx%d style %d wealth %d form %s: %s\n", bldgTypeName((Building)t), sizes[s][0], sizes[s][1], c, w, bld::formName((bld::Form)f), ow.c_str());
                openBad++;
              }
            }
            for (const bld::Volume& v : b1.vols) if (bld::openFaceKind(v.face) && !v.doorHere) { openOther++; break; }
          }
    }
  if (nondet) { out("FAIL: builder: %d designs not deterministic\n", nondet); bad++; }
  bad += invalid;
  printf("builder designs: %d (every purpose x 13 styles x 4 wealths x %d forms x 3 footprints), %d invalid, %d non-deterministic\n", designs,
         (int)bld::Form::COUNT, invalid, nondet);
  bad += openBad;
  {
    static const char* fn[] = {"windows", "shopfront", "forge bay", "barn door", "slits", "sacred", "colonnade", "arcade", "veranda", "blank", "gate",
                               "iwan", "vents", "arcane", "lattice"};
    std::string fl;
    for (auto& kv : openByFace) fl += std::string(" ") + (kv.first < 15 ? fn[kv.first] : "?") + " " + std::to_string(kv.second);
    printf("open fronts: %d designs entered between pillars (%s; %d raised: ladder / steps), %d failures; %d designs with an open face beside a closed "
           "entrance, %d of them with galleries walked into\n", openDesigns, fl.c_str(), openRaised, openBad, openOther, openGalleries);
  }

  // ---- 3. the society
  int socBad = 0;
  std::map<int, int> govs, seats;
  for (int c = 0; c < NA; c++)
    for (uint32_t s = 0; s < 8; s++) {
      const cult::Culture K = cult::Atlas::make((cult::Archetype)c, 500u + s * 977u, 2);
      const cult::Society S = cult::societyOf(K);
      govs[(int)S.government]++; seats[(int)S.seat]++;
      if (S.government >= cult::Government::COUNT || S.seat >= cult::Seat::COUNT || !S.rulerTitle || !S.lordTitle || !S.seatTitle) {
        out("FAIL: society of %s seed %u is incomplete\n", cult::archetypeName((cult::Archetype)c), s); socBad++; continue;
      }
      for (int ti = 0; ti < (int)cult::SettleTier::COUNT; ti++) {
        const auto req = cult::requiredBuildings(S, K, (cult::SettleTier)ti, 0, s);
        bool inn = false, smith = false, temple = false, seat = false;
        for (const cult::BuildingReq& q : req) {
          if (q.purpose >= Building::COUNT) { out("FAIL: society %s tier %d asks for an unknown purpose\n", cult::archetypeName((cult::Archetype)c), ti); socBad++; }
          if (q.purpose == Building::Inn && q.required) inn = true;
          if (q.purpose == Building::Smithy && q.required) smith = true;
          if (q.purpose == Building::Temple && q.required) temple = true;
          if ((q.civic & bld::CIVIC_SEAT) && q.required) seat = true;
        }
        const bool needTemple = ti >= (int)cult::SettleTier::Town, needSeat = ti >= (int)cult::SettleTier::City;
        if (!inn || !smith || (needTemple && !temple) || (needSeat && !seat)) {
          out("FAIL: society %s seed %u tier %d lacks%s%s%s%s\n", cult::archetypeName((cult::Archetype)c), s, ti, inn ? "" : " an inn", smith ? "" : " a smith",
              needTemple && !temple ? " a temple" : "", needSeat && !seat ? " the seat" : "");
          socBad++;
        }
      }
    }
  bad += socBad;
  std::string gl, sl;
  for (auto& kv : govs) gl += std::string(" ") + cult::governmentName((cult::Government)kv.first) + " " + std::to_string(kv.second);
  for (auto& kv : seats) sl += std::string(" ") + cult::seatName((cult::Seat)kv.first) + " " + std::to_string(kv.second);
  printf("societies: %d archetypes x 8 seeds, %d failures; governments:%s; seats:%s\n", NA, socBad, gl.c_str(), sl.c_str());

  // ---- 4..6. placed buildings, the capital repetition audit, the society's requirements met
  int placedBad = 0, cloneTotal = 0, worstGroupAll = 0, missingTotal = 0, built = 0;
  int placedOpen = 0, placedOpenBad = 0, placedBays = 0, interiorBays = 0, galleryAtDoor = 0;
  for (uint64_t seed = a; seed <= b; seed++) {
    g_curSeed = seed;
    for (int c = 0; c < NA; c++) {
      static const struct { SiteType t; bool cap; } kinds4[4] = {{SiteType::City, true}, {SiteType::City, false}, {SiteType::Town, false}, {SiteType::Village, false}};
      for (const auto& kd : kinds4) {
        ew::SettlementOut so;
        cult::Culture K;
        const uint32_t sseed = (uint32_t)ew::mix64(seed * 1315423911ull + (uint64_t)c * 2654435761ull + (uint64_t)kd.t * 97u + (kd.cap ? 5u : 0u));
        buildSite(kd.t, kd.cap, c, sseed, so, K);
        built++;
        const std::vector<Bldg>& B = so.buf.bldgs;
        std::vector<uint64_t> sig(B.size());
        for (size_t i = 0; i < B.size(); i++) {
          const Bldg& x = B[i];
          if (x.wealth > 3 || x.form >= (uint8_t)bld::Form::COUNT) { out("FAIL: %s %s building %zu: wealth %d form %d\n", cult::archetypeName(K.archetype), siteTypeName(kd.t), i, x.wealth, x.form); placedBad++; }
          const bld::Blueprint bp = bldgBlueprint(x);
          const char* why = bld::validate(bp);
          if (*why) { if (placedBad < 8) out("FAIL: %s %s %s at %d,%d: %s\n", cult::archetypeName(K.archetype), siteTypeName(kd.t), bldgTypeName(x.type), x.r.x, x.r.y, why); placedBad++; }
          sig[i] = lookSignature(bp);
        }
        // (owner) every open front placed: its bays open in the world (walkable entry tiles that enter it) and lead
        // into the building (the ground floor opens onto the porch in matching bays, each onto the room's floor)
        {
          Map M = so.buf;
          M.rebuildSolid();
          for (size_t i = 0; i < B.size(); i++) {
            const Bldg& x = M.bldgs[i];
            const Bldg::Open& o = bldgOpenFront(x);
            if (!o.open && (o.mask & (o.mask - 1u)) == 0) continue;   // a door alone
            placedOpen++;
            const std::vector<int> cols = bldgEntryColumns(x);
            std::string why;
            for (int cx : cols) {
              if (!bldgEntryAt(x, cx, x.doorY())) why = "column " + std::to_string(cx - x.r.x) + " does not enter";
              else if (M.in(cx, x.doorY()) && M.blocked(cx, x.doorY())) why = "entry column " + std::to_string(cx - x.r.x) + " is blocked (ground " + std::to_string((int)M.at(cx, x.doorY())) + " prop " + std::to_string(M.propAt(cx, x.doorY())) + " height " + std::to_string((int)M.heightBits(cx, x.doorY())) + ") raised " + std::to_string((int)o.raised) + " z0 " + std::to_string((int)bldgBlueprint(x).vols[(size_t)bldgBlueprint(x).doorVol()].z0) + " feat " + std::to_string((unsigned)bldgBlueprint(x).vols[(size_t)bldgBlueprint(x).doorVol()].feat) + " stilts " + std::to_string((int)x.arch.stilts);
            }
            placedBays += (int)cols.size();
            if (why.empty()) {
              Map in;
              genInterior(in, x, x.seed, 0);
              const int ni = in.exits.empty() ? 1 : (int)in.exits.size();
              if (!o.open && cols.size() > 1 && in.exits.size() < 2) galleryAtDoor++;   // galleries beside a door the rooms behind can't open onto: in by the door
              else if (!o.raised && cols.size() > 1 && in.exits.size() < 2) why = "the interior has one doorway for an open front of " + std::to_string(cols.size()) + " bays";
              for (int k = 0; k < ni && why.empty(); k++) {
                const int ex = in.exits.empty() ? in.exitX : in.exits[(size_t)k];
                if (!in.isExit(ex, in.exitY) || groundSolid(in.at(ex, in.exitY)) || groundSolid(in.at(ex, in.exitY - 1))) why = "an interior bay does not open onto the floor";
              }
              interiorBays += ni;
            }
            if (!why.empty()) {
              if (placedOpenBad < 8) out("FAIL: open front placed: %s %s %s at %d,%d: %s\n", cult::archetypeName(K.archetype), kd.cap ? "capital" : siteTypeName(kd.t), bldgTypeName(x.type), x.r.x, x.r.y, why.c_str());
              placedOpenBad++;
            }
          }
        }
        // 6. the society's requirements
        const cult::Society S = cult::societyOf(K);
        const auto req = cult::requiredBuildings(S, K, tierOf(kd.t, kd.cap), (int)ew::Archetype::Plain, sseed);
        int missing = 0;
        std::string miss;
        for (const cult::BuildingReq& q : req) {
          if (!q.required) continue;
          int have = 0;
          for (const Bldg& x : B) if (x.type == q.purpose && (!(q.civic & bld::CIVIC_SEAT) || (x.civic & bld::CIVIC_SEAT))) have++;
          if (have < 1) { missing++; miss += std::string(" ") + bldgTypeName(q.purpose) + ((q.civic & bld::CIVIC_SEAT) ? "(seat)" : ""); }
        }
        missingTotal += missing;
        if (missing && (socStrict || verbose)) out("%s%s %s%s: missing%s\n", socStrict ? "FAIL: society: " : "  ", cult::archetypeName(K.archetype), kd.cap ? "capital" : siteTypeName(kd.t), "", miss.c_str());
        // 5. the capital repetition audit
        if (!kd.cap) continue;
        int clones = 0, worst = 0;
        std::map<uint64_t, int> groups;
        for (size_t i = 0; i < B.size(); i++) {
          groups[sig[i]]++;
          for (size_t j = 0; j < B.size(); j++) {
            if (i == j || sig[i] != sig[j]) continue;
            const int dx = B[i].r.cx() - B[j].r.cx(), dy = B[i].r.cy() - B[j].r.cy();
            if (dx * dx + dy * dy <= 12 * 12) {
              clones++;
              if (std::getenv("EMB_BUILDER_CLONES"))   // debugging aid: which buildings clone a neighbour
                out("  clone: %s %s %dx%d w%d f%d at %d,%d ~ %s %dx%d at %d,%d\n", cult::archetypeName(K.archetype), bldgTypeName(B[i].type), B[i].r.w, B[i].r.h, B[i].wealth, B[i].form,
                    B[i].r.x, B[i].r.y, bldgTypeName(B[j].type), B[j].r.w, B[j].r.h, B[j].r.x, B[j].r.y);
              break;
            }
          }
        }
        for (auto& kv : groups) worst = std::max(worst, kv.second);
        cloneTotal += clones;
        worstGroupAll = std::max(worstGroupAll, worst);
        if (verbose || (strict && (clones || worst > 3)))
          out("%scapital of %s (seed %llu): %zu buildings, %zu looks, %d with a clone within 12 tiles, largest look group %d\n",
              strict && (clones || worst > 3) ? "FAIL: builder repetition: " : "  ", cult::archetypeName(K.archetype), (unsigned long long)seed, B.size(), groups.size(), clones, worst);
        if (strict && (clones || worst > 3)) bad++;
      }
    }
  }
  bad += placedBad + placedOpenBad;
  printf("open fronts placed: %d buildings, %d entry bays outside, %d bays inside (%d with galleries arriving by the door), %d failures\n", placedOpen, placedBays, interiorBays, galleryAtDoor, placedOpenBad);
  if (socStrict && missingTotal) bad++;
  printf("builder settlements: %d built (seeds %llu..%llu, 4 tiers x %d archetypes), %d invalid blueprints; capitals: %d buildings with a "
         "clone neighbour, largest look group %d (target 0 / <= 3%s); society requirements missing: %d%s\n",
         built, (unsigned long long)a, (unsigned long long)b, NA, placedBad, cloneTotal, worstGroupAll, strict ? ", gated" : ", --strict gates",
         missingTotal, socStrict ? " (gated)" : " (--society-strict gates)");
  printf("builder: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--builder", "M3b builder + society contracts: registry, design matrix, societies, placed blueprints [--seeds A..B] [--strict: capital repetition] [--society-strict: requirements placed] [--verbose]", cmdBuilder);
