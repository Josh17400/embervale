// Save-format checks for the CURRENT save version only (owner, 2026-10-04: old saves are not a concern; an older save
// is refused and the title offers a new game). The SIM lane owns this file during M1.
//   save_test [fixtureDir]           run every check
//   save_test --make-fixture out.bin write tests/fixtures/save_v5.bin: an endless game (seed 5150) with a created
//                                    character, the innkeeper's job taken, bot play, a looted chest, saved outdoors.
//                                    Regenerate it whenever the v5 layout changes on purpose (before M1 ships), and
//                                    paste the printed FIX5 line below.
// What is checked:
//   1. the fixture loads and shows the values it was made with (format-level facts only: player, inventory, character,
//      quests, flags; never generator output, so generator work does not invalidate it)
//   2. round trips are byte-identical: endless games after play, after a long walk (many window shifts), upstairs in an
//      inn with a rented room; a classic island game
//   3. older, newer and damaged files are refused, and saveVersion() reports what they are
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

namespace {

int bad = 0;
void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); bad++; }
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

// ---- the character every check uses
constexpr uint32_t FIX_SKIN = 0xFF4A6E96u, FIX_HAIRC = 0xFF2A62D2u, FIX_EYE = 0xFF3C8C30u, FIX_TOP = 0xFF283C8Cu, FIX_BOTTOM = 0xFF203040u;
constexpr uint32_t FIX_FLAGS = SF_CREATED | SF_FIRST_WEAPON | (1u << 9);
const char* const FIX_NAME = "BRYNJA";
Item fixtureWear(ItemKind k, const char* name, art::Icon icon, int power) {
  Item it;
  it.kind = k; it.name = name; it.icon = icon; it.power = (int16_t)power; it.value = power * 10; it.tint = tierTint(0);
  return it;
}
void makeCharacter(Game& g) {
  g.app.name = FIX_NAME; g.app.female = true; g.app.build = 1; g.app.skinTone = 5; g.app.skin = FIX_SKIN;
  g.app.hair = (uint8_t)art::Hair::Braids; g.app.hairColor = FIX_HAIRC; g.app.beard = false; g.app.eyeColor = FIX_EYE;
  g.app.topColor = FIX_TOP; g.app.bottomColor = FIX_BOTTOM; g.app.created = true;
  g.background = Background::Hunter;
  g.storyFlags = FIX_FLAGS;
  g.inv.push_back(fixtureWear(ItemKind::Gloves, "LEATHER GLOVES", art::Icon::Gloves, 2));
  g.inv.push_back(fixtureWear(ItemKind::Boots, "LEATHER BOOTS", art::Icon::Boots, 3));
  g.inv.push_back(fixtureWear(ItemKind::Cloak, "HUNTER'S CLOAK", art::Icon::Armor, 1));
  int n = (int)g.inv.size();
  g.useItem(n - 3); g.useItem(n - 2); g.useItem(n - 1);
}

int startInn(Game& g) {
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++)
    if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) return b;
  return -1;
}

// take the start innkeeper's job, walk back out, play, loot the nearest overworld chest
void play(Game& g, uint64_t seed, float botSecs) {
  g.mode = Mode::Play;
  g.godMode = true;
  int inn = startInn(g);
  if (inn >= 0) {
    const Bldg B = g.world.over.bldgs[(size_t)inn];
    g.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
    g.update(SIM_DT, Input());
    for (size_t k = 1; k < g.actors.size(); k++)
      if (g.actors[k].role == Role::Innkeeper) {
        g.pl().p = g.actors[k].p + Vec2(0, 20);
        Input t; t.interact = true;
        g.update(SIM_DT, t);
        for (size_t o = 0; o < g.dlg.opts.size(); o++)
          if (g.dlg.opts[o].label.find("WORK") != std::string::npos) { g.dialogueChoose((int)o); g.dialogueChoose(0); break; }
        break;
      }
    g.mode = Mode::Play;
  }
  if (g.inside) {
    g.pl().p = Vec2(g.sub.exitX * 16 + 8.0f, (g.sub.exitY - 2) * 16 + 8.0f);
    Input down; down.move = Vec2(0, 1);
    for (int f = 0; f < 120 && g.inside; f++) g.update(SIM_DT, down);
  }
  bot(g, botSecs, seed);
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
    }
  }
  g.mode = Mode::Play;
}

// what a reload must restore (world-independent facts plus where the player stands)
std::string summary(const Game& g) {
  char b[512];
  snprintf(b, sizeof b, "seed %llu lvl %d xp %d gold %d inv %zu quests %zu looted %zu kills %d inside %d floor %d pos %.2f,%.2f win %d,%d name %s bg %d flags %u",
           (unsigned long long)g.seed, g.plLevel, g.plXp, g.gold, g.inv.size(), g.quests.size(), g.looted.size(), g.kills, g.inside ? 1 : 0, g.subFloor,
           g.pl().p.x, g.pl().p.y, g.world.ox, g.world.oy, g.app.name.c_str(), (int)g.background, g.storyFlags);
  std::string s = b;
  for (const Quest& q : g.quests) s += " | " + q.title + ":" + std::to_string((int)q.state) + ":" + std::to_string(q.have);
  return s;
}

// save, load into a fresh game, save again: identical bytes and the same state
void roundTrip(Game& g, const char* label) {
  std::vector<uint8_t> a, b;
  g.serialize(a);
  Game h(1);
  std::string l = label;
  check(h.deserialize(a), (l + ": save did not load").c_str());
  h.serialize(b);
  check(a == b, (l + ": round trip is not byte-identical").c_str());
  check(summary(h) == summary(g), (l + ": reload restores different state").c_str());
  if (summary(h) != summary(g)) printf("  saved:    %s\n  reloaded: %s\n", summary(g).c_str(), summary(h).c_str());
  printf("%s: %zu bytes\n", label, a.size());
}

// ---- the fixture
constexpr uint64_t FIX_SEED = 5150;
int makeFixture(const char* out) {
  Game g(FIX_SEED);
  g.newEndlessGame(FIX_SEED);
  makeCharacter(g);
  play(g, FIX_SEED, 90);
  if (g.inside) { printf("the fixture must be saved outdoors\n"); return 1; }
  std::vector<uint8_t> buf;
  g.serialize(buf);
  FILE* f = fopen(out, "wb");
  if (!f) { printf("cannot write %s\n", out); return 1; }
  fwrite(buf.data(), 1, buf.size(), f);
  fclose(f);
  printf("wrote %s (%zu bytes)\n", out, buf.size());
  printf("constexpr Fix5 FIX5 = {%d, %d, %d, %zu, %zu, %zu, %d, %d, %.3ff, %.3ff, %d, %.3ff, %d};\n", g.plLevel, g.plXp, g.gold, g.inv.size(),
         g.quests.size(), g.looted.size(), g.kills, g.world.ox, g.pl().p.x, g.pl().p.y, g.world.oy, g.hour, g.day);
  return 0;
}
// values printed by --make-fixture (format-level facts only)
struct Fix5 { int level, xp, gold; size_t inv, quests, looted; int kills, ox; float px, py; int oy; float hour; int day; };
constexpr Fix5 FIX5 = {1, 20, 30, 7, 3, 1, 0, -192, 1432.000f, 2776.000f, -256, 11.081f, 1};

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture")) return makeFixture(argv[2]);
#ifdef EMB_SOURCE_DIR
  std::string dir = argc >= 2 ? argv[1] : std::string(EMB_SOURCE_DIR) + "/tests/fixtures";
#else
  std::string dir = argc >= 2 ? argv[1] : "tests/fixtures";
#endif
  printf("save_test: SAVE_VER %d\n", Game::currentSaveVersion());

  // ---- 1. the fixture
  std::vector<uint8_t> fx;
  if (!readFile(dir + "/save_v5.bin", fx)) check(false, "cannot read tests/fixtures/save_v5.bin");
  else {
    check(Game::saveVersion(fx) == Game::currentSaveVersion(), "fixture version is not the current SAVE_VER (regenerate it)");
    Game g(1);
    check(g.deserialize(fx), "fixture did not load");
    check(g.seed == FIX_SEED && g.world.endless, "fixture seed / world kind");
    check(g.plLevel == FIX5.level && g.plXp == FIX5.xp && g.gold == FIX5.gold && g.inv.size() == FIX5.inv && g.quests.size() == FIX5.quests &&
              g.looted.size() == FIX5.looted && g.kills == FIX5.kills, "fixture level / xp / gold / inventory / quests / looted / kills");
    check(g.world.ox == FIX5.ox && g.world.oy == FIX5.oy && std::fabs(g.pl().p.x - FIX5.px) < 0.01f && std::fabs(g.pl().p.y - FIX5.py) < 0.01f,
          "fixture window origin / position");
    check(std::fabs(g.hour - FIX5.hour) < 0.01f && g.day == FIX5.day, "fixture time of day");
    check(g.app.name == FIX_NAME && g.app.female && g.app.skin == FIX_SKIN && g.app.hairColor == FIX_HAIRC && g.app.eyeColor == FIX_EYE &&
              g.app.topColor == FIX_TOP && g.app.bottomColor == FIX_BOTTOM && g.app.created, "fixture appearance");
    check(g.background == Background::Hunter && g.storyFlags == FIX_FLAGS, "fixture background / story flags");
    auto wears = [&](int e, ItemKind k, const char* nm) { return e >= 0 && e < (int)g.inv.size() && g.inv[(size_t)e].kind == k && g.inv[(size_t)e].name == nm; };
    check(wears(g.eqGloves, ItemKind::Gloves, "LEATHER GLOVES") && wears(g.eqBoots, ItemKind::Boots, "LEATHER BOOTS") &&
              wears(g.eqCloak, ItemKind::Cloak, "HUNTER'S CLOAK"), "fixture equipment");
    bool main = false, blade = false;
    for (const Quest& q : g.quests) { if (q.type == QType::Main) main = true; if (q.title == "A BLADE OF YOUR OWN") blade = true; }
    check(main && blade, "fixture quests (the main quest and the opening)");
  }

  // ---- 2. round trips
  for (uint64_t s : {5150ull, 7ull, 99ull}) {
    Game g(s);
    g.newEndlessGame(s);
    makeCharacter(g);
    play(g, s, 45);
    roundTrip(g, ("endless seed " + std::to_string(s) + " after play").c_str());
    // a long walk east (many window shifts), then save far from home
    g.noWildSpawns = true;
    for (int t = 0; t < 500; t += 2) { g.pl().p += Vec2(2.0f * TILE, 0); g.update(SIM_DT, Input()); }
    roundTrip(g, ("endless seed " + std::to_string(s) + " far east").c_str());
  }
  {
    Game g(707);
    g.newEndlessGame(707);
    makeCharacter(g);
    g.mode = Mode::Play;
    int inn = startInn(g);
    check(inn >= 0, "endless seed 707: no inn in the start village");
    if (inn >= 0 && g.world.over.bldgs[(size_t)inn].floors() >= 2 && g.debugEnterBuilding(inn, 1)) {
      int room = -1;
      for (size_t i = 0; i < g.sub.rooms.size() && room < 0; i++) if (g.sub.rooms[i].kind == RoomKind::GuestRoom) room = (int)i;
      g.lodging.bldg = inn; g.lodging.floor = 1; g.lodging.room = room; g.lodging.untilDay = 2;
      roundTrip(g, "endless upstairs in the inn");
      std::vector<uint8_t> a;
      g.serialize(a);
      Game h(1);
      if (h.deserialize(a)) check(h.inside && h.subFloor == 1 && h.lodging.floor == 1 && h.lodging.room == room && h.lodging.untilDay == 2 &&
                                      h.world.over.bldgs[(size_t)h.lodging.bldg].id == g.world.over.bldgs[(size_t)inn].id,
                                  "upstairs / rented room not restored");
    } else check(false, "endless seed 707: cannot go upstairs in the start inn");
  }
  {
    Game g(5150);
    g.newGame(5150, WORLDGEN_LATEST);
    makeCharacter(g);
    play(g, 5150, 45);
    roundTrip(g, "classic island seed 5150");
  }

  // ---- 2b. the save soak (VISION_PLAN 3.3, M1 size budget: about 150 KB after long play): a long journey on the
  //      endless world (20 000 tiles walked, which marks the fog of war across many regions), every overworld chest met
  //      looted, every camp's people killed, every site met discovered, 80 quests in the journal. The save must stay
  //      within the budget, write fast, and round-trip byte-identically.
  {
    Game g(4242);
    g.newEndlessGame(4242);
    makeCharacter(g);
    g.mode = Mode::Play;
    g.godMode = true;
    g.noWildSpawns = true;
    const int legs[4][2] = {{1, 0}, {0, 1}, {1, 0}, {0, -1}};
    int walked = 0, chests = 0;
    for (int leg = 0; walked < 20000; leg = (leg + 1) % 4) {
      for (int t = 0; t < 1250 && walked < 20000; t += 2, walked += 2) {
        g.pl().p += Vec2(legs[leg][0] * 2.0f * TILE, legs[leg][1] * 2.0f * TILE);
        g.update(SIM_DT, Input());
        g.events.clear();
        g.mode = Mode::Play;
        if (g.inside) { g.inside = false; g.sub = Map(); g.subBldg = -1; g.subSite = -1; }
      }
      // what the window holds now: its chests looted, its camps' people killed, its sites discovered
      const Map& m = g.world.over;
      for (int i = 0; i < m.w * m.h; i++)
        if (m.prop[(size_t)i] == (int)art::Prop::Chest + 1) { g.looted.insert(g.lootKey(i % m.w, i / m.w)); chests++; }
      for (const Spawn& sp : m.spawns)
        if (!sp.npc && sp.site >= 0) g.killedSlots[0].insert(sp.site * 4096 + sp.slot);
      for (Site& s : g.world.sites) if (m.in(s.ex, s.ey)) s.discovered = true;
    }
    Quest tpl;
    for (const Quest& q : g.quests) if (q.type != QType::Main) tpl = q;
    for (int k = 0; k < 80; k++) {
      Quest q = tpl;
      q.id = g.nextQuestId++;
      q.type = QType::Clear; q.state = QState::Done;
      q.target = k % (int)g.world.sites.size();
      q.giverSite = (k * 7) % (int)g.world.sites.size();
      q.title = "CLEAR " + g.world.sites[(size_t)q.target].name;
      q.desc = "SOMETHING FOUL HAS MADE ITS NEST IN " + g.world.sites[(size_t)q.target].name + ". KILL WHATEVER LEADS THEM AND THE ROADS WILL BE SAFER.";
      g.quests.push_back(q);
      g.npcQuestsDone[(uint64_t)k * 0x9E3779B97F4A7C15ull]++;
    }
    std::vector<uint8_t> a;
    auto t0 = std::chrono::steady_clock::now();
    g.serialize(a);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("save soak: %d tiles walked, %zu fog regions, %d chests looted, %zu sites, %zu quests: save %zu bytes (%.1f KB), written in %.2f ms\n",
           walked, g.explored.regions.size(), chests, g.world.sites.size(), g.quests.size(), a.size(), a.size() / 1024.0, ms);
    check(a.size() <= 150 * 1024, "save soak: the save is over the 150 KB budget");
    check(ms < 50.0, "save soak: writing the save took too long");
    roundTrip(g, "endless save soak");
  }

  // ---- 3. refusals
  {
    Game n(31);
    n.newEndlessGame(31);
    n.mode = Mode::Play;
    std::vector<uint8_t> b;
    n.serialize(b);
    auto patched = [&](size_t at, uint32_t v) { std::vector<uint8_t> c = b; for (int k = 0; k < 4; k++) c[at + k] = (uint8_t)(v >> (8 * k)); return c; };
    Game x(1);
    check(!x.deserialize(patched(4, 4)) && Game::saveVersion(patched(4, 4)) == 4, "accepted (or misread) a SAVE_VER 4 save");
    check(!x.deserialize(patched(4, 1)), "accepted a SAVE_VER 1 save");
    check(!x.deserialize(patched(4, 99)), "accepted a save from a newer format");
    check(!x.deserialize(patched(0, 0x12345678u)) && Game::saveVersion(patched(0, 0x12345678u)) == 0, "accepted bad magic");
    std::vector<uint8_t> cut(b.begin(), b.begin() + (std::ptrdiff_t)(b.size() / 2));
    check(!x.deserialize(cut), "accepted a truncated save");
    std::vector<uint8_t> genv = b;
    genv[9] = (uint8_t)(genv[9] + 1);   // the endless generator version (u32 after the u8 world kind)
    check(!x.deserialize(genv), "accepted a save from another endless generator");
  }
  // ---- 4. damaged saves never crash (unknown or missing ids, counts and values out of range): each either loads into
  //      a playable game or is refused. Bytes after the header are flipped at random, and the file is cut short.
  {
    Game n(5150);
    n.newEndlessGame(5150);
    makeCharacter(n);
    play(n, 5150, 20);
    std::vector<uint8_t> b;
    n.serialize(b);
    Rng r(77);
    int loaded = 0, refused = 0;
    for (int it = 0; it < 48; it++) {
      std::vector<uint8_t> c = b;
      int flips = 1 + r.irange(4);
      for (int k = 0; k < flips; k++) c[(size_t)(32 + r.irange((int)c.size() - 32))] ^= (uint8_t)(1u << r.irange(8));
      Game x(1);
      if (x.deserialize(c)) {
        loaded++;
        x.mode = Mode::Play;
        x.godMode = true;
        std::vector<uint8_t> again;
        x.serialize(again);
        for (int f = 0; f < 30; f++) { x.update(SIM_DT, Input()); x.events.clear(); if (x.mode != Mode::Play) x.mode = Mode::Play; }
        for (const Quest& q : x.quests) (void)x.questStatus(q);
        check(x.map().in((int)(x.pl().p.x / 16), (int)(x.pl().p.y / 16)), "a damaged save put the player off the map");
      } else refused++;
    }
    for (int k = 1; k < 16; k++) {
      std::vector<uint8_t> cut(b.begin(), b.begin() + (std::ptrdiff_t)(b.size() * k / 16));
      Game x(1);
      check(!x.deserialize(cut), "accepted a save cut short");
    }
    printf("damaged saves: %d loaded and played, %d refused, 15 cut short refused, none crashed\n", loaded, refused);
  }

  printf(bad ? "save_test: FAILED (%d)\n" : "save_test: ALL OK\n", bad);
  return bad ? 1 : 0;
}
