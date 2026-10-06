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
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

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

int Game::tollOf(int si) const {
  if (si < 0 || si >= (int)world.sites.size()) return 0;
  return 10 + (int)(ew::mix64(world.sites[(size_t)si].id ^ 0x70110ull) % 16);   // 10..25 gold
}
bool Game::tollPaid(int si) const {
  if (si < 0 || si >= (int)world.sites.size()) return false;
  auto it = marks.find(markKey(world.sites[(size_t)si].id, Mk::Toll));
  return it != marks.end() && it->second == day;
}

// ---------------------------------------------------------------- rumours
int Game::rumourSite(int32_t gx, int32_t gy) {
  if (!world.endless || !world.src) return -1;
  const int rings = 2;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  ew::Gid best = 0;
  float bd = 1e30f;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        const SiteType t = p.type;
        if (t != SiteType::Cave && t != SiteType::Ruin && t != SiteType::BanditCamp && t != SiteType::Shrine && t != SiteType::Vignette && t != SiteType::Wonder) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d < 12.0f || d > 640.0f) continue;
        const int h = world.siteHandle(p.id);
        if (h >= 0 && (world.sites[(size_t)h].discovered || world.sites[(size_t)h].rumoured)) continue;
        if (d < bd) { bd = d; best = p.id; }
      }
    }
  return best ? world.ensureSite(best) : -1;
}

std::string Game::rumourLine(int si, int32_t fx, int32_t fy) const {
  const Site& s = world.sites[(size_t)si];
  const int32_t gx = world.ox + s.ex, gy = world.oy + s.ey;
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
  const int si = rumourSite(gx, gy);
  if (si < 0) return "";
  world.sites[(size_t)si].rumoured = true;
  return rumourLine(si, gx, gy);
}

// ---------------------------------------------------------------- the people of the wayside
bool Game::waysideSpawn(int si, const Spawn& sp) {
  if (si < 0 || si >= (int)world.sites.size() || world.sites[(size_t)si].type != SiteType::Vignette) return false;
  const Site& st = world.sites[(size_t)si];
  const Vec2 at(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f);
  if (!sp.npc && !sp.bandit) {
    // a beast keeps the place: the toll bridge's troll (peaceful until the toll is refused)
    const int id = spawnMonster(sp.mon, at, std::max(1, st.level + (sp.boss ? 1 : 0)), false);
    Actor& a = actors[(size_t)findActor(id)];
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
    const float dx = pl().p.x / TILE - (st.ex + 0.5f), dy = pl().p.y / TILE - (st.ey + 0.5f);
    if (dx * dx + dy * dy > 8.5f * 8.5f) return true;
    spawnHuman(sp, at);
    Actor& a = actors.back();
    a.aggro = true; a.target = pl().id;
    const uint64_t k = markKey(st.id, Mk::Ambush);
    if (!marks.count(k)) {
      marks[k] = day;
      emit(Ev::Notice, pl().p, (int)rgba(255, 110, 80), 0, "AMBUSH!");
      say("AMBUSH! THE CARAVAN WAS BAIT!");
      sfx((int)Sfx::Roar, pl().p, 1.4f, 0.6f);
    }
    return true;
  }
  if (sp.bandit && isVignette(st, ew::VignetteKind::Watchtower)) {
    spawnHuman(sp, at);
    Actor& a = actors.back();
    a.ranged = true; a.look.weapon = 3; a.name = "BANDIT ARCHER";
    return true;
  }
  return false;
}

void Game::tollTalk(Actor& a) {
  dlg = Dialogue();
  dlg.actor = a.id;
  dlg.speaker = a.name;
  dlg.role = a.role;
  const int si = a.site, toll = tollOf(si);
  if (tollPaid(si)) {
    dlg.text = "YOU PAID. CROSS, BEFORE " + a.name + " CHANGES HIS MIND.";
    dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  } else {
    dlg.text = "HRRM. NOBODY CROSSES " + a.name + "'S BRIDGE WITHOUT PAYING. " + std::to_string(toll) + " GOLD. OR BONES. " + a.name + " LIKES BONES.";
    dlg.opts.push_back({"PAY " + std::to_string(toll) + " GOLD", A_TOLLPAY, si});
    dlg.opts.push_back({"I WON'T PAY YOU.", A_TOLLREFUSE, si});
    dlg.opts.push_back({"NOT TODAY.", A_BYE, 0});
  }
  mode = Mode::Dialogue;
  sfx((int)Sfx::Roar, a.p, 0.6f, 0.4f);
}

// once per step outdoors: the toll bridge's troll stops whoever steps on the bridge without paying
void Game::waysideTick(float dt) {
  if (tollAskT_ > 0) tollAskT_ -= dt;
  if (inside) return;
  const int tx = (int)std::floor(pl().p.x / TILE), ty = (int)std::floor((pl().p.y - 2) / TILE);
  const bool onBridge = world.over.at(tx, ty) == Ground::Bridge;
  if (!onBridge) { offBridge_ = pl().p; tollAskT_ = 0; return; }   // (off the bridge: the next step on it is asked again)
  const int si = world.siteAt(tx, ty, 4);
  if (si < 0 || !isVignette(world.sites[(size_t)si], ew::VignetteKind::TollBridge) || tollPaid(si)) return;
  int troll = -1;
  for (size_t k = 1; k < actors.size(); k++)
    if (actors[k].site == si && actors[k].npc && !actors[k].human && actors[k].st != AState::Dead) { troll = (int)k; break; }
  if (troll < 0 || tollAskT_ > 0) return;
  tollAskT_ = 3.0f;
  if (len2(offBridge_) > 1.0f && len2(offBridge_ - pl().p) < (6.0f * TILE) * (6.0f * TILE)) pl().p = offBridge_;   // turned back
  pl().vel = Vec2();
  tollTalk(actors[(size_t)troll]);
}

bool Game::useWaysideProp(Prop p, int tx, int ty) {
  if (inside) return false;
  const int si = world.siteAt(tx, ty, 2);
  if (p == Prop::StandingStone) {
    if (si < 0 || !isVignette(world.sites[(size_t)si], ew::VignetteKind::StandingStones)) { say("AN OLD STONE, WORN SMOOTH BY THE WIND."); return true; }
    const Site& s = world.sites[(size_t)si];
    if (std::abs(tx - s.ex) > 1 || std::abs(ty - s.ey) > 1) { say("THE STONE IS COLD. THE HEART STONE STANDS AT THE MIDDLE OF THE RING."); return true; }
    const uint64_t key = markKey(s.id, Mk::Stones);
    auto it = marks.find(key);
    if (it != marks.end() && it->second == day) { say("THE STONES ARE SILENT. COME BACK TOMORROW."); return true; }
    marks[key] = day;
    blessName = "BLESSING OF THE STONES";
    blessT = blessingSecs();
    pl().hp = pl().maxHp; mp = maxMp; stamina = maxSt;
    recalcPlayer();
    say(blessName + ": +ARMOR, FASTER HEALING");
    emit(Ev::Heal, pl().p);
    emit(Ev::Sparkle, Vec2(s.ex * TILE + 8.0f, s.ey * TILE - 8.0f));
    sfx((int)Sfx::Heal, pl().p, 0.6f);
    return true;
  }
  if (p != Prop::GraveCairn) return false;
  // a lone grave
  dlg = Dialogue();
  dlg.actor = -1;
  dlg.speaker = "A LONE GRAVE";
  dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  mode = Mode::Dialogue;
  if (si < 0 || !isVignette(world.sites[(size_t)si], ew::VignetteKind::LoneGrave)) { dlg.text = "A LONE GRAVE. THE NAME IS WORN AWAY."; return true; }
  const Site s = world.sites[(size_t)si];   // (a copy: a rumour or a quest loads more sites)
  // an heirloom brought back for this grave is laid on it
  for (Quest& q : quests)
    if (q.type == QType::Heirloom && q.state == QState::Complete && (q.flags & QF_GRAVE) && q.giverSite == si) {
      completeQuest(q);
      dlg.text = "YOU LAY THE " + q.subject + " ON THE CAIRN. A WARM WIND PASSES THROUGH THE GRASS, AND SOMETHING GLINTS AMONG THE STONES.";
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
  if (marks.count(key)) {
    dlg.text = "HERE LIES " + dead + ". THE REST OF THE WORDS YOU ALREADY KNOW BY HEART.";
    return true;
  }
  marks[key] = 1;
  const int32_t gx = world.ox + s.ex, gy = world.oy + s.ey;
  const bool heirloom = (ew::mix64(s.id ^ 0x9EAull) & 1) != 0;
  if (heirloom) {
    bool danger = false;
    const int t = pickRadiant(gr.f() < 0.5f ? SiteType::Ruin : SiteType::Cave, gx, gy, gr, danger);
    if (t >= 0) {
      const Site& T = world.sites[(size_t)t];
      static const char* const keep[] = {"SWORD", "SIGNET RING", "HORN", "LOCKET", "SHIELD BOSS", "AMULET"};
      const std::string thing = keep[gr.irange(6)];
      Quest q;
      q.type = QType::Heirloom;
      q.giverSite = si; q.giverBldg = -1; q.giverSlot = -2; q.giverName = "THE LONE GRAVE"; q.giverId = s.id;
      q.target = t; q.targetId = T.id; q.hasPos = true; q.tgx = world.ox + T.ex; q.tgy = world.oy + T.ey;
      q.subject = dead + "'S " + thing;
      q.flags = QF_GRAVE | ((gr.next() & 0xFFFFu) << 16) | (danger ? QF_DANGER : 0);
      q.gold = 40 + T.level * 12; q.xp = 60 + T.level * 10;
      q.title = "THE GRAVE OF " + dead;
      q.desc = "THE GRAVE OF " + dead + " SAYS " + (female ? "HER " : "HIS ") + thing + " LIES IN " + T.name + ". BRING IT BACK AND LAY IT ON THE GRAVE.";
      dlg.text = "HERE LIES " + dead + ", WHO WENT INTO " + T.name + " AND DID NOT COME BACK. " + (female ? "HER " : "HIS ") + thing +
                 " LIES THERE STILL. WHOEVER BRINGS IT HOME, LAY IT HERE.";
      acceptQuest(q);
      return true;
    }
  }
  const std::string line = hearRumour();
  if (line.empty()) dlg.text = "HERE LIES " + dead + ". MAY THE ROAD BE KINDER WHERE " + (female ? "SHE" : "HE") + " WALKS NOW.";
  else dlg.text = "HERE LIES " + dead + ". SCRATCHED BELOW, IN ANOTHER HAND: \"" + line + ". DO NOT GO ALONE.\"";
  return true;
}
