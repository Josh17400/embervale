// M12 "Rise and Fall": fief tasks (QType::Fief; rpg/sim/lord_ops.h). LORD lane. The steward speaks of the village's
// needs (lord_talk.cpp) and the lord takes them on as quests: Quest::stage the task id, Quest::targetId the holding,
// the kind in Quest::flags bits 24..27 (lord::Task), Quest::subject the line the journal and the marker read.
//   Unrest    loyalty under 25: hold a feast, or lower the tax until they settle (done at 35)
//   Raid      a siege or raid of the holding: break it (done when the realm says the siege was broken; failed if lost)
//   Camp      a bandit camp within 140 tiles of the holding: clear it (done when the camp's chief falls)
//   Settlers  the village is half empty: bring refugees to settle (a refugee's COME AND SETTLE IN ...: lord_talk.cpp)
#include <algorithm>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/world/source.h"

namespace {
uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
std::string siteName(const Game& g, ew::Gid site) {
  const int si = g.world.siteHandle(site);
  return si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE VILLAGE");
}
Quest* taskQuest(Game& g, uint32_t id) {
  for (Quest& q : g.quests) if (q.type == QType::Fief && q.stage == (int)id) return &q;
  return nullptr;
}
// the nearest bandit camp (not cleared) within 140 tiles of the holding: its Camp record (nullptr none)
const realm::Camp* campNear(const Game& g, const realm::SettlementState& st) {
  const realm::Camp* best = nullptr;
  int64_t bd = 140LL * 140LL;
  for (const realm::Camp& c : g.realm.camps()) {
    if (c.flags & (realm::CF_CLEARED | realm::CF_WARLORD)) continue;
    const int si = g.world.siteHandle(c.site);
    if (si >= 0 && g.world.sites[(size_t)si].cleared) continue;
    const int64_t dx = c.gx - st.gx, dy = c.gy - st.gy, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = &c; }
  }
  return best;
}
}  // namespace

uint32_t LordOps::giveTask(Game& g, lord::Holding& h, lord::Task t) {
  if (h.task) if (Quest* q = taskQuest(g, h.task)) if (q->state != QState::Done) return 0;   // one at a time
  const realm::SettlementState* st = g.realm.settlement(h.site);
  if (!st) return 0;
  lord::Lordship& L = g.lordship;
  const std::string where = siteName(g, h.site);
  Quest q;
  q.type = QType::Fief;
  q.stage = (int)L.nextTask++;
  q.targetId = h.site;
  q.hasPos = true;
  q.tgx = st->gx; q.tgy = st->gy;
  q.giverName = "THE STEWARD OF " + where;
  q.need = 1;
  q.gold = std::min(60, std::max(0, L.treasury / 4));
  q.xp = 150;
  q.flags = (uint32_t)t << lord::TASK_SHIFT;
  switch (t) {
    case lord::Task::Unrest:
      q.title = "CALM THE UNREST IN " + where;
      q.desc = "THE PEOPLE GRUMBLE AT THE TAX. HOLD A FEAST, OR EASE THE TAX UNTIL THEY SETTLE.";
      break;
    case lord::Task::Raid: {
      const realm::Siege* sg = g.realm.siegeAt(h.site);
      if (!sg) return 0;
      q.title = "BREAK THE SIEGE OF " + where;
      q.desc = "SOLDIERS CAMP OUTSIDE " + where + ". STAND WITH THE MILITIA AND DRIVE THEM OFF.";
      q.tgx = sg->campX; q.tgy = sg->campY;
      q.have = (int)sg->id;   // the siege it is about
      break;
    }
    case lord::Task::Camp: {
      const realm::Camp* c = campNear(g, *st);
      if (!c) return 0;
      q.title = "CLEAR THE CAMP NEAR " + where;
      q.desc = "BANDITS FROM THE CAMP IN THE HILLS RAID OUR FIELDS. BREAK THEM, MY LORD.";
      q.tgx = c->gx; q.tgy = c->gy;   // the camp's record is found again by its heart
      break;
    }
    case lord::Task::Settlers:
      q.title = "BRING SETTLERS TO " + where;
      q.desc = "HALF THE HOUSES STAND EMPTY. REFUGEES ON THE ROADS WOULD FIND A HOME HERE.";
      break;
    default: return 0;
  }
  q.subject = q.title;
  acceptQuest(g, q);
  h.task = (uint32_t)q.stage;
  h.taskDay = dayU16(g.day);
  return h.task;
}

void LordOps::finishTask(Game& g, uint32_t id) {
  Quest* q = taskQuest(g, id);
  if (!q || q->state == QState::Done) return;
  q->state = QState::Complete;
  completeQuest(g, *q);   // -> questDone: loyalty, the coffers pay the reward
}

void LordOps::tasksStep(Game& g) {
  lord::Lordship& L = g.lordship;
  for (lord::Holding& h : L.holdings) {
    if (!h.task) continue;
    Quest* q = taskQuest(g, h.task);
    if (!q || q->state == QState::Done) { h.task = 0; continue; }
    const realm::SettlementState* st = g.realm.settlement(h.site);
    switch (lord::taskOf(q->flags)) {
      case lord::Task::Unrest:
        if (h.loyalty >= 35) finishTask(g, h.task);
        break;
      case lord::Task::Raid: {
        const realm::Siege* sg = nullptr;
        for (const realm::Siege& s : g.realm.sieges()) if ((int)s.id == q->have) sg = &s;
        if (!sg || sg->over) {
          if (sg && !sg->attackerWon) finishTask(g, h.task);
          else { q->state = QState::Done; q->flags |= QF_FAILED; h.task = 0; }
        }
        break;
      }
      case lord::Task::Camp: {
        bool cleared = false, found = false;
        for (const realm::Camp& c : g.realm.camps())
          if (c.gx == q->tgx && c.gy == q->tgy) {
            found = true;
            const int si = g.world.siteHandle(c.site);
            cleared = (c.flags & realm::CF_CLEARED) || (si >= 0 && g.world.sites[(size_t)si].cleared);
          }
        if (cleared) finishTask(g, h.task);
        else if (!found) { q->state = QState::Done; q->flags |= QF_FAILED; h.task = 0; }
        break;
      }
      case lord::Task::Settlers:
        (void)st;
        break;   // a refugee's word finishes it (lord_talk.cpp)
      default: break;
    }
  }
}
