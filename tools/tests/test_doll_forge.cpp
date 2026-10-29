// rpg_test --doll-forge and --forge-picks (P2c forge styles, FORGE-DOLL; docs/P2C_CONTRACTS.md 3c, docs/PLAN.md "The forge
// slice", gates C16 and C17).
//
//   rpg_test --forge-picks [--people NAME] [--seeds A..B]   every alloy's name, metal word, sheen, recipe and its seven picks
//                                                          (gear::alloyPick), with the distances the pick rule found
//   rpg_test --doll-forge [--worlds A..B] [--seeds A..B] [--report] [--selftest]
//       C16b  a culture's alloys pairwise d 7 or more of 13 with two silhouette axes apart (every people, seeds 1..64)
//       C16c  same-rank alloys of neighbouring families d 5 or more (the world atlases' 6 x 6 blocks, worlds A..B)
//       C16d  every alloy against every hero style d 4 or more, against its metal word's 5 or more; the walk held
//       C16h  the hero styles pairwise (STYLE_P2 28.4: 12 or 13 of 13 but for a few pairs)
//   (the drawn checks, C16a and C16s, and C17 run in --doll --gates: tools/tests/test_doll_forge.cpp dollForgeGates)
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear_ladder.h"
#include "tools/tests/tests.h"

int dollShapeGates(bool report, std::vector<std::string>& printed, const std::vector<std::pair<std::string, art::HumanLook>>& forgeLooks);
int forgeShapeData(int w0, int w1, int s0, int s1, bool report, bool selftest, std::vector<std::string>& printed);
int forgeSilhouettes(int w0, int w1, bool report, bool selftest, std::vector<std::string>& printed);
int dollHeadGates(bool report, bool selftest, std::vector<std::string>& printed);   // test_doll_envelope.cpp: C16k

namespace dollforge {

const char* kForgeLooks =   // (P2c forge) tools/doll/forge_looks.txt (tools/doll/embed_looks.py writes it)
    "hero     man    -  walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "fm_home  man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=cloth,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_tan   man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=leather,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_bronze man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=bronze,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_iron  man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=iron,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_steel man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=steel,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_gilt  man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=gilded,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_obsid man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=obsidian,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_ember man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=ember,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_star  man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=starmetal,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_myth  man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "f5_steel man    hero=1,kit=heartland:5:7,mat=steel,palette=look  stay,walk\n"
    "f5_gilt  man    hero=1,kit=heartland:5:7,mat=gilded,palette=look  stay,walk\n"
    "f5_obsid man    hero=1,kit=heartland:5:7,mat=obsidian,palette=look  stay,walk\n"
    "f5_ember man    hero=1,kit=heartland:5:7,mat=ember,palette=look  stay,walk\n"
    "f5_star  man    hero=1,kit=heartland:5:7,mat=starmetal,palette=look  stay,walk\n"
    "fw_bronze woman  heroine=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=bronze,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fw_steel woman  heroine=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=steel,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fw_star  woman  heroine=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=starmetal,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "a_hl1    man    kit=highland:4:1,alloy=1,head=4,hair=1,hairVar=1,hairColor=#8A3A22,beard=1,beardStyle=0,skin=#E2A882,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "a_hl2    man    kit=highland:4:1,alloy=2,head=4,hair=1,hairVar=1,hairColor=#8A3A22,beard=1,beardStyle=0,skin=#E2A882,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "a_hl3    man    kit=highland:4:1,alloy=3,head=4,hair=1,hairVar=1,hairColor=#8A3A22,beard=1,beardStyle=0,skin=#E2A882,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "a_fj2    man    kit=fjordfolk:4:1,alloy=2,head=3,hair=1,hairVar=2,hairColor=#C8A050,beard=1,beardStyle=3,skin=#F0C8A8,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "a_jade3  man    kit=jade:5:5,wpn=spear,head=6,hair=1,hairVar=1,hairColor=#1E1A22,skin=#E2B58A,palette=look  stay,walk,run,didle,hit1,hurt,"
    "fall,lying\n"
    "a_star3  woman  kit=starspire:5:1,hair=5,hairVar=0,hairColor=#E8DCC0,skin=#F0D2B8,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "mr_1     man    kit=sylvan:4:1,alloy=1,palette=look  stay,walk\n"
    "mr_2     man    kit=starspire:4:1,alloy=1,palette=look  stay,walk\n"
    "mr_3     man    kit=steppe:4:1,alloy=1,palette=look  stay,walk\n"
    "mr_4     man    kit=jade:4:2,alloy=1,palette=look  stay,walk\n"
    "mr_5     man    kit=river:4:1,alloy=3,palette=look  stay,walk\n"
    "mr_6     man    kit=marsh:4:2,alloy=3,palette=look  stay,walk\n"
    "lt1      man    hero=1,kit=heartland:1:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt2      man    hero=1,kit=heartland:2:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt3      man    hero=1,kit=heartland:3:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt4      man    hero=1,kit=heartland:4:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt5      man    hero=1,kit=heartland:5:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt6      man    hero=1,kit=heartland:6:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt6m     man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look  stay,walk,run,didle,hit1\n"
    "lt6w     woman  heroine=1,kit=heartland:6:7,wpn=spear,hairColor=#8C5A2A,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "nband    man    npc=heartland:bandit:0:7,palette=look  stay,walk,run,hit1,fall,lying\n"
    "nroyal   woman  npc=heartland:guard:2:7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "nking    man    npc=heartland:king:0:7,palette=look  stay,walk\n"
    "ncapt_hl man    npc=highland:captain:1:1,palette=look  stay,walk\n"
    "ncapt_jd man    npc=jade:captain:1:5,palette=look  stay,walk\n"
    "g_dune1  man    kit=dune:5:1,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_mars2  man    kit=marsh:4:2,alloy=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_hl2    man    kit=highland:4:1,alloy=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_fj1    man    kit=fjordfolk:5:1,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_step1  man    kit=steppe:5:1,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_sylv1w woman  kit=sylvan:4:1,alloy=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_hl1    man    kit=highland:4:1,alloy=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_hl3    man    kit=highland:5:1,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_jd3a   man    kit=jade:4:5,alloy=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_jd3b   man    kit=jade:4:5,alloy=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_jd2w   woman  kit=jade:5:2,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_star3  woman  kit=starspire:5:1,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_sun1   man    kit=suntemple:5:3,alloy=3,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov1w  woman  kit=heartland:4:1,alloy=1,palette=look,shieldForm=9,weapon=0,shape=layer:sash+arm:wide  stay,walk,run,didle,hit1,hurt,fall,"
    "lying\n"
    "g_cov2w  woman  kit=imperial:4:1,alloy=2,palette=look,shieldForm=9,weapon=0,shape=arm:heavy+layer:sash  stay,walk,run,didle,hit1,hurt,fall,"
    "lying\n"
    "g_cov3w  woman  kit=imperial:4:1,alloy=1,palette=look,shape=skirt:strips  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov4w  woman  kit=heartland:4:1,alloy=1,palette=look,shieldForm=9,weapon=0,shape=layer:tabard  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov5w  woman  kit=jade:4:1,alloy=1,palette=look,shieldForm=9,weapon=0,shape=skirt:bell  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov6   man    kit=highland:2:1,forge=foundry,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov7   man    kit=highland:2:1,forge=parade,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "g_cov8   man    kit=heartland:1:1,wpn=sword,backItem=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n";
std::string dollForgeLooksTextLocal() { return kForgeLooks; }

namespace F = art::doll::forge;

const char* const kPeople[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                 "sylvan", "starspire"};
const char* const kWord[6] = {"BRONZE", "BRASS", "IRON", "STEEL", "SILVER", "GLASS"};
const char* const kSheen[8] = {"matte", "bright", "dark", "banded", "iridescent", "glowing", "pale", "burnished"};

bool g_report = false;

struct Line {
  std::string id;
  int checks = 0, fails = 0;
  std::string first;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    fails++;
    if (first.empty()) first = why;
    if (g_report && (fails <= 12 || std::getenv("FORGE_ALL"))) std::printf("    %s: %s\n", id.c_str(), why.c_str());
  }
  void print() const {
    std::printf("%s %-26s %6d checks, %4d failed%s%s\n", fails ? "FAIL " : "ok   ", id.c_str(), checks, fails, fails ? ": " : "", first.c_str());
  }
};
std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}
bool range(const char* s, int& a, int& b) {
  if (std::sscanf(s, "%d..%d", &a, &b) == 2) return true;
  if (std::sscanf(s, "%d", &a) == 1) { b = a; return true; }
  return false;
}
std::string recipeText(const cult::Alloy& A) {
  static const char* const kOre[6] = {"copper", "tin", "iron", "coal", "silver", "rare"};
  std::string o;
  for (const cult::Ingredient& g : A.recipe) {
    if (!o.empty()) o += "+";
    o += g.ore >= 0 && g.ore < 6 ? kOre[g.ore] : g.reagent;
  }
  return o;
}

// ---------------------------------------------------------------- --forge-picks
int picksCmd(int argc, char** argv) {
  int s0 = 1, s1 = 4, only = -1;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) range(argv[++i], s0, s1);
    else if (!std::strcmp(argv[i], "--people") && i + 1 < argc) {
      cult::Archetype a;
      if (gear::peopleOf(argv[++i], a)) only = (int)a;
    }
  }
  for (int a = 0; a < 12; a++) {
    if (only >= 0 && a != only) continue;
    for (int sd = s0; sd <= s1; sd++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)a, (uint32_t)sd);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
        const cult::Alloy& A = c.arms.alloys[(size_t)i];
        const gear::ForgePick p = gear::alloyPick(c, i);
        static const char* const kHead[15] = {"-", "nasal", "kettle", "great", "spangen", "?", "?", "?", "mask", "crested", "?", "coif",
                                              "turban", "jingasa", "aventail"};
        const art::doll::forge::Guise gu = art::doll::forge::unpackGuise(gear::alloyGuise(c, i));
        std::printf("%-9s s%-3d %d %-12s %-6s %-10s school %d fam %d | %-58s | d culture %2d hero %2d word %2d steps %2d%s%s | head %s skirt %d cloak %d pd %d | %s\n", kPeople[a], sd,
                    i + 1, A.name.c_str(), kWord[gear::alloyMetalWord(A, std::min(i, 2))], kSheen[(int)A.sheen & 7], gear::forgeSchool(c),
                    gear::alloyFamilyOf(c), gear::describeForge(p.style).c_str(), p.minCulture == 99 ? 0 : p.minCulture, p.minHero, p.wordHero,
                    p.steps, p.ok ? "" : " FAIL ", p.why.c_str(), kHead[gu.helm < 15 ? gu.helm : 0], gu.skirt, gu.cloak, gu.pauldron, recipeText(A).c_str());
      }
    }
  }
  return 0;
}

// ---------------------------------------------------------------- the data gates
void heroPairs(Line& L) {
  for (int a = F::B_HOMESPUN; a < F::B_COUNT; a++)
    for (int b = a + 1; b < F::B_COUNT; b++) {
      uint8_t x[F::AX_COUNT], y[F::AX_COUNT];
      // each on its own family (HOMESPUN the padded coat, TANNER'S the leather, the metals plate)
      auto famOf = [](int base) { return base == F::B_HOMESPUN ? F::F_PADDED : base == F::B_TANNER ? F::F_LEATHER : F::F_PLATE; };
      F::heroAxes(a, famOf(a), x);
      F::heroAxes(b, famOf(b), y);
      const int d = F::distance(x, y);
      // STYLE_P2 28.4: every two differ on every axis but, for a few pairs, the fittings (12 or 13); the soft styles' crest
      // none and their own cut columns count as values
      if (d < 11) L.bad(fmt("%s vs %s: d %d of 13", F::valueName(F::AX_BASE, a), F::valueName(F::AX_BASE, b), d));
      else L.ok();
    }
}
void cultureGates(int s0, int s1, Line& b, Line& d) {
  for (int a = 0; a < 12; a++)
    for (int sd = s0; sd <= s1; sd++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)a, (uint32_t)sd);
      std::vector<gear::ForgeStyle> st;
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
        const gear::ForgePick p = gear::alloyPick(c, i);
        st.push_back(p.style);
        const std::string who = fmt("%s s%d %s", kPeople[a], sd, c.arms.alloys[(size_t)i].name.c_str());
        if (!p.ok || p.minHero < 4 || p.wordHero < 5) d.bad(fmt("%s: hero d %d, word d %d, steps %d %s", who.c_str(), p.minHero, p.wordHero, p.steps, p.why.c_str()));
        else d.ok();
      }
      for (size_t i = 0; i < st.size(); i++)
        for (size_t j = i + 1; j < st.size(); j++) {
          const int dd = gear::styleDistance(st[i], st[j]), sil = gear::silhouetteDiffs(st[i], st[j]);
          if (dd < 7 || sil < 2)
            b.bad(fmt("%s s%d %s vs %s: d %d, silhouette axes %d", kPeople[a], sd, c.arms.alloys[i].name.c_str(), c.arms.alloys[j].name.c_str(), dd, sil));
          else b.ok();
        }
    }
}
void neighbourGates(int w0, int w1, Line& c, int* histo) {
  for (int w = w0; w <= w1; w++) {
    cult::Atlas at((uint64_t)w);
    for (int cj = 0; cj < 6; cj++)
      for (int ci = 0; ci < 6; ci++) {
        const cult::Culture& A = at.family(ci, cj);
        for (int k = 0; k < 2; k++) {
          const cult::Culture& B = k == 0 ? at.family(ci + 1, cj) : at.family(ci, cj + 1);
          const size_t n = std::min(A.arms.alloys.size(), B.arms.alloys.size());
          for (size_t i = 0; i < n; i++) {
            const int ra = std::min((int)i, 2);
            const int dd = gear::styleDistance(gear::alloyStyle(A, (int)i), gear::alloyStyle(B, (int)i));
            if (histo) histo[std::clamp(dd, 0, 13)]++;
            if (dd < 5)
              c.bad(fmt("world %d cell %d,%d %s %s vs %s %s (rank %d): d %d [%s | %s] schools %d %d", w, ci, cj, A.name.c_str(), A.arms.alloys[i].name.c_str(), B.name.c_str(),
                        B.arms.alloys[i].name.c_str(), ra, dd, gear::describeForge(gear::alloyStyle(A, (int)i)).c_str(),
                        gear::describeForge(gear::alloyStyle(B, (int)i)).c_str(), gear::forgeSchool(A), gear::forgeSchool(B)));
            else c.ok();
          }
        }
      }
  }
}

int forgeCmd(int argc, char** argv) {
  int w0 = 1, w1 = 6, s0 = 1, s1 = 64;
  bool selftest = false, silhouettes = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--worlds") && i + 1 < argc) range(argv[++i], w0, w1);
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) range(argv[++i], s0, s1);
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--selftest")) selftest = true;
    else if (!std::strcmp(argv[i], "--silhouettes")) silhouettes = true;
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {
      std::string err;
      if (!art::doll::loadPoolFile(argv[++i], &err)) { std::printf("FAIL: --pool %s\n", err.c_str()); return 1; }
    }
  }
  // (forge round 2) --silhouettes: C18 alone (the drawn outlines over the board set and the worlds' windows)
  if (silhouettes) {
    const int was = art::rigVersion();
    art::setRigVersion(2);
    std::vector<std::string> printed;
    const int f = forgeSilhouettes(w0, w1, g_report, selftest, printed);
    for (const std::string& l : printed) std::printf("%s\n", l.c_str());
    std::printf("doll-forge --silhouettes: %d failure(s)\n", f);
    art::setRigVersion(was);
    return f ? 1 : 0;
  }
  // (forge round 2) C16b, C16c, round 1's C16d and C16g are retired (the texture distances and the guise heads): the
  // silhouette's data lines C16d and C16u stand in their place (test_doll_shapes.cpp forgeShapeData)
  Line h{"C16h hero styles"};
  heroPairs(h);
  // C17 at the ramp level: every family under every sheen, the band metals, against A3's skin (and the darkest the game
  // draws) ; the six named alloys as their own rows
  Line m{"C17 metal read (ramps)"}, mn{"C17 the six named"};
  {
    const uint32_t skins[1] = {rgba(0xE9, 0xA0, 0x77)};   // (A3's skin: the alloy model's own reference)
    for (int f = 0; f < art::doll::alloyFamilyCount(); f++)
      for (int sh = 1; sh <= 8; sh++) {
        uint32_t r[5];
        art::doll::alloyRamp(f, (uint8_t)sh, r);
        std::string why;
        bool ok = true;
        for (uint32_t sk : skins) ok = ok && art::doll::metalRead(r, sk, &why);
        if (ok) ok = art::doll::metalRead2(r, &why);   // (forge round 2) warm to gold, greens cold, out of the skin box
        if (!ok) m.bad(fmt("%s %s: %s", art::doll::alloyFamilyName(f), kSheen[sh - 1], why.c_str()));
        else m.ok();
      }
    static const char* const kBand[12] = {"", "", "bronze", "iron", "steel", "gilded", "", "obsidian", "ember", "starmetal", "", "mythic"};
    for (int k : {2, 3, 4, 5, 7, 8, 9, 11}) {
      uint32_t r[5];
      art::doll::metalKindRamp(k, r);
      for (uint32_t& x : r) x |= 0xFF000000u;
      std::string why;
      bool ok = true;
      for (uint32_t sk : skins) ok = ok && art::doll::metalRead(r, sk, &why);
      if (!ok) m.bad(fmt("band %s: %s", kBand[k], why.c_str()));
      else m.ok();
    }
    // the six (alloys panel 2026-10-23): their real culture, family and sheen
    struct N { const char* people; uint32_t seed; int idx; };
    const N six[6] = {{"sylvan", 1, 0}, {"starspire", 1, 0}, {"steppe", 1, 0}, {"jade", 2, 0}, {"river", 1, 2}, {"marsh", 2, 2}};
    for (const N& n : six) {
      cult::Archetype a;
      gear::peopleOf(n.people, a);
      const cult::Culture c = cult::Atlas::make(a, n.seed);
      const std::vector<uint8_t> fam = gear::alloyFamilies(c);
      const gear::AlloyKeys k = gear::alloyKeys(c, n.idx);
      uint32_t r[5];
      art::doll::alloyRamp(fam[(size_t)n.idx], k.sheen, r);
      std::string why;
      if (!art::doll::metalRead(r, skins[0], &why)) mn.bad(fmt("%s: %s", c.arms.alloys[(size_t)n.idx].name.c_str(), why.c_str()));
      else mn.ok();
    }
  }
  // (forge round 2) the owner's round-2 list (STYLE_P2 28.8: read as cloth, clay, leather or skin) as report rows: each on its
  // real culture, family and sheen, the round-1 and round-2 terms
  Line m2{"C17 the round-2 list"};
  {
    const char* const kList = " THORNIRON FROSTSTEEL UHABRASS JAONBRASS ENKHOBRASS PAPIRON ANIBRASS CRANESILVER UNSHISILVER VENSTEEL BRIARSTEEL ";
    for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
      if (!bc.owner) continue;
      const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
      const std::vector<uint8_t> fam = gear::alloyFamilies(c);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
        const std::string nm = c.arms.alloys[(size_t)i].name;
        if (!std::strstr(kList, (" " + nm + " ").c_str())) continue;
        const gear::AlloyKeys k = gear::alloyKeys(c, i);
        uint32_t r[5];
        art::doll::alloyRamp(fam[(size_t)i], k.sheen, r);
        std::string why;
        const bool ok = art::doll::metalRead(r, rgba(0xE9, 0xA0, 0x77), &why) && art::doll::metalRead2(r, &why);
        if (g_report) std::printf("    C17 round-2 row %-12s %-16s %s\n", nm.c_str(), art::doll::alloyFamilyName(fam[(size_t)i]), ok ? "metal" : why.c_str());
        if (!ok) m2.bad(nm + ": " + why); else m2.ok();
      }
    }
  }
  // C15f (the forge re-plan: the alloy panel's clusters of pale silvers, mints, slates and teals): no dE-10 ball (round any
  // family's mid step under its people's own sheen... the plain ramp) holds more than 3 of the 96 families; each slot's 12
  // families span 3 or more hue sextants (neutrals, chroma under 8, count as one sextant of their own)
  Line sp{"C15f the spread"};
  {
    const int nf = art::doll::alloyFamilyCount();
    std::vector<uint32_t> mid((size_t)nf);
    for (int f = 0; f < nf; f++) { uint32_t r[5]; art::doll::alloyRamp(f, 0, r); mid[(size_t)f] = r[2]; }
    int worst = 0, worstF = 0;
    for (int f = 0; f < nf; f++) {
      int n = 0;
      for (int g = 0; g < nf; g++) if (art::doll::deltaE76(mid[(size_t)f], mid[(size_t)g]) <= 10.0f) n++;
      if (n > worst) { worst = n; worstF = f; }
      if (n > 3) sp.bad(fmt("%s: %d families within dE 10", art::doll::alloyFamilyName(f), n)); else sp.ok();
    }
    for (int slot = 0; slot < 8; slot++) {
      std::set<int> sext;
      for (int pe = 0; pe < 12; pe++) {
        const uint32_t c = mid[(size_t)art::doll::alloyFamilyAt(slot, pe)];
        const double R = (c & 255) / 255.0, G = (c >> 8 & 255) / 255.0, B = (c >> 16 & 255) / 255.0;
        const double mx = std::max({R, G, B}), mn2 = std::min({R, G, B});
        if (mx - mn2 < 0.08) { sext.insert(6); continue; }
        double hh = mx == R ? std::fmod((G - B) / (mx - mn2), 6.0) : mx == G ? (B - R) / (mx - mn2) + 2 : (R - G) / (mx - mn2) + 4;
        if (hh < 0) hh += 6;
        sext.insert((int)hh % 6);
      }
      if (sext.size() < 3) sp.bad(fmt("slot %d: %zu hue sextants", slot, sext.size())); else sp.ok();
    }
    if (g_report) std::printf("  C15f densest ball: %s with %d\n", art::doll::alloyFamilyName(worstF), worst);
    if (std::getenv("FORGE_RAMP")) {   // tools/doll/alloys/sim.py's check: a family's ramp under a sheen (FORGE_RAMP=fam,sheen)
      int f = 0, sh = 0;
      std::sscanf(std::getenv("FORGE_RAMP"), "%d,%d", &f, &sh);
      uint32_t r[5];
      art::doll::alloyRamp(f, (uint8_t)sh, r);
      for (uint32_t c : r) std::printf(" (%u, %u, %u)", c & 255u, c >> 8 & 255u, c >> 16 & 255u);
      std::printf("\n");
    }
  }
  int fails = 0;
  for (Line* l : {&h, &m, &mn, &m2, &sp}) { l->print(); fails += l->fails; }
  {
    std::vector<std::string> printed;
    fails += forgeShapeData(w0, w1, std::min(s0, 64), std::min(s1, 64), g_report, selftest, printed);
    // (P2c envelope, contracts 3d) C16k: a head each (the owner's 37 apart, every culture's alloys apart, the stability test)
    fails += dollHeadGates(g_report, true, printed);
    for (const std::string& l : printed) std::printf("%s\n", l.c_str());
  }
  if (selftest) {
    // a style against itself must fail C16b; a hero style copied as an alloy must fail C16d
    int caught = 0;
    gear::ForgeStyle s = gear::baseStyle(4, cult::BodyArm::Plate);
    if (gear::styleDistance(s, s) < 7) caught++;
    gear::ForgeStyle t = s; t.base = 0;
    if (gear::styleDistance(t, gear::baseStyle(4, cult::BodyArm::Plate)) < 4) caught++;
    // a clay ramp (STYLE_P2 28.8: the lights keep their colour, no dark core) must FAIL C17
    const uint32_t clay[5] = {rgba(0x3A, 0x24, 0x18), rgba(0x7A, 0x4A, 0x30), rgba(0xA8, 0x6A, 0x48), rgba(0xC8, 0x8A, 0x64),
                              rgba(0xE0, 0xA8, 0x80)};
    if (!art::doll::metalRead(clay, rgba(0xE9, 0xA0, 0x77))) caught++;
    std::printf("  C16 C17 data selftest: %s\n", caught == 3 ? "ALL CAUGHT" : "MISSED");
    if (caught != 3) fails++;
  }
  std::printf("doll-forge: %d failure(s)\n", fails);
  return fails ? 1 : 0;
}

// ---------------------------------------------------------------- the drawn gates (C16 a, b, c, s; C17; C12a restated)
// docs/PLAN.md's gates table: with the colour removed (ComposeOpts::grey's ramp: every material slot one neutral ramp), the
// same figure but its style, idle D and R and the walk's passing frame R
namespace doll = art::doll;
using art::Clip;
using art::HumanLook;
using art::RigPart;

uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
HumanLook baseLook(int who) {   // the gates' A3 bases (test_doll_ladder.cpp a3Hero / a3Woman)
  HumanLook L;
  if (who == 0) {
    L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
    L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
    L.eyeColor = hexc(0x3A78B0);
  } else {
    L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.outfit = art::Outfit::Dress;
    L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
    L.eyeColor = hexc(0x3F8A5A);
  }
  return L;
}
struct GLook { std::string tag; int who = 0; HumanLook L; doll::DollRecipe r; bool ok = false; };
GLook glook(const std::string& tag, int who, const std::string& spec) {
  gear::installLadderMacros();
  GLook g;
  g.tag = tag; g.who = who;
  g.L = baseLook(who);
  bool pal = false;
  std::string bad;
  doll::applyLookSpec(g.L, spec, &pal, nullptr, &bad);
  g.r = doll::resolve(g.L);
  g.ok = bad.empty();
  return g;
}
std::vector<GLook> forgeLooks() {
  std::vector<GLook> v;
  const std::string t = dollForgeLooksTextLocal();
  size_t a = 0;
  while (a < t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    const std::string ln = t.substr(a, b - a);
    a = b + 1;
    char tag[128] = {}, who[16] = {}, spec[1400] = {};
    if (ln.empty() || ln[0] == '#' || std::sscanf(ln.c_str(), "%127s %15s %1399s", tag, who, spec) < 2) continue;
    v.push_back(glook(tag, !std::strcmp(who, "woman"), std::strcmp(spec, "-") ? spec : ""));
  }
  return v;
}
const GLook* findLook(const std::vector<GLook>& v, const char* tag) {
  for (const GLook& g : v) if (g.tag == tag) return &g;
  return nullptr;
}

using Pt = std::pair<int, int>;
struct Grey {
  bool ok = false;
  std::map<Pt, int> v;      // the grey value per pixel (pivot-relative): a material slot's step (100 + step), else its code
  std::set<Pt> gear;        // pixels of gear (armour, helm, pauldron, shield, overlays)
  std::map<Pt, uint8_t> code;
  std::map<Pt, int> owner;
  doll::FrameProbe p;
};
bool gearRole(const doll::FrameProbe::It& it) {
  using doll::Role;
  return it.has(Role::Armour) || it.has(Role::Helm) || it.has(Role::Pauldron) || it.has(Role::Shield) || it.has(Role::Overlay);
}
Grey greyOf(const doll::DollRecipe& r, RigPart part, int facing, Clip clip, int i) {
  Grey g;
  g.p = doll::probeFrame(r, part, facing, clip, 0, i, true);
  if (!g.p.ok) return g;
  g.ok = true;
  for (int y = 0; y < g.p.h; y++)
    for (int x = 0; x < g.p.w; x++) {
      const size_t k = (size_t)y * g.p.w + x;
      const uint8_t c = g.p.code[k];
      if (c < 2) continue;
      const int o = g.p.owner[k];
      if (o >= 0 && g.p.items[(size_t)o].has(doll::Role::Held)) continue;   // no weapon
      const Pt q{x - g.p.px, y - g.p.py};
      const doll::Slot s = doll::codeSlot(c);
      g.v[q] = doll::greySlot(s) ? 100 + doll::codeStep(c) : c;
      g.code[q] = c;
      g.owner[q] = o;
      if (o >= 0 && gearRole(g.p.items[(size_t)o])) g.gear.insert(q);
    }
  return g;
}
double xorShare(const Grey& a, const Grey& b) {
  int uni = 0, x = 0;
  for (const auto& kv : a.v) { uni++; if (!b.v.count(kv.first)) x++; }
  for (const auto& kv : b.v) if (!a.v.count(kv.first)) { uni++; x++; }
  return uni ? (double)x / uni : 0;
}
std::set<Pt> edgesOf(const Grey& g, int x0, int y0, int x1, int y1) {
  std::set<Pt> e;
  for (const Pt& q : g.gear) {
    if (q.first < x0 || q.first > x1 || q.second < y0 || q.second > y1) continue;
    const int v = g.v.at(q);
    for (const Pt& n : {Pt{q.first + 1, q.second}, Pt{q.first, q.second + 1}}) {
      auto it = g.v.find(n);
      if (it != g.v.end() && it->second != v) { e.insert(q); break; }
    }
  }
  return e;
}
// 1 minus the Jaccard of the step-edge pixels over the two figures' gear box
double edgeDistance(const Grey& a, const Grey& b) {
  int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20);
  for (const Grey* g : {&a, &b})
    for (const Pt& q : g->gear) { x0 = std::min(x0, q.first); y0 = std::min(y0, q.second); x1 = std::max(x1, q.first); y1 = std::max(y1, q.second); }
  const std::set<Pt> ea = edgesOf(a, x0, y0, x1, y1), eb = edgesOf(b, x0, y0, x1, y1);
  int inter = 0;
  for (const Pt& q : ea) if (eb.count(q)) inter++;
  const int uni = (int)ea.size() + (int)eb.size() - inter;
  return uni ? 1.0 - (double)inter / uni : 0;
}
struct Pair { double xorD = 0, xorR = 0, edge = 0; };
Pair comparePair(const doll::DollRecipe& A, const doll::DollRecipe& B) {
  doll::DollRecipe a = A, b = B;
  a.ramps = doll::greyRamps(A.ramps);
  b.ramps = doll::greyRamps(B.ramps);
  Pair p;
  const Grey aD = greyOf(a, RigPart::Loco, 0, Clip::Idle, 0), bD = greyOf(b, RigPart::Loco, 0, Clip::Idle, 0);
  const Grey aR = greyOf(a, RigPart::Loco, 2, Clip::Idle, 0), bR = greyOf(b, RigPart::Loco, 2, Clip::Idle, 0);
  const Grey aW = greyOf(a, RigPart::Loco, 2, Clip::Walk, 1), bW = greyOf(b, RigPart::Loco, 2, Clip::Walk, 1);
  p.xorD = xorShare(aD, bD);
  p.xorR = xorShare(aR, bR);
  p.edge = std::max({edgeDistance(aD, bD), edgeDistance(aR, bR), edgeDistance(aW, bW)});
  return p;
}
// the hero styles filled black: the gear's silhouette on one body (no weapon), pairwise XOR (C16s)
double gearXor(const doll::DollRecipe& A, const doll::DollRecipe& B, int facing) {
  const Grey a = greyOf(A, RigPart::Loco, facing, Clip::Idle, 0), b = greyOf(B, RigPart::Loco, facing, Clip::Idle, 0);
  int uni = 0, x = 0;
  for (const Pt& q : a.gear) { uni++; if (!b.gear.count(q)) x++; }
  for (const Pt& q : b.gear) if (!a.gear.count(q)) { uni++; x++; }
  return uni ? (double)x / uni : 0;
}

double lumaOf(uint32_t c) { return (c & 255) * 0.299 + (c >> 8 & 255) * 0.587 + (c >> 16 & 255) * 0.114; }
// C17 on one look's idle frame (facing): the metal ramp's read, the step-4 count, the skin / leather / cloth distance, the
// sheen's tell. Returns "" or the first failure
std::string c17Look(const GLook& g, int facing, bool* sheenChecked = nullptr) {
  const doll::DollRecipe& r = g.r;
  const doll::FrameProbe p = doll::probeFrame(r, RigPart::Loco, facing, Clip::Idle, 0, 0, true);
  if (!p.ok) return "no idle frame";
  int metal = 0, step4 = 0, glow = 0, m2 = 0;
  std::set<int> m2rows, mrows;
  int run4 = 0, maxRun4 = 0;
  for (int y = 0; y < p.h; y++) {
    run4 = 0;
    for (int x = 0; x < p.w; x++) {
      const size_t k = (size_t)y * p.w + x;
      const uint8_t c = p.code[k];
      const int o = p.owner[k];
      const bool held = o >= 0 && p.items[(size_t)o].has(doll::Role::Held);
      if (c < 2 || held) { run4 = 0; continue; }
      const doll::Slot s = doll::codeSlot(c);
      if (s == doll::Slot::Metal) { metal++; mrows.insert(y); if (doll::codeStep(c) == 4) { maxRun4 = std::max(maxRun4, ++run4); } else run4 = 0; }
      else run4 = 0;
      if (s == doll::Slot::Glow && doll::codeStep(c) >= 2) glow++;
      if (s == doll::Slot::Metal2) { m2++; m2rows.insert(y); }
    }
  }
  if (metal < 12) return "";   // (a soft kit: no metal armour to read)
  // the glints: 8-connected groups of step-4 metal pixels on the body's armour and shoulders (the helm, shield and blade
  // carry their own; a glint is one cluster, a 2 px run on a bright sheen still one)
  {
    std::set<Pt> g4;
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        const size_t k = (size_t)y * p.w + x;
        const int o = p.owner[k];
        if (p.code[k] < 2 || o < 0 || doll::codeSlot(p.code[k]) != doll::Slot::Metal || doll::codeStep(p.code[k]) != 4) continue;
        if (!p.items[(size_t)o].has(doll::Role::Armour) && !p.items[(size_t)o].has(doll::Role::Pauldron) &&
            !(r.guiseHead && p.items[(size_t)o].has(doll::Role::Headwear))) continue;
        g4.insert({x, y});
      }
    std::set<Pt> seen;
    for (const Pt& q : g4) {
      if (seen.count(q)) continue;
      step4++;
      std::vector<Pt> st{q};
      seen.insert(q);
      while (!st.empty()) {
        const Pt c0 = st.back(); st.pop_back();
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const Pt n{c0.first + dx, c0.second + dy};
            if (g4.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
          }
      }
    }
  }
  const uint32_t* M = r.ramps.c[(int)doll::Slot::Metal];
  const double l1 = lumaOf(M[1]), l2 = lumaOf(M[2]), l3 = lumaOf(M[3]);
  const char* a = nullptr;
  const int top = std::max<int>(r.pieceTier[0], std::max<int>(r.pieceTier[1], r.pieceTier[2]));
  {
    // (T1 compresses the glint into the lit step: STYLE_P2 27.3's matte T1; the glint is read from T2)
    uint32_t Mc[5] = {M[0], M[1], M[2], M[3], top <= 1 ? 0xFFFFFFFFu : M[4]};
    std::string why;
    if (!doll::metalRead(Mc, hexc(0xE9A077), &why)) return "metal read: " + why;
  }
  const int fam = r.armourFam;
  const double need31 = fam == art::doll::forge::F_PLATE ? 60.0 : 45.0;
  if (l3 - l1 < need31) return fmt("step 3 over step 1 by %.0f luma (%.0f)", l3 - l1, need31);
  int bodyMetal = 0;
  for (size_t k = 0; k < p.code.size(); k++) {
    const int o = p.owner[k];
    // (INTEGRATE silhouettes i3) the lit metal only (steps 1-4): a plate whose only visible pixels are its step-0 edge beside a
    // tabard (UHABRASS: tabard, no shoulders) shows no metal plane to glint; its helm and greaves carry the glints
    if (p.code[k] >= 2 && o >= 0 && doll::codeSlot(p.code[k]) == doll::Slot::Metal && doll::codeStep(p.code[k]) >= 1 &&
        (p.items[(size_t)o].has(doll::Role::Armour) || p.items[(size_t)o].has(doll::Role::Pauldron) ||
         (r.guiseHead && p.items[(size_t)o].has(doll::Role::Headwear)))) bodyMetal++;
  }
  // (forge i2) 1-3 to T4; T5-T6 at least 1 (the living finish's glints climb with the tier: C12b counts them rising)
  if (top >= 2 && bodyMetal >= 12 && (step4 < 1 || (step4 > 3 && top <= 4))) return fmt("%d glints of step-4 metal on the armour (1-3 from T2, 1 or more at T5-T6)", step4);
  // steps 2-3 off the look's skin (20), leather and cloth (15)
  for (int st = 2; st <= 3; st++) {
    if (doll::deltaE76(M[st], r.ramps.c[(int)doll::Slot::Skin][2]) < 20.0f) return fmt("metal step %d within 20 dE of the skin", st);
    for (doll::Slot s : {doll::Slot::Leather, doll::Slot::Cloth1, doll::Slot::Cloth2})
      if (doll::deltaE76(M[st], r.ramps.c[(int)s][st]) < 15.0f) return fmt("metal step %d within 15 dE of %s", st, doll::slotName(s));
  }
  // (forge round 2) the round-2 terms: warm to gold and out of the skin box, greens cold (the composed ramp)
  {
    std::string why;
    if (!doll::metalRead2(M, &why)) return "round 2: " + why;
  }
  // KNAPPED's dark glass: the body's luma median 50 or less, the lit plane's hue 200-290 degrees (HSV)
  if (g.L.metalKind == 7) {
    std::vector<double> lum;
    for (size_t k = 0; k < p.code.size(); k++) {
      const int o = p.owner[k];
      if (p.code[k] >= 2 && o >= 0 && doll::codeSlot(p.code[k]) == doll::Slot::Metal && p.items[(size_t)o].has(doll::Role::Armour))
        lum.push_back(lumaOf(r.ramps.c[(int)doll::Slot::Metal][doll::codeStep(p.code[k])]));
    }
    std::sort(lum.begin(), lum.end());
    const double med = lum.empty() ? 0 : lum[lum.size() / 2];
    const uint32_t c3 = M[3];
    const double R = (c3 & 255) / 255.0, G = (c3 >> 8 & 255) / 255.0, B = (c3 >> 16 & 255) / 255.0, mx = std::max({R, G, B}), mn = std::min({R, G, B});
    double h = mx - mn < 1e-9 ? 0 : mx == R ? std::fmod((G - B) / (mx - mn), 6.0) : mx == G ? (B - R) / (mx - mn) + 2 : (R - G) / (mx - mn) + 4;
    h = h * 60.0 < 0 ? h * 60.0 + 360.0 : h * 60.0;
    if (med > 50.0 || h < 200.0 || h > 290.0) return fmt("dark glass: body median %.0f luma (50), lit hue %.0f (200-290)", med, h);
  }
  // FURNACE and a glowing alloy: 8 glow pixels or more on the seams at T4 (D28, D40)
  if ((g.r.has("glowseams")) && top == 4 && glow < 8) return fmt("glow seams: %d glow pixels at T4 (8)", glow);
  // iridescent: two hue groups 40 degrees or more apart, each 20 % or more of the metal pixels (Metal and Metal 2 bands)
  if (g.L.sheen == 5 && g.r.irisBands && metal + m2 > 0) {
    // (on the body's armour: the torso, its skirt, its motif; the helm, shield and limbs carry their own single hue)
    int bm = 0, b2 = 0;
    for (size_t k = 0; k < p.code.size(); k++) {
      const int o = p.owner[k];
      if (p.code[k] < 2 || o < 0 || !(p.items[(size_t)o].has(doll::Role::Armour) || p.items[(size_t)o].has(doll::Role::Overlay))) continue;
      const doll::Slot sl = doll::codeSlot(p.code[k]);
      if (sl == doll::Slot::Metal) bm++;
      else if (sl == doll::Slot::Metal2) b2++;
    }
    const double share = bm + b2 ? (double)b2 / (double)(bm + b2) : 0.0;
    if (share < 0.2 || share > 0.8 || m2rows.size() < 2) return fmt("iridescent: the second hue on %.0f %% of the metal (20-80), %zu rows", share * 100, m2rows.size());
    // two hue groups 40 degrees or more apart (the body's and Metal 2's mid steps, HSV hue)
    auto hue = [](uint32_t c) {
      const double R = (c & 255) / 255.0, G = (c >> 8 & 255) / 255.0, B = (c >> 16 & 255) / 255.0, mx = std::max({R, G, B}), mn = std::min({R, G, B});
      if (mx - mn < 1e-6) return 0.0;
      double h = mx == R ? std::fmod((G - B) / (mx - mn), 6.0) : mx == G ? (B - R) / (mx - mn) + 2 : (R - G) / (mx - mn) + 4;
      h *= 60.0;
      return h < 0 ? h + 360.0 : h;
    };
    const double dh = std::fabs(std::fmod(hue(M[2]) - hue(r.ramps.c[(int)doll::Slot::Metal2][2]) + 540.0, 360.0) - 180.0);
    if (dh < 40.0) return fmt("iridescent: the two hues %.0f degrees apart (40)", dh);
  }
  // the sheen's tell (HumanLook::sheen = cult::Sheen + 1)
  if (sheenChecked) *sheenChecked = true;
  switch (g.L.sheen) {
    case 1: if (maxRun4 > 1) a = "matte: a step-4 run over 1 px"; break;
    case 2: if (maxRun4 < 2) a = "bright: no step-4 run of 2 px"; break;
    case 3: {
      // (P2c envelope, C17 restated: D52) on bronze and brass the dark sheen reads ANTIQUE: black recesses (step 1 at 45 or
      // less), the body at luma 95-125, a hard lit plane (step 3 at 175 or more), a glint; the cold metals keep the blackened body
      const int fam = g.L.alloyFam ? g.L.alloyFam - 1 : doll::alloyFamilyOf(g.L.armourTint, g.L.sheen);
      const int mw = doll::alloyFamilyMetal(fam);
      if (g.L.armourTint && (mw == 0 || mw == 1)) {
        static std::string why;
        if (l2 < 95.0 || l2 > 125.0 || l1 > 45.0 || l3 < 175.0 || lumaOf(M[4]) < 196.0) {
          why = fmt("dark (antique, D52): step 1 %.0f (45), step 2 %.0f (95-125), step 3 %.0f (175), glint %.0f (196)", l1, l2, l3, lumaOf(M[4]));
          a = why.c_str();
        }
      } else if (l2 > 90.0 || lumaOf(M[4]) < 196.0) a = "dark: step 2 over 90 luma or no near-white glint";
      break;
    }
    case 6: if (top >= 4 && glow < 6) a = "glowing: under 6 lit glow pixels from T4"; break;
    case 7: if (l2 < 150.0) a = "pale: step 2 under 150 luma"; break;
    case 5: if (m2rows.size() < 2) a = "iridescent: under 2 rows of the second hue"; break;
    default: break;
  }
  (void)a;
  return a ? std::string(a) : std::string();
}

int dollForgeGatesImpl(bool report, std::vector<std::string>& printed) {
  g_report = report;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const std::vector<GLook> looks = forgeLooks();
  Line a{"C16a hero styles"}, s{"C16s silhouettes"}, m17{"C17 metal and sheen"},
       c1f{"C1 source F"}, c12{"C12a T2 to T3"}, blade{"C17 blades"}, st{"C16 C17 selftests"};
  static const char* const kHero[10] = {"fm_home", "fm_tan", "fm_bronze", "fm_iron", "fm_steel", "fm_gilt", "fm_obsid", "fm_ember", "fm_star", "fm_myth"};
  // C1 source F: every forge look resolves with nothing missing
  for (const GLook& g : looks) {
    const std::vector<doll::Missing> mm = doll::dollMissing(g.L);
    if (!mm.empty()) c1f.bad(g.tag + ": " + mm[0].what());
    else if (!g.ok) c1f.bad(g.tag + ": an unknown field");
    else c1f.ok();
  }
  // C16a and C16s over the ten hero styles (the fm_* line: one kit, re-materialled)
  for (int i = 0; i < 10; i++)
    for (int j = i + 1; j < 10; j++) {
      const GLook *A = findLook(looks, kHero[i]), *B = findLook(looks, kHero[j]);
      if (!A || !B) { a.bad(fmt("%s or %s missing", kHero[i], kHero[j])); continue; }
      const Pair p = comparePair(A->r, B->r);
      if (report) std::printf("    C16a %s vs %s: XOR D %.1f %% R %.1f %%, edge %.2f\n", kHero[i], kHero[j], p.xorD * 100, p.xorR * 100, p.edge);
      if (std::max(p.xorD, p.xorR) < 0.04 || p.edge < 0.30)
        a.bad(fmt("%s vs %s: XOR %.1f %% (4), edge %.2f (0.30)", kHero[i], kHero[j], std::max(p.xorD, p.xorR) * 100, p.edge));
      else a.ok();
      const double sx = std::max(gearXor(A->r, B->r, 0), gearXor(A->r, B->r, 2));
      if (sx < 0.04) s.bad(fmt("%s vs %s: gear silhouette XOR %.1f %% (4)", kHero[i], kHero[j], sx * 100)); else s.ok();
    }
  // (forge round 2) C16b and C16c (round 1's texture distances) are retired: C16o, C16m, C16n and the visor term
  // (test_doll_shapes.cpp dollShapeGates) measure the outline at 1x with the colour removed
  // C17: every forge look (D, R), and every family x sheen on its people's T4 kit
  for (const GLook& g : looks)
    for (int fc : {0, 2}) {
      const std::string why = c17Look(g, fc);
      if (!why.empty()) m17.bad(fmt("%s %c: %s", g.tag.c_str(), "DURL"[fc], why.c_str())); else m17.ok();
    }
  for (int f = 0; f < doll::alloyFamilyCount(); f++)
    for (int sh = 1; sh <= 8; sh++) {
      GLook g = glook(fmt("fam%d.%d", f, sh), 0, fmt("kit=%s:4:1,alloy=1,palette=look", kPeople[f % 12]));
      g.L.alloyFam = (uint8_t)(f + 1);
      g.L.sheen = (uint8_t)sh;
      g.r = doll::resolve(g.L);
      const std::string why = c17Look(g, 0);
      if (!why.empty()) m17.bad(fmt("%s %s (%s kit) D: %s", doll::alloyFamilyName(f), kSheen[sh - 1], kPeople[f % 12], why.c_str())); else m17.ok();
    }
  // the blades (D30): every held weapon's metal in Slot::Blade on every drawn and action frame; a blade of another metal
  // than the armour's draws in its own ramp
  for (const GLook& g : looks) {
    if (g.L.weapon < 1 || g.L.weapon > 8 || g.L.civil) continue;
    int bad = 0, frames = 0;
    for (RigPart rp : {RigPart::Drawn, RigPart::Action})
      for (int fc = 0; fc < 4; fc++)
        for (int cl = 0; cl < art::CLIP_COUNT; cl++)
          for (int i = 0; i < 6; i++) {
            const doll::FrameProbe p = doll::probeFrame(g.r, rp, fc, (Clip)cl, 0, i, false);
            if (!p.ok) continue;
            frames++;
            for (size_t k = 0; k < p.code.size(); k++) {
              const int o = p.owner[k];
              if (o < 0 || p.code[k] < 2 || !p.items[(size_t)o].has(doll::Role::Held)) continue;
              if (doll::codeSlot(p.code[k]) == doll::Slot::Metal) bad++;
            }
          }
    const int bk = g.L.bladeKind, mk = g.L.metalKind;
    bool rampOk = true;
    const int mkEff = mk == 1 ? 3 : mk;   // (a leather wearer's metal is plain iron: rampsOf)
    if (bk && mk && bk != mkEff && bk != 1)
      rampOk = std::memcmp(g.r.ramps.c[(int)doll::Slot::Blade], g.r.ramps.c[(int)doll::Slot::Metal], sizeof(uint32_t) * 5) != 0;
    if (bad || !rampOk) blade.bad(fmt("%s: %d held px in the armour's metal over %d frames%s", g.tag.c_str(), bad, frames, rampOk ? "" : "; the blade's ramp is the armour's"));
    else blade.ok();
  }
  // C12a restated: T2 to T3 differ in 25 % of the union of their gear pixels, the outline 6 % bigger
  {
    const GLook *t2 = findLook(looks, "lt2"), *t3 = findLook(looks, "lt3");
    if (!t2 || !t3) c12.bad("lt2 or lt3 missing");
    else
      for (int fc : {0, 2}) {
        const Grey g2 = greyOf(t2->r, RigPart::Loco, fc, Clip::Idle, 0), g3 = greyOf(t3->r, RigPart::Loco, fc, Clip::Idle, 0);
        // gear pixels' union, differing where one has gear and the other not or the colours differ
        std::set<Pt> uni(g2.gear.begin(), g2.gear.end());
        uni.insert(g3.gear.begin(), g3.gear.end());
        int diff = 0;
        for (const Pt& q : uni) {
          const bool a2 = g2.gear.count(q) > 0, a3 = g3.gear.count(q) > 0;
          if (a2 != a3) { diff++; continue; }
          const uint32_t c2 = t2->r.ramps.c[(int)doll::codeSlot(g2.code.at(q))][doll::codeStep(g2.code.at(q))];
          const uint32_t c3 = t3->r.ramps.c[(int)doll::codeSlot(g3.code.at(q))][doll::codeStep(g3.code.at(q))];
          if (c2 != c3) diff++;
        }
        const double d = uni.empty() ? 0 : (double)diff / uni.size();
        const double grow = g2.v.empty() ? 0 : (double)g3.v.size() / g2.v.size() - 1.0;
        if (report) std::printf("    C12a lt2 to lt3 %c: gear %.0f %%, outline %+.1f %%\n", "DURL"[fc], d * 100, grow * 100);
        if (d < 0.25 || grow < 0.06) c12.bad(fmt("lt2 to lt3 %c: gear %.0f %% (25), outline %+.1f %% (6)", "DURL"[fc], d * 100, grow * 100));
        else c12.ok();
      }
  }
  // selftests: the T5 plate recoloured must FAIL C16a; a clay ramp must FAIL C17
  {
    int caught = 0;
    if (const GLook* f5 = findLook(looks, "f5_steel")) {
      GLook re = *f5;
      re.L.metalKind = 5;   // gilded paint, the same style
      re.r = doll::resolve(re.L);
      const Pair p = comparePair(f5->r, re.r);
      if (std::max(p.xorD, p.xorR) < 0.04 || p.edge < 0.30) caught++;
      GLook clay = *f5;
      const uint32_t cl[5] = {rgba(0x3A, 0x24, 0x18), rgba(0x7A, 0x4A, 0x30), rgba(0xA8, 0x6A, 0x48), rgba(0xC8, 0x8A, 0x64), rgba(0xE0, 0xA8, 0x80)};
      for (int i = 0; i < 5; i++) clay.r.ramps.c[(int)doll::Slot::Metal][i] = cl[i];
      if (!c17Look(clay, 0).empty()) caught++;
    }
    // (forge round 2) a one-hue iridescent (Metal 2 the body's own ramp) must FAIL C17
    {
      GLook one = glook("one", 1, "kit=starspire:5:1,alloy=3,palette=look");
      for (int i = 0; i < 5; i++) one.r.ramps.c[(int)doll::Slot::Metal2][i] = one.r.ramps.c[(int)doll::Slot::Metal][i];
      if (!c17Look(one, 0).empty()) caught++;
      else printed.push_back("  selftest MISSED: a one-hue iridescent passes C17");
    }
    if (caught == 3) st.ok(); else st.bad(fmt("%d of 3 caught", caught));
    printed.push_back(fmt("  C16 C17 selftest: %s", caught == 3 ? "ALL CAUGHT" : "MISSED"));
  }
  int fails = 0;
  for (Line* l : {&c1f, &a, &s, &m17, &blade, &c12, &st}) {
    printed.push_back(fmt("%s %-24s %4d checks, %4d failed%s%s", l->fails ? "FAIL:" : "ok   ", l->id.c_str(), l->checks, l->fails, l->fails ? "; first: " : "", l->first.c_str()));
    fails += l->fails ? 1 : 0;
  }
  {   // (forge round 2) C16o, C16m, C16n, the visor term
    std::vector<std::pair<std::string, art::HumanLook>> fl;
    for (const GLook& g : looks) fl.push_back({g.tag, g.L});
    fails += dollShapeGates(report, printed, fl);
  }
  art::setRigVersion(was);
  return fails;
}

// ---------------------------------------------------------------- --forge-mech (the compositor's forge mechanisms)
// rpg_test --forge-mech --pool FILE [--look SPEC]: with a pool that draws cut.fluted, edge.cusped, crest.knife and
// collar.gorget (FORGE-DOLL's isolated test parts, or the art), on the look (default the fm_steel line): each part resolves,
// draws, and does what its mode says (the cut tile moves only lit metal steps of the armour; the edge stamp adds pixels at
// the hem; the crest lands on the crown; the collar on the neck; a rest-keyed `when cut-` overlay serves the idle key)
int mechCmd(int argc, char** argv) {
  std::string spec = "hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=steel";
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {
      std::string err;
      if (!art::doll::loadPoolFile(argv[++i], &err)) { std::printf("FAIL: --pool %s\n", err.c_str()); return 1; }
    } else if (!std::strcmp(argv[i], "--look") && i + 1 < argc) spec = argv[++i];
  }
  art::setRigVersion(2);
  namespace doll = art::doll;
  art::HumanLook L;
  doll::applyLookSpec(L, spec);
  const doll::DollRecipe r = doll::resolve(L);
  int fails = 0;
  auto check = [&](bool ok, const std::string& what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); if (!ok) fails++; };
  auto partIx = [&](const char* role) {
    for (int p : r.parts) if (doll::partHasRole(p, doll::Role::Overlay, role)) return p;
    return -1;
  };
  const int cut = partIx("cut.fluted"), edge = partIx("edge.cusped"), crest = partIx("crest.knife"), collar = partIx("collar.gorget");
  check(cut >= 0 && edge >= 0 && crest >= 0 && collar >= 0, fmt("resolve: cut %d edge %d crest %d collar %d", cut, edge, crest, collar));
  std::string pd;
  for (const doll::Need& n : r.needs) if (n.role == doll::Role::Pauldron) pd = n.value;
  check(pd.rfind("flared.", 0) == 0, "pauldron shape: " + pd);
  auto without = [&](int part) { doll::DollRecipe q = r; q.parts.erase(std::remove(q.parts.begin(), q.parts.end(), part), q.parts.end()); return q; };
  const doll::FrameProbe a = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, false);
  if (std::getenv("FORGE_MECH_SLOTS")) {   // the look's idle D: each slot's steps in use (B8's colour census)
    std::map<int, std::set<int>> used;
    const doll::FrameProbe fp = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
    for (uint8_t c : fp.code) if (c >= 2) used[(int)doll::codeSlot(c)].insert(doll::codeStep(c));
    int n = 0;
    for (const auto& kv : used) { std::printf("  slot %-8s steps %zu\n", doll::slotName((doll::Slot)kv.first), kv.second.size()); n += (int)kv.second.size(); }
    std::printf("  %d slot-steps\n", n);
  }
  // the cut: only lit metal pixels of the armour change, by a step
  {
    const doll::FrameProbe b = doll::probeFrame(without(cut), art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, false);
    int diff = 0, bad = 0;
    for (size_t k = 0; k < a.code.size(); k++) {
      if (a.code[k] == b.code[k]) continue;
      diff++;
      if (a.code[k] < 2 || b.code[k] < 2 || doll::codeSlot(a.code[k]) != doll::codeSlot(b.code[k]) || doll::codeStep(b.code[k]) == 0) bad++;
    }
    check(diff > 0 && bad == 0, fmt("cut tile: %d px moved a step, %d other changes", diff, bad));
  }
  {
    const doll::FrameProbe b = doll::probeFrame(without(edge), art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, false);
    int added = 0, lowA = 0, lowB = 0;
    for (size_t k = 0; k < a.code.size(); k++) {
      // a pixel the hang now owns that it did not (over the clear ground or the legs below the hem)
      if (a.code[k] >= 2 && a.owner[k] >= 0 && a.items[(size_t)a.owner[k]].part.find("_hang") != std::string::npos &&
          (b.owner[k] < 0 || b.items[(size_t)b.owner[k]].part.find("_hang") == std::string::npos)) added++;
      const int y = (int)(k / (size_t)a.w);
      (void)y;
    }
    (void)lowA; (void)lowB;
    check(added > 0, fmt("edge stamp: %d px added at the hem", added));
  }
  auto ownerBox = [&](const doll::FrameProbe& p, const char* part, int& x0, int& y0, int& x1, int& y1) {
    x0 = y0 = 9999; x1 = y1 = -1;
    for (size_t k = 0; k < p.code.size(); k++) {
      const int o = p.owner[k];
      if (o < 0 || p.items[(size_t)o].part != part) continue;
      const int x = (int)(k % (size_t)p.w), y = (int)(k / (size_t)p.w);
      x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
    }
    return x1 >= 0;
  };
  {
    int x0, y0, x1, y1;
    const bool on = ownerBox(a, "test/crest_knife", x0, y0, x1, y1);
    // the crest sits over the head box's top (head anchor y), centred on its middle (head x + 8)
    check(on && y1 <= a.headY + 3 && std::abs((x0 + x1) / 2 - (a.headX + 8)) <= 2,
          fmt("crest on the crown: box %d,%d-%d,%d, head %d,%d", x0, y0, x1, y1, a.headX, a.headY));
    const bool c2 = ownerBox(a, "test/collar_gorget", x0, y0, x1, y1);
    check(c2 && y0 >= a.headY + 10 && y0 <= a.headY + 18, fmt("collar on the neck: box %d,%d-%d,%d, head y %d", x0, y0, x1, y1, a.headY));
  }
  // the key fallback: in every frame where the plate draws a key with no `when cut-<v>` view of its own (the swings, the
  // drawn idle), the rest-keyed overlay is drawn too
  {
    int frames = 0, served = 0;
    std::string firstMiss;
    for (art::RigPart rp : {art::RigPart::Loco, art::RigPart::Run, art::RigPart::Action, art::RigPart::Drawn})
      for (int fc = 0; fc < 4; fc++)
        for (int cl = 0; cl < art::CLIP_COUNT; cl++)
          for (int i = 0; i < 8; i++) {
            const doll::FrameInfo fi = doll::frameInfo(r, rp, fc, (art::Clip)cl, 0, i);
            if (!fi.ok || fi.turn) continue;
            std::map<std::string, std::set<std::string>> keys;
            for (const auto& p : fi.parts) keys[p.part].insert(p.key);
            auto it = keys.find("armour/man/plate");
            if (it == keys.end()) continue;
            std::string k;
            for (const std::string& x : it->second) if (x != "rest") k = x;
            if (k.empty() || k == "runl" || k == "lie") continue;   // (rest: the overlay's own key; runl and lie are drawn)
            frames++;
            if (it->second.count("rest")) served++;
            else if (firstMiss.empty()) firstMiss = fmt("part %d facing %d clip %d frame %d key %s", (int)rp, fc, cl, i, k.c_str());
          }
    check(frames > 0 && served == frames, fmt("key fallback: %d of %d frames on another key carry the rest overlay %s", served, frames, firstMiss.c_str()));
  }
  // grey: every material slot neutral
  {
    doll::ComposeOpts op;
    op.grey = true;
    const art::RigSheet s = doll::compose(r, art::RigPart::Loco, &op);
    int colour = 0, n = 0;
    for (uint32_t p : s.atlas.px) {
      if (!(p >> 24)) continue;
      n++;
      const int R = p & 255, G = p >> 8 & 255, B = p >> 16 & 255;
      if (std::max({R, G, B}) - std::min({R, G, B}) > 40) colour++;
    }
    check(n > 0 && colour * 100 < n * 15, fmt("grey compose: %d of %d px coloured (skin, hair, eyes)", colour, n));
  }
  std::printf("forge-mech: %d failure(s)\n", fails);
  return fails ? 1 : 0;
}

}  // namespace dollforge

std::string dollForgeLooksText() { return dollforge::kForgeLooks; }
// (P2c forge) the drawn gates in rpg_test --doll --gates (test_doll.cpp)
int dollForgeGates(bool report, std::vector<std::string>& printed) { return dollforge::dollForgeGatesImpl(report, printed); }

// ---------------------------------------------------------------- C1 source F (rpg_test --doll-coverage --scope forge)
// every forge look (tools/doll/forge_looks.txt) resolves with nothing missing, and every forge value it asks for is drawn:
// the collar, cut (a cut tile, or a `when cut-<v>` view in the body's armour), edge, fittings and limb tiles, the crest, the
// pauldron shape (any size). --pool FILE previews an isolated pool; --gates fails on anything missing
int dollForgeCoverage(int argc, char** argv) {
  namespace doll = art::doll;
  namespace F = art::doll::forge;
  bool gates = false, report = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--report")) report = true;
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {
      std::string err;
      if (!doll::loadPoolFile(argv[++i], &err)) { std::printf("FAIL: --pool %s\n", err.c_str()); return 1; }
    }
  }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const doll::pool::Pool& P = doll::pool::get();
  auto overlayDrawn = [&](const std::string& v, doll::BodySet body) {
    for (int i : P.partsWithRole(doll::Role::Overlay, v)) if (P.parts[(size_t)i].bodyMask >> (int)body & 1) return true;
    return false;
  };
  auto whenDrawn = [&](const std::string& flag, doll::BodySet body) {
    for (size_t pi = 0; pi < P.parts.size(); pi++) {
      const doll::pool::PartRec& p = P.parts[pi];
      if (!(p.bodyMask >> (int)body & 1)) continue;
      for (uint32_t wi : p.whenViews) {
        const std::string& w = P.s(P.views[wi].when);
        for (size_t k = 0; (k = w.find(flag, k)) != std::string::npos; k++) {
          const size_t e = k + flag.size();
          if ((k == 0 || w[k - 1] == ',' || w[k - 1] == '!') && (e == w.size() || w[e] == ',')) return true;
        }
      }
    }
    return false;
  };
  auto pauldronDrawn = [&](const char* shape) {
    for (int sz = 2; sz <= 4; sz++) if (!P.partsWithRole(doll::Role::Pauldron, std::string(shape) + "." + std::to_string(sz)).empty()) return true;
    return false;
  };
  std::map<std::string, int> missing;
  int total = 0, clean = 0;
  // (forge round 2) the silhouette: every value the resolver could not apply (r.forgeMissing), and a sleeve part's keys the
  // forge clips name for its arm channel (no key fallback: a missing key draws the close arm)
  auto shapeMissing = [&](const doll::DollRecipe& r, std::vector<std::string>& mine) {
    for (const std::string& m : r.forgeMissing) mine.push_back(m);
    const uint16_t armR = P.strId("armR"), armL = P.strId("armL");
    for (int pi : r.parts) {
      const doll::pool::PartRec& p = P.parts[(size_t)pi];
      std::string sv;
      for (const doll::pool::RoleRec& rr : p.roles)
        if (rr.cat == (uint8_t)doll::Role::Overlay && P.s(rr.value).rfind("sleeve.", 0) == 0) sv = P.s(rr.value);
      if (sv.empty()) continue;
      for (uint8_t part : {(uint8_t)art::RigPart::Loco, (uint8_t)art::RigPart::Run, (uint8_t)art::RigPart::Drawn, (uint8_t)art::RigPart::Action})
        for (const doll::pool::ClipRec* c : doll::pool::clipsOf(r.bodyKey, part, r.family, -1, &r))
          for (uint16_t fi = 0; fi < c->nFrames; fi++) {
            const doll::pool::FrameRec& fr = P.frames[c->firstFrame + fi];
            for (const doll::pool::KeyRec& k : fr.keys) {
              if (k.channel != p.channel || (k.channel != armR && k.channel != armL) || k.key == P.ids.dash) continue;
              const int facing = (fr.turn || c->clip == (uint8_t)art::Clip::Lying) ? 3 : c->facing;
              if (!doll::pool::viewOf(p, facing, k.key))
                mine.push_back(sv + " (" + doll::bodySetName(r.body) + ") " + P.s(p.name) + " " + "DURL"[facing & 3] + " key " + P.s(k.key));
            }
          }
    }
  };
  for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
    const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
    for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
      total++;
      art::HumanLook L;
      L.hair = art::Hair::Short; L.hairVar = 2; L.weapon = 1;
      L = gear::alloyKitLook(L, c, i);
      const doll::DollRecipe r = doll::resolve(L);
      std::vector<std::string> mine;
      for (const doll::Missing& x : doll::dollMissing(L)) mine.push_back(x.what());
      shapeMissing(r, mine);
      std::sort(mine.begin(), mine.end());
      mine.erase(std::unique(mine.begin(), mine.end()), mine.end());
      if (mine.empty()) { clean++; continue; }
      for (const std::string& m : mine) missing[m]++;
      if (report) std::printf("    board %s: %d missing; first %s\n", c.arms.alloys[(size_t)i].name.c_str(), (int)mine.size(), mine[0].c_str());
    }
  }
  for (const dollforge::GLook& g : dollforge::forgeLooks()) {
    total++;
    std::vector<std::string> mine;
    for (const doll::Missing& x : doll::dollMissing(g.L)) mine.push_back(x.what());
    const doll::DollRecipe& r = g.r;
    shapeMissing(r, mine);
    const doll::BodySet body = r.body;
    const uint8_t* b = r.forgeAx[art::HumanLook::TIER_BODY];
    if (g.L.forge[art::HumanLook::TIER_BODY] && r.armourFam >= 0) {
      if (b[F::AX_SHOULDER] && g.L.pauldron >= 2 && g.L.tier[0] != 1 && !pauldronDrawn(F::valueName(F::AX_SHOULDER, b[F::AX_SHOULDER])))
        mine.push_back(std::string("pauldron ") + F::valueName(F::AX_SHOULDER, b[F::AX_SHOULDER]));
      if (b[F::AX_COLLAR] && !overlayDrawn(std::string("collar.") + F::valueName(F::AX_COLLAR, b[F::AX_COLLAR]), body))
        mine.push_back(std::string("collar.") + F::valueName(F::AX_COLLAR, b[F::AX_COLLAR]));
      if (b[F::AX_CUT]) {
        const std::string v = F::valueName(F::AX_CUT, b[F::AX_CUT]);
        if (!overlayDrawn("cut." + v, body) && !whenDrawn("cut-" + v, body)) mine.push_back("cut." + v + " (" + doll::bodySetName(body) + ")");
      }
      if (b[F::AX_EDGE] && std::strcmp(F::valueName(F::AX_EDGE, b[F::AX_EDGE]), "plain") && !overlayDrawn(std::string("edge.") + F::valueName(F::AX_EDGE, b[F::AX_EDGE]), body) &&
          !whenDrawn(std::string("edge-") + F::valueName(F::AX_EDGE, b[F::AX_EDGE]), body))
        mine.push_back(std::string("edge.") + F::valueName(F::AX_EDGE, b[F::AX_EDGE]));
      if (b[F::AX_FIT] && !overlayDrawn(std::string("fit.") + F::valueName(F::AX_FIT, b[F::AX_FIT]), body))
        mine.push_back(std::string("fit.") + F::valueName(F::AX_FIT, b[F::AX_FIT]));
      if (b[F::AX_LIMB] && std::strcmp(F::valueName(F::AX_LIMB, b[F::AX_LIMB]), "plain") && !overlayDrawn(std::string("limb.") + F::valueName(F::AX_LIMB, b[F::AX_LIMB]), body))
        mine.push_back(std::string("limb.") + F::valueName(F::AX_LIMB, b[F::AX_LIMB]));
    }
    const uint8_t* h = r.forgeAx[art::HumanLook::TIER_HELM];
    if (h[F::AX_CREST] && g.L.tier[1] != 1 && (g.L.helmet || g.L.helmStyle || g.L.helmForm) &&
        !overlayDrawn(std::string("crest.") + F::valueName(F::AX_CREST, h[F::AX_CREST]), body))
      mine.push_back(std::string("crest.") + F::valueName(F::AX_CREST, h[F::AX_CREST]));
    for (int sl : {(int)art::HumanLook::TIER_GLOVES, (int)art::HumanLook::TIER_BOOTS}) {
      const int lv = r.forgeAx[sl][F::AX_LIMB];
      if (lv && std::strcmp(F::valueName(F::AX_LIMB, lv), "plain") && !overlayDrawn(std::string("limb.") + F::valueName(F::AX_LIMB, lv), body)) mine.push_back(std::string("limb.") + F::valueName(F::AX_LIMB, lv));
    }
    std::sort(mine.begin(), mine.end());
    mine.erase(std::unique(mine.begin(), mine.end()), mine.end());
    if (mine.empty()) { clean++; continue; }
    for (const std::string& m : mine) missing[m]++;
    if (report) std::printf("    F %s: %d missing; first %s\n", g.tag.c_str(), (int)mine.size(), mine[0].c_str());
  }
  std::vector<std::pair<int, std::string>> top;
  for (const auto& kv : missing) top.push_back({kv.second, kv.first});
  std::sort(top.rbegin(), top.rend());
  for (size_t i = 0; i < top.size() && i < 60; i++) std::printf("  missing x%d: %s\n", top[i].first, top[i].second.c_str());
  std::printf("%s C1 source F (forge)   %d looks, %d covered, %d with something missing\n", clean == total ? "ok   " : "FAIL:", total, clean, total - clean);
  art::setRigVersion(was);
  return clean != total ? 1 : 0;   // (wave 2 r3) a printed FAIL always fails, --gates or not
}

RPG_TEST_CMD("--forge-mech", "P2c forge: the compositor's forge mechanisms on a test pool --pool f [--look spec]", dollforge::mechCmd);

RPG_TEST_CMD("--forge-picks", "P2c forge: every alloy's seven picks [--people name] [--seeds A..B]", dollforge::picksCmd);
RPG_TEST_CMD("--doll-forge", "P2c forge gates C16b-d, C16h [--worlds A..B] [--seeds A..B] [--report] [--selftest]", dollforge::forgeCmd);

// (forge i2) rpg_test --doll-forge-drawn [--report] [--pool FILE]: the drawn forge gates alone (C1 F, C16 a s b c, C17, C17
// blades, C12a, the selftests), every failure listed with --report (FORGE_ALL=1: past 12 per line)
namespace dollforge {
int drawnCmd(int argc, char** argv) {
  bool report = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--report")) report = true;
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {
      std::string err;
      if (!art::doll::loadPoolFile(argv[++i], &err)) { std::printf("FAIL: --pool %s\n", err.c_str()); return 1; }
    }
  }
  if (std::getenv("FORGE_RAMPS")) {   // the forge looks' metal, cloak, cloth and hair ramps' lumas (FORGE_RAMPS=1)
    const int was = art::rigVersion();
    art::setRigVersion(2);
    std::vector<GLook> ls = forgeLooks();
    if (std::strcmp(std::getenv("FORGE_RAMPS"), "1")) {   // FORGE_RAMPS=<looks file>: that file's looks
      ls.clear();
      if (FILE* fp = std::fopen(std::getenv("FORGE_RAMPS"), "rb")) {
        char ln[2048];
        while (std::fgets(ln, sizeof ln, fp)) {
          char tag[128] = {}, who[16] = {}, spec[1400] = {};
          if (ln[0] == '#' || std::sscanf(ln, "%127s %15s %1399s", tag, who, spec) < 2) continue;
          ls.push_back(glook(tag, !std::strcmp(who, "woman"), std::strcmp(spec, "-") ? spec : ""));
        }
        std::fclose(fp);
      }
    }
    for (const GLook& g : ls) {
      std::printf("  %-9s", g.tag.c_str());
      for (doll::Slot s : {doll::Slot::Metal, doll::Slot::Cloak, doll::Slot::Cloth1, doll::Slot::Cloth2, doll::Slot::Hair, doll::Slot::Accent}) {
        std::printf(" %s", doll::slotName(s));
        for (int i = 0; i < 5; i++) std::printf(" %3.0f", lumaOf(g.r.ramps.c[(int)s][i]));
      }
      std::printf("\n");
    }
    art::setRigVersion(was);
  }
  std::vector<std::string> printed;
  const int f = dollForgeGatesImpl(report, printed);
  for (const std::string& s : printed) std::printf("%s\n", s.c_str());
  return f ? 1 : 0;
}
}  // namespace dollforge
RPG_TEST_CMD("--doll-forge-drawn", "P2c forge: the drawn gates C1 F, C16 a s b c, C17, C12a alone [--report] [--pool f]", dollforge::drawnCmd);
