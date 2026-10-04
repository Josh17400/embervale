// Save-format and world-gen versioning checks.
//   save_test [fixtureDir]            load tests/fixtures/save_v1.bin, check its values and its regenerated world, round-trip v2
//   save_test --make-fixture out.bin  play a fixed seed with the bot and write a save in the CURRENT format
//                                     (run once with SAVE_VER 1 code to create save_v1.bin; do not overwrite it)
//   save_test --make-fixture-v2 out.bin   the v2 fixture (seed 5150, generator v2), written by today's code: a v3 save
//                                     with the v3 character block cut off and the version set to 2, which is
//                                     byte-for-byte what SAVE_VER 2 code wrote (v3 only appended that block)
//   save_test --make-fixture-v3 out.bin   the v3 fixture (seed 6061, generator v2, a created character with a
//                                     background, story flags and gloves/boots/cloak equipped)
//   save_test --make-fixture-gen3 / -gen4 / -gen5 / -gen6 out.bin   SAVE_VER 3 saves of a generator-v3 world
//                                     (seed 33) and a generator-v4 world (seed 3): they lock those generators' output,
//                                     walls and gates included (paste the printed GENLOCK line into this file)
// Fixtures are made once and never regenerated: they are the old formats. Every fixture's world uses a generator
// version whose output is frozen (v1..v6), so generator work on a newer WORLDGEN_LATEST cannot invalidate them.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

namespace {

// ---- the checked-in v1 fixture: made by `save_test --make-fixture` with SAVE_VER 1 code (generator v1), 859 bytes.
// Seed 4242: took the start-village innkeeper's bounty, 150 s of bot play, looted one overworld chest, then 60 s
// of bot play inside the nearest cave (SHADOW WARREN), saved inside it. Never regenerate it: it is the old format.
constexpr uint64_t FIX_SEED = 4242;
constexpr int FIX_LEVEL = 2, FIX_GOLD = 30, FIX_QUESTS = 2, FIX_INV = 5, FIX_LOOTED = 1, FIX_KILLMAPS = 1, FIX_KILLS = 7;
constexpr bool FIX_INSIDE = true;
constexpr int FIX_SUBSITE = 27;
constexpr float FIX_PX = 91.026f, FIX_PY = 194.942f;
constexpr int FIX_BOUNTY_SITE = 17, FIX_BOUNTY_BLDG = 142;   // quest 2: BOUNTY: WEEPING CAMP, from the SALTHOLD innkeeper
// the world seed 4242 generated with generator v1
constexpr int FIX_SITES = 57, FIX_BLDGS = 148, FIX_GATES = 8, FIX_START = 17, FIX_CAPITAL = 2, FIX_LAIR = 56;
constexpr uint32_t FIX_SITEHASH = 0xA94319E3, FIX_BLDGHASH = 0xC733FDAF, FIX_GROUNDHASH = 0x20C1960B;
const char* const FIX_START_NAME = "SALTHOLD";

// ---- the v2 fixture (seed 5150, generator v2); values printed by --make-fixture-v2
constexpr uint64_t FIX2_SEED = 5150;

uint32_t fnv(uint32_t h, const void* p, size_t n) {
  const uint8_t* b = (const uint8_t*)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
  return h;
}
uint32_t fnvI(uint32_t h, int v) { return fnv(h, &v, 4); }

uint32_t siteHash(const World& w) {
  uint32_t h = 2166136261u;
  for (const Site& s : w.sites) {
    h = fnvI(h, (int)s.type); h = fnv(h, s.name.data(), s.name.size());
    h = fnvI(h, s.r.x); h = fnvI(h, s.r.y); h = fnvI(h, s.r.w); h = fnvI(h, s.r.h); h = fnvI(h, s.ex); h = fnvI(h, s.ey);
    h = fnvI(h, s.level); h = fnvI(h, s.bldgFirst); h = fnvI(h, s.bldgCount); h = fnvI(h, (int)s.seed); h = fnvI(h, s.mainQuest); h = fnvI(h, (int)s.theme);
  }
  return h;
}
uint32_t bldgHash(const World& w) {
  uint32_t h = 2166136261u;
  for (const Bldg& b : w.over.bldgs) {
    h = fnvI(h, (int)b.type); h = fnvI(h, b.r.x); h = fnvI(h, b.r.y); h = fnvI(h, b.r.w); h = fnvI(h, b.r.h);
    h = fnvI(h, (int)b.roof); h = fnvI(h, (int)b.seed); h = fnvI(h, b.site); h = fnvI(h, (int)b.owner);
  }
  return h;
}
uint32_t groundHash(const World& w) {
  uint32_t h = fnv(2166136261u, w.over.ground.data(), w.over.ground.size());
  return fnv(h, w.over.wall.data(), w.over.wall.size());
}
// city gatehouses and wall openings (the v3/v4 wall passes), plus the overworld props (lamps, trees by the walls)
uint32_t gateHash(const World& w) {
  uint32_t h = 2166136261u;
  for (auto& g : w.gates) { h = fnvI(h, g.first); h = fnvI(h, g.second); }
  for (const IRect& r : w.wallGaps) { h = fnvI(h, r.x); h = fnvI(h, r.y); h = fnvI(h, r.w); h = fnvI(h, r.h); }
  return fnv(h, w.over.prop.data(), w.over.prop.size());
}

// the rpg_test wandering bot
void bot(Game& g, float secs, uint64_t rs) {
  Rng r(rs);
  Input in;
  int frames = (int)(secs * 60);
  for (int f = 0; f < frames; f++) {
    int tgt = -1; float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead) continue;
      float d2 = len2(e.p - g.pl().p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    if (tgt >= 0) in.move = norm(g.actors[tgt].p - g.pl().p) * (bd > 18 * 18 ? 1.0f : 0.0f);
    else if (f % 90 == 0) { float a = r.f() * TAU; in.move = Vec2(std::cos(a), std::sin(a)); }
    in.attack = (f % 20) == 0 && tgt >= 0 && bd < 30 * 30;
    in.bow = (f % 97) == 0;
    in.spell = (f % 151) == 0;
    in.roll = (f % 211) == 0;
    in.interact = (f % 60) == 0;
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.dialogueChoose(0), g.mode = Mode::Play;
    if (g.mode == Mode::Shop || g.mode == Mode::LevelUp) { if (g.mode == Mode::LevelUp) g.chooseLevelUp(f % 3); g.mode = Mode::Play; }
    if (g.mode == Mode::Dead) g.respawn();
  }
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
  fclose(f);
  return true;
}

// v3 tail = eqGloves, eqBoots, eqCloak (12) + background (1) + story flags (4) + u16 length + appearance block
size_t v3TailSize(const std::vector<uint8_t>& b) {
  for (size_t L = 0; L + 2 + 17 < b.size() && L < 4096; L++) {
    size_t at = b.size() - L - 2;
    if ((size_t)(b[at] | (b[at + 1] << 8)) == L) return L + 2 + 17;
  }
  return 0;
}
std::vector<uint8_t> asV2(std::vector<uint8_t> b) {
  size_t tail = v3TailSize(b);
  if (!tail) return {};
  b.resize(b.size() - tail);
  b[4] = 2; b[5] = b[6] = b[7] = 0;
  return b;
}

// the v3 fixture's character (also what section 4 checks after loading it)
constexpr uint64_t FIX3_SEED = 6061;
// SAVE_VER 3 saves of worlds from generator v3 (the M0 buildings, walls and interiors as first shipped) and v4 (walls
// built before the buildings). They lock each generator's output, walls, gates, openings and props included. Made
// once with --make-fixture-gen3 / --make-fixture-gen4 / --make-fixture-gen5, which also print the GENLOCK lines below.
// v5 (M0 fix round 2: gates on every road through the wall, no gate by a river, building/stall clearances): seed 8.
// v6 (M0 fix round 3: the cave top-up for worlds with fewer than 8 caves): seed 52, which v5 left with 3 caves.
constexpr uint64_t FIXG3_SEED = 33, FIXG4_SEED = 3, FIXG5_SEED = 8, FIXG6_SEED = 52;
struct GenLock { int sites, bldgs, gates, gaps; uint32_t site, bldg, ground, gate; };
constexpr GenLock GENLOCK_V3 = {59, 170, 6, 12, 0x7EC87F5E, 0x9E0BE718, 0x5E5712E2, 0x4DE6008C};
constexpr GenLock GENLOCK_V4 = {56, 157, 5, 12, 0x0D1BA049, 0x6F34B156, 0xDA5CD4DC, 0x500A7CA3};
constexpr GenLock GENLOCK_V5 = {63, 154, 6, 14, 0xBCDF1A11, 0x1D2EE86D, 0xDEA1AF28, 0x5BCE217E};
constexpr GenLock GENLOCK_V6 = {55, 144, 4, 13, 0x191E3503, 0x9ADCD698, 0x90982C97, 0x54D00250};
constexpr uint32_t FIX3_SKIN = 0xFF4A6E96u, FIX3_HAIRC = 0xFF2A62D2u, FIX3_EYE = 0xFF3C8C30u, FIX3_TOP = 0xFF283C8Cu, FIX3_BOTTOM = 0xFF203040u;
constexpr uint32_t FIX3_FLAGS = SF_CREATED | SF_FIRST_WEAPON | (1u << 9);
const char* const FIX3_NAME = "BRYNJA";
Item fixtureWear(ItemKind k, const char* name, art::Icon icon, int power) {
  Item it;
  it.kind = k; it.name = name; it.icon = icon; it.power = (int16_t)power; it.value = power * 10; it.tint = tierTint(0);
  return it;
}
void makeCharacter(Game& g) {
  g.app.name = FIX3_NAME; g.app.female = true; g.app.build = 1; g.app.skinTone = 5; g.app.skin = FIX3_SKIN;
  g.app.hair = (uint8_t)art::Hair::Braids; g.app.hairColor = FIX3_HAIRC; g.app.beard = false; g.app.eyeColor = FIX3_EYE;
  g.app.topColor = FIX3_TOP; g.app.bottomColor = FIX3_BOTTOM; g.app.created = true;
  g.background = Background::Hunter;
  g.storyFlags = FIX3_FLAGS;
  g.inv.push_back(fixtureWear(ItemKind::Gloves, "LEATHER GLOVES", art::Icon::Gloves, 2));
  g.inv.push_back(fixtureWear(ItemKind::Boots, "LEATHER BOOTS", art::Icon::Boots, 3));
  g.inv.push_back(fixtureWear(ItemKind::Cloak, "HUNTER'S CLOAK", art::Icon::Armor, 1));
  int n = (int)g.inv.size();
  g.useItem(n - 3); g.useItem(n - 2); g.useItem(n - 1);
}

int makeFixture(const char* out, uint64_t seed = FIX_SEED, int genVer = WORLDGEN_LATEST, int form = 0) {
  Game g(seed);
  g.newGame(seed, genVer);
  if (form == 3) makeCharacter(g);
  g.mode = Mode::Play;
  g.godMode = true;
  // take the innkeeper's job in the start village
  const Site& home = g.world.sites[g.world.startSite];
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++)
    if (g.world.over.bldgs[b].type == art::Building::Inn) {
      const Bldg& B = g.world.over.bldgs[b];
      g.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
      g.update(SIM_DT, Input());
      for (size_t k = 1; k < g.actors.size(); k++)
        if (g.actors[k].role == Role::Innkeeper) {
          g.pl().p = g.actors[k].p + Vec2(0, 20);
          Input t; t.interact = true;
          g.update(SIM_DT, t);
          for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.opts[o].label.find("WORK") != std::string::npos) { g.dialogueChoose((int)o); g.dialogueChoose(0); break; }
          break;
        }
      g.mode = Mode::Play;
      break;
    }
  printf("after inn: quests %zu inside %d\n", g.quests.size(), g.inside);
  if (g.inside) {   // walk out of the inn
    g.pl().p = Vec2(g.sub.exitX * 16 + 8.0f, (g.sub.exitY - 2) * 16 + 8.0f);
    Input down; down.move = Vec2(0, 1);
    for (int f = 0; f < 120 && g.inside; f++) g.update(SIM_DT, down);
  }
  bot(g, 150, seed);
  printf("after bot: lvl %d gold %d kills %d inside %d\n", g.plLevel, g.gold, g.kills, g.inside);
  // loot the nearest overworld chest
  if (!g.inside) {
    const Map& m = g.world.over;
    int ptx = (int)(g.pl().p.x / 16), pty = (int)(g.pl().p.y / 16), best = -1, bd = 1 << 30;
    for (int i = 0; i < m.w * m.h; i++)
      if (m.prop[(size_t)i] == (int)art::Prop::Chest + 1 && !m.blocked(i % m.w, i / m.w + 1)) {
        int dx = i % m.w - ptx, dy = i / m.w - pty;
        if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; }
      }
    if (best >= 0) {
      int cx = best % m.w, cy = best / m.w;
      g.pl().p = Vec2(cx * 16 + 8.0f, (cy + 1) * 16 + 8.0f);
      g.pl().aim = Vec2(0, -1);
      Input t; t.interact = true;
      g.update(SIM_DT, t);
      g.mode = Mode::Play;
      printf("chest at %d,%d: looted %zu\n", cx, cy, g.looted.size());
    }
  }
  // fight through part of the nearest cave and save inside it
  int cave = g.world.nearestSite((int)(g.pl().p.x / 16), (int)(g.pl().p.y / 16), SiteType::Cave);
  if (cave >= 0 && !g.inside) {
    const Site& s = g.world.sites[cave];
    const Map& m = g.world.over;
    for (int y = s.r.y - 2; y < s.r.y + s.r.h + 2 && !g.inside; y++)
      for (int x = s.r.x - 2; x < s.r.x + s.r.w + 2 && !g.inside; x++)
        if (m.propAt(x, y) == (int)art::Prop::CaveEntrance + 1) {
          g.pl().p = Vec2(x * 16 + 8.0f, y * 16 + 10.0f);
          g.update(SIM_DT, Input());
        }
    printf("cave %s: inside %d\n", s.name.c_str(), g.inside);
    bot(g, 60, seed + 1);
  }
  g.mode = Mode::Play;
  std::vector<uint8_t> buf;
  g.serialize(buf);
  if (form == 2) buf = asV2(buf);
  if (buf.empty()) { printf("cannot cut the v3 tail\n"); return 1; }
  FILE* f = fopen(out, "wb");
  if (!f) { printf("cannot write %s\n", out); return 1; }
  fwrite(buf.data(), 1, buf.size(), f);
  fclose(f);
  // what a fresh load of this save must show
  Game h(1);
  if (!h.deserialize(buf)) { printf("FAIL: fresh save does not load\n"); return 1; }
  size_t km = 0;
  for (auto& kv : h.killedSlots) if (!kv.second.empty()) km++;
  printf("wrote %s (%zu bytes)\n", out, buf.size());
  printf("constexpr int FIX_LEVEL = %d, FIX_GOLD = %d, FIX_QUESTS = %zu, FIX_INV = %zu, FIX_LOOTED = %zu, FIX_KILLMAPS = %zu, FIX_KILLS = %d;\n",
         h.plLevel, h.gold, h.quests.size(), h.inv.size(), h.looted.size(), km, h.kills);
  printf("constexpr bool FIX_INSIDE = %s;\nconstexpr int FIX_SUBSITE = %d;\nconstexpr float FIX_PX = %.3ff, FIX_PY = %.3ff;\n", h.inside ? "true" : "false", h.subSite, h.pl().p.x, h.pl().p.y);
  World w;
  w.generate(seed, genVer);
  printf("constexpr int FIX_SITES = %zu, FIX_BLDGS = %zu, FIX_GATES = %zu, FIX_START = %d, FIX_CAPITAL = %d, FIX_LAIR = %d;\n",
         w.sites.size(), w.over.bldgs.size(), w.gates.size(), w.startSite, w.capital, w.lair);
  printf("constexpr uint32_t FIX_SITEHASH = 0x%08X, FIX_BLDGHASH = 0x%08X, FIX_GROUNDHASH = 0x%08X;\n", siteHash(w), bldgHash(w), groundHash(w));
  printf("const char* const FIX_START_NAME = \"%s\";\n", w.sites[w.startSite].name.c_str());
  printf("constexpr GenLock GENLOCK_V%d = {%zu, %zu, %zu, %zu, 0x%08X, 0x%08X, 0x%08X, 0x%08X};\n", w.genVersion, w.sites.size(), w.over.bldgs.size(),
         w.gates.size(), w.wallGaps.size(), siteHash(w), bldgHash(w), groundHash(w), gateHash(w));
  for (auto& q : h.quests) printf("  quest %d type %d state %d giver %d/%d/%d: %s\n", q.id, (int)q.type, (int)q.state, q.giverSite, q.giverBldg, q.giverSlot, q.title.c_str());
  return 0;
}

int bad = 0;
void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); bad++; }
}

bool sameWorldV1(const World& w, const char* label) {
  bool ok = (int)w.sites.size() == FIX_SITES && (int)w.over.bldgs.size() == FIX_BLDGS && (int)w.gates.size() == FIX_GATES &&
            w.startSite == FIX_START && w.capital == FIX_CAPITAL && w.lair == FIX_LAIR && siteHash(w) == FIX_SITEHASH &&
            bldgHash(w) == FIX_BLDGHASH && groundHash(w) == FIX_GROUNDHASH && w.sites[w.startSite].name == FIX_START_NAME;
  if (!ok)
    printf("FAIL: %s: world differs from the v1 world: sites %zu bldgs %zu gates %zu start %d capital %d lair %d hashes %08X %08X %08X\n", label,
           w.sites.size(), w.over.bldgs.size(), w.gates.size(), w.startSite, w.capital, w.lair, siteHash(w), bldgHash(w), groundHash(w));
  if (!ok) bad++;
  return ok;
}

// the state a save should restore, for comparing two loads
std::string summary(const Game& g) {
  char b[512];
  size_t km = 0, ks = 0;
  for (auto& kv : g.killedSlots) if (!kv.second.empty()) { km++; ks += kv.second.size(); }
  int disc = 0;
  for (auto& s : g.world.sites) disc += (s.discovered ? 1 : 0) + (s.cleared ? 2 : 0);
  snprintf(b, sizeof b, "seed %llu gen %d lvl %d xp %d gold %d inv %zu quests %zu looted %zu killed %zu/%zu kills %d inside %d/%d/%d pos %.2f,%.2f hp %.1f day %d hour %.2f disc %d",
           (unsigned long long)g.seed, g.world.genVersion, g.plLevel, g.plXp, g.gold, g.inv.size(), g.quests.size(), g.looted.size(), km, ks, g.kills,
           (int)g.inside, g.subSite, g.subBldg, g.pl().p.x, g.pl().p.y, g.pl().hp, g.day, g.hour, disc);
  return b;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture")) return makeFixture(argv[2]);
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-v2")) return makeFixture(argv[2], FIX2_SEED, WORLDGEN_V2, 2);
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-v3")) return makeFixture(argv[2], FIX3_SEED, WORLDGEN_V2, 3);
  if (argc >= 3 && !strcmp(argv[1], "--gen-stats")) {   // --gen-stats N: buildings per city/town, generator v3 vs latest
    int n = atoi(argv[2]);
    for (int sd = 1; sd <= n; sd++)
      for (int gv : {(int)WORLDGEN_V3, (int)WORLDGEN_LATEST}) {
        World w;
        w.generate((uint64_t)sd, gv);
        int city = 0, town = 0, nc = 0, nt = 0;
        for (const Site& st : w.sites) {
          if (st.type == SiteType::City) { city += st.bldgCount; nc++; }
          if (st.type == SiteType::Town) { town += st.bldgCount; nt++; }
        }
        printf("seed %d gen %d: %zu buildings, %.1f per city, %.1f per town, %zu gates, %zu openings\n", sd, gv, w.over.bldgs.size(),
               nc ? city / (float)nc : 0.0f, nt ? town / (float)nt : 0.0f, w.gates.size(), w.wallGaps.size());
      }
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-gen3")) return makeFixture(argv[2], FIXG3_SEED, WORLDGEN_V3, 3);
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-gen4")) return makeFixture(argv[2], FIXG4_SEED, WORLDGEN_V4, 3);
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-gen5")) return makeFixture(argv[2], FIXG5_SEED, WORLDGEN_V5, 3);
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture-gen6")) return makeFixture(argv[2], FIXG6_SEED, WORLDGEN_V6, 3);
  std::string dir = argc >= 2 ? argv[1] : "";
#ifdef EMB_SOURCE_DIR
  if (dir.empty()) dir = std::string(EMB_SOURCE_DIR) + "/tests/fixtures";
#endif
  if (dir.empty()) dir = "tests/fixtures";
  std::vector<uint8_t> v1;
  if (!readFile(dir + "/save_v1.bin", v1)) { printf("FAIL: cannot read %s/save_v1.bin\n", dir.c_str()); return 1; }

  // 1. the v1 fixture loads with its values
  Game g(1);
  check(g.deserialize(v1), "v1 fixture did not load");
  printf("v1 load: %s\n", summary(g).c_str());
  check(g.world.genVersion == WORLDGEN_V1, "v1 save did not use generator v1");
  check(!g.worldChanged, "v1 save flagged a changed world");
  check(g.seed == FIX_SEED && g.plLevel == FIX_LEVEL && g.gold == FIX_GOLD && (int)g.quests.size() == FIX_QUESTS && (int)g.inv.size() == FIX_INV, "v1 level/gold/quests/inventory");
  check((int)g.looted.size() == FIX_LOOTED && g.kills == FIX_KILLS, "v1 looted chests / kills");
  size_t km = 0;
  for (auto& kv : g.killedSlots) if (!kv.second.empty()) km++;
  check((int)km == FIX_KILLMAPS, "v1 killed slots");
  check(g.inside == FIX_INSIDE && g.subSite == FIX_SUBSITE && std::fabs(g.pl().p.x - FIX_PX) < 0.01f && std::fabs(g.pl().p.y - FIX_PY) < 0.01f, "v1 position / inside the cave");
  // quest-giver identity still points at the same NPC's building
  const Quest* bounty = g.questById(2);
  check(bounty && bounty->type == QType::Bounty && bounty->giverSite == FIX_BOUNTY_SITE && bounty->giverBldg == FIX_BOUNTY_BLDG, "v1 bounty quest");
  if (bounty && bounty->giverBldg >= 0 && bounty->giverBldg < (int)g.world.over.bldgs.size()) {
    const Bldg& b = g.world.over.bldgs[bounty->giverBldg];
    check(b.type == art::Building::Inn && b.site == bounty->giverSite, "v1 bounty giver's building is no longer the start inn");
  }
  // the looted overworld chest is still open after loading
  for (uint64_t k : g.looted)
    if ((k >> 32) == 0) check(g.world.over.prop[(uint32_t)k] == (int)art::Prop::ChestOpen + 1, "v1 looted chest closed again");
  // 2. the regenerated world is the one the save was made in
  sameWorldV1(g.world, "v1 load");
  World w1;
  w1.generate(FIX_SEED, WORLDGEN_V1);
  sameWorldV1(w1, "generate(seed, WORLDGEN_V1)");
  check(w1.fingerprint() == g.world.fingerprint(), "fingerprint not deterministic");

  // pre-v3 saves get the default character
  check(!g.app.created && g.background == Background::None && g.storyFlags == 0 && g.eqGloves < 0 && g.eqBoots < 0 && g.eqCloak < 0,
        "v1 save did not get the default character block");

  // 3. current-format (v3) round trip of the v1 game
  std::vector<uint8_t> cur;
  g.serialize(cur);
  BinR hr(cur);
  uint32_t magic = hr.u32(), ver = hr.u32(), gv = hr.u32();
  check(magic == 0x454D4256 && ver == 3 && (int)gv == WORLDGEN_V1, "v3 header (magic, version, world-gen version)");
  Game h(1);
  check(h.deserialize(cur), "v3 save did not load");
  check(!h.worldChanged, "v3 reload flagged a changed world");
  std::vector<uint8_t> curb;
  h.serialize(curb);
  check(curb == cur, "v3 round trip is not byte-identical");
  check(summary(h) == summary(g), "v3 reload restores different state");
  printf("v3 save of the v1 game: %zu bytes (v1 %zu)\n", cur.size(), v1.size());

  // 3b. the v2 fixture (generator v2): loads with its values and its world; re-saved and cut back to v2 it is the
  //     same bytes, which proves v3 only appended the character block
  {
    std::vector<uint8_t> v2;
    if (!readFile(dir + "/save_v2.bin", v2)) { printf("FAIL: cannot read %s/save_v2.bin\n", dir.c_str()); bad++; }
    else {
      Game a(1);
      check(a.deserialize(v2), "v2 fixture did not load");
      printf("v2 load: %s\n", summary(a).c_str());
      check(a.seed == FIX2_SEED && a.world.genVersion == WORLDGEN_V2 && !a.worldChanged, "v2 fixture seed / generator / fingerprint");
      check(a.plLevel == 2 && a.gold == 198 && a.quests.size() == 2 && a.inv.size() == 8 && a.looted.size() == 1 && a.kills == 14,
            "v2 level/gold/quests/inventory/looted/kills");
      check(a.inside && a.subSite == 24 && std::fabs(a.pl().p.x - 971.264f) < 0.01f && std::fabs(a.pl().p.y - 723.546f) < 0.01f,
            "v2 position / inside the cave");
      const Quest* b2 = a.questById(2);
      check(b2 && b2->type == QType::Bounty && b2->state == QState::Complete && b2->giverSite == 9 && b2->giverBldg == 91, "v2 bounty quest");
      const World& w = a.world;
      bool same = w.sites.size() == 57 && w.over.bldgs.size() == 161 && w.gates.size() == 8 && w.startSite == 9 && w.capital == 0 &&
                  w.lair == 56 && siteHash(w) == 0xD05AD85Fu && bldgHash(w) == 0xF21F6A27u && groundHash(w) == 0xD449AD59u &&
                  w.sites[w.startSite].name == "RAVENGATE" && !w.dens.empty();
      if (!same)
        printf("FAIL: v2 world differs from the generator-v2 world: sites %zu bldgs %zu gates %zu hashes %08X %08X %08X\n", w.sites.size(),
               w.over.bldgs.size(), w.gates.size(), siteHash(w), bldgHash(w), groundHash(w));
      if (!same) bad++;
      check(!a.app.created && a.background == Background::None && a.storyFlags == 0 && a.eqGloves < 0, "v2 save did not get the default character block");
      std::vector<uint8_t> re;
      a.serialize(re);
      check(asV2(re) == v2, "v2 fixture re-saved and cut back to v2 is not byte-identical");
    }
  }

  // 3c. the v3 fixture: a created character with a background, story flags and gloves/boots/cloak equipped
  {
    std::vector<uint8_t> v3;
    if (!readFile(dir + "/save_v3.bin", v3)) { printf("FAIL: cannot read %s/save_v3.bin\n", dir.c_str()); bad++; }
    else {
      Game a(1);
      check(a.deserialize(v3), "v3 fixture did not load");
      printf("v3 load: %s\n", summary(a).c_str());
      check(a.seed == FIX3_SEED && a.world.genVersion == WORLDGEN_V2 && !a.worldChanged, "v3 fixture seed / generator / fingerprint");
      check(a.inside && a.subSite == 18 && a.quests.size() == 2 && a.inv.size() == 7 && a.looted.size() == 1, "v3 position/quests/inventory");
      const World& w = a.world;
      bool same = w.sites.size() == 49 && w.over.bldgs.size() == 158 && w.gates.size() == 12 && siteHash(w) == 0x127A1874u &&
                  bldgHash(w) == 0x10D35626u && groundHash(w) == 0x303FDFA5u && w.sites[w.startSite].name == "STONEDALE";
      if (!same) { printf("FAIL: v3 fixture world differs: hashes %08X %08X %08X\n", siteHash(w), bldgHash(w), groundHash(w)); bad++; }
      const Appearance& ap = a.app;
      check(ap.name == FIX3_NAME && ap.female && ap.build == 1 && ap.skinTone == 5 && ap.skin == FIX3_SKIN &&
                ap.hair == (uint8_t)art::Hair::Braids && ap.hairColor == FIX3_HAIRC && !ap.beard && ap.eyeColor == FIX3_EYE &&
                ap.topColor == FIX3_TOP && ap.bottomColor == FIX3_BOTTOM && ap.created,
            "v3 appearance");
      check(a.background == Background::Hunter && a.storyFlags == FIX3_FLAGS, "v3 background / story flags");
      auto wears = [&](int idx, ItemKind k, const char* name) {
        return idx >= 0 && idx < (int)a.inv.size() && a.inv[idx].kind == k && a.inv[idx].name == name;
      };
      check(wears(a.eqGloves, ItemKind::Gloves, "LEATHER GLOVES") && wears(a.eqBoots, ItemKind::Boots, "LEATHER BOOTS") &&
                wears(a.eqCloak, ItemKind::Cloak, "HUNTER'S CLOAK"),
            "v3 gloves/boots/cloak slots");
      check(a.pl().look.skin == FIX3_SKIN && a.pl().look.hairColor == FIX3_HAIRC, "v3 appearance did not reach the player's look");
      std::vector<uint8_t> re;
      a.serialize(re);
      check(re == v3, "v3 fixture round trip is not byte-identical");
      // the appearance block is length-prefixed: a longer block (fields from a later build) is skipped...
      size_t tail = v3TailSize(v3);
      check(tail > 0, "v3 tail not found");
      if (tail > 0) {
        size_t lenAt = v3.size() - (tail - 17);
        int blen = v3[lenAt] | (v3[lenAt + 1] << 8);
        std::vector<uint8_t> longer = v3;
        longer.push_back(0xAB); longer.push_back(0xCD); longer.push_back(0xEF);
        longer[lenAt] = (uint8_t)(blen + 3); longer[lenAt + 1] = (uint8_t)((blen + 3) >> 8);
        Game b(1);
        check(b.deserialize(longer) && b.app.name == FIX3_NAME && b.app.created && b.background == Background::Hunter,
              "a longer appearance block (newer build) did not load");
        // ...and a shorter one (an older build with fewer fields) loads with defaults for the missing fields
        std::vector<uint8_t> shorter(v3.begin(), v3.end() - 1);
        shorter[lenAt] = (uint8_t)(blen - 1); shorter[lenAt + 1] = (uint8_t)((blen - 1) >> 8);
        Game c(1);
        check(c.deserialize(shorter) && c.app.name == FIX3_NAME && !c.app.created && c.app.bottomColor == FIX3_BOTTOM,
              "a shorter appearance block (older build) did not load with defaults");
      }
    }
  }

  // 3d. worlds from generators v3, v4, v5 and v6 (SAVE_VER 3 saves): they load on their own generator with no change, and the
  //     regenerated world (sites, buildings, ground, walls, gates, openings and props) is exactly the locked one
  {
    struct GenFix { const char* file; uint64_t seed; int gen; GenLock lock; };
    const GenFix fx[] = {
        {"save_v3_gen3.bin", FIXG3_SEED, WORLDGEN_V3, GENLOCK_V3},
        {"save_v3_gen4.bin", FIXG4_SEED, WORLDGEN_V4, GENLOCK_V4},
        {"save_v3_gen5.bin", FIXG5_SEED, WORLDGEN_V5, GENLOCK_V5},
        {"save_v3_gen6.bin", FIXG6_SEED, WORLDGEN_V6, GENLOCK_V6},
    };
    for (const GenFix& f : fx) {
      std::vector<uint8_t> b;
      if (!readFile(dir + "/" + f.file, b)) { printf("FAIL: cannot read %s/%s\n", dir.c_str(), f.file); bad++; continue; }
      Game a(1);
      if (!a.deserialize(b)) { printf("FAIL: %s did not load\n", f.file); bad++; continue; }
      printf("%s load: %s\n", f.file, summary(a).c_str());
      if (a.seed != f.seed || a.world.genVersion != f.gen || a.worldChanged) { printf("FAIL: %s seed / generator / fingerprint\n", f.file); bad++; }
      // the lock is taken on a freshly generated world (a played save has opened chests on the prop layer)
      World w;
      w.generate(f.seed, f.gen);
      if (w.fingerprint() != a.world.fingerprint() || groundHash(w) != groundHash(a.world)) { printf("FAIL: %s: loaded world differs from its generator\n", f.file); bad++; }
      const GenLock& L = f.lock;
      bool same = (int)w.sites.size() == L.sites && (int)w.over.bldgs.size() == L.bldgs && (int)w.gates.size() == L.gates &&
                  (int)w.wallGaps.size() == L.gaps && siteHash(w) == L.site && bldgHash(w) == L.bldg && groundHash(w) == L.ground &&
                  gateHash(w) == L.gate;
      if (!same) {
        printf("FAIL: %s: generator v%d output drifted: {%zu, %zu, %zu, %zu, 0x%08X, 0x%08X, 0x%08X, 0x%08X}\n", f.file, f.gen, w.sites.size(),
               w.over.bldgs.size(), w.gates.size(), w.wallGaps.size(), siteHash(w), bldgHash(w), groundHash(w), gateHash(w));
        bad++;
      }
      std::vector<uint8_t> re;
      a.serialize(re);
      check(re == b, "gen-v3..v6 fixture round trip is not byte-identical");
    }
  }

  // 4. new games use the latest generator and round-trip it
  {
    Game n(7);
    n.newGame(7);
    check(n.world.genVersion == WORLDGEN_LATEST, "new game is not on WORLDGEN_LATEST");
    std::vector<uint8_t> b, b2;
    n.serialize(b);
    Game m(1);
    check(m.deserialize(b) && m.world.genVersion == WORLDGEN_LATEST && !m.worldChanged, "new-game save reload");
    m.serialize(b2);
    check(b == b2, "new-game round trip is not byte-identical");
  }

  // 5. rejects: newer save version, newer generator, bad magic, truncation; a mismatching fingerprint is flagged
  {
    auto patched = [&](size_t at, uint32_t v) {
      std::vector<uint8_t> b = cur;
      for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k));
      return b;
    };
    Game x(1);
    check(!x.deserialize(patched(4, 99)), "accepted a save from a newer format");
    check(!x.deserialize(patched(8, (uint32_t)WORLDGEN_LATEST + 1)), "accepted a world from a newer generator");
    check(!x.deserialize(patched(0, 0x12345678u)), "accepted bad magic");
    const size_t cuts[] = {6, 14, cur.size() / 2, cur.size() - 1};
    for (size_t cut : cuts) {
      std::vector<uint8_t> b(cur.begin(), cur.begin() + (long)cut);
      if (x.deserialize(b)) { printf("FAIL: accepted a save truncated to %zu bytes\n", cut); bad++; }
    }
    std::vector<uint8_t> t(v1.begin(), v1.end() - 1);
    check(!x.deserialize(t), "accepted a truncated v1 save");
    Game y(1);
    check(y.deserialize(patched(12, ~h.world.fingerprint())) && y.worldChanged, "a mismatching world fingerprint was not flagged");
  }

  // 6. feature sub-seeds: deterministic, independent per feature and per world seed
  check(genSubSeed(FIX_SEED, "dens") == genSubSeed(FIX_SEED, "dens"), "genSubSeed not deterministic");
  check(genSubSeed(FIX_SEED, "dens") != genSubSeed(FIX_SEED, "roads") && genSubSeed(FIX_SEED, "dens") != genSubSeed(FIX_SEED + 1, "dens"), "genSubSeed collisions");
  // sites and buildings carry their world's generator version (sub-level generators gate on it)
  bool tagged = true;
  for (auto& s : w1.sites) tagged = tagged && s.genVer == WORLDGEN_V1;
  for (auto& b : w1.over.bldgs) tagged = tagged && b.genVer == WORLDGEN_V1;
  check(tagged, "sites/buildings not tagged with the generator version");

  printf(bad ? "FAILED (%d)\n" : "ALL OK\n", bad);
  return bad ? 1 : 0;
}
