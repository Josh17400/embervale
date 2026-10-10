// Save-format checks for the CURRENT save version only (owner, 2026-10-04: old saves are not a concern; an older save
// is refused and the title offers a new game). M7: SAVE_VER 14 (the home block after the craft block: a lot with a cottage
// in the local style, a field, a coop, a pen and a stable, chickens, a cow and a horse ridden, a chest with a harvest;
// ENDLESS_GEN_VER 16). M6b: SAVE_VER 13 (the story block v4: the sagas' repetition guard and its offers; a
// generated story running, saved by its spec id). M6: SAVE_VER 12 (items gain level, material, culture, affixes, unique power;
// the craft block after the life block; ENDLESS_GEN_VER 15). M5: SAVE_VER 11 (the layout of 10 plus the life block after the story
// block; ENDLESS_GEN_VER 14). M4: SAVE_VER 10 (the realm and story blocks after the marks). The CITIZENS lane owns this
// file and the fixture in M5.
//   save_test [fixtureDir]           run every check
//   save_test --make-fixture out.bin write tests/fixtures/save_v14.bin: an endless game (seed 5150) with a created
//                                    character, the innkeeper's job taken, bot play, a looted chest, the M2 fields
//                                    (marks, a quest's subject / flags / deadline, a rumoured site), saved outdoors.
//                                    Regenerate it whenever the layout changes on purpose, and paste the printed FIX6
//                                    line below.
// What is checked:
//   1. the fixture loads and shows the values it was made with (format-level facts only: player, inventory, character,
//      quests, flags; never generator output, so generator work does not invalidate it)
//   2. round trips are byte-identical: endless games after play, after a long walk (many window shifts), upstairs in an
//      inn with a rented room (M2 retired the classic island)
//   3. older, newer and damaged files are refused, and saveVersion() reports what they are
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

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
// M3 (SAVE_VER 7) appearance facts
constexpr uint64_t FIX_HOMELAND = 0x2001E0050003FFFEull;
cult::Heraldry fixHeraldry() {
  cult::Heraldry h;
  h.field = 0xFF7A2A1Eu; h.field2 = 0xFFE8D8A0u; h.charge = 0xFF30C8F0u; h.division = 3; h.chargeKind = 1; h.emblem = 5;
  h.glyphSeed = 0xC0FFEE11u; h.shape = 2;
  return h;
}
Item fixtureWear(ItemKind k, const char* name, art::Icon icon, int power) {
  Item it;
  it.kind = k; it.name = name; it.icon = icon; it.power = (int16_t)power; it.value = power * 10; it.tint = tierTint(0);
  return it;
}
void makeCharacter(Game& g) {
  g.app.name = FIX_NAME; g.app.female = true; g.app.build = 1; g.app.skinTone = 5; g.app.skin = FIX_SKIN;
  g.app.hair = (uint8_t)art::Hair::Braids; g.app.hairColor = FIX_HAIRC; g.app.beard = false; g.app.eyeColor = FIX_EYE;
  g.app.topColor = FIX_TOP; g.app.bottomColor = FIX_BOTTOM; g.app.created = true;
  // M3 (SAVE_VER 7): an elf from a far homeland with her own arms
  g.app.people = 2; g.app.homeland = FIX_HOMELAND; g.app.heraldry = fixHeraldry();
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
// M2 (SAVE_VER 6) fields the fixture carries: a mark, a quest's subject / flags / deadline, a rumoured site
constexpr uint64_t FIX_MARK_KEY = 0xE2000000000000A5ull;
constexpr int32_t FIX_MARK_VAL = 7;
const char* const FIX_SUBJECT = "GRANDMOTHER'S SILVER RING";
constexpr uint32_t FIX_QFLAGS = 5;
constexpr int FIX_DEADLINE = 3;
void addM2Facts(Game& g) {
  g.marks[FIX_MARK_KEY] = FIX_MARK_VAL;
  for (Quest& q : g.quests)
    if (q.type != QType::Main) { q.subject = FIX_SUBJECT; q.flags = FIX_QFLAGS; q.deadlineDay = FIX_DEADLINE; break; }
  if (g.world.capital >= 0 && g.world.capital != g.world.startSite) g.world.sites[(size_t)g.world.capital].rumoured = true;
}
// M4 (SAVE_VER 10) facts in the realm block: a famine forced on the start village, the player's standing with its kingdom
constexpr int FIX_REP = 12;
void addM4Facts(Game& g) {
  const Site& sv = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid k = sv.kingdom >= 0 ? g.world.kingdoms[(size_t)sv.kingdom].id : 0;
  g.realm.noteSite(sv.id, k, (uint8_t)sv.type, g.world.ox + sv.ex, g.world.oy + sv.ey);
  g.realm.forceFamine(sv.id, g.day);
  g.realm.addRep(k, FIX_REP);
}
// M5 (SAVE_VER 11) facts in the life block: the player Well Fed and Rested, a friend among the start village's people
constexpr float FIX_FED = 3.5f, FIX_RESTED = 6.0f;
constexpr int FIX_FRIEND = 1;   // the census index befriended
constexpr int FIX_DEAD = 0;     // (CITIZENS lane) the census index who died: their household grieves
constexpr int FIX_FAMINE_DAYS = 4;
void addM5Facts(Game& g) {
  g.life.player.fedH = FIX_FED;
  g.life.player.restedH = FIX_RESTED;
  const ew::Gid sv = g.world.sites[(size_t)g.world.startSite].id;
  if (g.life.census(g.world, g.world.startSite)) {
    g.life.befriend(sv, FIX_FRIEND);
    g.life.residentDied(sv, FIX_DEAD, g.day);
    g.life.forceFamine(sv, g.day, FIX_FAMINE_DAYS);
  }
}
// M6 (SAVE_VER 12): a legendary culture spear with affixes, a stack of ore, the smith's knowledge
constexpr uint64_t FIX_CULT = (1ull << 60) | (3ull << 46) | (5ull << 32);   // cult::familyId(3, 5)
constexpr int FIX_SKILL = 237, FIX_SECRET = 60, FIX_TRUST = 42, FIX_ORE = 7;
constexpr uint64_t FIX_SMITH = 0xABCDEF12345ull;
void addM6Facts(Game& g) {
  Rng r(77);
  Item it = gear::makeGear(r, ItemKind::Weapon, (int)WeaponType::Spear, 12, Rarity::Legendary, FIX_CULT, Mat::Steel);
  it.name = "FIXTURE SPEAR";
  it.affix[0] = {Affix::FireDmg, 8};
  it.affix[1] = {Affix::CritChance, 5};
  it.unique = Unique::ChainLightning;
  it.form = 2;
  it.flags = IF_CRAFTED;
  g.inv.push_back(it);
  g.inv.push_back(craft::makeStuff(craft::Stuff::IronOre, FIX_ORE));
  g.craft.skill = FIX_SKILL;
  g.craft.advance(FIX_CULT, 1, craft::SecretKind::AlloyRecipe, FIX_SECRET, craft::HOW_RUINS);
  g.craft.trust[FIX_SMITH] = FIX_TRUST;
}
// M6b (SAVE_VER 13, story block v4): a generated story running (started at the start village by a census resident, its
// first choice made), one told record and one offered record in the repetition guard
const char* const FIX_SAGA = "saga1~the_pit~~~~v2~m2~0000a5a5";
constexpr int32_t FIX_SEEN_DAY = 2, FIX_SEEN_GX = 4242, FIX_SEEN_GY = -777;
constexpr uint32_t FIX_SEEN_SHAPE = 0x5A6A, FIX_OFFER_HOOK = 0x0FF3A;
void addM6bFacts(Game& g) {
  // the teller: a census resident of the start village (THE BROTHER IN THE PIT casts the giver's own kin), out in the
  // street or brought into play as the life simulation does when the player comes near
  uint32_t id = 0;
  const int ss = g.world.startSite;
  for (size_t k = 1; k < g.actors.size() && !id; k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || !a.human || a.resident < 0 || story::homeSiteOf(g, a) != ss) continue;
    id = g.story.start(g, FIX_SAGA, a.id, ss);
  }
  if (!id && story::host().spawnHuman)
    if (life::Census* C = g.life.census(g.world, ss))
      for (size_t ri = 0; ri < C->res.size() && !id; ri++) {
        const life::Resident r = C->res[ri];
        if ((r.flags & (life::RF_DEAD | life::RF_AWAY)) || r.age < 20 || (r.actor >= 0 && story::host().findActor(g, r.actor) >= 0)) continue;
        Spawn sp;
        sp.npc = true; sp.role = life::jobRole(r.job); sp.site = ss; sp.slot = 3000 + (int)ri;
        const int aid = story::host().spawnHuman(g, sp, g.pl().p.x + 24.0f, g.pl().p.y);
        const int ai = story::host().findActor(g, aid);
        if (ai < 0) continue;
        Actor& a = g.actors[(size_t)ai];
        a.site = ss; a.bldg = -1; a.slot = 3000 + (int)ri; a.resident = (int)ri; a.name = r.name; a.fromMap = false;
        id = g.story.start(g, FIX_SAGA, aid, ss);
        const int ai2 = story::host().findActor(g, aid);
        if (ai2 >= 0) g.actors.erase(g.actors.begin() + ai2);
      }
  if (id) g.story.choose(g, id, 0, true, 1);   // "I'LL GO TO ... AND LOOK.": the stage `road`
  else printf("save_test: the fixture saga %s could not be cast\n", FIX_SAGA);
  story::saga::Spec s;
  story::saga::parseSpecId(FIX_SAGA, s);
  g.story.guard_.note(s, FIX_SEEN_SHAPE, FIX_SEEN_DAY, FIX_SEEN_GX, FIX_SEEN_GY, {7u, 7u, 9u});
  s.twist[0] = "rival_claim";
  g.story.guard_.noteOffer(s, FIX_OFFER_HOOK, FIX_SEEN_DAY + 1, FIX_SEEN_GX + 10, FIX_SEEN_GY);
}
// M7 (SAVE_VER 14): the home block: a lot by the start village with a finished cottage in the local style, a field of
// barley, a coop with chickens, a pen with a cow, a stable with a horse being ridden, a chest with a harvest in it
constexpr int FIX_CROPS = 4, FIX_ANIMALS = 4, FIX_STORE = 3;
void addM7Facts(Game& g) {
  const int goldWas = g.gold;
  g.gold = 1000000;
  const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  const int pi = home::debugLot(g, px + 3, py - 6, home::LotSize::Large);
  if (pi < 0) { printf("save_test: the fixture lot was refused\n"); g.gold = goldWas; return; }
  const uint64_t cul = g.world.src->cultureAt(px, py);
  if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
  std::string why;
  if (!home::startBuild(g, pi, home::Shell::Cottage, cul, 0, 0, why)) printf("save_test: fixture build: %s\n", why.c_str());
  home::Plot& P = g.home.plots[(size_t)pi];
  P.buildDoneDay = (uint16_t)g.day;
  P.state = home::PlotState::Built;
  auto place = [&](home::Obj o) {
    std::string w;
    for (int y = 1; y < P.h; y++)
      for (int x = 1; x < P.w; x++)
        if (home::canPlaceOutside(P, o, x, y, false, w)) { home::placeObj(g, pi, o, x, y, false, false, w); return; }
    printf("save_test: no room for a %s\n", home::objInfo(o).name);
  };
  place(home::Obj::Coop);
  place(home::Obj::Pen);
  place(home::Obj::Stable);
  for (int k = 0; k < FIX_CROPS; k++) {
    home::setGround(P, 2 + k, P.h - 2, 2);
    home::CropRec c;
    c.x = (uint8_t)(2 + k); c.y = (uint8_t)(P.h - 2); c.kind = (uint8_t)home::Crop::Barley; c.stage = 2; c.grown = 3;
    c.plantedDay = (uint16_t)g.day; c.lastWaterDay = (uint16_t)g.day; c.lastGrowDay = (uint16_t)g.day;
    P.crops.push_back(c);
  }
  home::buyAnimal(g, pi, home::Animal::Chicken, why);
  home::buyAnimal(g, pi, home::Animal::Chicken, why);
  home::buyAnimal(g, pi, home::Animal::Cow, why);
  home::buyAnimal(g, pi, home::Animal::Horse, why);
  home::Plot& Q = g.home.plots[(size_t)pi];
  if (!Q.stores.empty()) Q.stores[0].items.push_back(home::makeCropItem(home::Crop::Barley, FIX_STORE, 2));
  home::mount(g, pi, (int)Q.animals.size() - 1, why);
  g.gold = goldWas;
}
int makeFixture(const char* out) {
  Game g(FIX_SEED);
  g.newEndlessGame(FIX_SEED);
  makeCharacter(g);
  play(g, FIX_SEED, 90);
  if (g.inside) g.debugLeave();   // (M7 phase B) the walk may end in a doorway: the fixture is saved outdoors
  addM2Facts(g);
  addM4Facts(g);
  addM5Facts(g);
  addM6Facts(g);
  addM6bFacts(g);
  addM7Facts(g);
  if (g.inside) { printf("the fixture must be saved outdoors\n"); return 1; }
  std::vector<uint8_t> buf;
  g.serialize(buf);
  FILE* f = fopen(out, "wb");
  if (!f) { printf("cannot write %s\n", out); return 1; }
  fwrite(buf.data(), 1, buf.size(), f);
  fclose(f);
  printf("wrote %s (%zu bytes)\n", out, buf.size());
  printf("constexpr Fix6 FIX6 = {%d, %d, %d, %zu, %zu, %zu, %d, %d, %.3ff, %.3ff, %d, %.3ff, %d};\n", g.plLevel, g.plXp, g.gold, g.inv.size(),
         g.quests.size(), g.looted.size(), g.kills, g.world.ox, g.pl().p.x, g.pl().p.y, g.world.oy, g.hour, g.day);
  return 0;
}
// values printed by --make-fixture (format-level facts only)
struct Fix6 { int level, xp, gold; size_t inv, quests, looted; int kills, ox; float px, py; int oy; float hour; int day; };
constexpr Fix6 FIX6 = {1, 20, 30, 8, 4, 1, 0, 0, 2184.000f, 2346.000f, -256, 8.867f, 1};

}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);   // (M4) progress shows as it happens (CI logs, a stalled section is visible)
  if (argc >= 3 && !strcmp(argv[1], "--make-fixture")) return makeFixture(argv[2]);
#ifdef EMB_SOURCE_DIR
  std::string dir = argc >= 2 ? argv[1] : std::string(EMB_SOURCE_DIR) + "/tests/fixtures";
#else
  std::string dir = argc >= 2 ? argv[1] : "tests/fixtures";
#endif
  printf("save_test: SAVE_VER %d\n", Game::currentSaveVersion());

  // ---- 1. the fixture
  std::vector<uint8_t> fx;
  if (!readFile(dir + "/save_v14.bin", fx)) check(false, "cannot read tests/fixtures/save_v14.bin");
  else {
    check(Game::saveVersion(fx) == Game::currentSaveVersion(), "fixture version is not the current SAVE_VER (regenerate it)");
    Game g(1);
    check(g.deserialize(fx), "fixture did not load");
    check(g.seed == FIX_SEED && g.world.endless, "fixture seed / world kind");
    check(g.plLevel == FIX6.level && g.plXp == FIX6.xp && g.gold == FIX6.gold && g.inv.size() == FIX6.inv && g.quests.size() == FIX6.quests &&
              g.looted.size() == FIX6.looted && g.kills == FIX6.kills, "fixture level / xp / gold / inventory / quests / looted / kills");
    check(g.world.ox == FIX6.ox && g.world.oy == FIX6.oy && std::fabs(g.pl().p.x - FIX6.px) < 0.01f && std::fabs(g.pl().p.y - FIX6.py) < 0.01f,
          "fixture window origin / position");
    check(std::fabs(g.hour - FIX6.hour) < 0.01f && g.day == FIX6.day, "fixture time of day");
    check(g.app.name == FIX_NAME && g.app.female && g.app.skin == FIX_SKIN && g.app.hairColor == FIX_HAIRC && g.app.eyeColor == FIX_EYE &&
              g.app.topColor == FIX_TOP && g.app.bottomColor == FIX_BOTTOM && g.app.created, "fixture appearance");
    check(g.background == Background::Hunter && g.storyFlags == FIX_FLAGS, "fixture background / story flags");
    check(g.app.people == 2 && g.app.homeland == FIX_HOMELAND && g.app.heraldry.key() == fixHeraldry().key(),
          "fixture people / homeland / heraldry (SAVE_VER 7)");
    auto wears = [&](int e, ItemKind k, const char* nm) { return e >= 0 && e < (int)g.inv.size() && g.inv[(size_t)e].kind == k && g.inv[(size_t)e].name == nm; };
    check(wears(g.eqGloves, ItemKind::Gloves, "LEATHER GLOVES") && wears(g.eqBoots, ItemKind::Boots, "LEATHER BOOTS") &&
              wears(g.eqCloak, ItemKind::Cloak, "HUNTER'S CLOAK"), "fixture equipment");
    bool main = false, blade = false;
    for (const Quest& q : g.quests) { if (q.type == QType::Main) main = true; if (q.title == "A BLADE OF YOUR OWN") blade = true; }
    check(main && blade, "fixture quests (the main quest and the opening)");
    // M2 fields
    auto mk = g.marks.find(FIX_MARK_KEY);
    // (the fixture's own mark; the play that made it may leave others, such as a quest giver's last offer)
    check(!g.marks.empty() && mk != g.marks.end() && mk->second == FIX_MARK_VAL, "fixture marks");
    bool subj = false;
    for (const Quest& q : g.quests) if (q.subject == FIX_SUBJECT && q.flags == FIX_QFLAGS && q.deadlineDay == FIX_DEADLINE) subj = true;
    check(subj, "fixture quest subject / flags / deadline");
    int rumoured = 0;
    for (const Site& st : g.world.sites) if (st.rumoured) rumoured++;
    check(rumoured == 1, "fixture rumoured site");
    // M4 (SAVE_VER 10): the realm block
    const Site& sv = g.world.sites[(size_t)g.world.startSite];
    const ew::Gid k = sv.kingdom >= 0 ? g.world.kingdoms[(size_t)sv.kingdom].id : 0;
    const realm::SettlementState* st = g.realm.settlement(sv.id);
    check(st && (st->flags & realm::SS_FAMINE) && st->food == 0, "fixture realm: the start village's famine (SAVE_VER 10)");
    check(!k || g.realm.rep(k) == FIX_REP, "fixture realm: the player's standing with the start kingdom (SAVE_VER 10)");
    // M6b (SAVE_VER 13): the story block v4: a generated story by its spec id, at its stage; the repetition guard
    check(g.story.running().size() == 1 && g.story.running()[0].script == FIX_SAGA && g.story.stageName(g.story.running()[0]) == "road",
          "fixture story block: the running saga and its stage (SAVE_VER 13)");
    check(g.story.guard_.seen.size() == 2 && g.story.guard_.seen[0].day == FIX_SEEN_DAY && g.story.guard_.seen[0].gx == FIX_SEEN_GX &&
              g.story.guard_.seen[0].gy == FIX_SEEN_GY && g.story.guard_.seen[0].shape == FIX_SEEN_SHAPE && !g.story.guard_.seen[0].offered &&
              g.story.guard_.phrases.size() == 2 && g.story.guard_.phrases.count(7u) && g.story.guard_.phrases.at(7u) == 2,
          "fixture story block: the repetition guard (SAVE_VER 13)");
    check(g.story.guard_.seen.size() == 2 && g.story.guard_.seen[1].offered == 1 && g.story.guard_.seen[1].hook == FIX_OFFER_HOOK &&
              g.story.guard_.seen[1].day == FIX_SEEN_DAY + 1 && g.story.guard_.seen[1].gx == FIX_SEEN_GX + 10,
          "fixture story block v4: an offer the guard remembers (its hook, the offered flag)");
    // M5 (SAVE_VER 11): the life block
    check(std::fabs(g.life.player.fedH - FIX_FED) < 0.01f && std::fabs(g.life.player.restedH - FIX_RESTED) < 0.01f &&
              (g.life.player.buffs() & (life::BUFF_WELLFED | life::BUFF_RESTED)) == (life::BUFF_WELLFED | life::BUFF_RESTED),
          "fixture life: the player's buffs (SAVE_VER 11)");
    const life::Census* lc = g.life.census(g.world, g.world.startSite);
    check(lc && (int)lc->res.size() > FIX_FRIEND && (lc->res[(size_t)FIX_FRIEND].flags & life::RF_BEFRIENDED),
          "fixture life: the befriended resident of the start village (SAVE_VER 11)");
    int grieving = 0;
    if (lc) for (const life::Resident& r : lc->res) grieving += (r.flags & life::RF_GRIEVING) != 0;
    check(lc && (lc->res[(size_t)FIX_DEAD].flags & life::RF_DEAD) && grieving > 0, "fixture life: a death in the start village and its grief");
    check(lc && lc->famineUntil == FIX6.day + FIX_FAMINE_DAYS && lc->stock[(size_t)ew::Good::Bread] == 0,
          "fixture life: the famine felt in the start village's streets");
    // M6 (SAVE_VER 12): the item fields and the craft block
    const Item* sp = nullptr;
    int ore = 0;
    for (const Item& it : g.inv) {
      if (it.name == "FIXTURE SPEAR") sp = &it;
      if (craft::isStuff(it, craft::Stuff::IronOre)) ore += it.count;
    }
    check(sp && sp->kind == ItemKind::Weapon && sp->sub == (uint8_t)WeaponType::Spear && sp->ilvl == 12 && sp->mat == Mat::Steel &&
              sp->culture == FIX_CULT && sp->form == 2 && sp->rarity == Rarity::Legendary && sp->unique == Unique::ChainLightning &&
              sp->affixValue(Affix::FireDmg) == 8 && sp->affixValue(Affix::CritChance) == 5 && sp->affixCount() == 2 && (sp->flags & IF_CRAFTED),
          "fixture M6 item: level, material, culture, form, rarity, unique power, affixes, flags (SAVE_VER 12)");
    check(ore == FIX_ORE, "fixture M6 material stack (SAVE_VER 12)");
    // M7 (SAVE_VER 14): the home block
    {
      const bool one = g.home.plots.size() == 1;
      const home::Plot* P = one ? &g.home.plots[0] : nullptr;
      check(P && P->kind == home::PlotKind::Lot && P->state == home::PlotState::Built && P->shell == (uint8_t)home::Shell::Cottage && P->style != 0 &&
                g.home.knowsStyle(P->style), "fixture home: the lot and its finished cottage in a discovered style (SAVE_VER 14)");
      check(P && (int)P->crops.size() == FIX_CROPS && P->crops[0].kind == (uint8_t)home::Crop::Barley && P->crops[0].stage == 2,
            "fixture home: the barley field");
      check(P && (int)P->animals.size() == FIX_ANIMALS && P->animals[2].kind == (uint8_t)home::Animal::Cow, "fixture home: the chickens, the cow, the horse");
      check(P && !P->stores.empty() && !P->stores[0].items.empty() && P->stores[0].items[0].count == FIX_STORE, "fixture home: the chest's harvest");
      check(g.homeRiding() && g.home.riding == FIX_ANIMALS - 1, "fixture home: the horse ridden");
      check(g.home.horse == g.home.riding && !g.home.horseInn, "fixture home: the horse out is the one ridden (home block v2)");
      check(!g.home.plots.empty() && g.home.plots[0].farmhandRes == 0xFFFF && g.home.plots[0].starter == 0, "fixture home: no farmhand, the starter furniture still to come (home block v2)");
    }
    check(g.craft.skill == FIX_SKILL && g.craft.progress(FIX_CULT, 1, craft::SecretKind::AlloyRecipe) == FIX_SECRET &&
              g.craft.trust.count(FIX_SMITH) && g.craft.trust.at(FIX_SMITH) == FIX_TRUST,
          "fixture craft block: skill, a secret's progress, a smith's trust (SAVE_VER 12)");
  }

  printf("%s\n", "2. round trips");
  // ---- 2. round trips
  for (uint64_t s : {5150ull, 7ull, 99ull}) {
    Game g(s);
    g.newEndlessGame(s);
    makeCharacter(g);
    play(g, s, 45);
    addM2Facts(g);
    addM4Facts(g);
    addM5Facts(g);
    addM6Facts(g);
    addM6bFacts(g);
    addM7Facts(g);
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

  printf("%s\n", "2a. the realm block after long ticking");
  // ---- 2a. (M4 REALM lane) a year and more of the living world (daily and weekly ticks), then the whole save
  //      round-trips byte-identically; a damaged realm block is refused or loads and ticks on without a crash, fast
  {
    Game g(5150);
    g.newEndlessGame(5150);
    makeCharacter(g);
    g.mode = Mode::Play;
    g.noWildSpawns = true;
    g.godMode = true;
    for (int i = 0; i < 400 && g.realm.focusPending(); i++) g.update(SIM_DT, Input());
    for (int d = 0; d < 120; d++) { g.day++; g.update(SIM_DT, Input()); }
    g.day += 300;   // a long rest: weekly ticks
    g.update(SIM_DT, Input());
    addM4Facts(g);
    roundTrip(g, "endless seed 5150 after 420 realm days");
    check(g.realm.day() >= g.day - 1, "the realm did not keep up with the clock");
    std::vector<uint8_t> blk;
    g.realm.serialize(blk);
    printf("realm block after %d days: %zu bytes, %zu kingdoms, %zu events\n", g.realm.day(), blk.size(), g.realm.kingdoms().size(),
           g.realm.events().size());
    check(blk.size() <= 64 * 1024, "the realm block is over 64 KB");
    auto t0 = std::chrono::steady_clock::now();
    uint64_t h = 0x5EEDull;
    int loaded = 0;
    for (int i = 0; i < 300; i++) {
      std::vector<uint8_t> bad = blk;
      h = ew::mix64(h);
      if (i % 3 == 0) bad.resize((size_t)(h % bad.size()));
      else for (int k = 0; k < 1 + (int)(h % 6); k++) bad[(size_t)(ew::mix64(h + k) % bad.size())] ^= (uint8_t)(1 + (h >> (8 + k)) % 255);
      realm::Realm r;
      if (!r.deserialize(bad)) continue;
      loaded++;
      r.focusNow(*g.world.src, g.world.ox + 200, g.world.oy + 200, r.day());
      r.advanceTo(*g.world.src, r.day() + 30);
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("damaged realm blocks: 300 tried, %d loaded and ticked 30 days, %.0f ms\n", loaded, ms);
    check(ms < 60000, "damaged realm blocks took over a minute");
  }

  printf("%s\n", "2b. the save soak");
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
    {
      std::vector<uint8_t> lb;
      g.life.serialize(lb);
      printf("save soak: the life block %zu bytes (%d censuses built)\n", lb.size(), g.life.stats.censuses);
      check(lb.size() <= 64 * 1024, "save soak: the life block is over 64 KB");
      uint64_t h = 0x11FEull;
      int loaded = 0;
      for (int i = 0; i < 300 && !lb.empty(); i++) {
        std::vector<uint8_t> bad = lb;
        h = ew::mix64(h);
        if (i % 3 == 0) bad.resize((size_t)(h % bad.size()));
        else for (int k = 0; k < 1 + (int)(h % 6); k++) bad[(size_t)(ew::mix64(h + k) % bad.size())] ^= (uint8_t)(1 + (h >> (8 + k)) % 255);
        life::Life L;
        if (!L.deserialize(bad)) continue;
        loaded++;
        for (int si : g.world.nearSites)
          if (si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].settlement()) L.census(g.world, si);
        std::vector<uint8_t> again;
        L.serialize(again);
      }
      printf("damaged life blocks: 300 tried, %d loaded and rebuilt without a crash\n", loaded);
    }
    check(ms < 50.0, "save soak: writing the save took too long");
    roundTrip(g, "endless save soak");
  }

  printf("%s\n", "3. refusals");
  // ---- 3. refusals
  {
    Game n(31);
    n.newEndlessGame(31);
    n.mode = Mode::Play;
    std::vector<uint8_t> b;
    n.serialize(b);
    auto patched = [&](size_t at, uint32_t v) { std::vector<uint8_t> c = b; for (int k = 0; k < 4; k++) c[at + k] = (uint8_t)(v >> (8 * k)); return c; };
    Game x(1);
    check(!x.deserialize(patched(4, 11)) && Game::saveVersion(patched(4, 11)) == 11, "accepted (or misread) a SAVE_VER 11 (M5) save");
    check(!x.deserialize(patched(4, 7)) && Game::saveVersion(patched(4, 7)) == 7, "accepted (or misread) a SAVE_VER 7 save");
    check(!x.deserialize(patched(4, 6)) && Game::saveVersion(patched(4, 6)) == 6, "accepted (or misread) a SAVE_VER 6 save");
    check(!x.deserialize(patched(4, 5)) && Game::saveVersion(patched(4, 5)) == 5, "accepted (or misread) a SAVE_VER 5 save");
    check(!x.deserialize(patched(4, 4)) && Game::saveVersion(patched(4, 4)) == 4, "accepted (or misread) a SAVE_VER 4 save");
    check(!x.deserialize(patched(4, 1)), "accepted a SAVE_VER 1 save");
    check(!x.deserialize(patched(4, 99)), "accepted a save from a newer format");
    check(!x.deserialize(patched(0, 0x12345678u)) && Game::saveVersion(patched(0, 0x12345678u)) == 0, "accepted bad magic");
    std::vector<uint8_t> cut(b.begin(), b.begin() + (std::ptrdiff_t)(b.size() / 2));
    check(!x.deserialize(cut), "accepted a truncated save");
    std::vector<uint8_t> classic = b;
    classic[8] = 0;   // the world kind: 0 was the retired classic island
    check(!x.deserialize(classic), "accepted a classic island save");
    std::vector<uint8_t> genv = b;
    genv[9] = (uint8_t)(genv[9] + 1);   // the endless generator version (u32 after the u8 world kind)
    check(!x.deserialize(genv), "accepted a save from another endless generator");
    check(!Game::saveFromOlderGenerator(genv) && !Game::saveFromOlderGenerator(b), "called a current or newer generator's save older");
    std::vector<uint8_t> oldGen = patched(9, (uint32_t)ew::ENDLESS_GEN_VER - 1);
    check(!x.deserialize(oldGen) && Game::saveFromOlderGenerator(oldGen), "an older endless generator's save is not refused as older");
  }
  printf("%s\n", "4. damaged saves");
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
