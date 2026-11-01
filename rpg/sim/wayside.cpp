// M2 Wayfinder, SIM lane: life at the wayside places (rpg/world/poi.h vignettes) and rumours v0 (VISION_PLAN 2.11,
// 4.6; PLAN.md task 4).
//   Hunter's camp, fishing hut, herb garden   a keeper who trades (pelts / fish / herbs and potions) and gives work
//   Wayfarers' rest                           a traveller with small talk and one free rumour a day
//   Standing stones                           the heart stone blesses once a day (Game::marks)
//   Overturned caravan                        the ambush rises when the player comes within about 8 tiles
//   Ruined watchtower                         archers that keep their distance; a chest at its foot
//   Toll bridge                               stepping on the bridge, the troll asks 10-25 gold: pay (free passage that
//                                             day, in marks) or refuse and fight
//   Lone grave                                its inscription is a rumour or an Heirloom hook (lay it on the grave)
// Rumours: innkeepers sell them (10 gold), travellers give one a day, graves give one. A rumour marks the nearest
// unknown dungeon, wayside place or wonder as rumoured (Site::rumoured: the map shows a "?"), with a line that says
// what it is and how far ("AN OLD WATCHTOWER, HALF A DAY NORTH-EAST"). Reaching it discovers it.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/world/source.h"
#include "rpg/sim/wayside_ops.h"
#include "rpg/sim/quest_ops.h"

using art::Monster;
using art::Prop;

namespace {
bool isVignette(const Site& s, ew::VignetteKind k) { return s.type == SiteType::Vignette && s.kind == (uint8_t)k; }

// the person a lone grave's title names (rpg/world/vignettes.cpp: "THE GRAVE OF <WHO>" or "<WHO>'S CAIRN"), so the
// inscription and the heirloom quest always speak of the same dead; and whether that name is a woman's (the
// vignettes' folk names)
std::string graveWho(const std::string& title) {
  static const std::string a = "THE GRAVE OF ", b = "'S CAIRN";
  if (title.size() > a.size() && title.compare(0, a.size(), a) == 0) return title.substr(a.size());
  if (title.size() > b.size() && title.compare(title.size() - b.size(), b.size(), b) == 0) return title.substr(0, title.size() - b.size());
  return std::string();
}
bool femaleFolkName(const std::string& n) {
  static const char* const f[] = {"ODA", "SIGNY", "TOVE", "GRETA", "EDDA", "MAREN", "HILDE", "TAMSIN", "ISOLDE", "ELSPETH",
                                  "INGA", "RHOSYN", "OLWEN", "DAGNY", "YRSA", "ASTA", "LISBET", "MABYN", "SOLVEIG", "BERIT",
                                  "DORTE", "FRIDA", "HELKA", "JORUNN", "LIV", "NESSA", "PERNILLE", "RAGNA", "THYRA", "VIGDIS",
                                  "AGNES", "CATRIN", "EIRA", "GISLA", "IDA", "KARIN", "MOIRA", "OTTILIE", "RUNA", "TILDE",
                                  "UNNA", "VALDIS", "BRYNJA", "DAVINA", "FENNA", "HEDDA", "KELDA", "MILDA", "ORLA", "RIKKE",
                                  "TORVI", "URSA", "WENDA", "BODIL", "DEIRDRE"};
  for (const char* x : f)
    if (n == x) return true;
  return false;
}

// what a rumour calls a place
std::string placeWords(const Site& s) {
  if (s.type == SiteType::Vignette) {
    switch ((ew::VignetteKind)s.kind) {
      case ew::VignetteKind::Caravan: return "AN OVERTURNED CARAVAN";
      case ew::VignetteKind::HunterCamp: return "A HUNTER'S CAMP";
      case ew::VignetteKind::StandingStones: return "A RING OF STANDING STONES";
      case ew::VignetteKind::Watchtower: return "AN OLD WATCHTOWER";
      case ew::VignetteKind::FishingHut: return "A FISHING HUT";
      case ew::VignetteKind::TollBridge: return "A TROLL UNDER A BRIDGE";
      case ew::VignetteKind::HerbGarden: return "A HERB GARDEN";
      case ew::VignetteKind::LoneGrave: return "A LONE GRAVE";
      case ew::VignetteKind::Wayrest: return "A WAYFARERS' REST";
      default: return "SOMETHING ODD BY THE ROAD";
    }
  }
  if (s.type == SiteType::Wonder) {
    switch ((ew::WonderKind)s.kind) {
      case ew::WonderKind::ElderTree: return "A TREE AS OLD AS THE WORLD";
      case ew::WonderKind::Colossus: return "THE STATUE OF A FORGOTTEN KING";
      case ew::WonderKind::Starfall: return "THE PLACE WHERE A STAR FELL";
      case ew::WonderKind::DragonBones: return "THE BONES OF A DRAGON";
      default: return "A WONDER";
    }
  }
  switch (s.type) {
    case SiteType::Cave: return "A CAVE";
    case SiteType::Ruin: return "AN OLD RUIN";
    case SiteType::BanditCamp: return "A BANDIT CAMP";
    case SiteType::Shrine: return "A WAYSIDE SHRINE";
    default: return "A PLACE";
  }
}
const char* timeWords(float tiles) {
  const float h = tiles / 30.0f;
  if (h < 0.75f) return "LESS THAN AN HOUR";
  if (h < 1.5f) return "AN HOUR";
  if (h < 4.5f) return "A FEW HOURS";
  if (h < 9.0f) return "HALF A DAY";
  if (h < 18.0f) return "A DAY";
  return "DAYS";
}
const char* trollName(ew::Gid id) {
  static const char* n[] = {"GRUMMOK", "BOLGAR", "HURGA", "OLD KNUCKLE", "MOSSJAW", "GRENDA", "UGGRIM", "STONEBELLY"};
  return n[ew::mix64(id ^ 0x7011ull) % 8];
}
}  // namespace

int WaysideOps::tollOf(const Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return 0;
  return 10 + (int)(ew::mix64(g.world.sites[(size_t)si].id ^ 0x70110ull) % 16);   // 10..25 gold
}
bool Game::tollPaid(int si) const {
  if (si < 0 || si >= (int)world.sites.size()) return false;
  auto it = marks.find(markKey(world.sites[(size_t)si].id, Mk::Toll));
  return it != marks.end() && it->second == day;
}

// ---------------------------------------------------------------- rumours
int WaysideOps::rumourSite(Game& g, int32_t gx, int32_t gy) {
  if (!g.world.endless || !g.world.src) return -1;
  const int rings = 2;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  ew::Gid best = 0;
  float bd = 1e30f;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        const SiteType t = p.type;
        if (t != SiteType::Cave && t != SiteType::Ruin && t != SiteType::BanditCamp && t != SiteType::Shrine && t != SiteType::Vignette && t != SiteType::Wonder) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d < 12.0f || d > 640.0f) continue;
        const int h = g.world.siteHandle(p.id);
        if (h >= 0 && (g.world.sites[(size_t)h].discovered || g.world.sites[(size_t)h].rumoured)) continue;
        if (d < bd) { bd = d; best = p.id; }
      }
    }
  return best ? g.world.ensureSite(best) : -1;
}

std::string WaysideOps::rumourLine(const Game& g, int si, int32_t fx, int32_t fy) {
  const Site& s = g.world.sites[(size_t)si];
  const int32_t gx = g.world.ox + s.ex, gy = g.world.oy + s.ey;
  const float d = std::hypot((float)(gx - fx), (float)(gy - fy));
  const std::string what = placeWords(s);
  std::string line = what + ", " + timeWords(d) + " " + dirWord(gx - fx, gy - fy, false);
  if (s.name != what && !s.name.empty()) line += ". THEY CALL IT " + s.name;
  return line;
}

std::string Game::hearRumour() {
  int px, py;
  overworldTile(*this, px, py);
  const int32_t gx = world.ox + px, gy = world.oy + py;
  // (P1 Fresh Roads) a person talking (an innkeeper, a traveller; not a grave or a book) also names a real job in
  // another settlement this week, every other rumour: a journal LEAD ("THE SMITH OF VOND IS SHORT OF GOODS")
  const bool person = mode == Mode::Dialogue && dlg.actor >= 0;
  if (person && (hash32((uint32_t)day * 2654435761u ^ (uint32_t)dlg.actor ^ (uint32_t)seed) & 1u)) JobOps::rumourJob(*this, curSite);
  const int si = WaysideOps::rumourSite(*this, gx, gy);
  if (si < 0) return "";
  world.sites[(size_t)si].rumoured = true;
  {   // the place itself becomes a lead too (the journal keeps what was heard, the map shows the "?")
    const Site& s = world.sites[(size_t)si];
    const std::string what = placeWords(s);
    JobOps::placeLead(*this, si, s.name != what && !s.name.empty() ? what + " CALLED " + s.name : what);
  }
  return WaysideOps::rumourLine(*this, si, gx, gy);
}

// ---------------------------------------------------------------- the people of the wayside
bool WaysideOps::spawn(Game& g, int si, const Spawn& sp) {
  if (si < 0 || si >= (int)g.world.sites.size() || g.world.sites[(size_t)si].type != SiteType::Vignette) return false;
  const Site& st = g.world.sites[(size_t)si];
  const Vec2 at(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f);
  if (!sp.npc && !sp.bandit) {
    // a beast keeps the place: the toll bridge's troll (peaceful until the toll is refused)
    const int id = g.spawnMonster(sp.mon, at, std::max(1, st.level + (sp.boss ? 1 : 0)), false);
    Actor& a = g.actors[(size_t)g.findActor(id)];
    a.fromMap = true; a.site = si; a.slot = sp.slot; a.home = at; a.goal = at;
    if (isVignette(st, ew::VignetteKind::TollBridge)) {
      a.hostile = false; a.npc = true; a.faction = Faction::Town; a.aggro = false;
      a.name = trollName(st.id);
      a.maxHp *= 1.6f; a.hp = a.maxHp;
    }
    return true;
  }
  if (sp.bandit && isVignette(st, ew::VignetteKind::Caravan)) {
    // the ambush lies low in the wreck until the player comes within about 8 tiles of it
    const float dx = g.pl().p.x / TILE - (st.ex + 0.5f), dy = g.pl().p.y / TILE - (st.ey + 0.5f);
    if (dx * dx + dy * dy > 8.5f * 8.5f) return true;
    g.spawnHuman(sp, at);
    Actor& a = g.actors.back();
    a.aggro = true; a.target = g.pl().id;
    const uint64_t k = markKey(st.id, Mk::Ambush);
    if (!g.marks.count(k)) {
      g.marks[k] = g.day;
      g.emit(Ev::Notice, g.pl().p, (int)rgba(255, 110, 80), 0, "AMBUSH!");
      g.say("AMBUSH! THE CARAVAN WAS BAIT!");
      g.sfx((int)Sfx::Roar, g.pl().p, 1.4f, 0.6f);
    }
    return true;
  }
  if (sp.bandit && isVignette(st, ew::VignetteKind::Watchtower)) {
    g.spawnHuman(sp, at);
    Actor& a = g.actors.back();
    a.ranged = true; a.look.weapon = 3; a.name = "BANDIT ARCHER";
    return true;
  }
  return false;
}

void WaysideOps::tollTalk(Game& g, Actor& a) {
  g.dlg = Dialogue();
  g.dlg.actor = a.id;
  g.dlg.speaker = a.name;
  g.dlg.role = a.role;
  const int si = a.site, toll = WaysideOps::tollOf(g, si);
  if (g.tollPaid(si)) {
    g.dlg.text = "YOU PAID. CROSS, BEFORE " + a.name + " CHANGES HIS MIND.";
    g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  } else {
    g.dlg.text = "HRRM. NOBODY CROSSES " + a.name + "'S BRIDGE WITHOUT PAYING. " + std::to_string(toll) + " GOLD. OR BONES. " + a.name + " LIKES BONES.";
    g.dlg.opts.push_back({"PAY " + std::to_string(toll) + " GOLD", A_TOLLPAY, si});
    g.dlg.opts.push_back({"I WON'T PAY YOU.", A_TOLLREFUSE, si});
    g.dlg.opts.push_back({"NOT TODAY.", A_BYE, 0});
  }
  g.mode = Mode::Dialogue;
  g.sfx((int)Sfx::Roar, a.p, 0.6f, 0.4f);
}

// once per step outdoors: the toll bridge's troll stops whoever steps on the bridge without paying
void WaysideOps::tick(Game& g, float dt) {
  if (g.tollAskT_ > 0) g.tollAskT_ -= dt;
  if (g.inside) return;
  const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor((g.pl().p.y - 2) / TILE);
  const bool onBridge = g.world.over.at(tx, ty) == Ground::Bridge;
  if (!onBridge) { g.offBridge_ = g.pl().p; g.tollAskT_ = 0; return; }   // (off the bridge: the next step on it is asked again)
  const int si = g.world.siteAt(tx, ty, 4);
  if (si < 0 || !isVignette(g.world.sites[(size_t)si], ew::VignetteKind::TollBridge) || g.tollPaid(si)) return;
  int troll = -1;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].site == si && g.actors[k].npc && !g.actors[k].human && g.actors[k].st != AState::Dead) { troll = (int)k; break; }
  if (troll < 0 || g.tollAskT_ > 0) return;
  g.tollAskT_ = 3.0f;
  if (len2(g.offBridge_) > 1.0f && len2(g.offBridge_ - g.pl().p) < (6.0f * TILE) * (6.0f * TILE)) g.pl().p = g.offBridge_;   // turned back
  g.pl().vel = Vec2();
  WaysideOps::tollTalk(g, g.actors[(size_t)troll]);
}

bool WaysideOps::useProp(Game& g, Prop p, int tx, int ty) {
  if (g.inside) return false;
  const int si = g.world.siteAt(tx, ty, 2);
  if (p == Prop::StandingStone) {
    if (si < 0 || !isVignette(g.world.sites[(size_t)si], ew::VignetteKind::StandingStones)) { g.say("AN OLD STONE, WORN SMOOTH BY THE WIND."); return true; }
    const Site& s = g.world.sites[(size_t)si];
    if (std::abs(tx - s.ex) > 1 || std::abs(ty - s.ey) > 1) { g.say("THE STONE IS COLD. THE HEART STONE STANDS AT THE MIDDLE OF THE RING."); return true; }
    const uint64_t key = markKey(s.id, Mk::Stones);
    auto it = g.marks.find(key);
    if (it != g.marks.end() && it->second == g.day) { g.say("THE STONES ARE SILENT. COME BACK TOMORROW."); return true; }
    g.marks[key] = g.day;
    g.blessName = "BLESSING OF THE STONES";
    g.blessT = g.blessingSecs();
    g.pl().hp = g.pl().maxHp; g.mp = g.maxMp; g.stamina = g.maxSt;
    g.recalcPlayer();
    g.say(g.blessName + ": +ARMOR, FASTER HEALING");
    g.emit(Ev::Heal, g.pl().p);
    g.emit(Ev::Sparkle, Vec2(s.ex * TILE + 8.0f, s.ey * TILE - 8.0f));
    g.sfx((int)Sfx::Heal, g.pl().p, 0.6f);
    return true;
  }
  if (p != Prop::GraveCairn) return false;
  // a lone grave
  g.dlg = Dialogue();
  g.dlg.actor = -1;
  g.dlg.speaker = "A LONE GRAVE";
  g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  g.mode = Mode::Dialogue;
  if (si < 0 || !isVignette(g.world.sites[(size_t)si], ew::VignetteKind::LoneGrave)) { g.dlg.text = "A LONE GRAVE. THE NAME IS WORN AWAY."; return true; }
  const Site s = g.world.sites[(size_t)si];   // (a copy: a rumour or a quest loads more sites)
  // an heirloom brought back for this grave is laid on it
  for (Quest& q : g.quests)
    if (q.type == QType::Heirloom && q.state == QState::Complete && (q.flags & QF_GRAVE) && q.giverSite == si) {
      QuestOps::completeQuest(g, q);
      g.dlg.text = "YOU LAY THE " + q.subject + " ON THE CAIRN. A WARM WIND PASSES THROUGH THE GRASS, AND SOMETHING GLINTS AMONG THE STONES.";
      return true;
    }
  Rng gr(ew::mix64(s.id ^ 0x6A7Eull));
  const float gf = gr.f();   // (drawn as before, so the rest of the roll keeps its stream)
  std::string dead = graveWho(s.name);
  bool female;
  if (dead.empty()) {
    female = gf < 0.45f;
    dead = makePersonName(gr, female);
  } else
    female = femaleFolkName(dead);
  const uint64_t key = markKey(s.id, Mk::Grave);
  if (g.marks.count(key)) {
    g.dlg.text = "HERE LIES " + dead + ". THE REST OF THE WORDS YOU ALREADY KNOW BY HEART.";
    return true;
  }
  g.marks[key] = 1;
  const int32_t gx = g.world.ox + s.ex, gy = g.world.oy + s.ey;
  const bool heirloom = (ew::mix64(s.id ^ 0x9EAull) & 1) != 0;
  if (heirloom) {
    bool danger = false;
    const int t = QuestOps::pickRadiant(g, gr.f() < 0.5f ? SiteType::Ruin : SiteType::Cave, gx, gy, gr, danger);
    if (t >= 0) {
      const Site& T = g.world.sites[(size_t)t];
      static const char* const keep[] = {"SWORD", "SIGNET RING", "HORN", "LOCKET", "SHIELD BOSS", "AMULET"};
      const std::string thing = keep[gr.irange(6)];
      Quest q;
      q.type = QType::Heirloom;
      q.giverSite = si; q.giverBldg = -1; q.giverSlot = -2; q.giverName = "THE LONE GRAVE"; q.giverId = s.id;
      q.target = t; q.targetId = T.id; q.hasPos = true; q.tgx = g.world.ox + T.ex; q.tgy = g.world.oy + T.ey;
      q.subject = dead + "'S " + thing;
      q.flags = QF_GRAVE | ((gr.next() & 0xFFFFu) << 16) | (danger ? QF_DANGER : 0);
      q.gold = 40 + T.level * 12; q.xp = 60 + T.level * 10;
      q.title = "THE GRAVE OF " + dead;
      q.desc = "THE GRAVE OF " + dead + " SAYS " + (female ? "HER " : "HIS ") + thing + " LIES IN " + T.name + ". BRING IT BACK AND LAY IT ON THE GRAVE.";
      g.dlg.text = "HERE LIES " + dead + ", WHO WENT INTO " + T.name + " AND DID NOT COME BACK. " + (female ? "HER " : "HIS ") + thing +
                 " LIES THERE STILL. WHOEVER BRINGS IT HOME, LAY IT HERE.";
      g.acceptQuest(q);
      return true;
    }
  }
  const std::string line = g.hearRumour();
  if (line.empty()) g.dlg.text = "HERE LIES " + dead + ". MAY THE ROAD BE KINDER WHERE " + (female ? "SHE" : "HE") + " WALKS NOW.";
  else g.dlg.text = "HERE LIES " + dead + ". SCRATCHED BELOW, IN ANOTHER HAND: \"" + line + ". DO NOT GO ALONE.\"";
  return true;
}
