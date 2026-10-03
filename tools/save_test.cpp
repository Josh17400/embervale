// Save-format and world-gen versioning checks.
//   save_test [fixtureDir]            load tests/fixtures/save_v1.bin, check its values and its regenerated world, round-trip v2
//   save_test --make-fixture out.bin  play a fixed seed with the bot and write a save in the CURRENT format
//                                     (run once with SAVE_VER 1 code to create save_v1.bin; do not overwrite it)
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

int makeFixture(const char* out) {
  Game g(FIX_SEED);
  g.newGame(FIX_SEED);
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
  bot(g, 150, FIX_SEED);
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
    bot(g, 60, FIX_SEED + 1);
  }
  g.mode = Mode::Play;
  std::vector<uint8_t> buf;
  g.serialize(buf);
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
  w.generate(FIX_SEED);
  printf("constexpr int FIX_SITES = %zu, FIX_BLDGS = %zu, FIX_GATES = %zu, FIX_START = %d, FIX_CAPITAL = %d, FIX_LAIR = %d;\n",
         w.sites.size(), w.over.bldgs.size(), w.gates.size(), w.startSite, w.capital, w.lair);
  printf("constexpr uint32_t FIX_SITEHASH = 0x%08X, FIX_BLDGHASH = 0x%08X, FIX_GROUNDHASH = 0x%08X;\n", siteHash(w), bldgHash(w), groundHash(w));
  printf("const char* const FIX_START_NAME = \"%s\";\n", w.sites[w.startSite].name.c_str());
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

  // 3. v2 round trip
  std::vector<uint8_t> v2;
  g.serialize(v2);
  BinR hr(v2);
  uint32_t magic = hr.u32(), ver = hr.u32(), gv = hr.u32();
  check(magic == 0x454D4256 && ver == 2 && (int)gv == WORLDGEN_V1, "v2 header (magic, version, world-gen version)");
  Game h(1);
  check(h.deserialize(v2), "v2 save did not load");
  check(!h.worldChanged, "v2 reload flagged a changed world");
  std::vector<uint8_t> v2b;
  h.serialize(v2b);
  check(v2b == v2, "v2 round trip is not byte-identical");
  check(summary(h) == summary(g), "v2 reload restores different state");
  printf("v2 save: %zu bytes (v1 %zu)\n", v2.size(), v1.size());

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
      std::vector<uint8_t> b = v2;
      for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k));
      return b;
    };
    Game x(1);
    check(!x.deserialize(patched(4, 99)), "accepted a save from a newer format");
    check(!x.deserialize(patched(8, (uint32_t)WORLDGEN_LATEST + 1)), "accepted a world from a newer generator");
    check(!x.deserialize(patched(0, 0x12345678u)), "accepted bad magic");
    const size_t cuts[] = {6, 14, v2.size() / 2, v2.size() - 1};
    for (size_t cut : cuts) {
      std::vector<uint8_t> b(v2.begin(), v2.begin() + (long)cut);
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
