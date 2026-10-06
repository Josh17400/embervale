// rpg_test --cultures [N] [--seeds A..B] [--report]: the culture engine (VISION_PLAN 5.4, M3). CULTURE lane.
// For each seed, the families of about N culture cells round the origin (a square of cells) and the dialects of the
// kingdoms there, measured with cult::distanceQ:
//   - neighbouring families (8-neighbourhood): d >= 0.45 and >= 3 of the 5 headline components differ;
//   - dialects: d >= 0.10 from their family, d >= 0.10 between neighbouring kingdoms of one family, and neighbouring
//     kingdoms' arms differ in >= 2 of {tinctures, division, charge};
//   - over all seeds, the 5th percentile of neighbour distances (target >= 0.5) is reported;
//   - order independence: the same cells asked for in another order (a fresh atlas, reversed) give identical cultures;
//   - names: 2000 of each kind per seed (places, men, women, kingdoms, dungeons, landmarks) plus every alloy and god:
//     upper case A-Z with space / apostrophe / hyphen only, 3..12 letters (people 3..10, dungeons <= 18), no run of
//     4 consonants or 4 vowels, no real-world fantasy names, no profanity; and varied (distinct share reported);
//   - every culture is complete (alloys 1..3 with 2..4 ingredients, a name, gods, a palette, a people mix);
//   - cost: families and dialects per ms (the atlas's own stats).
// --report prints the numbers without failing.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

// the lists the generator screens against, written out again here so the test checks independently
const char* const kIP[] = {"SKYRIM", "TAMRIEL", "WESTEROS", "GONDOR", "MORDOR", "ROHAN", "NARNIA", "HOGWARTS", "AZEROTH", "HYRULE",
                           "RIVENDELL", "ISENGARD", "LOTHLORIEN", "WINTERFELL", "MORROWIND", "NUMENOR", "VALINOR", "FAERUN",
                           "MIDGARD", "ASGARD", "ANDOR", "ZELDA", "SAURON", "GANDALF"};
const char* const kRude[] = {"FUCK", "SHIT", "CUNT", "COCK", "DICK", "PISS", "TWAT", "SLUT", "WHORE", "FAG", "NIGG", "RAPE",
                             "PENIS", "VAGIN", "TITS", "BITCH", "ANUS", "PORN", "SEX", "CRAP", "NAZI", "HITLER", "JIZZ", "WANK",
                             "ARSE", "TURD", "SPERM", "COON", "SPIC", "KIKE", "GOOK", "CHINK", "DAMN", "CUM", "BUTT", "BOOB"};

bool isV(char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y'; }
// "" when fine, else why not
// maxCons: 3 for words of one tongue, 4 for compounds ("STORMSTEEL")
const char* nameProblem(const std::string& s, size_t maxLen, int maxCons = 3) {
  if (s.size() < 3) return "too short";
  if (s.size() > maxLen) return "too long";
  int cons = 0, vow = 0;
  for (char c : s) {
    if (c == ' ' || c == '-' || c == '\'') { cons = vow = 0; continue; }
    if (c < 'A' || c > 'Z') return "bad character";
    if (isV(c)) { vow++; cons = 0; } else { cons++; vow = 0; }
    if (cons > maxCons) return "too many consonants in a row";
    if (vow > 3) return "4 vowels in a row";
    (void)0;
  }
  for (const char* b : kIP) if (s.find(b) != std::string::npos) return "real-world fantasy name";
  for (const char* b : kRude) if (s.find(b) != std::string::npos) return "profanity";
  return "";
}

int heraldryDiffs(const cult::Heraldry& a, const cult::Heraldry& b) {
  const bool tinct = a.field != b.field || a.field2 != b.field2 || a.charge != b.charge;
  const bool divi = a.division != b.division;
  const bool chg = a.chargeKind != b.chargeKind || a.emblem != b.emblem || (a.chargeKind == 1 && a.glyphSeed != b.glyphSeed);
  return (int)tinct + (int)divi + (int)chg;
}

uint64_t fingerprint(const cult::Culture& c) {
  uint64_t k = 0xCBF29CE484222325ull;
  auto mx = [&](uint64_t v) { k ^= v; k *= 0x100000001B3ull; };
  auto ms = [&](const std::string& s) { for (char ch : s) mx((uint8_t)ch); mx(0xFF); };
  ms(c.name); ms(c.adjective);
  mx((uint64_t)c.archetype | (uint64_t)c.archetype2 << 8 | (uint64_t)c.isolated << 16);
  mx(c.arch.key()); mx(c.props.key()); mx(c.music.pack()); mx(c.heraldry.key());
  for (uint32_t cl : c.dress.cloth) mx(cl);
  for (const cult::Alloy& a : c.arms.alloys) ms(a.name);
  for (const std::string& g : c.faith.names) ms(g);
  mx((uint64_t)c.town.layout | (uint64_t)c.town.wall << 8 | (uint64_t)c.dress.cutM << 16 | (uint64_t)c.arms.helm[0] << 24);
  return k;
}

int cmdCultures(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  int n = 400;
  bool report = false;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--report")) report = true;
    else if (argv[i][0] >= '0' && argv[i][0] <= '9') n = std::max(9, atoi(argv[i]));
  }
  int side = 3;
  while (side * side < n) side++;
  int bad = 0;
  std::vector<float> all, dial;
  int archCount[(int)cult::Archetype::COUNT] = {};
  int isolated = 0, families = 0;
  double famMs = 0, dialMs = 0;
  int famMade = 0, dialMade = 0, candidates = 0;
  size_t namesChecked = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    ew::EndlessSource src(seed);
    cult::Atlas& at = src.atlas();
    auto t0 = std::chrono::steady_clock::now();
    const int h = side / 2;
    int famFail = 0, headFail = 0, pairs = 0;
    float dmin = 1;
    for (int j = -h; j < side - h; j++)
      for (int i = -h; i < side - h; i++) {
        const cult::Culture& c = at.family(i, j);
        archCount[(int)c.archetype]++;
        isolated += c.isolated;
        families++;
        for (int dj = 0; dj <= 1; dj++)
          for (int di = -1; di <= 1; di++) {
            if (dj == 0 && di <= 0) continue;   // each unordered neighbour pair once
            if (i + di >= side - h || j + dj >= side - h || i + di < -h) continue;
            const cult::Culture& o = at.family(i + di, j + dj);
            const float d = cult::distance(c, o);
            all.push_back(d);
            dmin = std::min(dmin, d);
            pairs++;
            if (d < 0.45f) famFail++;
            if (cult::headlineDiffs(c, o) < 3) headFail++;
          }
      }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    famMs += at.stats().ms;
    famMade += at.stats().families;
    candidates += at.stats().candidates;

    // order independence: a fresh atlas asked in reverse order
    int orderFail = 0;
    {
      ew::EndlessSource src2(seed);
      cult::Atlas& at2 = src2.atlas();
      for (int j = side - h - 1; j >= -h; j--)
        for (int i = side - h - 1; i >= -h; i--)
          if (fingerprint(at.family(i, j)) != fingerprint(at2.family(i, j))) orderFail++;
    }

    // dialects over the kingdom cells of the same square
    int dialFail = 0, sibFail = 0, armsFail = 0, kingdoms = 0;
    {
      const double d0 = at.stats().ms;
      const int d0n = at.stats().dialects;
      const int32_t k0 = ew::floorDiv(-h * ew::CCELL, ew::KCELL), k1 = ew::floorDiv((side - h) * ew::CCELL, ew::KCELL);
      std::vector<const ew::KingdomPlan*> grid;
      const int kw = k1 - k0;
      grid.assign((size_t)kw * kw, nullptr);
      for (int32_t ky = k0; ky < k1; ky++)
        for (int32_t kx = k0; kx < k1; kx++) {
          const ew::KingdomPlan* K = src.kingdom(ew::makeId(kx, ky, ew::IdKind::Kingdom, 0));
          if (!K || !K->culture) continue;
          grid[(size_t)((ky - k0) * kw + (kx - k0))] = K;
          kingdoms++;
          const cult::Culture& D = src.culture(K->culture);
          const cult::Culture& F = at.family(cult::cultureCi(K->culture), cult::cultureCj(K->culture));
          const float d = cult::distance(D, F);
          dial.push_back(d);
          if (d < 0.10f) dialFail++;
        }
      for (int y = 0; y < kw; y++)
        for (int x = 0; x < kw; x++) {
          const ew::KingdomPlan* K = grid[(size_t)(y * kw + x)];
          if (!K) continue;
          for (int dy = 0; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              if (dy == 0 && dx <= 0) continue;
              if (x + dx < 0 || x + dx >= kw || y + dy >= kw) continue;
              const ew::KingdomPlan* O = grid[(size_t)((y + dy) * kw + x + dx)];
              if (!O) continue;
              if (heraldryDiffs(K->heraldry, O->heraldry) < 2) armsFail++;
              if (cult::familyOf(K->culture) == cult::familyOf(O->culture) &&
                  cult::distance(src.culture(K->culture), src.culture(O->culture)) < 0.10f)
                sibFail++;
            }
        }
      dialMs += at.stats().ms - d0;
      dialMade += at.stats().dialects - d0n;
    }

    // names: 2000 of each kind, round the families of the square
    int nameFail = 0;
    std::set<std::string> distinctPlaces;
    {
      std::vector<const cult::Culture*> cs;
      for (int j = -h; j < side - h; j++)
        for (int i = -h; i < side - h; i++) cs.push_back(&at.family(i, j));
      auto check = [&](const std::string& s, size_t maxLen, const char* kind, const cult::Culture& c, int maxCons = 3) {
        namesChecked++;
        const char* why = nameProblem(s, maxLen, maxCons);
        if (*why) {
          if (nameFail < 8) out("  bad %s name \"%s\" (%s, %s)\n", kind, s.c_str(), why, cult::archetypeName(c.archetype));
          nameFail++;
        }
      };
      for (uint32_t k = 0; k < 2000; k++) {
        const cult::Culture& c = *cs[k % cs.size()];
        const uint32_t s = k * 2654435761u + (uint32_t)seed;
        const std::string pl = cult::placeName(c, s);
        distinctPlaces.insert(pl);
        check(pl, 12, "place", c);
        check(cult::personName(c, s, false), 10, "man's", c);
        check(cult::personName(c, s, true), 10, "woman's", c);
        check(cult::kingdomName(c, s), 12, "kingdom", c);
        check(cult::landmarkName(c, s, (int)(k % 10)), 12, "landmark", c);
        check(cult::dungeonName(c, s, 3 + (int)(k % 5)), 18, "dungeon", c);
      }
      for (const cult::Culture* c : cs) {
        check(c->name, 12, "culture", *c);
        if (c->arms.alloys.empty() || c->arms.alloys.size() > 3) { out("  culture %s: %zu alloys\n", c->name.c_str(), c->arms.alloys.size()); nameFail++; }
        for (const cult::Alloy& a : c->arms.alloys) {
          check(a.name, 12, "alloy", *c, 4);
          if (a.recipe.size() < 2 || a.recipe.size() > 4) { out("  alloy %s: %zu ingredients\n", a.name.c_str(), a.recipe.size()); nameFail++; }
        }
        if (c->faith.names.empty()) { out("  culture %s has no gods\n", c->name.c_str()); nameFail++; }
        for (const std::string& g : c->faith.names) check(g, 10, "god", *c);
        if (!c->arch.roofTint || !c->arch.wallTint || !c->arch.accentTint || !c->dress.cloth[0]) { out("  culture %s: palette hole\n", c->name.c_str()); nameFail++; }
      }
    }

    out("cultures seed %llu: %d families, %d pairs, min d %.3f, %d below 0.45, %d with < 3 headline diffs, order mismatches %d | "
        "%d kingdoms: %d dialects < 0.10 from family, %d neighbour dialects < 0.10, %d neighbour arms alike | names: %d bad, "
        "%zu distinct places of 2000 | %.0f ms\n",
        (unsigned long long)seed, side * side, pairs, dmin, famFail, headFail, orderFail, kingdoms, dialFail, sibFail, armsFail,
        nameFail, distinctPlaces.size(), ms);
    if (orderFail) { out("FAIL: cultures depend on the order they are asked for (seed %llu)\n", (unsigned long long)seed); bad++; }
    if (!report && (famFail || headFail)) {
      out("FAIL: neighbouring culture families too alike (seed %llu: %d below d 0.45, %d with < 3 headline diffs)\n",
          (unsigned long long)seed, famFail, headFail);
      bad++;
    }
    if (!report && (dialFail || sibFail || armsFail)) {
      out("FAIL: dialects too alike (seed %llu: %d from their family, %d neighbours, %d neighbour arms)\n", (unsigned long long)seed,
          dialFail, sibFail, armsFail);
      bad++;
    }
    if (!report && nameFail) { out("FAIL: %d bad names (seed %llu)\n", nameFail, (unsigned long long)seed); bad++; }
  }
  if (!all.empty()) {
    std::sort(all.begin(), all.end());
    const float p5 = all[all.size() / 20];
    out("cultures: %zu neighbour pairs, 5th percentile d %.3f (target >= 0.5), median %.3f\n", all.size(), p5, all[all.size() / 2]);
  }
  if (!dial.empty()) {
    std::sort(dial.begin(), dial.end());
    out("dialects: %zu, distance from their family min %.3f, median %.3f\n", dial.size(), dial.front(), dial[dial.size() / 2]);
  }
  out("archetypes:");
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) out(" %s %d", cult::archetypeName((cult::Archetype)a), archCount[a]);
  out(" | isolated %d of %d\n", isolated, families);
  out("cost: %d families in %.0f ms (%.2f per ms, %.1f candidates each), %d dialects in %.1f ms (%.1f per ms); %zu names checked\n",
      famMade, famMs, famMs > 0 ? famMade / famMs : 0.0, famMade ? (double)candidates / famMade : 0.0, dialMade, dialMs,
      dialMs > 0 ? dialMade / dialMs : 0.0, namesChecked);
  if (report) {   // a sample of every archetype, to read
    for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)a, 77u + (uint32_t)a * 1000u);
      std::string pl, pe, al, go;
      for (uint32_t k = 0; k < 6; k++) pl += cult::placeName(c, k * 31u) + " ";
      for (uint32_t k = 0; k < 4; k++) pe += cult::personName(c, k * 17u, k & 1) + " ";
      for (const cult::Alloy& x : c.arms.alloys) al += x.name + " ";
      for (const std::string& g : c.faith.names) go += g + " ";
      out("  %-10s %s / %s | places %s| people %s| realm %s | %s| gods %s| %s, %s\n", cult::archetypeName(c.archetype),
          c.name.c_str(), c.adjective.c_str(), pl.c_str(), pe.c_str(), cult::kingdomName(c, 5).c_str(), al.c_str(), go.c_str(),
          cult::dungeonName(c, 9, 4).c_str(), cult::dungeonName(c, 11, 5).c_str());
    }
  }
  {   // unit costs: a whole culture (Atlas::make) and one name
    auto t0 = std::chrono::steady_clock::now();
    std::vector<cult::Culture> made;
    for (int i = 0; i < 120; i++) made.push_back(cult::Atlas::make((cult::Archetype)(i % 12), 1000u + (uint32_t)i));
    const double mk = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 120;
    t0 = std::chrono::steady_clock::now();
    size_t len = 0;
    for (int i = 0; i < 12000; i++) len += cult::placeName(made[(size_t)(i % 120)], (uint32_t)i).size();
    const double nm = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 12000;
    // the maximin alone (an atlas with no climate callback: every family born in temperate plains)
    cult::Atlas plain(A);
    t0 = std::chrono::steady_clock::now();
    for (int j = 0; j < 20; j++)
      for (int i = 0; i < 20; i++) plain.family(i, j);
    const double mx = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    out("unit cost: a culture %.1f us, a place name %.2f us (avg %.1f letters); 400 families without climate %.0f ms "
        "(%.1f candidates each)\n", mk, nm, (double)len / 12000, mx, (double)plain.stats().candidates / plain.stats().families);
  }
  printf(bad ? "cultures: FAILED (%d)\n" : "cultures: OK\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--cultures", "culture engine: neighbour distinctness, dialects, arms, names, order independence, cost [N] [--seeds A..B] [--report]", cmdCultures);
