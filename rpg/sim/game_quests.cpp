// EMBERVALE RPG: the M0 quest core: accepting, errands, completing, the main quest's stages (SOLID 1.1).
// Public: Game::questById, Game::acceptQuest, Game::openErrand, Game::finishErrand, Game::completeQuest,
//   Game::questKill, Game::checkDungeonCleared, Game::advanceMain, Game::questTarget, Game::rewardWaiting,
//   Game::openingQuest, Game::questStatus, Game::giveFirstWeapon
#include "rpg/sim/game_rpg_detail.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/quest_ops.h"

const Quest* Game::questById(int id) const {
  for (auto& q : quests) if (q.id == id) return &q;
  return nullptr;
}

// hasOffer: rpg/sim/quests.cpp (P1 Fresh Roads: the JOBS lane's weekly pools decide who offers work)

// makeOffer: rpg/sim/quests.cpp (M2: radiant targets over the region plans, the new quest types, the text grammar)

void Game::acceptQuest(const Quest& q0, QuestStart how) {
  Quest q = q0;
  q.id = nextQuestId++;
  q.state = QState::Active;
  if (q.target >= 0 && q.target < (int)world.sites.size()) {
    Site& T = world.sites[(size_t)q.target];
    // the place goes on the map: a dungeon as found (the giver told the way), a delivery's settlement as heard of
    if (q.type == QType::Deliver || q.type == QType::Protect) { if (!T.discovered) T.rumoured = true; }
    else T.discovered = true;
    if (T.cleared && (q.type == QType::Clear || q.type == QType::Bounty || q.type == QType::NamedBandit)) q.state = QState::Complete;
    if (!q.targetId) q.targetId = T.id;
  }
  // M2: a parcel to carry; the giver's next job is of another kind (quests.cpp)
  if (q.type == QType::Deliver) {
    Item parcel;
    parcel.kind = ItemKind::Quest; parcel.name = "PARCEL FOR " + q.subject; parcel.icon = art::Icon::Letter;
    parcel.tint = rgba(200, 170, 120); parcel.questId = q.id; parcel.value = 0;
    addItem(parcel, false);
    q.flags |= QF_PARCEL;
  }
  if (q.giverSlot >= 0 || q.giverBldg >= 0) marks[markKey(npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot), Mk::LastOffer)] = (int)q.type + 1;
  quests.push_back(q);
  JobOps::accepted(*this, q);   // (P1) the week's pool job given out, the leads that named it taken (jobs.cpp)
  // (P4 item 2) a quest the PLAYER just took (QuestStart::Player: I'LL DO IT., a notice from the board) is the one they
  // follow: it takes the pin, loudly (the banner and the toast say QUEST STARTED: <title>, the HUD's goal line and the
  // map's marker move to it), and the journal's TRACK switches back. (This reverses the 2026-10 rule "a new job never
  // takes the pin from an open quest" for the player's own take, after the W7 testers took the wolves and found the
  // pointer still on the main quest; that rule's complaint, a SILENT switch, is met by the banner and the goal line.)
  // (P4 fix r1, judge: "the Mist chain took A BLADE OF YOUR OWN's pin from a new hero") a quest the GAME starts (the Mist
  // chain, a grave read, a fief task, an army's order or mission, a war joined) keeps the 2026-10 rule: it never takes
  // the pin from an open quest being followed (the toast says NEW QUEST: <title>), only when nothing open is tracked.
  // (P1 fixer) a street errand keeps its own rule: it takes the pin, and finishErrand hands the pin back to whatever was
  // tracked before it
  const Quest* pinned = trackedQuest >= 0 && trackedQuest != q.id ? questById(trackedQuest) : nullptr;
  const bool keepPin = how == QuestStart::Game && q.type != QType::Errand && pinned && pinned->state != QState::Done;
  if (q.type == QType::Errand) errandPrevTracked_ = trackedQuest;
  if (!keepPin) trackedQuest = q.id;
  emit(Ev::QuestUpdate, pl().p, q.id, 0, (keepPin ? "NEW QUEST: " : "QUEST STARTED: ") + q.title);
  if (how == QuestStart::Player) events.back().id = 1;   // (the view's banner reads QUEST STARTED for the player's take)
  sfx((int)Sfx::QuestStart, pl().p);
}

// P1 Fresh Roads (game.h): a street incident's quest, taken on the spot, ended at once (LIFE lane)
int Game::openErrand(const Quest& q0) {
  Quest q = q0;
  q.type = QType::Errand;
  acceptQuest(q);
  return quests.empty() ? 0 : quests.back().id;
}

void Game::finishErrand(int qid, bool success) {
  for (Quest& q : quests) {
    if (q.id != qid || q.type != QType::Errand || q.state == QState::Done) continue;
    const bool hadPin = trackedQuest == q.id;   // (before completeQuest moves it)
    if (success) {
      const bool keyed = q.giverSlot >= 0 || q.giverBldg >= 0;
      const uint64_t key = npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot);
      QuestOps::completeQuest(*this, q);
      if (!keyed) { auto it = npcQuestsDone.find(key); if (it != npcQuestsDone.end() && --it->second <= 0) npcQuestsDone.erase(it); }
    } else {
      q.state = QState::Done;
      q.flags |= QF_FAILED;
      emit(Ev::QuestUpdate, pl().p, q.id, 2, "QUEST FAILED: " + q.title);
    }
    // (P1 fixer) the pin goes back to the quest tracked before the errand (still open), whatever completeQuest picked
    const int prev = errandPrevTracked_;
    errandPrevTracked_ = -1;
    bool prevOpen = false;
    for (const Quest& o : quests) if (o.id == prev && o.state != QState::Done) prevOpen = true;
    if (prevOpen && hadPin) trackedQuest = prev;
    else if (hadPin && trackedQuest == q.id) {
      trackedQuest = -1;
      for (const Quest& o : quests) if (o.state != QState::Done && o.type != QType::Main) { trackedQuest = o.id; break; }
      if (trackedQuest < 0) for (const Quest& o : quests) if (o.state != QState::Done) { trackedQuest = o.id; break; }
    }
    return;
  }
}

void QuestOps::completeQuest(Game& g, Quest& q) {
  q.state = QState::Done;
  // M2: a quest's own items (a parcel, a keepsake) go to whoever the quest was for
  for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Quest && g.inv[(size_t)i].questId == q.id) g.dropItem(i);
  g.giveGold(q.gold);
  g.gainXp(q.xp);
  g.npcQuestsDone[g.npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot)]++;
  PartyOps::quest(g, q, true);   // M8: the companions' view of the job done (party_game.cpp)
  SeaOps::questDone(g, q);   // M10: a hidden land's people warm to the hero; a fishmonger takes the fish (sea_chain.cpp)
  ServiceOps::questDone(g, q);   // M12: a campaign order or a diplomacy mission done (service_game.cpp)
  LordOps::questDone(g, q);      // M12: a fief's task done (lord_game.cpp)
  JobOps::questDone(g, q);       // P1: the week's pool job done (jobs.cpp)
  g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, "QUEST COMPLETE: " + q.title);
  g.sfx((int)Sfx::QuestDone, g.pl().p);
  // occasional item reward (M6, 7.5 rule 5): by the quest's danger, never by the player's level; Uncommon to Epic
  Rng r(hash32((uint32_t)q.id * 977u) ^ (uint32_t)g.seed);
  if (r.f() < 0.5f) {
    int D = 1;
    if (q.target >= 0 && q.target < (int)g.world.sites.size()) D = g.world.sites[(size_t)q.target].level;
    else if (q.hasPos && g.world.src) D = g.world.src->danger(q.tgx, q.tgy);
    else if (q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size()) D = g.world.sites[(size_t)q.giverSite].level;
    if (q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size()) D = std::max(D, g.world.sites[(size_t)q.giverSite].level);
    const cult::Culture* gc = q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size() ? g.world.cultureOf(q.giverSite) : nullptr;
    if (gc && g.world.src) gc = &g.world.src->culture(gc->id);
    // (M8b, 15.27.4) a quest's reward: Uncommon to Epic by its danger and the finder's level gate; a hard mission (the
    // target well beyond the hero, or a war's) is boosted a step
    gear::DropSource ds;
    ds.D = std::clamp(D, 1, gear::MAX_D);
    ds.src = gear::LootSrc::Quest;
    ds.playerLevel = g.plLevel;
    ds.culture = gc ? gc->id : 0;
    ds.atLeast = Rarity::Uncommon;
    ds.atMost = Rarity::Epic;
    const bool hard = ds.D >= g.plLevel + 3 || (q.flags & QF_DANGER);
    ds.boost = (int8_t)((hard ? 1 : 0) + (q.type == QType::War ? 1 : 0));
    const Rarity rar = gear::rollRarity(r, ds);
    static const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots,
                                     ItemKind::Cloak, ItemKind::Bow, ItemKind::Ring, ItemKind::Amulet};
    const ItemKind k = kinds[r.irange(10)];
    const int sub = k == ItemKind::Weapon ? r.irange(5) : 0;
    Item it = gear::makeGearC(r, k, sub, gear::itemLevelFor(ds), rar, gc);
    it.flags |= IF_QUESTREWARD;
    g.addItem(it);
  }
  if (g.trackedQuest == q.id) {
    // next: an open local job first (a nearby, level-fitting step), the main quest only when nothing else is open
    g.trackedQuest = -1;
    for (auto& o : g.quests) if (o.state != QState::Done && o.type != QType::Main) { g.trackedQuest = o.id; break; }
    if (g.trackedQuest < 0) for (auto& o : g.quests) if (o.state != QState::Done) { g.trackedQuest = o.id; break; }
  }
}

void QuestOps::questKill(Game& g, const Actor& a) {
  for (auto& q : g.quests) {
    if (q.state != QState::Active || q.type != QType::Hunt) continue;
    if (a.human || a.mon != q.mon) continue;
    q.have++;
    if (q.have >= q.need) {
      q.state = QState::Complete;
      std::string m = rewardReadyMsg(g.world, q);
      g.emit(Ev::QuestUpdate, a.p, q.id, 2, m);
      g.say(m);
      g.sfx((int)Sfx::QuestStart, a.p, 1.2f);
    }
    else g.emit(Ev::Text, a.p + Vec2(0, -26), (int)rgba(255, 220, 120), 0, std::to_string(q.have) + "/" + std::to_string(q.need));
  }
}

void QuestOps::checkDungeonCleared(Game& g, int si) {
  Site& st = g.world.sites[si];
  if (st.cleared) return;
  st.cleared = true;
  g.dungeonsCleared++;
  g.emit(Ev::QuestUpdate, g.pl().p, -1, 3, st.name + " CLEARED");
  g.sfx((int)Sfx::QuestDone, g.pl().p, 0.9f);
  bool forQuest = false;
  for (auto& q : g.quests) {
    if (q.state == QState::Active && q.target == si && (q.type == QType::Clear || q.type == QType::Bounty || q.type == QType::NamedBandit)) {
      q.state = QState::Complete;
      forQuest = true;
      std::string m = (q.type == QType::NamedBandit ? q.subject + " IS DEAD. " : std::string()) + rewardReadyMsg(g.world, q);
      g.emit(Ev::QuestUpdate, g.pl().p, q.id, 2, m);
      g.say(m);
    }
    if (q.type == QType::Main && st.mainQuest && q.stage == 1) {
      q.have++;
      Item shard; shard.kind = ItemKind::Quest; shard.name = "EMBER SHARD"; shard.icon = art::Icon::Sigil; shard.tint = rgba(255, 120, 40);
      shard.rarity = Rarity::Legendary; shard.questId = q.id; shard.value = 0;
      g.addItem(shard);
      if (q.have >= 3) QuestOps::advanceMain(g, 2);
      else g.emit(Ev::QuestUpdate, g.pl().p, q.id, 2, "EMBER SHARDS " + std::to_string(q.have) + "/3");
    }
  }
  if (si == g.world.lair) {
    for (auto& q : g.quests) if (q.type == QType::Main && q.stage == 3) QuestOps::advanceMain(g, 4);
  }
  // the wrong camp: never silent. Point at the place the bounty or job actually wants cleared.
  if (!forQuest && !st.mainQuest && si != g.world.lair) {
    const Quest* want = nullptr;
    float bd = 1e30f;
    for (const auto& q : g.quests) {
      if (q.state != QState::Active || (q.type != QType::Clear && q.type != QType::Bounty) || q.target < 0 || q.target == si) continue;
      const Site& t = g.world.sites[q.target];
      float d = std::hypot((float)(t.ex - st.ex), (float)(t.ey - st.ey)) * (q.id == g.trackedQuest ? 0.01f : 1.0f);
      if (d < bd) { bd = d; want = &q; }
    }
    if (want) {
      const Site& t = g.world.sites[want->target];
      std::string m = "NOT YOUR TARGET: " + t.name + " IS " + dirWord(t.ex - st.ex, t.ey - st.ey, false) + " OF HERE";
      g.emit(Ev::Notice, g.pl().p, (int)rgba(255, 170, 70), 0, m);
      g.say(m);
    }
  }
}

void QuestOps::advanceMain(Game& g, int stage) {
  int pendingMain = 0;   // a stage that completes immediately chains into the next one
  for (auto& q : g.quests) {
    if (q.type != QType::Main) continue;
    q.stage = stage;
    const Site& cap = g.world.sites[g.world.capital];
    switch (stage) {
      case 1:
        q.title = "THE EMBER SHARDS";
        q.desc = "THE " + lordTitleAt(g.world, g.world.capital) + " SAYS ONLY THE EMBER CROWN CAN BIND THE DRAGON ASHFANG. ITS THREE SHARDS LIE WITH DRAUGR WARLORDS IN ANCIENT RUINS. RECOVER ALL THREE.";
        q.need = 3; q.have = 0;
        for (auto& s : g.world.sites) if (s.mainQuest) s.discovered = true;
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 0, "MAIN QUEST: THE EMBER SHARDS");
        g.sfx((int)Sfx::QuestStart, g.pl().p);
        // warlords the player already slew before hearing of the shards still count (otherwise the quest soft-locks)
        for (auto& s : g.world.sites) {
          if (!s.mainQuest || !s.cleared) continue;
          q.have++;
          Item shard; shard.kind = ItemKind::Quest; shard.name = "EMBER SHARD"; shard.icon = art::Icon::Sigil; shard.tint = rgba(255, 120, 40);
          shard.rarity = Rarity::Legendary; shard.questId = q.id; shard.value = 0;
          g.addItem(shard);
        }
        if (q.have >= 3) pendingMain = 2;
        break;
      case 2:
        q.title = "RETURN TO THE " + lordTitleAt(g.world, g.world.capital);
        q.desc = "YOU HAVE ALL THREE EMBER SHARDS. BRING THEM TO THE " + lordTitleAt(g.world, g.world.capital) + " IN " + cap.name + ".";
        q.target = g.world.capital;
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 2, "ALL SHARDS FOUND! RETURN TO " + cap.name);
        g.sfx((int)Sfx::QuestDone, g.pl().p);
        break;
      case 3:
        q.title = "DRAGONSLAYER";
        q.desc = "THE CROWN IS WHOLE AND ASHFANG CAN BE KILLED. CLIMB TO " + g.lairName() + " AND END THE DRAGON.";
        q.target = g.world.lair;
        g.world.sites[g.world.lair].discovered = true;
        // the shards are consumed
        for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[i].kind == ItemKind::Quest && g.inv[i].name == "EMBER SHARD") g.dropItem(i);
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 0, "MAIN QUEST: DRAGONSLAYER");
        g.sfx((int)Sfx::QuestStart, g.pl().p);
        break;
      case 4:
        q.state = QState::Done;
        q.title = "DRAGONSLAYER";
        q.desc = "ASHFANG IS DEAD. THE SONGS OF EMBERVALE WILL CARRY YOUR NAME. THE WORLD IS STILL YOURS TO EXPLORE.";
        g.gainXp(1500);
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, "ASHFANG IS SLAIN! YOU ARE THE DRAGONSLAYER");
        g.sfx((int)Sfx::QuestDone, g.pl().p);
        break;
      default: break;
    }
  }
  if (pendingMain) QuestOps::advanceMain(g, pendingMain);
}

bool Game::questTarget(int qid, int& tx, int& ty) const {
  const Quest* q = questById(qid);
  if (!q || q->state == QState::Done) return false;
  if (q->type == QType::Main) {
    if (q->stage == 1) {
      // nearest uncleared shard ruin
      const Actor& p = pl();
      float bd = 1e30f; int best = -1;
      for (int i = 0; i < (int)world.sites.size(); i++) {
        const Site& s = world.sites[i];
        if (!s.mainQuest || s.cleared) continue;
        float d = std::hypot(s.ex * 16.0f - p.p.x, s.ey * 16.0f - p.p.y);
        if (d < bd) { bd = d; best = i; }
      }
      if (best < 0) return false;
      tx = world.sites[best].ex; ty = world.sites[best].ey;
      return true;
    }
    int t = q->stage == 3 ? world.lair : world.capital;
    if (t < 0) return false;
    if (q->stage == 0 || q->stage == 2) {
      // the keep's door (M3b: the city lord's seat, whatever the society built it as; a keep first, as before)
      const Site& c = world.sites[t];
      for (int pass = 0; pass < 2; pass++)
        for (int b = c.bldgFirst; b < c.bldgFirst + c.bldgCount; b++) {
          const Bldg& B = world.over.bldgs[b];
          if (pass == 0 ? B.type == art::Building::Keep : (bldgIsSeat(B) && !bldgIsRoyalSeat(B))) { tx = B.doorX(); ty = B.doorY(); return true; }
        }
    }
    tx = world.sites[t].ex; ty = world.sites[t].ey;
    return true;
  }
  // (fixer M6b r3) a story's "speak with" step points at the person themself while they are about: a census resident
  // walks the streets, and the binding's tile is only where they stood when the step began (the marker floated over an
  // empty tile while she stood beside the hero)
  if (q->type == QType::Story && q->state == QState::Active && !inside) {
    for (const story::Instance& in : story.running()) {
      if (in.questId != qid || in.done) continue;
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& a = actors[i];
        if (!a.npc || a.st == AState::Dead || !story.talksTo(*this, in, a)) continue;
        tx = (int)std::floor(a.p.x / TILE);
        ty = (int)std::floor(a.p.y / TILE);
        return true;
      }
      break;
    }
  }
  if (QuestOps::questTargetM2(*this, *q, tx, ty)) return true;   // M2 quest types (quests.cpp)
  // an open hunt points at the quarry: the nearest one about (outdoors), else the nearest den of its kind, else the
  // giver's home (where the beasts are said to roam)
  if (q->type == QType::Hunt && q->state == QState::Active) {
    int px, py;
    overworldTile(*this, px, py);
    float bd = 1e30f;
    if (!inside)
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& a = actors[i];
        if (a.st == AState::Dead || a.human || a.npc || a.mon != q->mon) continue;
        float d = len2(a.p - pl().p);
        if (d < bd) { bd = d; tx = (int)std::floor(a.p.x / TILE); ty = (int)std::floor(a.p.y / TILE); }
      }
    if (bd < 1e30f) return true;
    bool found = false;
    bd = 90.0f * 90.0f;   // a den within a day's walk; further off, the giver's home is the better hint
    for (int di = 0; di < (int)world.dens.size(); di++) {
      const Den& dn = world.dens[di];
      if (dn.mon != q->mon || denClearedDay(di) >= 0) continue;
      float d = (float)((dn.x - px) * (dn.x - px) + (dn.y - py) * (dn.y - py));
      if (d < bd) { bd = d; tx = dn.x; ty = dn.y; found = true; }
    }
    if (found) return true;
  }
  // a finished job (and the opening) points at whoever pays: their door, or the giver themself out on the street
  bool toGiver = q->state == QState::Complete || q->type == QType::Retrieve;
  int t = toGiver ? q->giverSite : q->target;
  if (t < 0) t = q->giverSite;
  if (t < 0) return false;
  // (P1 Fresh Roads) givers live their day now (no plaza pin, no post kept for an open job): the pin follows the
  // giver themself while they are about, then where the census says they are (LIFE lane), and only then their door
  if (toGiver && !inside)
    for (size_t i = 1; i < actors.size(); i++)
      if (isGiver(*q, actors[i]) && actors[i].st != AState::Dead) {
        tx = (int)std::floor(actors[i].p.x / TILE); ty = (int)std::floor(actors[i].p.y / TILE);
        return true;
      }
  if (toGiver && !inside && LifeHooks::giverSpot(*this, *q, tx, ty)) return true;
  if (toGiver && q->giverBldg >= 0 && q->giverBldg < (int)world.over.bldgs.size()) {
    tx = world.over.bldgs[q->giverBldg].doorX(); ty = world.over.bldgs[q->giverBldg].doorY();
    return true;
  }
  tx = world.sites[t].ex; ty = world.sites[t].ey;
  return true;
}

// Bounty clarity (M0): true when a quest's reward waits with this NPC (a finished job, or the opening's old blade).
// The view draws a bobbing "!" over them (render_markers.cpp).
bool Game::rewardWaiting(const Actor& a) const {
  if (!a.npc || a.st == AState::Dead) return false;
  for (const Quest& q : quests) {
    if (q.type == QType::Main) continue;
    bool waiting = q.state == QState::Complete || (q.type == QType::Retrieve && q.state == QState::Active);
    if (waiting && isGiver(q, a)) return true;
  }
  return false;
}

int QuestOps::openingQuest(const Game& g) {
  for (const Quest& q : g.quests) if (q.type == QType::Retrieve && q.state == QState::Active && q.giverSlot < 0) return q.id;
  return -1;
}

// One journal line under the quest: what to do next ("RETURN TO X IN Y", "CLEAR <CAMP> (NORTH-EAST)", "3/5 WOLVES").
std::string Game::questStatus(const Quest& q) const {
  if (q.state == QState::Done) return "";
  int px, py;
  overworldTile(*this, px, py);
  auto dirTo = [&](int sx, int sy, bool shortForm) { return dirWord(sx - px, sy - py, shortForm); };
  const std::string town = giverTown(world, q);
  if (q.type == QType::Main) {
    const std::string cap = world.sites[world.capital].name;
    switch (q.stage) {
      case 0: {
        const std::string l = lordTitleAt(world, world.capital);
        // (fixer M4 r3) already in the capital: no "travel to" the place the hero stands in; the seat is here
        const IRect& cr = world.sites[world.capital].r;
        if (inside ? subSite == world.capital : (px >= cr.x - 2 && py >= cr.y - 2 && px < cr.x + cr.w + 2 && py < cr.y + cr.h + 2))
          return fitLine({"YOU ARE IN " + cap + ": SEEK THE " + l + "'S HALL", "SEEK THE " + l + "'S HALL HERE", "SEEK THE " + l + " HERE"});
        return fitLine({"SPEAK TO THE " + l + " IN " + cap, "THE " + l + " IN " + cap, "SPEAK TO THE " + l});
      }
      case 1: {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < (int)world.sites.size(); i++) {
          const Site& s = world.sites[i];
          if (!s.mainQuest || s.cleared) continue;
          float d = std::hypot((float)(s.ex - px), (float)(s.ey - py));
          if (d < bd) { bd = d; best = i; }
        }
        std::string n = std::to_string(q.have) + "/3 SHARDS";
        if (best < 0) return n;
        const Site& s = world.sites[best];
        return fitLine({n + ": " + s.name + " (" + dirTo(s.ex, s.ey, false) + ")", n + ": " + s.name + " (" + dirTo(s.ex, s.ey, true) + ")",
                        n + " (" + dirTo(s.ex, s.ey, false) + ")"});
      }
      case 2: return fitLine({"RETURN TO THE " + lordTitleAt(world, world.capital) + " IN " + cap, "RETURN TO " + cap});
      case 3: {
        if (world.lair < 0) return "SLAY ASHFANG";
        const Site& L = world.sites[world.lair];
        return fitLine({"SLAY ASHFANG AT " + L.name + " (" + dirTo(L.ex, L.ey, false) + ")", "SLAY ASHFANG (" + dirTo(L.ex, L.ey, false) + ")"});
      }
      default: return "";
    }
  }
  const std::string inTown = town.empty() ? "" : " IN " + town;
  if (q.type == QType::Retrieve)
    return fitLine({"TALK TO THE INNKEEPER" + inTown, "TALK TO THE INNKEEPER"});
  if (std::string m2 = QuestOps::questStatusM2(*this, q); !m2.empty()) return m2;   // M2 quest types (quests.cpp)
  if (q.state == QState::Complete)
    return fitLine({"RETURN TO " + q.giverName + inTown, "RETURN TO " + q.giverName, "RETURN TO" + inTown});
  if (q.type == QType::Hunt) {
    std::string n = std::to_string(q.have) + "/" + std::to_string(q.need) + " " + monsterPlural(q.mon);
    return fitLine({town.empty() ? n : n + " NEAR " + town, n});
  }
  if ((q.type == QType::Clear || q.type == QType::Bounty) && q.target >= 0 && q.target < (int)world.sites.size()) {
    const Site& t = world.sites[q.target];
    const std::string dl = " (" + dirTo(t.ex, t.ey, false) + ")", ds = " (" + dirTo(t.ex, t.ey, true) + ")";
    // a bounty is won by the chief's death (Game::kill), so its line says so
    if (q.type == QType::Bounty)
      return fitLine({"KILL THE CHIEF AT " + t.name + dl, "KILL THE CHIEF AT " + t.name + ds, "KILL THE CHIEF: " + t.name + ds,
                      "CHIEF AT " + t.name + ds, t.name + ds});
    return fitLine({"CLEAR " + t.name + dl, "CLEAR " + t.name + ds, t.name + ds});
  }
  return "";
}

// The opening: the start village innkeeper hands over an old blade (and a draught for the road) for a small favour.
void QuestOps::giveFirstWeapon(Game& g, Quest& q) {
  Rng r(hash32((uint32_t)g.seed ^ 0xB1ADEu));
  // (M6) an item-level-1 heirloom in the old heartland make: worn below a new bronze sword's 8
  Item w = gear::makeGearC(r, ItemKind::Weapon, (int)WeaponType::Sword, 1, Rarity::Common, nullptr, Mat::Iron);
  w.name = "OLD BLADE"; w.power = 7; w.value = 12; w.ench = Ench::None; w.enchPow = 0;
  g.addItem(w);
  Item pot = makePotion(PotionType::Health, 0); pot.count = 1;
  g.addItem(pot);
  q.state = QState::Done;
  g.storyFlags |= SF_FIRST_WEAPON;
  g.gainXp(q.xp);
  g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, "QUEST COMPLETE: " + q.title);
  g.sfx((int)Sfx::QuestDone, g.pl().p);
  if (g.trackedQuest == q.id) {
    // next: an open local job first (a nearby, level-fitting step), the main quest only when nothing else is open
    g.trackedQuest = -1;
    for (auto& o : g.quests) if (o.state != QState::Done && o.type != QType::Main) { g.trackedQuest = o.id; break; }
    if (g.trackedQuest < 0) for (auto& o : g.quests) if (o.state != QState::Done) { g.trackedQuest = o.id; break; }
  }
}

