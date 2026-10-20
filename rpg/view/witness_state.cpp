// The Witness snapshot (witness_state.h). Positions are in tiles (float): global tiles outside (the endless world's
// one frame), the building's or cave's own tiles inside; px is the player's pixel position in the loaded map. tests
// can use dist(); state.entities lists the people and beasts within 40 tiles, nearest first, then (for agent
// playtests) the public buildings' doors outside, the exit and stairs inside.
#ifdef EMB_WITNESS
#include "rpg/view/witness_state.h"
#include "rpg/art/doll/doll.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rpg/sim/anim.h"
#include "rpg/sim/factions.h"
#include "rpg/sim/game.h"
#include "rpg/view/screen.h"
#include "rpg/view/view.h"

namespace {
const char* roleName(Role r) {
  static const char* n[] = {"Villager", "Guard", "Merchant", "Smith", "Innkeeper", "Priest", "Jarl", "Farmer", "Child", "Mage", "Bandit", "King",
                            "Hunter", "Fisher", "Herbalist", "Traveller", "Herald", "Soldier", "Captain", "Refugee",
                            "Harbourmaster", "Shipwright", "Sailor", "Pirate", "Steward", "Warlord", "Envoy"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Role::COUNT, "a name for every role");
  return (int)r < (int)Role::COUNT ? n[(int)r] : "Person";
}
// the script driver's `fight` names, capitalised: what count('Wolf') matches
const char* monsterType(art::Monster m) {
  static const char* n[] = {"Wolf", "Boar", "Bear", "Slime", "Spider", "Bat", "Skeleton", "Draugr", "Goblin", "Troll", "Wraith", "Mudcrab",
                            "Icewolf", "Frostspider", "Sandworm", "Dragon", "Scorpion", "Hyena", "Lurker", "Yeti", "Wisp", "Emberhound",
                            "Blightspawn", "Harpy", "Golem", "Seaserpent", "Kraken", "Merrow"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Monster::COUNT, "a type for every monster");
  return (int)m < (int)art::Monster::COUNT ? n[(int)m] : "Monster";
}
// a village or farm animal (Actor::critter - 1): what count('Hen') matches; a person's role never names one
const char* critterType(art::Critter c) {
  static const char* n[] = {"Dog", "Cat", "Hen", "Rooster", "Goat", "Pig", "Duck", "Cow", "Sheep", "Horse"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Critter::COUNT, "a type for every critter");
  return (int)c < (int)art::Critter::COUNT ? n[(int)c] : "Animal";
}
// a public building's entity type (its door is listed for agent playtests), nullptr for homes and workshops
const char* buildingType(art::Building b) {
  using B = art::Building;
  switch (b) {
    case B::Inn: return "Inn";
    case B::Shop: return "Shop";
    case B::Smithy: return "Smithy";
    case B::Temple: return "Temple";
    case B::Keep: return "Keep";
    case B::Palace: return "Palace";
    case B::Barracks: return "Barracks";
    case B::Tower: return "Tower";
    case B::Guildhall: return "Guildhall";
    case B::MeadHall: return "MeadHall";
    case B::TeaHouse: return "TeaHouse";
    case B::Lodge: return "Lodge";
    case B::CouncilHall: return "CouncilHall";
    case B::Exchange: return "Exchange";
    case B::Bathhouse: return "Bathhouse";
    default: return nullptr;
  }
}
const char* modeName(Mode m) {
  static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
  return (size_t)m < sizeof(n) / sizeof(n[0]) ? n[(int)m] : "other";
}
const char* stateName(AState s) {
  static const char* n[] = {"idle", "walk", "windup", "strike", "recover", "roll", "hurt", "dead", "cast", "down"};
  return (size_t)s < sizeof(n) / sizeof(n[0]) ? n[(int)s] : "other";
}
const char* questState(QState s) { return s == QState::Active ? "active" : s == QState::Complete ? "complete" : "done"; }
// an actor's entity type as state.entities names it: Player; a village or farm animal by its kind; a person by role;
// else a beast by its monster type (a companion hound, the toll bridge's troll). The reset param hide matches it too.
const char* entityType(const Actor& a) {
  if (a.player) return "Player";
  if (a.critter != 0) return critterType((art::Critter)(a.critter - 1));
  return a.human ? roleName(a.role) : monsterType(a.mon);
}
// a cell a step onto changes the scene (Witness PLAYTEST.md "Cells to keep off": map '+'): outside, a building's door or
// walk-in bay (bldgEntryAt) and a cave's or ruin's mouth; inside, the way out and the stairs (Game::update's triggers)
bool doorCell(const Game& g, const Map& m, int x, int y) {
  if (!m.in(x, y)) return false;
  if (!g.inside) {
    const int bi = m.bldgAt[(size_t)y * m.w + x];
    if (bi >= 0 && bi < (int)m.bldgs.size() && bldgEntryAt(m.bldgs[(size_t)bi], x, y)) return true;
    const int pr = m.propAt(x, y);
    if (pr == (int)art::Prop::CaveEntrance + 1 || pr == (int)art::Prop::IronDoor + 1) {
      const int si = g.world.siteAt(x, y);
      return si >= 0 && (g.world.sites[(size_t)si].type == SiteType::Cave || g.world.sites[(size_t)si].type == SiteType::Ruin);
    }
    return false;
  }
  if (m.isExit(x, y)) return true;
  const int pr = m.propAt(x, y);
  return (g.sub.up.valid() && pr == (int)art::Prop::StairsUp + 1) || (g.sub.down.valid() && pr == (int)art::Prop::StairsDown + 1);
}
// to 1/100 (positions in tiles, hp, the hour): the double nearest the 2-decimal value, so the JSON stays short
double r2(double v) {
  char b[48];
  std::snprintf(b, sizeof b, "%.2f", v);
  return std::strtod(b, nullptr);
}
// a position in tiles: outside, GLOBAL tiles (the endless window's origin added), so positions never jump when the
// window shifts as the player walks on; inside, the building's or cave's own tiles
void tilePos(witness::Json& j, Vec2 p, const Game& g) {
  const double ox = g.inside ? 0.0 : (double)g.world.ox, oy = g.inside ? 0.0 : (double)g.world.oy;
  j.vec(r2(p.x / TILE + ox), r2(p.y / TILE + oy));
}
}  // namespace

void witnessState(witness::Json& j, const Game& g, const View& v, const WitnessExtra& x) {
  const Actor& P = g.pl();
  j.begin_object();
  j.field("mode", modeName(g.mode)).field("day", g.day).field("hour", r2(g.hour)).field("time", r2(g.time));
  if (g.mode == Mode::Menu) j.field("menu", v.menuTab());
  if (g.mode == Mode::Dialogue) {
    j.key("dialogue").begin_object().field("speaker", g.dlg.speaker).field("text", g.dlg.text.substr(0, 1200)).key("options").begin_array();
    // only the options on show (the page the player sees: the first page's headers, or the open group's options), in
    // screen order, so a playtester's `choose N` (N - 1 arrow presses) lands on the option it named (P2b fix round 1:
    // listing the grouped options too made "choose 6) RENT A ROOM" press FAREWELL)
    for (const DlgOpt& o : g.dlg.opts) if (g.dlg.shows(o)) j.value(o.label);
    j.end_array().end_object();
  } else j.key("dialogue").value(nullptr);   // (Witness 0.6 playtest-ready: null while none is open)
  // (Witness 0.6/0.7) the outcome: trace --xy and heatmap mark deaths from it, a playtest wakes on it
  j.field("dead", g.mode == Mode::Dead);
  if (g.noticeT > 0 && !g.notice.empty()) j.field("notice", g.notice);

  // the player
  auto eq = [&](int i) -> std::string { return i >= 0 && i < (int)g.inv.size() ? g.inv[(size_t)i].name : std::string(); };
  j.key("player").begin_object().field("id", P.id).field("type", "Player").field("name", g.app.name.empty() ? std::string("YOU") : g.app.name);
  j.key("pos"); tilePos(j, P.p, g);
  j.key("px").vec(r2(P.p.x), r2(P.p.y));
  if (!g.inside) {
    const int gx = g.world.ox + (int)std::floor(P.p.x / TILE), gy = g.world.oy + (int)std::floor((P.p.y - 2) / TILE);
    j.key("global").vec(gx, gy);
    // tests have no memory between steps: how far the player is from where the reset put them (global tiles)
    if (x.hasStart) j.key("start").vec(x.startGX, x.startGY).field("moved", r2(std::hypot(gx - x.startGX, gy - x.startGY)));
  }
  j.field("hp", r2(P.hp)).field("maxHp", r2(P.maxHp)).field("magicka", r2(g.mp)).field("maxMagicka", r2(g.maxMp))
      .field("stamina", r2(g.stamina)).field("maxStamina", r2(g.maxSt)).field("state", stateName(P.st))
      .field("gold", g.gold).field("level", g.plLevel).field("xp", g.plXp).field("xpNext", g.xpForNext())
      .field("kills", g.kills).field("god", g.godMode).field("inside", g.inside).field("floor", g.subFloor)
      .field("facing", P.face == 1 ? "N" : P.face == 2 ? "E" : P.face == 3 ? "W" : "S");   // (agent playtests: attacks and talks land)
  j.key("equipped").begin_object().field("weapon", eq(g.eqWeapon)).field("bow", eq(g.eqBow)).field("armor", eq(g.eqArmor))
      .field("helmet", eq(g.eqHelmet)).field("shield", eq(g.eqShield)).end_object();
  j.field("items", (int)g.inv.size());
  {   // (P2a) the hero as drawn last frame: rig, figure height, the opaque box and the feet point in device px. It and
      // state.screen are what the window decides: witness.json's replay.ignore leaves both out of a replay's end check
    const View::HeroSprite& hs = v.heroSprite;
    const Pix::Layer& wl = v.worldLayerForTests();
    j.key("sprite").begin_object().field("rig", art::rigVersion()).field("figH", art::rig().figH(P.look)).field("drawn", hs.on);
    if (hs.on && hs.bx1 >= hs.bx0) {
      const double s = wl.scale;
      j.key("box").begin_array().value(r2(wl.offX + (hs.x + hs.bx0) * s)).value(r2(wl.offY + (hs.y + hs.by0) * s))
          .value(r2(wl.offX + (hs.x + hs.bx1 + 1) * s)).value(r2(wl.offY + (hs.y + hs.by1 + 1) * s)).end_array();
      j.field("boxH", hs.by1 - hs.by0 + 1);   // the opaque height in art px
    }
    j.key("pivot").vec(r2(wl.offX + hs.pivX * wl.scale), r2(wl.offY + hs.pivY * wl.scale));
    if (hs.part >= 0) j.field("part", hs.part == 0 ? "loco" : hs.part == 1 ? "action" : hs.part == 2 ? "death" : hs.part == 3 ? "run" : "posture");
    {   // (P2b) the doll: whether it covers the hero's look (rig 2 draws the doll, else v1 whole) and what it still lacks
      const bool cov = art::rig2Covers(P.look);
      j.key("doll").begin_object().field("covered", cov).field("pool", art::doll::partCount());
      j.key("missing").begin_array();
      if (!cov) {
        int n = 0;
        for (const art::doll::Missing& m : art::doll::dollMissing(P.look)) { if (n++ >= 12) break; j.value(m.what()); }
      }
      j.end_array();
      if (!v.p2b.dollPart.empty()) j.field("part", v.p2b.dollPart);
      j.end_object();
    }
    j.end_object();
  }
  {   // (P2a) what the hero shows (rpg/sim/anim.h animPick, or the frozen `pose`)
    AnimPick k = animPick(g, P);
    if (v.p2a.poseOn) { k.clip = v.p2a.poseClip; k.family = v.p2a.poseFamily; k.variant = (uint8_t)v.p2a.poseVariant; k.frame = (uint8_t)v.p2a.poseFrame; }
    // (P2b) the gait the speed asks for (walk, or run above art::doll::kGaitSwitch px/s) and the speed itself
    // (vel when the sim sets it, else the walker's own speed: the hero's movement is not kept in vel)
    const float sp = P.st == AState::Walk ? (len(P.vel) > 0.5f ? len(P.vel) : P.speed) : len(P.vel);
    j.key("anim").begin_object().field("clip", art::clipName(k.clip)).field("family", art::familyName(k.family)).field("variant", (int)k.variant)
        .field("frame", (int)k.frame).field("frames", (int)k.frames).field("frozen", v.p2a.poseOn)
        .field("speed", r2(sp)).field("gait", sp > art::doll::kGaitSwitch ? "run" : "walk").end_object();
  }
  j.end_object();

  // (P2a) the screen: the window, the UI fit, the world layer and its zoom numbers, the rig's draw counters
  {
    const Pix::Layer& ul = v.uiLayerForTests();
    const Pix::Layer& wl = v.worldLayerForTests();
    int pw = 0, ph = 0;
    v.outputSizeForTests(pw, ph);
    const screen::Settings ss = screen::get();
    j.key("screen").begin_object();
    j.key("window").vec(pw, ph);
    j.key("ui").begin_object().field("w", ul.w).field("h", ul.h).field("scale", r2(ul.scale))
        .field("integer", std::fabs(ul.scale - std::round(ul.scale)) < 1e-4f).end_object();
    j.key("world").begin_object().field("w", wl.w).field("h", wl.h).field("scale", r2(wl.scale))
        .field("integer", std::fabs(wl.scale - std::round(wl.scale)) < 1e-4f).field("d0", screen::defaultWorldScale())
        .field("want", screen::worldScale()).key("levels").begin_array();
    for (int d : screen::zoomLevels()) j.value(d);
    j.end_array().end_object();
    j.field("zoom", ss.zoom).field("zoomName", screen::zoomName(ss.zoom)).field("ring", ss.ring)
        .field("safe", std::to_string(Pix::SL) + " " + std::to_string(Pix::ST) + " " + std::to_string(Pix::SR) + " " + std::to_string(Pix::SB))
        .field("v1Draws", v.v1Draws).field("peopleMB", r2(v.peopleMB())).field("figShare", r2(ph > 0 && v.heroSprite.on && v.heroSprite.by1 >= v.heroSprite.by0 ? 100.0 * (v.heroSprite.by1 - v.heroSprite.by0 + 1) * wl.scale / ph : 0.0));
    j.end_object();
    // (Witness 0.7.1) the layers in screen pixels, so measure scale / doctor read each one at its own scale: the world
    // layer (integer D) and the UI layer (its fit scale); rect [x, y, w, h]
    auto layer = [&](const char* name, const Pix::Layer& l) {
      j.begin_object().field("name", name).key("rect").begin_array().value(l.offX).value(l.offY)
          .value((int)std::lround(l.w * l.scale)).value((int)std::lround(l.h * l.scale)).end_array().end_object();
    };
    j.key("layers").begin_array();
    layer("world", wl);
    layer("ui", ul);
    j.end_array();
  }

  // where a quest's next step happens, outside: the objective's tile centre in the frame `pos` uses (Game::questTarget,
  // else the quest's own place: a den, a camp), global tiles as pos is outside; false inside or when it has no place
  auto questPos = [&](const Quest& q, double& x, double& y) -> bool {
    if (g.inside || q.state == QState::Done) return false;
    int tx = 0, ty = 0;
    if (!g.questTarget(q.id, tx, ty)) {
      if (!q.hasPos) return false;
      tx = q.tgx - g.world.ox; ty = q.tgy - g.world.oy;
    }
    x = g.world.ox + tx + 0.5; y = g.world.oy + ty + 0.5;
    return true;
  };
  // quests (the tracked one first, then by id)
  std::vector<const Quest*> qs;
  for (const Quest& q : g.quests) qs.push_back(&q);
  std::stable_sort(qs.begin(), qs.end(), [&](const Quest* a, const Quest* b) { return (a->id == g.trackedQuest) > (b->id == g.trackedQuest); });
  {   // (Witness 0.6 playtest-ready) the goal: the tracked open quest (else the first active one), its count and next step,
      // and where that happens (witness.json play.goal words it the same way and adds the other quests' count)
    const Quest* gq = nullptr;
    for (const Quest* q : qs) if (q->state != QState::Done && q->id == g.trackedQuest) { gq = q; break; }
    if (!gq) for (const Quest* q : qs) if (q->state == QState::Active) { gq = q; break; }
    if (gq) {
      std::string text = gq->title;
      if (gq->need) text += " (" + std::to_string(gq->have) + "/" + std::to_string(gq->need) + ")";
      const std::string st = g.questStatus(*gq);
      if (!st.empty()) text += ": " + st;
      j.key("goal").begin_object().field("text", text).field("quest", gq->id);
      double x = 0, y = 0;
      if (questPos(*gq, x, y)) j.key("pos").vec(x, y);
      j.end_object();
    } else j.key("goal").value(nullptr);
  }
  j.key("quests").begin_array();
  for (size_t i = 0; i < qs.size() && i < 24; i++) {
    const Quest& q = *qs[i];
    j.begin_object().field("id", q.id).field("title", q.title).field("type", (int)q.type).field("state", questState(q.state))
        .field("have", q.have).field("need", q.need).field("giver", q.giverName).field("tracked", q.id == g.trackedQuest);
    // (agent playtests) the journal's next step ("TALK TO THE INNKEEPER IN VOND") and, outside, where it leads: the
    // objective's tile centre in the frame `pos` uses (Game::questTarget, else the quest's own place: a den, a camp)
    if (q.state != QState::Done) {
      j.field("status", g.questStatus(q));
      double x = 0, y = 0;
      if (questPos(q, x, y)) j.key("pos").vec(x, y);   // (global, as pos is outside)
    }
    j.end_object();
  }
  j.end_array();

  // where
  j.key("location").begin_object().field("region", g.locName);
  const int si = g.inside && g.subBldg >= 0 ? g.world.over.bldgs[(size_t)g.subBldg].site : g.inside ? g.subSite : g.curSite;
  if (si >= 0 && si < (int)g.world.sites.size()) {
    const Site& s = g.world.sites[(size_t)si];
    j.field("site", s.name).field("siteType", siteTypeName(s.type)).field("capital", s.capital);
    if (const Kingdom* K = g.world.kingdomOf(si)) j.field("kingdom", K->name);
  }
  j.end_object();
  // in every mode with a world in play (a menu over it too: an agent playtest reads a change of scene as a new place)
  const bool inWorld = g.mode != Mode::Title && g.mode != Mode::Creator;
  if (inWorld) j.field("scene", g.inside ? "inside" : "overworld");

  // the ground round the player, for an agent playtest's pathing (Witness PLAYTEST.md "Snapshot conventions"): 41 x 41
  // tiles centred on the player's tile, '#' where Map::blocked (walls, buildings, solid props, deep water, cliffs, off
  // the map), '.' where a body can stand. Positions are in the frame `pos` uses, so origin is the top-left tile; id
  // names that frame (inside: the building and floor; outside: "ow:global", one frame for the whole endless world, so
  // a window shift is not a new place). Read-only: nothing here touches the sim.
  if (inWorld) {
    const Map& m = g.map();
    constexpr int R = 20;
    const int px = (int)std::floor(P.p.x / TILE), py = (int)std::floor(P.p.y / TILE);
    char id[64];
    if (g.inside) std::snprintf(id, sizeof id, "in:%d:%d:%d", g.subSite, g.subBldg, g.subFloor);
    else std::snprintf(id, sizeof id, "ow:global");
    const int ox = g.inside ? 0 : g.world.ox, oy = g.inside ? 0 : g.world.oy;
    // '+': a door, a walk-in bay, a cave mouth, the way out or the stairs (doorCell): stepping on it changes the scene, so
    // a playtest's paths keep off it unless it is the goal (Witness 0.6 "Cells to keep off")
    j.key("map").begin_object().field("id", std::string(id)).field("cell", 1).key("origin").vec(ox + px - R, oy + py - R).field("solid", "#")
        .field("door", "+");
    j.key("rows").begin_array();
    std::string row((size_t)(2 * R + 1), '.');
    for (int y = py - R; y <= py + R; y++) {
      for (int x = px - R; x <= px + R; x++) row[(size_t)(x - px + R)] = m.blocked(x, y) ? '#' : doorCell(g, m, x, y) ? '+' : '.';
      j.value(row);
    }
    j.end_array().end_object();
  }

  // the people and beasts within 40 tiles, nearest first (at most 60)
  std::vector<std::pair<float, const Actor*>> near;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    const float d = len(a.p - P.p) / TILE;
    if (d <= 40.0f) near.push_back({d, &a});
  }
  std::stable_sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first || (a.first == b.first && a.second->id < b.second->id); });
  j.key("entities").begin_array();
  j.begin_object().field("id", P.id).field("type", "Player").key("pos"); tilePos(j, P.p, g); j.field("hp", r2(P.hp)).end_object();
  int hostiles = 0;
  for (size_t i = 0; i < near.size() && i < 60; i++) {
    const Actor& a = *near[i].second;
    // a person (human: a role), an animal of a village or farm (critter: its kind), else a beast (its monster type: a
    // companion hound, the toll bridge's troll). talk: the player can talk to them (TALK on E or a tap)
    const bool animal = a.critter != 0, person = a.human && !animal;
    const bool talk = a.npc && !animal && !a.hostile && a.st != AState::Dead && !(a.aggro && a.target == P.id);
    if (a.hostile && a.st != AState::Dead) hostiles++;
    j.begin_object().field("id", a.id).field("type", entityType(a))
        .field("kind", a.comp ? "companion" : animal ? "animal" : person ? "person" : "beast").field("talk", talk).field("name", a.name);
    j.key("pos"); tilePos(j, a.p, g);
    j.field("dist", r2(near[i].first)).field("hp", r2(a.hp)).field("maxHp", r2(a.maxHp)).field("level", a.level)
        .field("hostile", a.hostile).field("aggro", a.aggro).field("faction", factionName(a.faction)).field("state", stateName(a.st));
    if (a.human) {   // (P2a) what a person shows, and how long the dead have lain
      const AnimPick k = animPick(g, a);
      j.key("anim").begin_object().field("clip", art::clipName(k.clip)).field("family", art::familyName(k.family)).field("variant", (int)k.variant)
          .field("frame", (int)k.frame).field("frames", (int)k.frames).end_object();
    }
    if (a.st == AState::Dead) j.key("corpse").begin_object().field("age", r2(a.stT)).end_object();
    if (a.human && !animal) {   // (P2b) the doll covers this person (rig 2 draws the doll), else the first value it lacks
      const bool cov = art::rig2Covers(a.look);
      j.key("doll").begin_object().field("covered", cov);
      if (!cov && art::doll::partCount() > 0) {
        const std::vector<art::doll::Missing> m = art::doll::dollMissing(a.look);
        j.field("missing", (int)m.size());
        if (!m.empty()) j.field("first", m[0].what());
      }
      j.end_object();
    }
    j.end_object();
  }
  // (agent playtests) outside: the doors of the public buildings within 40 tiles, nearest first (at most 8), so "goto
  // Inn" walks in (stepping onto a door enters). kind "building"; the id is the door's global tile (stable while the
  // window shifts), far above any actor id
  if (!g.inside && inWorld) {
    struct Door { float d; int bi; };
    std::vector<Door> doors;
    const auto& B = g.world.over.bldgs;
    for (size_t bi = 0; bi < B.size(); bi++) {
      if (!buildingType(B[bi].type) || B[bi].charred || B[bi].home == 3) continue;
      const float d = std::hypot(B[bi].doorX() + 0.5f - P.p.x / TILE, B[bi].doorY() + 0.5f - P.p.y / TILE);
      if (d <= 40.0f) doors.push_back({d, (int)bi});
    }
    std::stable_sort(doors.begin(), doors.end(), [](const Door& a, const Door& b) { return a.d < b.d; });
    for (size_t i = 0; i < doors.size() && i < 8; i++) {
      const Bldg& b = B[(size_t)doors[i].bi];
      const int32_t gx = g.world.ox + b.doorX(), gy = g.world.oy + b.doorY();
      const int id = 50000000 + (int)(((uint32_t)gx & 0xFFFu) << 12 | ((uint32_t)gy & 0xFFFu));
      // door: the playtest keeps off it unless it is the goal; leadsTo: `goto Inn` enters by it
      j.begin_object().field("id", id).field("type", buildingType(b.type)).field("kind", "building").field("talk", false)
          .field("door", true).field("leadsTo", buildingType(b.type)).field("enter", "n");   // doors face south: push north (up) onto the door tile
      j.key("pos").vec(gx + 0.5, gy + 0.5);
      j.field("dist", r2(doors[i].d)).field("hostile", false).end_object();
    }
  }
  // (Witness 0.6 playtest-ready) every open quest with a place to go, as a marker thing at that place (type Quest, named
  // by its title, quest: the title, objective: true), so `goto "<title>"` walks there; ids 49100000 + the quest's id
  for (const Quest* q : qs) {
    double x = 0, y = 0;
    if (!questPos(*q, x, y)) continue;
    j.begin_object().field("id", 49100000 + q->id).field("type", "Quest").field("kind", "quest").field("name", q->title)
        .field("quest", q->title).field("objective", true).field("talk", false).field("hostile", false);
    j.key("pos").vec(x, y);
    j.field("dist", r2(std::hypot(x - (g.world.ox + P.p.x / TILE), y - (g.world.oy + P.p.y / TILE)))).end_object();
  }
  // inside: the way out (stepping onto it leaves) and the stairs, so "goto Exit" / "goto StairsUp" work
  if (g.inside && inWorld) {
    auto place = [&](int id, const char* type, int tx, int ty, const char* enter) {
      j.begin_object().field("id", id).field("type", type).field("kind", "place").field("talk", false).field("door", true);
      if (enter) j.field("enter", enter);   // the push direction that uses it (the way out is on the south wall)
      j.key("pos").vec(tx + 0.5, ty + 0.5);
      j.field("dist", r2(std::hypot(tx + 0.5f - P.p.x / TILE, ty + 0.5f - P.p.y / TILE))).field("hostile", false).end_object();
    };
    if (g.sub.exitY >= 0) place(49000001, "Exit", g.sub.exitX, g.sub.exitY, "s");
    if (g.sub.up.valid()) place(49000002, "StairsUp", g.sub.up.x, g.sub.up.y, nullptr);
    if (g.sub.down.valid()) place(49000003, "StairsDown", g.sub.down.x, g.sub.down.y, nullptr);
  }
  j.end_array();

  // counts (as --perf and the CI suites track them) and the harness's own facts
  j.key("counts").begin_object().field("actors", (int)g.actors.size()).field("nearby", (int)near.size()).field("hostilesNearby", hostiles)
      .field("npcAwake", g.perf.npcAwake).field("npcAsleep", g.perf.npcAsleep).field("hostiles", g.perf.hostiles)
      .field("activeSites", g.perf.activeSites).field("pickups", (int)g.pickups.size()).field("projectiles", (int)g.projs.size())
      .field("quests", (int)g.quests.size()).field("dungeonsCleared", g.dungeonsCleared).end_object();
  j.key("script").begin_object().field("busy", x.scriptBusy).field("fails", x.scriptFails).field("lastFail", x.lastFail).end_object();
  j.key("save").begin_object().field("enabled", x.saveEnabled).field("written", x.savesWritten).end_object();
  {   // FNV-1a of witnessDigest: every actor and the clock exactly (two runs that agree here agree on the whole world)
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : witnessDigest(g)) { h ^= ch; h *= 1099511628211ull; }
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", (unsigned long long)h);
    j.field("digest", std::string(b));
  }
  j.end_object();
}

std::string witnessDigest(const Game& g) {
  // exact (hex floats): every actor, not only the 60 nearest the snapshot lists, and the clock
  std::string s;
  char b[256];
  std::snprintf(b, sizeof b, "t%a h%a d%d ox%d oy%d in%d site%d n%d|", g.time, g.hour, g.day, g.world.ox, g.world.oy, (int)g.inside, g.curSite,
                (int)g.actors.size());
  s += b;
  for (const Actor& a : g.actors) {
    std::snprintf(b, sizeof b, "%d:%d:%d:%d:%a,%a:%a:%d;", a.id, (int)a.npc, (int)a.role, (int)a.mon, a.p.x, a.p.y, a.hp, (int)a.st);
    s += b;
  }
  std::snprintf(b, sizeof b, "|g%d xp%d k%d inv%zu q%zu p%zu", g.gold, g.plXp, g.kills, g.inv.size(), g.quests.size(), g.pickups.size());
  return s + b;
}

bool View::witnessHides(const Game& g, const Actor& a) const {
  (void)g;
  for (int id : wHide.ids) if (id == a.id) return true;
  if (wHide.types.empty()) return false;
  const char* t = entityType(a);
  for (const std::string& s : wHide.types) if (s == t) return true;
  return false;
}

void View::witnessRelease() {
  shutdown();            // the bakers joined (a rebuilt View starts its own on its first prefetch)
  bldgAsync_.clear();    // (std::async futures: waits for a paint still running)
  bldgAsyncKeys_.clear();
  if (pix_) pix_->destroyUnmarked();
  witnessResetBeasts();
  witnessResetWorldMap();
  witnessResetPaperdoll();
  witnessResetPartyHud();
  witnessResetHud();
}
#endif
