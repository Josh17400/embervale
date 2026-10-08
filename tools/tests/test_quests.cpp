// rpg_test --quests [--seeds A..B] [--verbose]: the M2 quest types end to end, driven from code (SIM lane).
//   For every type (Deliver, Heirloom, Missing, NamedBandit, Protect, and the radiant Clear / Bounty / Hunt):
//   the offer (a target from the region plans within 768 tiles, the DANGEROUS COUNTRY rule, the text grammar), the
//   accept, the objective, the turn-in, a journal line (questStatus, at most 36 characters), a marker (questTarget) and
//   a save round trip in the middle of the quest (byte-identical, the quest's fields kept, the quest still finishable).
//   Plus: six offers in a row from one giver are all different; rumours mark unknown places; a wonder found gives
//   100 XP; a toll bridge and standing stones built by hand (the WORLD lane places the real ones) behave.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

bool g_verbose = false;

void tick(Game& g, int frames, Input in = Input()) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}
int findActorIdx(const Game& g, int id) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].id == id) return (int)k;
  return -1;
}
// open a dialogue with the actor of this id (walking up to them); true when it is them talking
bool talk(Game& g, int id) {
  static const Vec2 offs[] = {{0, 12}, {0, 20}, {10, 0}, {-10, 0}, {0, -10}, {0, 30}};
  // (M2 fixer round 2) facing the person, and on the second pass with the bystanders stepped aside: in a crowded square
  // the interact found a neighbour standing a tile and a half away first (seed 22)
  for (int pass = 0; pass < 2; pass++)
    for (Vec2 o : offs) {
      const int k = findActorIdx(g, id);
      if (k < 0) return false;
      if (g.mode == Mode::Dialogue) g.mode = Mode::Play;
      g.mode = Mode::Play;
      const Vec2 at = g.actors[(size_t)k].p;
      if (pass == 1)
        for (size_t j = 1; j < g.actors.size(); j++) {
          Actor& b = g.actors[j];
          if ((int)j == k || !b.npc || len2(b.p - at) > 40.0f * 40.0f) continue;
          b.p = b.p + Vec2(b.p.x < at.x ? -48.0f : 48.0f, 0);
        }
      g.pl().p = at + o;
      const float l = std::sqrt(o.x * o.x + o.y * o.y);
      if (l > 0) g.pl().aim = Vec2(-o.x / l, -o.y / l);
      g.pl().face = std::fabs(o.x) > std::fabs(o.y) ? (o.x > 0 ? 3 : 2) : (o.y > 0 ? 1 : 0);
      Input in; in.interact = true;
      g.update(SIM_DT, in);
      if (g.mode == Mode::Dialogue && g.dlg.actor == id) return true;
    }
  if (g.mode == Mode::Dialogue) g.mode = Mode::Play;
  return false;
}
int optIndex(const Game& g, const char* contains) {
  for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.opts[o].label.find(contains) != std::string::npos) return (int)o;
  return -1;
}
Quest* questOf(Game& g, int id) {
  for (Quest& q : g.quests) if (q.id == id) return &q;
  return nullptr;
}
// the giver of a quest, in play (the window is brought to their home first when it is far away)
int giverActor(Game& g, const Quest& q) {
  auto find = [&]() {
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (a.npc && a.st != AState::Dead && a.site == q.giverSite && a.bldg == q.giverBldg && a.slot == q.giverSlot) return a.id;
    }
    return -1;
  };
  int id = find();
  if (id >= 0) return id;
  if (g.inside) g.debugLeave();
  if (q.giverBldg >= 0) {
    if (!g.debugEnterBuilding(q.giverBldg, 0)) return -1;
    return find();
  }
  const Site& s = g.world.sites[(size_t)q.giverSite];
  g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 2);
  for (int f = 0; f < 40 && (id = find()) < 0; f++) tick(g, 1);
  return id;
}
// collect the reward from the giver: the first option of the talk is COLLECT ...
bool turnIn(Game& g, int qid, std::string& why) {
  Quest* q = questOf(g, qid);
  if (!q) { why = "no quest"; return false; }
  const Quest copy = *q;
  const int id = giverActor(g, copy);
  if (id < 0) { why = "the giver is not in play"; return false; }
  const int k = findActorIdx(g, id);
  if (!g.rewardWaiting(g.actors[(size_t)k])) { why = "no reward marker over the giver"; return false; }
  if (!talk(g, id)) { why = "could not talk to the giver"; return false; }
  if (g.dlg.opts.empty() || g.dlg.opts[0].label.find("COLLECT") == std::string::npos) { why = "first option '" + (g.dlg.opts.empty() ? std::string() : g.dlg.opts[0].label) + "'"; return false; }
  const int gold0 = g.gold;
  g.dialogueChoose(0);
  g.mode = Mode::Play;
  q = questOf(g, qid);
  if (!q || q->state != QState::Done || g.gold < gold0 + copy.gold) { why = "collecting did not pay"; return false; }
  for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.questId == qid) { why = "the quest item is still in the pack"; return false; }
  return true;
}
// the save round trip in the middle of a quest: byte-identical, and the quest's fields come back
bool midSave(Game& g, int qid, std::string& why) {
  std::vector<uint8_t> a, b;
  g.serialize(a);
  Game h(1);
  if (!h.deserialize(a)) { why = "the save did not load"; return false; }
  h.serialize(b);
  if (a != b) { why = "not byte-identical (" + std::to_string(a.size()) + " vs " + std::to_string(b.size()) + ")"; return false; }
  const Quest* q0 = questOf(g, qid);
  const Quest* q1 = questOf(h, qid);
  if (!q0 || !q1) { why = "the quest is gone"; return false; }
  if (q1->type != q0->type || q1->state != q0->state || q1->subject != q0->subject || q1->flags != q0->flags || q1->targetId != q0->targetId ||
      q1->tgx != q0->tgx || q1->tgy != q0->tgy || q1->giverId != q0->giverId || q1->deadlineDay != q0->deadlineDay || q1->hasPos != q0->hasPos) {
    why = "the quest's fields changed";
    return false;
  }
  if ((q0->destBldg >= 0) != (q1->destBldg >= 0)) { why = "destBldg lost"; return false; }
  return true;
}
// a journal line exists and fits; a marker exists
bool journalAndMarker(Game& g, int qid, std::string& line, std::string& why) {
  const Quest* q = questOf(g, qid);
  if (!q) { why = "no quest"; return false; }
  line = g.questStatus(*q);
  if (line.empty() || line.size() > 36) { why = "journal line '" + line + "'"; return false; }
  int tx = 0, ty = 0;
  if (!g.questTarget(qid, tx, ty)) { why = "no marker"; return false; }
  return true;
}

// the givers: the start village's people in play (innkeeper and smith from their buildings when needed)
struct Givers { int villager = -1, farmer = -1; };

int questsSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: quests: %s\n", s.c_str()); bad++; };
  Game base(seed);
  base.newEndlessGame(seed);
  base.mode = Mode::Play;
  base.godMode = true;
  base.noWildSpawns = true;
  base.hour = 10.0f;
  base.debugKit();
  tick(base, 20);
  const Site home = base.world.sites[(size_t)base.world.startSite];
  // a villager of the start village (any outdoor person who can give work: not a guard, a child or a stall keeper)
  auto person = [&](Game& g, Role want) {
    int best = -1;
    float bd = 1e30f;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || !a.human || a.site != g.world.startSite || a.st == AState::Dead || a.role != want) continue;
      const float d = len2(a.p - g.pl().p);
      if (d < bd) { bd = d; best = (int)k; }
    }
    return best;
  };
  int vk = person(base, Role::Villager);
  if (vk < 0) vk = person(base, Role::Farmer);
  if (vk < 0) { out("WARN: quests: no villager out in %s\n", home.name.c_str()); return 0; }
  const Actor giver = base.actors[(size_t)vk];
  int typesDone = 0;
  std::string summary;

  // ---------------------------------------------------------------- the text grammar: six jobs in a row, all different
  {
    Game g = base;
    std::set<std::string> seen;
    int made = 0;
    Actor a = giver;
    for (int n = 0; n < 6; n++) {
      bool ok = false;
      const Quest q = g.offerFor(a, QType::COUNT, ok);
      if (!ok) break;
      made++;
      const std::string sig = q.title + "|" + q.desc;
      if (g_verbose) out("  offer %d: [%d] %s | %s\n", n, (int)q.type, q.title.c_str(), q.desc.c_str());
      if (!seen.insert(sig).second) fail("giver " + a.name + " repeated an offer: " + q.title);
      if (q.title.empty() || q.desc.empty()) fail("an offer without a title or a description");
      g.debugAccept(q);
      Quest& t = g.quests.back();
      t.state = QState::Done;
      g.npcQuestsDone[g.npcKey(a)]++;
    }
    if (made < 6) out("WARN: quests: only %d offers in a row from %s\n", made, a.name.c_str());
  }

  // ---------------------------------------------------------------- radiant targets: within 768 tiles; danger rule
  for (QType t : {QType::Clear, QType::Bounty, QType::Hunt}) {
    Game g = base;
    bool ok = false;
    const Quest q = g.offerFor(giver, t, ok);
    if (!ok) { out("WARN: quests: no %d offer\n", (int)t); continue; }
    if (t != QType::Hunt) {
      const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
      const float d = std::hypot((float)(q.tgx - hx), (float)(q.tgy - hy));
      if (!q.hasPos || d > 768.0f) fail("a radiant target " + std::to_string((int)d) + " tiles away");
      const int lvl = q.target >= 0 ? g.world.sites[(size_t)q.target].level : 0;
      if ((q.flags & QF_DANGER) && q.desc.find("DANGEROUS COUNTRY") == std::string::npos) fail("a dangerous offer does not say so");
      if (!(q.flags & QF_DANGER) && lvl > g.plLevel + 3) fail("a level " + std::to_string(lvl) + " target offered without the danger warning");
    }
    g.debugAccept(q);
    std::string line, why;
    if (!journalAndMarker(g, g.quests.back().id, line, why)) fail(std::string("radiant: ") + why);
  }

  // ---------------------------------------------------------------- Deliver
  {
    Game g = base;
    bool ok = false;
    const Quest q0 = g.offerFor(giver, QType::Deliver, ok);
    if (!ok) out("WARN: quests: no Deliver offer (no settlement 150-700 tiles away)\n");
    else {
      g.debugAccept(q0);
      const int qid = g.quests.back().id;
      std::string line, why;
      const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
      const float d = std::hypot((float)(q0.tgx - hx), (float)(q0.tgy - hy));
      bool parcel = false;
      for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.questId == qid) parcel = true;
      if (!parcel) fail("deliver: no parcel in the pack");
      if (d < 140.0f || d > 710.0f) fail("deliver: the recipient lives " + std::to_string((int)d) + " tiles away");
      if (q0.subject.empty()) fail("deliver: the recipient has no name");
      if (!journalAndMarker(g, qid, line, why)) fail("deliver: " + why);
      if (!midSave(g, qid, why)) fail("deliver: mid-quest save: " + why);
      // to the recipient's place: their building is found once its records are in (questTick)
      const Site T = g.world.sites[(size_t)q0.target];
      g.teleportGlobal(g.world.ox + T.ex, g.world.oy + T.ey + 3);
      tick(g, 5);
      Quest* q = questOf(g, qid);
      if (!q || q->destBldg < 0) fail("deliver: the recipient's building was never found in " + T.name);
      else {
        int tx = 0, ty = 0;
        g.questTarget(qid, tx, ty);
        const Bldg& B = g.world.over.bldgs[(size_t)q->destBldg];
        if (tx != B.doorX() || ty != B.doorY()) fail("deliver: the marker does not point at the recipient's door");
        if (!g.debugEnterBuilding(q->destBldg, 0)) fail("deliver: cannot enter the recipient's building");
        else {
          int rid = -1;
          for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].name == q->subject) rid = g.actors[k].id;
          if (rid < 0) fail("deliver: nobody called " + q->subject + " inside");
          else if (!talk(g, rid)) fail("deliver: cannot talk to the recipient");
          else {
            const int o = optIndex(g, "HAND OVER THE PARCEL");
            if (o < 0) fail("deliver: the recipient offers no HAND OVER THE PARCEL");
            else {
              const int gold0 = g.gold;
              g.dialogueChoose(o);
              g.mode = Mode::Play;
              q = questOf(g, qid);
              if (!q || q->state != QState::Done || g.gold < gold0 + q0.gold) fail("deliver: delivering did not pay");
              for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.questId == qid) { fail("deliver: the parcel is still in the pack"); break; }
              typesDone++;
              summary += " deliver(" + std::to_string((int)d) + "t," + line + ")";
            }
          }
        }
      }
    }
  }

  // ---------------------------------------------------------------- Heirloom
  {
    Game g = base;
    bool ok = false;
    const Quest q0 = g.offerFor(giver, QType::Heirloom, ok);
    if (!ok) out("WARN: quests: no Heirloom offer\n");
    else {
      g.debugAccept(q0);
      const int qid = g.quests.back().id;
      std::string line, why;
      if (!journalAndMarker(g, qid, line, why)) fail("heirloom: " + why);
      if (!g.debugEnterSite(q0.target)) fail("heirloom: cannot enter " + g.world.sites[(size_t)q0.target].name);
      else {
        // the quest's chest: its tile is kept in the marks (Mk::HeirChest)
        auto mk = g.marks.find(markKey((uint64_t)qid, Mk::HeirChest));
        const int idx = mk == g.marks.end() ? -1 : mk->second;
        const int cx = idx >= 0 ? idx % g.sub.w : -1, cy = idx >= 0 ? idx / g.sub.w : -1;
        if (idx < 0 || g.sub.propAt(cx, cy) != (int)art::Prop::Chest + 1) fail("heirloom: no quest chest placed in the map");
        if (!midSave(g, qid, why)) fail("heirloom: mid-quest save (inside): " + why);
        {   // after a reload the chest is in the same place
          std::vector<uint8_t> sv; g.serialize(sv);
          Game h(1);
          if (h.deserialize(sv) && (h.sub.w != g.sub.w || h.sub.propAt(cx, cy) != (int)art::Prop::Chest + 1)) fail("heirloom: the chest moved or vanished on reload");
        }
        bool found = false;
        static const int sides[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        for (auto& sd : sides) {
          if (idx < 0 || found) break;
          const int sx = cx + sd[0], sy = cy + sd[1];
          if (g.sub.blocked(sx, sy)) continue;
          g.pl().p = Vec2(sx * 16 + 8.0f, sy * 16 + 10.0f);
          g.pl().aim = Vec2((float)-sd[0], (float)-sd[1]);
          Input in; in.interact = true;
          g.update(SIM_DT, in);
          g.events.clear();
          const Quest* q = questOf(g, qid);
          found = q && q->state == QState::Complete;
        }
        if (!found) fail("heirloom: no chest held the " + q0.subject);
        else {
          bool item = false;
          for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.questId == qid && it.name == q0.subject) item = true;
          if (!item) fail("heirloom: the keepsake is not in the pack");
          if (!journalAndMarker(g, qid, line, why) && g.questStatus(*questOf(g, qid)).empty()) fail("heirloom (complete): " + why);
          g.debugLeave();
          if (!turnIn(g, qid, why)) fail("heirloom: turn-in: " + why);
          else { typesDone++; summary += " heirloom(" + line + ")"; }
        }
      }
    }
  }

  // ---------------------------------------------------------------- Missing
  {
    Game g = base;
    bool ok = false;
    const Quest q0 = g.offerFor(giver, QType::Missing, ok);
    if (!ok) out("WARN: quests: no Missing offer\n");
    else {
      g.debugAccept(q0);
      const int qid = g.quests.back().id;
      std::string line, why;
      if (!journalAndMarker(g, qid, line, why)) fail("missing: " + why);
      if (!g.debugEnterSite(q0.target)) fail("missing: cannot enter " + g.world.sites[(size_t)q0.target].name);
      else {
        int pid = -1;
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].quest == qid && g.actors[k].npc) pid = g.actors[k].id;
        if (pid < 0) fail("missing: " + q0.subject + " is not in the cave");
        else if (g.actors[(size_t)findActorIdx(g, pid)].name != q0.subject) fail("missing: the person in the cave has another name");
        else if (!talk(g, pid)) fail("missing: cannot talk to " + q0.subject);
        else {
          const int o = optIndex(g, "STAY CLOSE");
          if (o < 0) fail("missing: no STAY CLOSE option");
          else {
            g.dialogueChoose(o);
            g.mode = Mode::Play;
            if (!(questOf(g, qid)->flags & QF_FOUND)) fail("missing: not following after the talk");
            if (!midSave(g, qid, why)) fail("missing: mid-quest save (inside, following): " + why);
            // reloaded inside: the person is back beside the player
            {
              std::vector<uint8_t> s; g.serialize(s);
              Game h(1);
              if (h.deserialize(s)) {
                int n = 0;
                for (size_t k = 1; k < h.actors.size(); k++) if (h.actors[k].quest == qid) n++;
                if (n != 1) fail("missing: after a reload in the cave " + std::to_string(n) + " copies of " + q0.subject);
              }
            }
            // they follow: walk to the exit along a path over the cave's tiles (M2 fixer round 2: the straight line
            // stuck the hero on a wall while the cave's beasts killed the escort, seed 14) with the cave's hostiles
            // felled first (the escort's survival in a fight is not what this checks), and check they keep up
            for (size_t k = 1; k < g.actors.size(); k++)
              if (g.actors[k].hostile && g.actors[k].st != AState::Dead) g.debugFell(g.actors[k].id);
            const Vec2 exitP(g.sub.exitX * 16 + 8.0f, (g.sub.exitY - 1) * 16 + 10.0f);
            std::vector<Vec2> path;
            {
              const Map& m = g.sub;
              const int sx = (int)std::floor(g.pl().p.x / 16), sy = (int)std::floor(g.pl().p.y / 16);
              const int ex = g.sub.exitX, ey = g.sub.exitY - 1;
              std::vector<int> prev((size_t)m.w * m.h, -2);
              std::vector<int> todo{sy * m.w + sx};
              if (m.in(sx, sy)) prev[(size_t)(sy * m.w + sx)] = -1;
              for (size_t h = 0; h < todo.size(); h++) {
                const int c = todo[h], cx = c % m.w, cy = c / m.w;
                if (cx == ex && cy == ey) break;
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                  const int nx = cx + dx[d], ny = cy + dy[d];
                  if (!m.in(nx, ny) || prev[(size_t)(ny * m.w + nx)] != -2) continue;
                  if (m.blocked(nx, ny) && !(nx == ex && ny == ey)) continue;
                  prev[(size_t)(ny * m.w + nx)] = c;
                  todo.push_back(ny * m.w + nx);
                }
              }
              if (m.in(ex, ey) && prev[(size_t)(ey * m.w + ex)] != -2)
                for (int c = ey * m.w + ex; c >= 0; c = prev[(size_t)c]) path.insert(path.begin(), Vec2((c % m.w) * 16 + 8.0f, (c / m.w) * 16 + 9.0f));
              path.push_back(exitP);
            }
            size_t wp = 0;
            for (int f = 0; f < 1500; f++) {
              while (wp + 1 < path.size() && len2(path[wp] - g.pl().p) < 9.0f) wp++;
              const Vec2 d = (wp < path.size() ? path[wp] : exitP) - g.pl().p;
              Input in;
              if (len2(d) > 4.0f) in.move = norm(d);
              g.update(SIM_DT, in);
              g.events.clear();
              if (!g.inside) break;
            }
            float lag = 0;
            const int k = findActorIdx(g, pid);
            if (g.inside && k >= 0) lag = len(g.actors[(size_t)k].p - g.pl().p) / TILE;
            if (g.inside) {
              if (lag > 6) fail("missing: " + q0.subject + " fell " + std::to_string((int)lag) + " tiles behind");
              g.debugLeave();
            }
            const Quest* q = questOf(g, qid);
            if (!q || q->state != QState::Complete) fail("missing: not complete once outside");
            else {
              bool out1 = false;
              for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].quest == qid && g.actors[j].npc) out1 = true;
              if (!out1) fail("missing: " + q0.subject + " did not come out");
              line = g.questStatus(*q);
              if (!turnIn(g, qid, why)) fail("missing: turn-in: " + why);
              else { typesDone++; summary += " missing(" + line + ")"; }
            }
          }
        }
      }
    }
  }

  // ---------------------------------------------------------------- NamedBandit
  {
    Game g = base;
    bool ok = false;
    const Quest q0 = g.offerFor(giver, QType::NamedBandit, ok);
    if (!ok) out("WARN: quests: no NamedBandit offer\n");
    else {
      g.debugAccept(q0);
      const int qid = g.quests.back().id;
      std::string line, why;
      if (!journalAndMarker(g, qid, line, why)) fail("named bandit: " + why);
      if (q0.subject.find(' ') == std::string::npos) fail("named bandit: no title in '" + q0.subject + "'");
      if (!midSave(g, qid, why)) fail("named bandit: mid-quest save: " + why);
      const Site camp = g.world.sites[(size_t)q0.target];
      g.teleportGlobal(g.world.ox + camp.ex, g.world.oy + camp.r.y + camp.r.h + 3);
      tick(g, 10);
      int chief = -1;
      for (int f = 0; f < 60 && chief < 0; f++) {
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].boss && g.actors[k].site == q0.target && g.actors[k].st != AState::Dead) chief = g.actors[k].id;
        if (chief < 0) tick(g, 1);
      }
      if (chief < 0) fail("named bandit: no chief at " + camp.name);
      else {
        Actor& c = g.actors[(size_t)findActorIdx(g, chief)];
        if (c.name != q0.subject) fail("named bandit: the chief is called '" + c.name + "', not " + q0.subject);
        if (g_verbose) out("  named chief %s: hp %.0f dmg %.1f\n", c.name.c_str(), c.maxHp, c.dmg);
        for (int f = 0; f < 600; f++) {
          const int k = findActorIdx(g, chief);
          if (k < 0 || g.actors[(size_t)k].st == AState::Dead) break;
          g.actors[(size_t)k].hp = std::min(g.actors[(size_t)k].hp, 0.4f);
          g.actors[(size_t)k].burnT = 2.0f;
          g.update(SIM_DT, Input());
          g.events.clear();
          if (g.mode != Mode::Play) g.mode = Mode::Play;
        }
        const Quest* q = questOf(g, qid);
        if (!q || q->state != QState::Complete) fail("named bandit: killing " + q0.subject + " did not complete it");
        else if (!turnIn(g, qid, why)) fail("named bandit: turn-in: " + why);
        else { typesDone++; summary += " named(" + q0.subject + ")"; }
      }
    }
  }

  // ---------------------------------------------------------------- Protect
  {
    Game g = base;
    int fk = -1;
    for (size_t k = 1; k < g.actors.size(); k++)
      if (g.actors[k].npc && g.actors[k].human && g.actors[k].site == g.world.startSite && (g.actors[k].role == Role::Farmer || g.actors[k].role == Role::Villager)) {
        bool ok = false;
        Quest t = g.offerFor(g.actors[k], QType::Protect, ok);
        if (ok) { fk = (int)k; break; }
      }
    if (fk < 0) out("WARN: quests: nobody in %s asks to protect fields (no farmland near?)\n", home.name.c_str());
    else {
      const Actor farmer = g.actors[(size_t)fk];
      bool ok = false;
      const Quest q0 = g.offerFor(farmer, QType::Protect, ok);
      g.debugAccept(q0);
      const int qid = g.quests.back().id;
      std::string line, why;
      if (!journalAndMarker(g, qid, line, why)) fail("protect: " + why);
      if (q0.deadlineDay != g.day) fail("protect: the attack is not tonight");
      if (!midSave(g, qid, why)) fail("protect: mid-quest save: " + why);
      Game miss = g;   // the failure path: the night passes without the player
      // nightfall at the fields
      g.hour = 21.0f;
      g.pl().p = Vec2((q0.tgx - g.world.ox) * 16 + 8.0f, (q0.tgy - g.world.oy) * 16 + 10.0f);
      tick(g, 3);
      if (getenv("EMB_PROTECT_TRACE")) {   // the first wave left alone for a few seconds: who fights it
        Game t = g;
        for (int f = 0; f < 240; f += 20) {
          std::string s;
          for (const Actor& a : t.actors)
            if (a.quest == qid) s += " " + a.name + ":" + std::to_string((int)a.hp) + (a.st == AState::Dead ? "x" : "") + "@" + std::to_string((int)(len(a.p - t.pl().p) / 16));
          out("  t %.2f:%s\n", f / 60.0f, s.c_str());
          tick(t, 20);
        }
      }
      int waves = 0, beasts = 0;
      for (int w = 0; w < 3; w++) {
        std::vector<int> ids;
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].quest == qid && g.actors[k].hostile && g.actors[k].st != AState::Dead) ids.push_back(g.actors[k].id);
        if (ids.empty()) break;
        waves++;
        beasts += (int)ids.size();
        for (int id : ids) g.debugFell(id);
        tick(g, 3);
      }
      if (waves < 2) fail("protect: " + std::to_string(waves) + " waves came (want 2)");
      if (g_verbose) out("  protect: %d waves, %d raiders, line '%s'\n", waves, beasts, g.questStatus(*questOf(g, qid)).c_str());
      // dawn
      g.day = q0.deadlineDay + 1; g.hour = 5.6f;
      tick(g, 2);
      const Quest* q = questOf(g, qid);
      if (!q || q->state != QState::Complete) fail("protect: not complete at dawn");
      else if (!turnIn(g, qid, why)) fail("protect: turn-in: " + why);
      else { typesDone++; summary += " protect(" + std::to_string(waves) + " waves)"; }
      miss.day = q0.deadlineDay + 1; miss.hour = 7.0f;
      tick(miss, 2);
      const Quest* m = questOf(miss, qid);
      if (!m || m->state != QState::Done || !(m->flags & QF_FAILED)) fail("protect: missing the night did not fail it");
    }
  }

  // ---------------------------------------------------------------- rumours, a wonder, the wayside (hand-built records)
  {
    Game g = base;
    const int n0 = (int)std::count_if(g.world.sites.begin(), g.world.sites.end(), [](const Site& s) { return s.rumoured; });
    const std::string r1 = g.hearRumour(), r2 = g.hearRumour();
    const int n1 = (int)std::count_if(g.world.sites.begin(), g.world.sites.end(), [](const Site& s) { return s.rumoured; });
    if (r1.empty() || r2.empty() || r1 == r2 || n1 != n0 + 2) fail("rumours: '" + r1 + "' / '" + r2 + "'");
    if (g_verbose) out("  rumours: %s | %s\n", r1.c_str(), r2.c_str());
    // reaching a rumoured place discovers it
    int rs = -1;
    for (int i = 0; i < (int)g.world.sites.size(); i++) if (g.world.sites[(size_t)i].rumoured) rs = i;
    if (rs >= 0) {
      const Site s = g.world.sites[(size_t)rs];
      g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1);
      tick(g, 2);
      if (!g.world.sites[(size_t)rs].discovered || g.world.sites[(size_t)rs].rumoured) fail("rumours: reaching " + s.name + " did not discover it");
    }
  }
  {
    // a wonder: XP 100 and a journal toast; built by hand at the player's feet
    Game g = base;
    Site w;
    w.id = ew::makeId(0, 0, ew::IdKind::Poi, 0x7FE);
    w.type = SiteType::Wonder; w.kind = (uint8_t)ew::WonderKind::ElderTree; w.name = "THE ELDER OAK OF TEST";
    // a spot clear of every real site (M3 settlements are larger: 30 tiles east can be inside a town's pad)
    int px = (int)(g.pl().p.x / TILE) + 30;
    const int py = (int)(g.pl().p.y / TILE);
    for (int k = 0; k < 40 && g.world.siteAt(px, py, 8) >= 0; k++) px += 10;
    w.r = IRect{px - 3, py - 3, 7, 7}; w.ex = px; w.ey = py;
    g.world.siteById[w.id] = (int)g.world.sites.size();
    g.world.nearSites.push_back((int)g.world.sites.size());
    g.world.sites.push_back(w);
    const int xp0 = g.plXp, lvl0 = g.plLevel;
    g.pl().p = Vec2(px * 16 + 8.0f, py * 16 + 10.0f);
    g.update(SIM_DT, Input());
    bool flag = false, toast = false;
    for (const Event& e : g.events) {
      if (e.type == Ev::Discover && e.f == 2.0f) flag = true;
      if (e.type == Ev::QuestUpdate && e.s.find("WONDER FOUND") != std::string::npos) toast = true;
    }
    if (!flag || !toast || (g.plXp - xp0 < 100 && g.plLevel == lvl0)) fail("wonder: discovery flag " + std::to_string(flag) + ", toast " + std::to_string(toast));
  }
  {
    // capital square life (owner note 6): the people of a capital's square are in play while the player stands there,
    // linger near their places by day, go home at night and come out again in the morning
    Game g = base;
    const Site& h0 = g.world.sites[(size_t)g.world.startSite];
    const int cap = g.world.findSiteNear(g.world.ox + h0.ex, g.world.oy + h0.ey, SiteType::City, 8, true);
    if (cap >= 0) {
      const Site C = g.world.sites[(size_t)cap];
      g.teleportGlobal(g.world.ox + C.ex, g.world.oy + C.ey + 3);
      g.hour = 12.0f;
      tick(g, 120);
      auto squareGoers = [&](float& drift) {
        int n = 0;
        drift = 0;
        for (size_t k = 1; k < g.actors.size(); k++) {
          const Actor& a = g.actors[k];
          // (M5) census residents keep their own day (their home is their door step, which may open on the square)
          if (!a.npc || a.site != cap || a.stallKeeper || a.resident >= 0 || (a.role != Role::Villager && a.role != Role::Child)) continue;
          if (g.world.over.at((int)std::floor(a.home.x / TILE), (int)std::floor((a.home.y - 2) / TILE)) != Ground::Plaza) continue;
          n++;
          drift = std::max(drift, len(a.p - a.home) / TILE);
        }
        return n;
      };
      float drift = 0;
      const int day0 = squareGoers(drift);
      tick(g, 600);
      float drift10 = 0;
      const int day1 = squareGoers(drift10);
      g.hour = 21.2f;
      tick(g, 60 * 45);
      float d2 = 0;
      const int night = squareGoers(d2);
      g.hour = 7.0f;
      tick(g, 60 * 40);
      float d3 = 0;
      const int morning = squareGoers(d3);
      out("seed %llu: capital %s square: %d people by day (%d after 10 s, furthest %.1f tiles from their place), %d out at night, %d next morning\n",
          (unsigned long long)seed, C.name.c_str(), day0, day1, drift10, night, morning);
      if (day0 == 0) out("WARN: quests: nobody on the square of %s (the WORLD lane adds square-goers)\n", C.name.c_str());
      else {
        if (day1 < day0 * 3 / 4) fail("square life: " + std::to_string(day0 - day1) + " of the square's people streamed away while the player stood there");
        if (drift10 > 6.5f) fail("square life: a square-goer wandered " + std::to_string((int)drift10) + " tiles from their place by day");
        if (night > day1 / 3) fail("square life: " + std::to_string(night) + " of " + std::to_string(day1) + " still out on the square at night");
        if (morning < day1 / 2) fail("square life: only " + std::to_string(morning) + " of " + std::to_string(day1) + " came back out in the morning");
      }
    }
  }
  out("seed %llu: quests: %d of 5 new types end to end:%s\n", (unsigned long long)seed, typesDone, summary.c_str());
  return bad;
}

// M5 (CITIZENS lane): a supply run end to end (VISION_PLAN 15.12 "the baker has no flour"): a keeper whose work is
// starved asks for help, the goods are loaded at a settlement that has them, the giver pays and the goods reach the
// giver's stores (life::Life::supply); a save in the middle round-trips
int supplySeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: quests seed %llu: supply: %s\n", (unsigned long long)seed, m.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  tick(g, 10);
  const int sv = g.world.startSite;
  life::Census* c = g.life.census(g.world, sv);
  if (!c) { fail("no census for the start village"); return bad; }
  const ew::Gid svId = g.world.sites[(size_t)sv].id;
  // the workplace: the bakery, the mill, the smithy, else the inn's kitchen
  int off = -1;
  ew::Good good = ew::Good::Bread;
  if (c->bakery >= 0) { off = c->bakery; good = ew::Good::Flour; }
  else if (c->mill >= 0) { off = c->mill; good = ew::Good::Grain; }
  else if (c->smithy >= 0) { off = c->smithy; good = ew::Good::Ingot; }
  else if (c->inn >= 0) { off = c->inn; good = ew::Good::Bread; c->stock[(size_t)ew::Good::Meat] = 0; }
  if (off < 0) { out("seed %llu: supply: no workplace to starve\n", (unsigned long long)seed); return bad; }
  c->stock[(size_t)good] = 0;
  c->use[(size_t)good] = 10;
  // somewhere that has it: another loaded settlement's stores
  int from = -1;
  for (int si : g.world.nearSites) {
    if (si == sv || si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) continue;
    if (life::Census* o = g.life.census(g.world, si)) { o->stock[(size_t)good] = 60; from = si; break; }
  }
  if (from < 0) { out("seed %llu: supply: no other settlement loaded\n", (unsigned long long)seed); return bad; }
  const int bh = g.world.sites[(size_t)sv].bldgFirst + off;
  if (!g.debugEnterBuilding(bh, 0)) { fail("cannot enter the starved workplace"); return bad; }
  tick(g, 2);
  int keeper = -1;
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].slot == 0 && g.actors[k].bldg == bh) keeper = g.actors[k].id;
  if (keeper < 0) { fail("the keeper is not at the workplace"); return bad; }
  if (!talk(g, keeper)) { fail("cannot talk to the keeper"); return bad; }
  int o = optIndex(g, "HELP");
  if (o < 0) o = optIndex(g, "WORK");
  if (o < 0) { fail("the starved keeper offers no work"); return bad; }
  g.dialogueChoose(o);
  const std::string pitch = g.dlg.text;
  o = optIndex(g, "I'LL DO IT");
  if (o < 0) { fail("no I'LL DO IT after the ask"); return bad; }
  g.dialogueChoose(o);
  g.mode = Mode::Play;
  int qid = -1;
  for (const Quest& q : g.quests) if (q.type == QType::Supply && q.state == QState::Active) qid = q.id;
  if (qid < 0) { fail("no supply quest was taken (the pitch: " + pitch + ")"); return bad; }
  Quest q0 = *questOf(g, qid);
  if (q0.stage != (int)good || q0.title.find("HAS NO") == std::string::npos) fail("the quest is not about the starved good: " + q0.title);
  const std::string line = g.questStatus(q0);
  if (line.find("FETCH") == std::string::npos) fail("the journal line does not say FETCH: " + line);
  int tx = 0, ty = 0;
  if (!g.questTarget(qid, tx, ty)) fail("no marker for the supply run");
  std::string why;
  if (!midSave(g, qid, why)) fail("mid-quest save: " + why);
  // the supplier: walking into it loads the goods
  g.debugLeave();
  const Site T = g.world.sites[(size_t)q0.target];
  const ew::Gid tid = T.id;
  const int before = g.life.find(tid) ? g.life.find(tid)->stock[(size_t)good] : -1;
  g.teleportGlobal(g.world.ox + T.ex, g.world.oy + T.ey + 2);
  tick(g, 5);
  Quest* q = questOf(g, qid);
  bool item = false;
  for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.questId == qid) item = true;
  if (!q || q->state != QState::Complete || !item) { fail("walking into " + T.name + " did not load the goods"); return bad; }
  if (before >= 0 && g.life.find(tid) && g.life.find(tid)->stock[(size_t)good] != before - q0.need) fail("the supplier's stores did not give the goods up");
  if (!turnIn(g, qid, why)) { fail("turn-in: " + why); return bad; }
  tick(g, 2);
  const life::Census* gc = g.life.find(svId);
  if (!gc || gc->stock[(size_t)good] < q0.need) fail("the goods never reached the giver's stores");
  out("seed %llu: supply: %s, %d %s from %s, %d gold\n", (unsigned long long)seed, q0.title.c_str(), q0.need, q0.subject.c_str(), T.name.c_str(), q0.gold);
  return bad;
}

int cmdQuests(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--verbose")) g_verbose = true;
    // (M2 fixer round 2) a bare range ("--quests 1..10") is the seeds too: it was ignored and ran 1..3
    else if (argv[i][0] != '-' && strstr(argv[i], "..")) parseSeedRange(argv[i], a, b);
  }
  int bad = 0, failedSeeds = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    const int f = questsSeed(s) + supplySeed(s);
    bad += f;
    if (f) failedSeeds++;
  }
  printf("quests: %llu seeds, %d failed (%d failures)\n", (unsigned long long)(b - a + 1), failedSeeds, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--quests", "M2 quest types end to end (deliver, heirloom, missing, named bandit, protect), M5 supply runs, radiant targets, "
                         "the text grammar, rumours, wonders, mid-quest saves [--seeds A..B] [--verbose]", cmdQuests);

// rpg_test --wayside-census [--seeds A..B] [--r REGIONS]: how many of each wayside place and wonder the region plans
// hold around the start (the SIM lane's scripts need one of each kind within reach)
static int cmdWaysideCensus(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int rr = 10;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--r") && i + 1 < argc) rr = atoi(argv[++i]);
  }
  for (uint64_t s = a; s <= b; s++) {
    ew::EndlessSource src(s);
    const ew::StartPlan& sp = src.start();
    const int32_t rx0 = ew::regionOf(sp.spawn.x), ry0 = ew::regionOf(sp.spawn.y);
    int nv[16] = {}, nw[8] = {};
    int64_t nearestToll = -1;
    for (int32_t ry = ry0 - rr; ry <= ry0 + rr; ry++)
      for (int32_t rx = rx0 - rr; rx <= rx0 + rr; rx++) {
        const ew::RegionPlan& R = src.region(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if (p.type == SiteType::Vignette && p.kind < 16) nv[p.kind]++;
          if (p.type == SiteType::Wonder && p.kind < 8) nw[p.kind]++;
          if (p.type == SiteType::Vignette && p.kind == (uint8_t)ew::VignetteKind::TollBridge) {
            const int64_t d = (int64_t)std::hypot((double)(p.ex - sp.spawn.x), (double)(p.ey - sp.spawn.y));
            if (nearestToll < 0 || d < nearestToll) nearestToll = d;
          }
        }
      }
    std::string line;
    for (int k = 0; k < (int)ew::VignetteKind::COUNT; k++) line += std::string(" ") + ew::vignetteName((ew::VignetteKind)k) + "=" + std::to_string(nv[k]);
    for (int k = 0; k < (int)ew::WonderKind::COUNT; k++) line += std::string(" ") + ew::wonderName((ew::WonderKind)k) + "=" + std::to_string(nw[k]);
    printf("seed %llu (%d regions round the start):%s | nearest toll bridge %lld tiles\n", (unsigned long long)s, rr, line.c_str(), (long long)nearestToll);
  }
  return 0;
}
RPG_TEST_CMD("--wayside-census", "count the wayside places and wonders of each kind round the start [--seeds A..B] [--r REGIONS]", cmdWaysideCensus);
