// M4 "Banners": notice boards and heralds (VISION_PLAN 4.6: "Notice boards (a new prop in town squares) list bounties,
// war proclamations, missing persons, and houses and plots for sale. Heralds in capitals announce declarations of war,
// with a fanfare sound"). STORY lane.
//
// Both are laid at RUNTIME, never by the generator (goldens untouched): a board on a free paved tile of every town's and
// city's main square (all eight neighbours open, clear of every door and stall: it can never close a way through),
// found by the same search on every visit so it stands in the same place; a herald by the seat of power of every
// capital, in the kingdom's service (Actor::realm), who proclaims war and peace as the news arrives.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {

namespace {

constexpr int BOARD_SLOT0 = 960;   // the giver slots of a board's bounties (a board has no person behind it)

bool boardWorthy(const Site& s) { return s.type == SiteType::Town || s.type == SiteType::City; }

bool doorNear(const Game& g, const Site& s, int x, int y) {
  const Map& m = g.world.over;
  for (int bi = s.bldgFirst; bi < s.bldgFirst + s.bldgCount && bi < (int)m.bldgs.size(); bi++) {
    const Bldg& B = m.bldgs[(size_t)bi];
    for (int c : bldgEntryColumns(B))
      if (std::abs(c - x) <= 2 && std::abs(B.doorY() + 1 - y) <= 2) return true;
    if (std::abs(B.doorX() - x) <= 2 && y >= B.doorY() - 1 && y <= B.doorY() + 3) return true;
  }
  return false;
}

void noticeEv(Game& g, const std::string& text, uint32_t col = rgba(255, 220, 140)) {
  Event e;
  e.type = Ev::Notice; e.p = g.pl().p; e.a = (int)col; e.f = 0; e.s = text;
  g.events.push_back(e);
}

// a notice that is neither a bounty nor news: the little life of a town (specific, a little funny, never filler)
const char* const kFlavour[] = {
    "LOST: A GREY GOAT, ANSWERS TO 'BUTTONS'. DO NOT FEED IT ROPE.",
    "WANTED: A STRONG BACK FOR THE MILL. PAY IN FLOUR AND HONEST WORDS.",
    "THE CHOIR MEETS AT DUSK. TONE-DEAF WELCOME. TUNELESS ALSO WELCOME.",
    "WHOEVER TOOK THE BELL ROPE: THE BELL WOULD LIKE IT BACK.",
    "FOUND: ONE BOOT, LEFT FOOT, VERY OLD. OWNER MAY HAVE IT IF THEY LIMP.",
    "WRESTLING ON THE GREEN AT THE NEW MOON. THE SMITH MAY NOT ENTER.",
    "TO THE ONE WHO LEFT BREAD ON MY STEP ALL WINTER: THANK YOU. - A WIDOW",
    "NO DICE IN THE TAVERN AFTER MIDNIGHT. THIS MEANS YOU, OSWIN.",
    "SEEKING A HUSBAND WHO CAN COOK. OR A COOK. EITHER. ASK AT THE BAKERY.",
    "THE WELL WATER IS SAFE AGAIN. THE THING IN IT HAS BEEN REMOVED.",
    "APPRENTICE WANTED BY THE TANNER. MUST HAVE NO SENSE OF SMELL.",
    "REWARD FOR THE RETURN OF MY GOOD LADDER. NO QUESTIONS ASKED. SOME GLARING."};

}  // namespace

bool isBoardSite(const Game& g, int si) { return si >= 0 && si < (int)g.world.sites.size() && boardWorthy(g.world.sites[(size_t)si]); }

bool boardTile(Game& g, int si, int& tx, int& ty) {
  if (!isBoardSite(g, si)) return false;
  const Site& s = g.world.sites[(size_t)si];
  const Map& m = g.world.over;
  const int board = (int)art::Prop::NoticeBoard + 1;
  for (int y = s.ey - 14; y <= s.ey + 14; y++)
    for (int x = s.ex - 14; x <= s.ex + 14; x++)
      if (m.propAt(x, y) == board) { tx = x; ty = y; return true; }
  int best = -1;
  for (int y = s.ey - 12; y <= s.ey + 12; y++)
    for (int x = s.ex - 12; x <= s.ex + 12; x++) {
      if (!m.in(x - 1, y - 1) || !m.in(x + 1, y + 1)) continue;
      if (m.at(x, y) != Ground::Plaza) continue;
      const int d2 = (x - s.ex) * (x - s.ex) + (y - s.ey) * (y - s.ey);
      if (d2 < 9) continue;   // not on the square's centrepiece
      bool ok = true;
      for (int dy = -1; dy <= 1 && ok; dy++)
        for (int dx = -1; dx <= 1 && ok; dx++) {
          const int qx = x + dx, qy = y + dy;
          const Ground gr = m.at(qx, qy);
          if (m.blocked(qx, qy) || m.propAt(qx, qy) || groundSolid(gr) || gr == Ground::Water || gr == Ground::DeepWater) ok = false;
          if (m.bldgAt[(size_t)qy * m.w + qx] >= 0) ok = false;
          if (m.wall.size() == m.prop.size() && m.wall[(size_t)qy * m.w + qx]) ok = false;
        }
      // one more ring of breathing room on its sides (people queue to read it), and a clear tile to stand on below
      for (int dx = -2; dx <= 2 && ok; dx += 4) if (m.blocked(x + dx, y)) ok = false;
      if (ok && (m.blocked(x, y + 2) || m.propAt(x, y + 2))) ok = false;
      // (fixer M4 r1, review: "a torch pole sticks up out of the board's roof") the board stands about two tiles tall:
      // nothing may stand on the tiles behind its height (a torch or a well there shows through its roof)
      for (int dy = -3; dy <= -2 && ok; dy++)
        for (int dx = -1; dx <= 1 && ok; dx++)
          if (m.in(x + dx, y + dy) && (m.propAt(x + dx, y + dy) || m.bldgAt[(size_t)(y + dy) * m.w + x + dx] >= 0)) ok = false;
      if (!ok || doorNear(g, s, x, y)) continue;
      const int d = (int)std::sqrt((float)d2);
      const int score = 400 - std::abs(d - 5) * 30 + (int)(hash2(x, y, (uint32_t)s.id) % 13);
      if (score > best) { best = score; tx = x; ty = y; }
    }
  return best >= 0;
}

void placeBoards(Game& g) {
  if (g.inside || !g.world.endless) return;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (int si : g.world.nearSites) {
    if (!isBoardSite(g, si)) continue;
    const Site& s = g.world.sites[(size_t)si];
    if (std::fabs(s.ex - px) > 70 || std::fabs(s.ey - py) > 60) continue;
    if (!g.world.over.in(s.ex - 15, s.ey - 15) || !g.world.over.in(s.ex + 15, s.ey + 15)) continue;
    int x = 0, y = 0;
    if (!boardTile(g, si, x, y)) continue;
    if (g.world.over.propAt(x, y) == (int)art::Prop::NoticeBoard + 1) continue;
    g.world.over.setProp(x, y, art::Prop::NoticeBoard);
    g.world.over.solid[(size_t)y * g.world.over.w + x] = 1;
  }
}

// ---------------------------------------------------------------- the board's notices
bool useBoard(Game& g, int tx, int ty) {
  if (g.inside || g.world.over.propAt(tx, ty) != (int)art::Prop::NoticeBoard + 1) return false;
  int si = g.world.siteAt(tx, ty, 4);
  if (!isBoardSite(g, si)) {
    float bd = 1e9f;
    for (int s2 : g.world.nearSites)
      if (isBoardSite(g, s2)) {
        const float d = std::hypot((float)(g.world.sites[(size_t)s2].ex - tx), (float)(g.world.sites[(size_t)s2].ey - ty));
        if (d < bd) { bd = d; si = s2; }
      }
    if (si < 0 || bd > 30) return false;
  }
  const Site S = g.world.sites[(size_t)si];
  g.dlg = Dialogue();
  g.dlg.actor = -1;
  g.dlg.speaker = "NOTICE BOARD OF " + S.name;
  g.dlg.role = Role::Villager;
  std::vector<std::string> lines;
  // rewards waiting here
  for (const Quest& q : g.quests)
    if (q.state == QState::Complete && q.giverSite == si && q.giverSlot >= BOARD_SLOT0 && q.giverSlot < BOARD_SLOT0 + 4 && q.giverBldg == -1)
      g.dlg.opts.push_back({"COLLECT BOUNTY (+" + std::to_string(q.gold) + " GOLD)", DLG_STORY + SA_BOARD + 1, q.id});
  // war proclamations and the realm's news posted by the crown (its kingdom's, the last 30 days)
  const ew::Gid k = S.kingdom >= 0 ? g.world.kingdoms[(size_t)S.kingdom].id : 0;
  const int32_t gx = g.world.ox + S.ex, gy = g.world.oy + S.ey;
  const realm::WorldEvent* procl = nullptr;
  for (const realm::WorldEvent& e : g.realm.events()) {
    const bool kind = e.type == realm::EvType::WarDeclared || e.type == realm::EvType::Peace || e.type == realm::EvType::Succession ||
                      e.type == realm::EvType::SiegeBegun || e.type == realm::EvType::Famine || e.type == realm::EvType::TownTaken;
    if (!kind || (e.a != k && e.b != k) || g.day - (int)e.day > 30 || !rumourReaches(e, g.day, gx, gy)) continue;
    if (!procl || e.id > procl->id) procl = &e;
  }
  if (procl) {
    std::string rn, rt;
    const bool ruler = g.story.rulerOf(g, k, rn, rt);
    lines.push_back("BY ORDER OF THE " + (ruler ? rt : std::string("CROWN")) + ": " + newsLine(g, *procl, 0));
    g.dlg.opts.push_back({"READ THE PROCLAMATION", DLG_STORY + SA_BOARD + 2, (int)procl->id});
  }
  // bounties: up to two posted jobs (radiant quests offered by the board, collected here)
  for (int kk = 0; kk < 2; kk++) {
    const int slot = BOARD_SLOT0 + kk;
    bool open = false;
    for (const Quest& q : g.quests) if (q.giverSite == si && q.giverSlot == slot && q.giverBldg == -1 && q.state != QState::Done) open = true;
    if (open) continue;
    Actor A;
    A.npc = true; A.human = true; A.site = si; A.bldg = -1; A.slot = slot; A.name = "THE NOTICE BOARD"; A.role = Role::Guard;
    bool ok = false;
    Quest q = g.offerFor(A, kk == 0 ? QType::Bounty : QType::Hunt, ok);
    if (!ok) q = g.offerFor(A, QType::COUNT, ok);
    if (!ok || q.title.empty()) continue;
    std::string label = q.title.rfind("BOUNTY", 0) == 0 ? q.title : "BOUNTY: " + q.title;
    if (label.size() > 40) label = label.substr(0, 37) + "...";
    g.dlg.opts.push_back({label, DLG_STORY + SA_BOARD + 0, kk});
    if (lines.size() < 2) lines.push_back((q.title.rfind("BOUNTY", 0) == 0 ? q.title : "BOUNTY: " + q.title) + ", " + std::to_string(q.gold) + " GOLD");
  }
  // missing persons and other stories pinned here
  const int st = g.story.offerForPlace(g, (int)dsl::HookKind::Board, si);
  if (st >= 0) {
    const dsl::Script& sc = dsl::library().scripts[(size_t)st];
    g.dlg.opts.insert(g.dlg.opts.begin(), {sc.pitch, DLG_STORY + SA_START + st, si});
    if (!sc.hint.empty()) lines.insert(lines.begin(), sc.hint);
  }
  // houses for sale: the homes of families the player's stories moved away
  for (const Fact& f : g.story.facts())
    if (f.site == S.id && f.text.find("HOUSE") != std::string::npos) { lines.push_back("FOR SALE: " + f.text); break; }
  if (lines.size() < 3) lines.push_back(kFlavour[(ew::mix64(S.id ^ (uint64_t)(g.day / 3)) >> 5) % (sizeof(kFlavour) / sizeof(kFlavour[0]))]);
  std::string text = "NOTICES, SOME FRESH, SOME RAIN-SOAKED.";
  for (size_t i = 0; i < lines.size() && i < 3; i++) {
    std::string l = lines[i];
    if (!l.empty() && l.back() != '.' && l.back() != '!' && l.back() != '?') l += ".";
    text += " " + l;
  }
  if (text.size() > 300) text = text.substr(0, 297) + "...";
  g.dlg.text = text;
  g.dlg.opts.push_back({"FAREWELL.", 0, 0});
  g.mode = Mode::Dialogue;
  if (host().sound) host().sound(g, (int)Sfx::MenuSelect, 0.8f, 0.7f);
  g.story.onUseProp(g, (int)art::Prop::NoticeBoard, tx, ty);
  return true;
}

bool boardChoose(Game& g, int action, int arg) {
  if (action >= SA_HERALD) {
    const int sub = action - SA_HERALD;
    int32_t gx, gy;
    playerGlobal(g, gx, gy);
    if (sub == 0) {
      std::string text;
      int n = 0;
      std::vector<const realm::WorldEvent*> evs;
      for (const realm::WorldEvent& e : g.realm.events()) if (rumourReaches(e, g.day, gx, gy)) evs.push_back(&e);
      std::sort(evs.begin(), evs.end(), [](const realm::WorldEvent* a, const realm::WorldEvent* b) { return a->id > b->id; });
      for (const realm::WorldEvent* e : evs) {
        if (n >= 2) break;
        const std::string l = hearEvent(g, *e, 0);
        text += (n ? " " : "") + l;
        n++;
      }
      g.dlg.text = n ? "HEAR THE NEWS OF THE REALM! " + text : "THE REALM IS QUIET, GOOD TRAVELLER, AND MAY IT STAY SO. NO NEWS IS GOOD NEWS.";
    } else {
      const ew::Gid k = landAt(g, gx, gy);
      const realm::KingdomState* K = g.realm.kingdom(k);
      std::string rn, rt;
      if (!K || !g.story.rulerOf(g, k, rn, rt)) g.dlg.text = "THE CROWN IS THE CROWN, TRAVELLER. ASK AT COURT.";
      else {
        const uint8_t t = K->ruler.traits;
        const char* temper = (t & realm::RT_AGGRESSIVE) ? "QUICK TO THE SWORD AND SLOW TO FORGIVE" : (t & realm::RT_CAUTIOUS) ? "CAREFUL, AND NEVER HURRIED"
                             : (t & realm::RT_GREEDY) ? "FOND OF A FULL TREASURY" : (t & realm::RT_PIOUS) ? "DEVOUT, FIRST AT EVERY RITE"
                             : (t & realm::RT_SCHOLARLY) ? "LEARNED, NEVER WITHOUT A BOOK" : "A KEEPER OF OATHS";
        const int rep = g.realm.rep(k);
        g.dlg.text = "THE " + rt + " " + rn + " OF " + K->name + ", " + temper + ". " +
                     (rep >= 20 ? std::string("YOUR NAME HAS REACHED THE THRONE, AND KINDLY.") : rep <= -20 ? std::string("YOUR NAME IS SPOKEN AT COURT, AND NOT KINDLY.") : std::string("THE COURT DOES NOT KNOW YOUR NAME. YET."));
        const std::vector<std::string> chron = g.realm.chronicle(k);
        if (chron.size() > 1 && g.dlg.text.size() + chron[1].size() < 290) g.dlg.text += " " + chron[1];
      }
    }
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.opts[i].action == DLG_STORY + action) { g.dlg.opts.erase(g.dlg.opts.begin() + (std::ptrdiff_t)i); break; }
    return true;
  }
  const int sub = action - SA_BOARD;
  int32_t gx, gy;
  playerGlobal(g, gx, gy);
  // the board this dialogue belongs to: the board site nearest the player
  int si = -1;
  float bd = 1e9f;
  for (int s2 : g.world.nearSites)
    if (isBoardSite(g, s2)) {
      const float d = std::hypot((float)(g.world.sites[(size_t)s2].ex - g.pl().p.x / TILE), (float)(g.world.sites[(size_t)s2].ey - g.pl().p.y / TILE));
      if (d < bd) { bd = d; si = s2; }
    }
  if (sub == 0 && si >= 0) {
    Actor A;
    A.npc = true; A.human = true; A.site = si; A.bldg = -1; A.slot = BOARD_SLOT0 + arg; A.name = "THE NOTICE BOARD"; A.role = Role::Guard;
    bool ok = false;
    Quest q = g.offerFor(A, arg == 0 ? QType::Bounty : QType::Hunt, ok);
    if (!ok) q = g.offerFor(A, QType::COUNT, ok);
    if (ok) {
      q.giverName = "THE NOTICE BOARD";
      g.debugAccept(q);
      g.dlg.text = "YOU TEAR THE NOTICE FROM ITS NAIL: " + q.desc + " REWARD: " + std::to_string(q.gold) + " GOLD, PAID HERE.";
    }
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.opts[i].action == DLG_STORY + SA_BOARD && g.dlg.opts[i].arg == arg) { g.dlg.opts.erase(g.dlg.opts.begin() + (std::ptrdiff_t)i); break; }
    return true;
  }
  if (sub == 1) {
    if (host().complete) host().complete(g, arg);
    g.dlg.text = "THE CLERK'S STRONGBOX UNDER THE BOARD OPENS FOR THE MARK ON YOUR NOTICE. THE TOWN PAYS ITS DEBTS.";
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.opts[i].action == DLG_STORY + SA_BOARD + 1 && g.dlg.opts[i].arg == arg) { g.dlg.opts.erase(g.dlg.opts.begin() + (std::ptrdiff_t)i); break; }
    return true;
  }
  if (sub == 2) {
    for (const realm::WorldEvent& e : g.realm.events())
      if ((int)e.id == arg) {
        const std::string l = hearEvent(g, e, 0);
        g.dlg.text = "SEALED WITH RED WAX AND NAILED TWICE: " + l;
        break;
      }
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.opts[i].action == DLG_STORY + SA_BOARD + 2) { g.dlg.opts.erase(g.dlg.opts.begin() + (std::ptrdiff_t)i); break; }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- heralds
namespace {
int heraldOf(const Game& g) {
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].role == Role::Herald && g.actors[k].slot == HERALD_SLOT && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}
}  // namespace

void spawnHeralds(Game& g) {
  if (g.inside || !g.world.endless || !host().spawnHuman) return;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  // one herald at a time: the nearest capital's (capitals lie hundreds of tiles apart)
  int cap = -1;
  float bd = 1e9f;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Site& s = g.world.sites[(size_t)si];
    if (!s.capital || !s.settlement()) continue;
    const float d = std::hypot(s.ex - px, s.ey - py);
    if (d < bd) { bd = d; cap = si; }
  }
  const int have = heraldOf(g);
  if (cap < 0 || bd > 70) {
    if (have >= 0 && !(g.mode == Mode::Dialogue && g.dlg.actor == g.actors[(size_t)have].id)) g.actors.erase(g.actors.begin() + have);
    return;
  }
  if (have >= 0) return;
  const Site S = g.world.sites[(size_t)cap];
  // by the seat of power: a few steps in front of its door (else the square)
  int ax = S.ex, ay = S.ey;
  for (int bi = S.bldgFirst; bi < S.bldgFirst + S.bldgCount && bi < (int)g.world.over.bldgs.size(); bi++) {
    const Bldg& B = g.world.over.bldgs[(size_t)bi];
    if (bldgIsRoyalSeat(B)) { ax = B.doorX(); ay = B.doorY() + 3; break; }
  }
  int tx = 0, ty = 0;
  if (!freeTileNear(g, ax, ay, tx, ty, (uint32_t)S.id) || !g.world.over.in(tx, ty)) return;
  Spawn sp;
  sp.npc = true; sp.role = Role::Herald; sp.site = cap; sp.slot = HERALD_SLOT; sp.x = tx; sp.y = ty;
  const int id = host().spawnHuman(g, sp, tx * TILE + 8.0f, ty * TILE + 10.0f);
  const int ai = host().findActor(g, id);
  if (ai < 0) return;
  Actor& a = g.actors[(size_t)ai];
  a.fromMap = false;
  a.site = -1;   // (not a townsperson: no jobs, no streaming; the herald is found by HERALD_SLOT)
  a.slot = HERALD_SLOT;
  a.realm = S.kingdom >= 0 ? g.world.kingdoms[(size_t)S.kingdom].id : 0;
  a.name = "ROYAL HERALD";
}

void heraldsProclaim(Game& g) {
  const int hi = heraldOf(g);
  const realm::WorldEvent* next = nullptr;
  for (const realm::WorldEvent& e : g.realm.events()) {
    if (e.id <= g.story.proclaimed_) continue;
    const bool kind = e.type == realm::EvType::WarDeclared || e.type == realm::EvType::Peace || e.type == realm::EvType::Succession ||
                      e.type == realm::EvType::KingdomFell;
    if (!kind || g.day - (int)e.day > 10) continue;
    next = &e;
    break;
  }
  if (!next) {
    // nothing to proclaim: the serial catches up with the log (old news is told by innkeepers, not heralds)
    if (!g.realm.events().empty()) {
      uint32_t last = g.story.proclaimed_;
      for (const realm::WorldEvent& e : g.realm.events()) {
        const bool kind = e.type == realm::EvType::WarDeclared || e.type == realm::EvType::Peace || e.type == realm::EvType::Succession ||
                          e.type == realm::EvType::KingdomFell;
        if (e.id > last && (!kind || g.day - (int)e.day > 10)) last = e.id;
        else if (kind && e.id > last) break;
      }
      g.story.proclaimed_ = last;
    }
    return;
  }
  if (hi < 0 || g.inside) return;
  Actor& h = g.actors[(size_t)hi];
  if (len2(h.p - g.pl().p) > (36.0f * TILE) * (36.0f * TILE)) return;
  g.story.proclaimed_ = next->id;
  const char* shout = next->type == realm::EvType::WarDeclared ? "HEAR YE! WAR!" : next->type == realm::EvType::Peace ? "HEAR YE! PEACE!"
                      : next->type == realm::EvType::Succession ? "HEAR YE! A NEW RULER!" : "HEAR YE! A REALM HAS FALLEN!";
  Event e;
  e.type = Ev::Text; e.p = h.p + Vec2(0, -26); e.a = (int)rgba(255, 230, 140); e.f = 0; e.s = shout;
  g.events.push_back(e);
  if (host().sound) { host().sound(g, (int)Sfx::LevelUp, 0.72f, 0.9f); host().sound(g, (int)Sfx::Bell, 1.2f, 0.6f); }
  const std::string line = hearEvent(g, *next, 0);
  // (fixer M4 r1) the herald's own line tells it: no second "NEWS:" toast saying the same thing under it
  if (!g.events.empty() && g.events.back().type == Ev::News) g.events.pop_back();
  // (fixer M4 r2) the whole proclamation: toasts wrap onto two lines; only a very long one is cut, at a word, with "..."
  std::string said = line;
  if (said.size() > 110) {
    size_t cut = said.rfind(' ', 106);
    if (cut == std::string::npos || cut < 60) cut = 106;
    said = said.substr(0, cut);
    while (!said.empty() && (said.back() == ',' || said.back() == ' ')) said.pop_back();
    said += "...";
  }
  noticeEv(g, "THE HERALD: " + said);
}

void heraldTalk(Game& g, Actor& a) {
  int32_t gx, gy;
  playerGlobal(g, gx, gy);
  const ew::Gid k = a.realm ? a.realm : landAt(g, gx, gy);
  std::string rn, rt;
  const bool ruler = g.story.rulerOf(g, k, rn, rt);
  const std::string realmName = kingdomName(g, k);
  const realm::WorldEvent* fresh = nullptr;
  for (const realm::WorldEvent& e : g.realm.events()) {
    const bool kind = e.type == realm::EvType::WarDeclared || e.type == realm::EvType::Peace || e.type == realm::EvType::Succession ||
                      e.type == realm::EvType::SiegeBegun || e.type == realm::EvType::TownTaken;
    if (kind && (e.a == k || e.b == k) && g.day - (int)e.day <= 20 && (!fresh || e.id > fresh->id)) fresh = &e;
  }
  if (fresh) g.dlg.text = "HEAR YE, HEAR YE! " + hearEvent(g, *fresh, 0) + " SO IT IS PROCLAIMED IN THE NAME OF THE " + (ruler ? rt : std::string("CROWN")) + ".";
  else if (ruler) g.dlg.text = "HEAR YE! THE " + rt + " " + rn + " RULES " + realmName + " IN PEACE AND PLENTY, AND THE ROADS ARE KEPT. LONG MAY " + rt + " " + rn + " REIGN!";
  else g.dlg.text = "HEAR YE! THE ROADS ARE KEPT AND THE GATES ARE OPEN. GOD SAVE THE CROWN!";
  g.dlg.opts.push_back({"WHAT NEWS FROM THE REALM?", DLG_STORY + SA_HERALD + 0, 0});
  if (ruler) {
    std::string l = "TELL ME OF THE " + rt + ".";
    if (l.size() > 40) l = "TELL ME OF YOUR RULER.";
    g.dlg.opts.push_back({l, DLG_STORY + SA_HERALD + 1, 0});
  }
  // the campaign may begin with the herald (a capital's crisis is the crown's to proclaim)
  int cap = -1;
  float bd = 1e9f;
  for (int si : g.world.nearSites)
    if (si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].capital) {
      const float d = len2(Vec2(g.world.sites[(size_t)si].ex * (float)TILE, g.world.sites[(size_t)si].ey * (float)TILE) - a.p);
      if (d < bd) { bd = d; cap = si; }
    }
  if (cap >= 0) {
    const int st = g.story.offerForPlace(g, (int)dsl::HookKind::Herald, cap);
    if (st >= 0) {
      const dsl::Script& sc = dsl::library().scripts[(size_t)st];
      if (!sc.hint.empty()) g.dlg.text = sc.hint;
      g.dlg.opts.insert(g.dlg.opts.begin(), {sc.pitch, DLG_STORY + SA_START + st, cap});
    }
  }
}

}  // namespace story
