// M5 "Hearth and Hall": the townsfolk as actors (VISION_PLAN 10.2-10.4, 15.2, 15.12). TOWNSFOLK lane.
//
// The census (rpg/sim/life.h, CITIZENS lane) says who lives where and what each resident does this hour; this file
// puts the residents near the player on the streets and in the rooms as actors, and makes them live it:
//   - spawning by schedule: the generator's street folk give way to the census (lifeSpawnAllowed); every resident whose
//     plan puts them outdoors this hour and who is within the stream radius comes out WHERE THEIR PLAN HAS THEM (a town
//     walked into is already mid-day: someone whose block began a few minutes ago is part-way along the way there, out
//     of the door they left by), as an actor with a stable identity (slot LIFE_SLOT0 + census index: quests and stories
//     key on it). Residents whose plan is indoors are not on the street; a walker whose plan turns indoors walks to the
//     door and goes in there (never vanishing in the open street; far off screen they are simply put away);
//   - door-to-door routes: A* over the window's walkable tiles (streets cheapest, buildings never cut through), cached
//     per (from, to) pair, at most 2 path requests per step and 40 walkers thinking per step (the rest coast on their
//     route); the stuck watchdog re-plans and, failing that, picks another spot;
//   - interiors by schedule (lifeInterior): the building's key person at their post in working hours (the innkeeper
//     always: the opening's innkeeper at 8:30), the household at home, asleep in their beds at night (Posture::Sleep),
//     at the table at mealtimes, workers at their stations, patrons at the gathering place's tables in the evening with
//     the bard performing; guests upstairs only; nobody in the room the player rented;
//   - furniture: seats (chairs, stools, benches, cushions) and beds are reserved one actor each (Actor::useX / useY),
//     stations (the anvil, the oven, the altar, the lectern, the loom, the bar) give their posture;
//   - tavern life: service (the server walks to a patron, who gets a mug), chat pairs facing each other with bubbles,
//     the bard's instrument from the culture's lead (MusicStyle), toasts and dancing on festival nights, closing time;
//   - social life: greetings by name between friends, gossip near the player, grief, beggars, harmless brawls, emigrants,
//     children at tag, the lamplighter's round;
//   - village animals (dogs, cats, hens, a rooster, goats, pigs, ducks): a handful per settlement near the player;
//   - the player's hooks in dialogue (feed, coin, employ, supply, befriend, the inn's hot meal) and the 15.2 buffs.
// P1 "Fresh Roads" (LIFE lane, VISION_PLAN 15.34): nobody in the street is a statue.
//   - the street's day: a resident's outdoor block is cut into segments of 22-57 game minutes (segOf: a length and a
//     phase of their own), each a task (TaskK): a plain spot of the plan's kind somewhere new (3+ tiles off), minding a
//     stall with trips for stock (back with a crate: Carry), a carry route between real doors (the bakery / smithy /
//     market / seat / inn to customers' homes and back), sweeping along a lane, reading or preaching in the square, a
//     visit to a friend's door, a round of the stalls. The trades with no workplace (WORK@STREET) live mostly on routes;
//     the leisure hours stroll, shop and visit; field hands move along their own fields. Everyone keeps a pace of their
//     own; a stop out of reach gives the next one, twice running a spot in plain sight, a wedge a short step out;
//   - staggered arrival: a fresh street (walked into, or the clock jumped) places 42 % of the outdoor folk part-way to
//     their stop (from inside the town, in plain sight of it: walking at once), the rest at it part-way through its
//     wait; spots a body cannot reach from the town's heart (a fenced yard, a gap between houses) are never used;
//   - no giver pins: an open quest's giver, a story's person, an interior keeper with an open job live their day; the
//     tracked quest's pin follows them (Game::lifeGiverSpot: their spot by the plan, else the door they are behind);
//   - stumble-on hooks: overheard gossip naming a real need (a journal LEAD, once per giver a week, two a town a day),
//     street incidents (a thief, a broken cart, a brawl: one a town an hour at most, by the site and the hour) that
//     open an Errand when the player steps in and leave an Incident lead when ignored (life.h forceIncident etc.).
#include "rpg/sim/life_ops.h"
#include "rpg/story/story_internal.h"   // (P1b, REQUESTS lane) overheard gossip of the new items: tellerOffer, the library

// ================================================================= overheard P1b gossip (here: it reads story internals)
const Mk LifeOps::MK_P1B_GOSSIP_TAG = (Mk)story::MK_P1B_GOSSIP;
const Mk LifeOps::MK_STORY_GIVER_TAG = (Mk)story::MK_STORY_GIVER;

uint32_t LifeOps::overheardP1b(Game& g, const Actor& speaker, int partnerId, bool force, int32_t today) {
  const int si = speaker.site;
  if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) return 0;
  const ew::Gid sid = g.world.sites[(size_t)si].id;
  const int week = g.day / 7;
  // the sweep: a P1b gossip lead whose teller has since begun their story is taken
  for (jobs::Lead& l : g.jobBook.leads) {
    if (l.state != jobs::LS_OPEN || l.from != jobs::LeadFrom::Gossip || l.qtype != (uint8_t)QType::Story || !l.who) continue;
    auto gm = g.marks.find(markKey(l.who, MK_STORY_GIVER_TAG));
    if (gm != g.marks.end() && gm->second >= l.day) l.state = jobs::LS_TAKEN;
  }
  // who in this settlement has a P1b item with gossip on offer (ids: tellerOffer may stream places in)
  std::vector<int> ids;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& o = g.actors[k];
    if (!o.npc || !o.human || o.hostile || o.comp || o.critter || o.st == AState::Dead || o.site != si || o.id == speaker.id || o.id == partnerId) continue;
    if (story::isStoryPerson(o) || (o.lifeBits & LB_THIEF)) continue;
    auto it = g.marks.find(markKey(g.npcKey(o), MK_P1B_GOSSIP_TAG));
    if (it != g.marks.end() && it->second == week + 1) continue;
    ids.push_back(o.id);
  }
  const story::dsl::Library& L = story::dsl::library();
  struct Cand { uint32_t h; int id; int lib; };
  std::vector<Cand> cand;
  for (int id : ids) {
    const int k = g.findActor(id);
    if (k <= 0) continue;
    const Actor o = g.actors[(size_t)k];
    const story::Offer of = story::tellerOffer(g, o);
    if (of.lib < 0 || (size_t)of.lib >= L.scripts.size()) continue;
    const story::dsl::Script& s = L.scripts[(size_t)of.lib];
    if (s.family.empty() || s.family == "test" || s.gossip.empty()) continue;
    cand.push_back({mixu(g.npcKey(o), (uint64_t)g.day * 24u + (uint64_t)g.hour), id, of.lib});
  }
  auto titleOf = [](const story::dsl::Script& s) {
    return s.title.find('{') != std::string::npos || s.title.find('[') != std::string::npos ? std::string() : s.title;
  };
  const std::string town = g.world.sites[(size_t)si].name;
  jobs::Lead Ld;
  Ld.from = jobs::LeadFrom::Gossip;
  Ld.qtype = (uint8_t)QType::Story;
  Ld.site = sid;
  std::string line;
  if (!cand.empty()) {
    std::sort(cand.begin(), cand.end(), [](const Cand& x, const Cand& y) { return x.h < y.h; });
    const int k = g.findActor(cand[0].id);
    if (k <= 0) return 0;
    const Actor giver = g.actors[(size_t)k];
    const story::dsl::Script& s = L.scripts[(size_t)cand[0].lib];
    line = s.gossip[(size_t)(cand[0].h % (uint32_t)s.gossip.size())];
    const std::string title = titleOf(s);
    std::string fn = firstName(giver.name);
    if (fn == story::dsl::tradeName((int)giver.role)) fn = "A " + fn;   // (a guard with no name of their own: A GUARD OF ...)
    Ld.who = g.npcKey(giver);
    leadPlace(g, giver, Ld);
    Ld.text = fn + " OF " + town + (title.empty() ? std::string() : ": " + title) + " - \"" + line + "\"";
    const uint32_t id = g.addLead(Ld);
    if (!id) return 0;
    g.marks[markKey(Ld.who, MK_P1B_GOSSIP_TAG)] = week + 1;
    g.marks[markKey(sid, mkLife(1))] = g.day * 4 + std::min(3, today + 1);
    say(g, speaker, "..." + spoken(line) + "...", rgba(232, 214, 150));
    rt(g).street.gossipLeads++;
    return id;
  }
  // no teller: a notice board's request (towns and cities; never in a forced test, which wants a teller)
  if (force) return 0;
  const SiteType t = g.world.sites[(size_t)si].type;
  if (t != SiteType::Town && t != SiteType::City) return 0;
  const uint64_t boardKey = sid ^ 0xB0A2D5A1Full;
  auto bm = g.marks.find(markKey(boardKey, MK_P1B_GOSSIP_TAG));
  if (bm != g.marks.end() && bm->second == week + 1) return 0;
  const int r = story::requestOfferForPlace(g, (int)story::dsl::HookKind::Board, si);
  if (r < 0 || (size_t)r >= L.scripts.size() || L.scripts[(size_t)r].gossip.empty()) return 0;
  const int si2 = g.world.siteHandle(sid);   // (the offer's try-cast may have streamed places in)
  if (si2 < 0) return 0;
  int bx = 0, by = 0;
  if (!story::boardTile(g, si2, bx, by)) return 0;
  const story::dsl::Script& s = L.scripts[(size_t)r];
  const uint32_t h = mixu(sid, (uint64_t)g.day * 24u + (uint64_t)g.hour);
  line = s.gossip[(size_t)(h % (uint32_t)s.gossip.size())];
  const std::string title = titleOf(s);
  Ld.hasPos = true;
  Ld.gx = g.world.ox + bx; Ld.gy = g.world.oy + by;
  Ld.text = "THE NOTICE BOARD OF " + town + (title.empty() ? std::string() : ": " + title) + " - \"" + line + "\"";
  const uint32_t id = g.addLead(Ld);
  if (!id) return 0;
  g.marks[markKey(boardKey, MK_P1B_GOSSIP_TAG)] = week + 1;
  g.marks[markKey(sid, mkLife(1))] = g.day * 4 + std::min(3, today + 1);
  const int sk = g.findActor(speaker.id);
  if (sk > 0) say(g, g.actors[(size_t)sk], "..." + spoken(line) + "...", rgba(232, 214, 150));
  rt(g).street.gossipLeads++;
  return id;
}

// ================================================================= the hooks
void Game::lifeStep(float dt) {
  LifeOps::RT& R = LifeOps::rt(*this);
  const double t0 = nowMs();
  R.pathBudget = 2;
  R.thinkBudget = 40;
  if (R.barkT > 0) R.barkT -= dt;
  // the clock: a jump (a script's `hour`, a night's sleep, a load) places everyone afresh
  const double abs = LifeOps::absHour(*this);
  const bool jump = R.lastAbsH >= 0 && (abs < R.lastAbsH - 1e-4 || abs - R.lastAbsH > 0.2);
  R.lastAbsH = abs;
  if (jump) R.sayLater.clear();
  if (jump && R.inc.kind) LifeOps::endIncident(*this);   // (P1: the street's moment passed with the hours)
  // the 15.2 buffs: tell the player when one comes or goes
  {
    const uint8_t b = life.player.buffs();
    const uint8_t gained = (uint8_t)(b & ~R.lastBuffs), lost = (uint8_t)(R.lastBuffs & ~b);
    if (R.stats.steps > 0) {
      if (gained & life::BUFF_WELLFED) emit(Ev::Notice, pl().p, (int)rgba(240, 200, 110), 0, "WELL FED: STAMINA AND HEALTH COME BACK FASTER");
      if (gained & life::BUFF_RESTED) emit(Ev::Notice, pl().p, (int)rgba(150, 200, 255), 0, "RESTED: +10% EXPERIENCE");
      if (gained & life::BUFF_HUNGRY) emit(Ev::Notice, pl().p, (int)rgba(220, 160, 110), 0, "HUNGRY: STAMINA COMES BACK SLOWER. EAT FOOD (ITEMS) OR A MEAL AT AN INN");
      if (gained & life::BUFF_WEARY) emit(Ev::Notice, pl().p, (int)rgba(170, 160, 200), 0, "WEARY: FIND A BED AND SLEEP");
      if ((lost & life::BUFF_WELLFED) && !(b & life::BUFF_HUNGRY)) emit(Ev::Notice, pl().p, (int)rgba(200, 190, 170), 0, "NO LONGER WELL FED");
    }
    R.lastBuffs = b;
  }
  // gone: walked in at a door, out of the room, off the map
  for (size_t k = actors.size(); k-- > 1;)
    if (actors[k].lifeBits & LB_GONE) LifeOps::removeActorAt(*this, k);
  if (inside) {
    // (out again, the street is placed afresh: as it is at that hour)
    for (auto& kv : R.sites) { kv.second.active = false; kv.second.crittersOut = false; }
    LifeOps::interiorTick(*this, dt);
    if (jump && subBldg >= 0) {
      // a jump of the clock indoors: the room as it is at the new hour
      for (size_t k = actors.size(); k-- > 1;)
        if (actors[k].lifeBits & 1u) LifeOps::removeActorAt(*this, k);
      LifeOps::populate(*this);
    }
  } else if (world.endless) {
    // the active settlements' people, each reconciled twice a second (staggered), afresh when it just came in
    for (int si : activeSites_) {
      if (si < 0 || si >= (int)world.sites.size() || !world.sites[(size_t)si].settlement()) continue;
      if (!LifeOps::censusOfSite(*this, si)) continue;
      auto& S = LifeOps::siteRt(*this, si);
      const bool fresh = !S.active || jump;
      S.active = true;
      S.reconcileT -= dt;
      if (fresh || S.reconcileT <= 0) {
        S.reconcileT = 0.5f + (float)(si % 5) * 0.03f;
        LifeOps::reconcile(*this, si, fresh);
        LifeOps::critters(*this, si);
      }
    }
    for (auto& kv : R.sites) {
      if (!kv.second.active) continue;
      const int h = world.siteHandle(kv.first);
      if (h < 0 || !activeSites_.count(h)) { kv.second.active = false; kv.second.crittersOut = false; }
    }
    // (P1) the street's incidents: the hour's decided and staged near the player, the live one followed to its end
    LifeOps::incidentTick(*this, dt);
  }
  if (inside && R.inc.kind) {   // (in at a door: the street's moment passes without them: a lead, or the errand lost)
    if (R.inc.engaged) LifeOps::finishIncident(*this, false);
    else LifeOps::incidentLead(*this, nullptr);
    LifeOps::endIncident(*this);
  }
  // the per-actor state of actors no longer in play
  if ((R.stats.steps & 63) == 0) {
    std::unordered_set<int> ids;
    for (const Actor& a : actors) ids.insert(a.id);
    for (const Actor& a : sheltered_) ids.insert(a.id);
    for (auto it = R.act.begin(); it != R.act.end();) {
      if (ids.count(it->first)) { ++it; continue; }
      LifeOps::releaseSpot(*this, it->second, it->first);
      it = R.act.erase(it);
    }
  }
  // stats
  LifeFolkStats& s = R.stats;
  s.residents = s.walking = s.seated = s.working = s.sleeping = s.critters = 0;
  int stuck = 0;
  for (size_t k = 1; k < actors.size(); k++) {
    const Actor& a = actors[k];
    if (a.critter) { s.critters++; continue; }
    if (a.resident < 0) continue;
    s.residents++;
    if (a.st == AState::Walk) s.walking++;
    if (LifeOps::isSeated(a.posture)) s.seated++;
    if (a.posture == Posture::Sleep) s.sleeping++;
    if (a.posture == Posture::Hammer || a.posture == Posture::Hoe || a.posture == Posture::Chop || a.posture == Posture::Fish || a.posture == Posture::Stir ||
        a.posture == Posture::Sweep || a.posture == Posture::Read)
      s.working++;
    auto it = R.act.find(a.id);
    if (it != R.act.end() && it->second.stuck) stuck++;
  }
  s.stuck = stuck;
  const double ms = nowMs() - t0 + R.folkMs;
  R.folkMs = 0;
  s.steps++;
  s.lastMs = ms;
  s.worstMs = std::max(s.worstMs, ms);
  s.sumMs += ms;
}

bool Game::lifeFolk(Actor& a, float dt) {
  if (a.lifeBits & LB_HOMESTEAD) return HomeOps::folk(*this, a, dt);   // (M7) the player's farm animals, horse, builders, farmhand
  if (!(a.lifeBits & (LB_LIFE | LB_CRITTER | LB_KEY | LB_GUEST | LB_THIEF))) return false;
  LifeOps::RT& R = LifeOps::rt(*this);
  const double t0 = nowMs();
  bool done;
  if (a.lifeBits & LB_THIEF) done = inside ? false : LifeOps::thiefFolk(*this, a, dt);
  else if (a.critter) done = LifeOps::critterFolk(*this, a, dt);
  else if (inside) done = LifeOps::interiorFolk(*this, a, dt);
  else if (a.lifeBits & LB_LIFE) done = LifeOps::streetFolk(*this, a, dt);
  else done = false;
  R.folkMs += nowMs() - t0;
  return done;
}

bool Game::lifeSpawnAllowed(int site, const Spawn& sp) {
  // the street folk of a settlement come from its census now (lifeStep): the generator's villagers, farmers and
  // children stay in; a stall's keeper keeps the stall's hours (home for the night); the watch is the war lane's
  if (!sp.npc || sp.bandit) return true;
  if (sp.role == Role::Guard || sp.role == Role::Herald || sp.role == Role::Soldier || sp.role == Role::Captain || sp.role == Role::King) return true;
  if (!LifeOps::censusOfSite(*this, site)) return true;
  // (P1 Fresh Roads: an open quest's giver or a story's person no longer keeps the generator's place: the census's
  //  resident the spawn is lives their day, and the quest's pin follows them)
  // (M2 owner note 6) a capital's square folk (idlers, a crier, children round the centrepiece) keep the square by day
  // and go home at night (ai.cpp's square-goers): they are the square's own, besides the census's passers-by
  if ((sp.role == Role::Villager || sp.role == Role::Child) && world.sites[(size_t)site].type == SiteType::City && world.over.at(sp.x, sp.y) == Ground::Plaza)
    return true;
  if (sp.role == Role::Merchant) return hour >= 6.5f && hour < 21.5f;
  return false;
}

void Game::lifeSpawned(Actor& a, const Spawn& sp) {
  if (!inside) return;   // (the street: stall keepers keep the generator's ways)
  if (subBldg < 0) return;
  const Bldg& B = world.over.bldgs[(size_t)subBldg];
  if (B.site < 0 || B.site >= (int)world.sites.size() || !world.sites[(size_t)B.site].settlement()) return;
  life::Census* c = life.census(world, B.site);
  if (!c) return;
  const int off = subBldg - world.sites[(size_t)B.site].bldgFirst;
  if (sp.role == Role::Guard || sp.role == Role::King || sp.role == Role::Jarl) {   // the watch and the court stay
    if (sp.slot == 0) {
      const int ri = life::spawnResident(*c, sp, off);
      if (ri >= 0) { a.resident = ri; c->res[(size_t)ri].actor = a.id; }
      a.lifeBits |= LB_KEY;
    }
    return;
  }
  if (sp.slot == 0) {
    // the building's key person: at their post in working hours (the innkeeper always), else elsewhere
    const int ri = life::spawnResident(*c, sp, off);
    if (ri < 0) { a.lifeBits |= LB_KEY; return; }
    life::Resident& r = c->res[(size_t)ri];
    const bool mage = B.type == art::Building::Tower;
    // (P1 Fresh Roads: an open quest no longer keeps its giver at the post out of hours: they live their day, and the
    //  tracked quest's pin follows them, lifeGiverSpot)
    if (!mage && !LifeOps::keyAtPost(*this, *c, r)) {
      // a home's head at home this hour is placed by the schedule instead (in bed at night, at the table...)
      a.lifeBits |= LB_DROP;
      return;
    }
    a.resident = ri;
    r.actor = a.id;
    a.altBldg = -1; a.altSlot = LIFE_SLOT0 + ri;   // (its street identity: spawnResident's)
    if (sp.role != Role::Priest && sp.role != Role::Mage) a.name = r.name;
    if (sp.role == Role::Priest) a.name = "PRIEST " + r.name;
    if (r.job == life::Job::Beggar) a.look.outfit = art::Outfit::Rags;
    a.lifeBits |= LB_KEY;
    return;
  }
  // every other generator spawn: inn travellers upstairs stay (not residents); the rest are the census's people
  if (B.type == art::Building::Inn && subFloor > 0 && sp.role == Role::Villager) {
    if (hour >= 9.0f && hour < 19.0f) a.lifeBits |= LB_DROP;   // out on the road by day
    else a.lifeBits |= LB_GUEST;
    return;
  }
  a.lifeBits |= LB_DROP;
}

void Game::lifeInterior() {
  if (!inside || subBldg < 0) return;
  if (home::houseEmpty(world, this, subBldg)) return;   // (M7) nobody lives in a vacant house or the player's own
  LifeOps::populate(*this);
}

void Game::lifeTalk(Actor& a) {
  const int si = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  life::Census* c = si >= 0 && si < (int)world.sites.size() && world.sites[(size_t)si].settlement() ? life.findMut(world.sites[(size_t)si].id) : nullptr;
  // the innkeeper (or a hall's keeper) sells a hot meal: the strongest Well Fed (15.2). (fixer M5 r3) The pot is filled
  // from the town's own food stock (15.12): dearer in a hungry town, dearer still in a famine, and nothing at all when
  // the stock is gone
  if (a.role == Role::Innkeeper) {
    const int food = c ? LifeOps::townFood(*c) : 99;
    const uint16_t mf = c ? c->moodFlags : (uint16_t)life::MF_CONTENT;
    if (food <= 0) dlg.opts.push_back({"ANYTHING TO EAT?", LifeOps::L_MEAL, 0});
    else {
      const int price = (mf & life::MF_FAMINE) ? 20 : (mf & life::MF_HUNGRY) ? 12 : 6;
      dlg.opts.push_back({"A HOT MEAL (" + std::to_string(price) + " GOLD)", LifeOps::L_MEAL, price});
    }
  }
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) return;
  life::Resident& r = c->res[(size_t)a.resident];
  const std::string town = world.sites[(size_t)si].name;
  // (P1) a street incident's person: the plea, and the offer to step in (the errand opens with it)
  {
    const LifeOps::RT::Incident& I = LifeOps::rt(*this).inc;
    if (I.kind && !I.engaged && (a.id == I.victim || a.id == I.other)) {
      const char* plea = I.kind == life::INC_THIEF ? "HE'S GOT MY PURSE! THAT WAY, QUICK!"
                         : I.kind == life::INC_CART ? "THE WHEEL'S COME OFF, AND I CAN'T LIFT THE CART ON MY OWN."
                                                    : "STAY OUT OF THIS, STRANGER! THIS IS BETWEEN HIM AND ME.";
      speech::propose(dlg, speech::P_URGENT, speech::Topic::Need, plea);   // (it is happening now: over a tale or a job)
      dlg.opts.insert(dlg.opts.begin(), {I.kind == life::INC_THIEF ? "I'LL CATCH THEM!" : I.kind == life::INC_CART ? "LEND A HAND" : "BREAK IT UP",
                                         LifeOps::L_HELP, 0});
    }
  }
  // a plain townsperson's own words (M7b: proposed, so a reward, an offer or a parcel still speaks over them; talkTo's
  // frame puts a stranger's introduction or an acquaintance's hello in front)
  bool asks = false;
  for (const DlgOpt& o : dlg.opts) if (o.action == A_ASK) asks = true;
  const bool friendOf = (r.flags & life::RF_BEFRIENDED) != 0;
  const std::string hello = friendOf ? std::string(timeGreeting(hour)) + ", FRIEND! " : std::string();
  const bool hard = (c->moodFlags & (life::MF_FAMINE | life::MF_HUNGRY | life::MF_WARTORN | life::MF_GRIEF)) != 0;
  const bool pressing = r.need[(int)life::Need::Hunger] < 25 || (r.flags & life::RF_GRIEVING);
  // (M5) a standing offer of work (A_ASK: an innkeeper, a farmer) still voices the town's need or mood when there is
  // one: the most-visited people in a hungry town must show the famine; with nothing pressing their own line stays
  if ((a.lifeBits & (LB_LIFE | LB_KEY)) && asks && r.job != life::Job::Beggar) {
    const std::string mood = hard ? LifeOps::moodLine(*c, town) : std::string();
    const std::string need = (pressing || mood.empty()) ? LifeOps::needBark(*c, r) : std::string();
    if (!need.empty()) speech::propose(dlg, pressing ? speech::P_NEED : speech::P_BARK, speech::Topic::Need, hello + need);
    else if (!mood.empty()) speech::propose(dlg, speech::P_BARK, speech::Topic::Bark, hello + mood);
  }
  if ((a.lifeBits & LB_LIFE) && !asks) {
    const std::string need = LifeOps::needBark(*c, r);
    const std::string mood = LifeOps::moodLine(*c, town);
    if (r.job == life::Job::Beggar) speech::propose(dlg, speech::P_NEED, speech::Topic::Need, "ALMS FOR THE POOR? A COIN, A CRUST, ANYTHING.");
    else if (!need.empty()) speech::propose(dlg, pressing ? speech::P_NEED : speech::P_BARK, speech::Topic::Need, hello + need);
    else if (!mood.empty() && (hard || !talkMem.saidRecently(speech::lineHash(mood), day, 0)))
      // a good mood is said once a day in a town, not by every person in the street
      speech::propose(dlg, hard ? speech::P_BARK : speech::P_SMALLTALK, speech::Topic::Bark, hello + mood);
    else if (r.job == life::Job::Bard)
      speech::propose(dlg, speech::P_SMALLTALK, speech::Topic::Smalltalk, hello + "COME TO THE HALL TONIGHT. I'VE A NEW SONG, AND IT NEEDS EARS.");
    else if (!hello.empty())
      speech::propose(dlg, speech::P_SMALLTALK, speech::Topic::Smalltalk, hello + "IT'S ALWAYS GOOD TO SEE YOU IN " + town + ".");
  }
  // the player's hooks (15.12)
  const int food = LifeOps::foodIndex(*this);
  const bool adult = r.job != life::Job::Child;
  if (food >= 0 && (r.need[(int)life::Need::Hunger] < 70 || r.job == life::Job::Beggar))
    dlg.opts.push_back({"SHARE YOUR " + inv[(size_t)food].name, LifeOps::L_FEED, food});
  if (r.job == life::Job::Beggar && gold >= 2) dlg.opts.push_back({"SPARE A COIN (2 GOLD)", LifeOps::L_COIN, 2});
  if (adult && r.keySlot != 0 && r.job != life::Job::Beggar && r.job != life::Job::Noble && r.need[(int)life::Need::Money] < 60) {
    auto it = marks.find(LifeOps::markOf(*this, si, r.idx, 2));
    if (it == marks.end() || it->second != day) dlg.opts.push_back({"HIRE A DAY'S HELP (12 GOLD)", LifeOps::L_EMPLOY, 12});
  }
  if ((a.role == Role::Merchant || a.role == Role::Innkeeper) && food >= 0 && (c->moodFlags & (life::MF_HUNGRY | life::MF_FAMINE)))
    dlg.opts.push_back({"SELL YOUR FOOD TO THE STORES", LifeOps::L_SUPPLY, food});
  if (adult && !(r.flags & life::RF_BEFRIENDED)) {
    auto it = marks.find(LifeOps::markOf(*this, si, r.idx, 1));
    if (it == marks.end() || it->second != day) dlg.opts.push_back({"HOW ARE YOU KEEPING?", LifeOps::L_CHAT, 0});
  }
}

int gsim::lifeOptKind(int action) {
  switch (action) {
    case LifeOps::L_MEAL: case LifeOps::L_SUPPLY: return 1;
    case LifeOps::L_FEED: case LifeOps::L_COIN: case LifeOps::L_EMPLOY: return 2;
    case LifeOps::L_CHAT: return 3;
    default: return 0;
  }
}

bool Game::lifeChoose(const DlgOpt& o) {
  const int ai = findActor(dlg.actor);
  if (o.action == LifeOps::L_MEAL) {
    // the keeper's town and people (the pot's stock, the dish's words)
    int msi = -1;
    if (ai >= 0) {
      const Actor& k = actors[(size_t)ai];
      msi = k.site >= 0 ? k.site : (k.bldg >= 0 && k.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)k.bldg].site : -1);
    }
    if (msi < 0 && inside && subBldg >= 0 && subBldg < (int)world.over.bldgs.size()) msi = world.over.bldgs[(size_t)subBldg].site;
    life::Census* mc = msi >= 0 && msi < (int)world.sites.size() && world.sites[(size_t)msi].settlement() ? life.findMut(world.sites[(size_t)msi].id) : nullptr;
    if (o.arg <= 0 || (mc && LifeOps::townFood(*mc) <= 0)) {
      dlg.text = "NOTHING IN THE POT, FRIEND. NOT A CRUST IN THE TOWN TO PUT IN IT. IF YOU CAN BRING FOOD IN, PEOPLE WILL BLESS YOU FOR IT.";
      dlg.opts = {{"I UNDERSTAND.", A_BYE, 0}};
      return true;
    }
    if (gold < o.arg) { dlg.text = "THAT'S " + std::to_string(o.arg) + " GOLD FOR THE MEAL, FRIEND."; return true; }
    gold -= o.arg;
    sfx((int)Sfx::Coin, pl().p);
    if (mc) LifeOps::takeFood(*mc);
    deed(party::Deed::Tavern, mc ? mc->site : 0);   // M8: a meal at the inn (the companions eat too)
    life::PlayerNeeds& n = life.player;
    n.sinceMealH = 0;
    n.mealQuality = 3;
    n.fedH = std::max(n.fedH, 9.0f);
    pl().hp = std::min(pl().maxHp, pl().hp + 30.0f);
    const cult::Culture* cu = msi >= 0 ? world.cultureOf(msi) : nullptr;
    dlg.text = LifeOps::mealLine(cu ? (int)cu->archetype : -1, mc ? mc->moodFlags : (uint16_t)life::MF_CONTENT);
    dlg.opts = {{"THANK YOU.", A_BYE, 0}};
    return true;
  }
  if (ai < 0) return false;
  Actor& a = actors[(size_t)ai];
  if (o.action == LifeOps::L_HELP) {   // (P1) stepping in to a street incident: the errand opens on the spot
    LifeOps::RT& R = LifeOps::rt(*this);
    if (!R.inc.kind || R.inc.engaged || (a.id != R.inc.victim && a.id != R.inc.other)) {
      dlg.text = "NEVER MIND. IT'S OVER NOW.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    if (R.inc.kind == life::INC_BRAWL) {
      LifeOps::engageIncident(*this, a);
      dlg.text = "ALL RIGHT, ALL RIGHT... WE'RE DONE. NO NEED FOR THAT.";
    } else {
      LifeOps::openIncidentErrand(*this, a);
      dlg.text = R.inc.kind == life::INC_THIEF ? "THANK YOU! HE WENT THAT WAY, RUNNING LIKE A HARE!"
                                               : "BLESS YOU. GET YOUR SHOULDER UNDER THE SIDE THERE, AND HOLD IT STEADY WHILE I KNOCK THE WHEEL ON.";
    }
    dlg.opts = {{"GO.", A_BYE, 0}};
    return true;
  }
  const int si = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  if (si < 0) return false;
  life::Census* c = life.findMut(world.sites[(size_t)si].id);
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) return false;
  life::Resident& r = c->res[(size_t)a.resident];
  auto kindness = [&](int amount) {
    int& k = marks[LifeOps::markOf(*this, si, r.idx, 0)];
    k += amount;
    if (k >= 3 && !(r.flags & life::RF_BEFRIENDED) && life.befriend(c->site, r.idx)) {
      emit(Ev::Notice, pl().p, (int)rgba(240, 150, 170), 0, "YOU HAVE A FRIEND IN " + world.sites[(size_t)si].name + ": " + r.name);
      deed(party::Deed::Befriended, life::npcId(c->site, r.idx));   // M8
      LifeOps::bubble(a, Bubble::Heart, 2.5f);
    }
  };
  switch (o.action) {
    case LifeOps::L_FEED: {
      const int fi = o.arg >= 0 && o.arg < (int)inv.size() && inv[(size_t)o.arg].kind == ItemKind::Food ? o.arg : LifeOps::foodIndex(*this);
      if (fi < 0) { dlg.text = "YOU HAVE NOTHING TO SHARE."; return true; }
      const int q = inv[(size_t)fi].power >= 25 ? 2 : 1;
      const std::string what = inv[(size_t)fi].name;
      if (--inv[(size_t)fi].count <= 0) {
        inv.erase(inv.begin() + fi);
        for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak})
          if (*s > fi) (*s)--;
      }
      life.feed(c->site, r.idx, q);
      if (r.flags & life::RF_REFUGEE) deed(party::Deed::HelpedRefugees, c->site, 2);   // M8 (FELLOWS): the war's homeless fed
      else deed(party::Deed::Charity, c->site, r.job == life::Job::Beggar ? 2 : 1);   // M8
      LifeOps::bubble(a, Bubble::Heart, 2.0f);
      kindness(r.job == life::Job::Beggar ? 2 : 1);
      dlg.text = r.job == life::Job::Beggar ? "BLESS YOU. BLESS YOU! THE FIRST THING I'VE EATEN IN DAYS." : what + " TO SHARE? THAT'S KIND OF YOU. I WON'T FORGET IT.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_COIN: {
      if (gold < o.arg) { dlg.text = "YOU HAVE NOTHING TO GIVE."; return true; }
      gold -= o.arg;
      r.coin = (uint16_t)std::min(60000, r.coin + o.arg);
      deed((r.flags & life::RF_REFUGEE) ? party::Deed::HelpedRefugees : party::Deed::Charity, c->site);   // M8
      r.need[(int)life::Need::Money] = (uint8_t)std::min(100, r.need[(int)life::Need::Money] + 15);
      sfx((int)Sfx::Coin, a.p, 1.2f);
      LifeOps::bubble(a, Bubble::Coin, 2.0f);
      kindness(1);
      dlg.text = "GODS KEEP YOU, TRAVELLER.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_EMPLOY: {
      if (gold < o.arg) { dlg.text = "YOU CAN'T PAY ME THAT."; return true; }
      gold -= o.arg;
      life.employ(c->site, r.idx, day);
      marks[LifeOps::markOf(*this, si, r.idx, 2)] = day;
      sfx((int)Sfx::Coin, a.p);
      kindness(1);
      dlg.text = "A DAY'S PAY FOR A DAY'S WORK? GLADLY. I'LL MEND FENCES AND HAUL WATER FOR THE WHOLE STREET IN YOUR NAME.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_SUPPLY: {
      int units = 0, pay = 0;
      for (int i = (int)inv.size(); i-- > 0;) {
        if (inv[(size_t)i].kind != ItemKind::Food) continue;
        units += inv[(size_t)i].count;
        pay += inv[(size_t)i].count * std::max(1, inv[(size_t)i].value);
        inv.erase(inv.begin() + i);
        for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak})
          if (*s > i) (*s)--;
      }
      if (!units) { dlg.text = "YOU HAVE NO FOOD TO SELL."; return true; }
      life.supply(c->site, ew::Good::Bread, units);
      giveGold(pay);
      kindness(1);
      dlg.text = "FOOD! " + std::to_string(units) + " PORTIONS FOR THE STORES. THAT'LL FEED A FEW HOUSES THIS WEEK. HERE, " + std::to_string(pay) + " GOLD.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_CHAT: {
      marks[LifeOps::markOf(*this, si, r.idx, 1)] = day;
      r.need[(int)life::Need::Social] = (uint8_t)std::min(100, r.need[(int)life::Need::Social] + 20);
      kindness(1);
      // (M7b) the answer is how they really are (their needs, the town's mood), and the ways the player could help come
      // up with it (the kindnesses the first page left out: talkToInner groups them under this question)
      const uint16_t mf = c->moodFlags;
      std::string t;
      if (r.flags & life::RF_GRIEVING) t = "NOT WELL. WE BURIED ONE OF OUR OWN THIS WEEK, AND THE HOUSE IS TOO QUIET.";
      else if (r.need[(int)life::Need::Hunger] < 40) t = "HUNGRY, IF I'M HONEST. THERE'S LITTLE ENOUGH IN THE POT THESE DAYS.";
      else if (mf & life::MF_FAMINE) t = "BADLY. WE WENT TO BED HUNGRY AGAIN, AND HALF THE STREET IS PACKING UP TO LEAVE.";   // (the opening said the famine)
      else if (r.need[(int)life::Need::Money] < 30) t = "SHORT OF COIN, LIKE EVERYONE HERE. BUT I GET BY.";
      else if (r.need[(int)life::Need::Rest] < 30) t = "TIRED. I'VE BEEN ON MY FEET SINCE BEFORE DAWN.";
      else if (mf & life::MF_WARTORN) t = "AS WELL AS ANYONE CAN, WITH THE WAR SO CLOSE.";
      else {
        std::vector<int> fr = life.friendsOf(*c, r.idx);
        t = "OH, GETTING BY. ";
        if (!fr.empty() && fr[0] < (int)c->res.size()) {
          // (M7b integration) a name that reads as a word ("MY FRIEND AND AND I") is not said
          const std::string fn = firstName(c->res[(size_t)fr[0]].name);
          static const char* const kWords[] = {"AND", "OR", "THE", "CAN", "OLD", "I", "A", "AN", "IS", "IT", "TO", "OF", "IN", "ON", "AT", "BUT", "SO", "NO", "YES", "WILL", "MAY", "MAN"};
          bool word = fn.empty();
          for (const char* w : kWords) if (fn == w) word = true;
          t += (word ? std::string("A FRIEND OF MINE") : "MY FRIEND " + fn) + " AND I ARE OFF TO THE " + std::string(c->gathering >= 0 ? "HALL" : "SQUARE") +
               " LATER, IF YOU'RE ABOUT.";
        }
        else t += "IT'S GOOD OF YOU TO ASK. MOST FOLK DON'T.";
      }
      dlg.text = t;
      std::vector<DlgOpt> keep;
      for (const DlgOpt& k : dlg.opts) if (k.grp == 2 || lifeOptKind(k.action) == 2) { DlgOpt k2 = k; k2.grp = 0; keep.push_back(k2); }
      keep.push_back({"FAREWELL.", A_BYE, 0});
      dlg.opts = keep;
      dlg.group = 0;
      return true;
    }
    default: return false;
  }
}

bool Game::lifeGiverOpen(int si, int idx) const {
  if (si < 0 || si >= (int)world.sites.size() || idx < 0) return false;
  const int slot = LIFE_SLOT0 + idx;
  for (const Quest& q : quests)
    if (q.state != QState::Done && q.type != QType::Main && q.giverSite == si && q.giverBldg == -1 && q.giverSlot == slot) return true;
  // (fixer M5 r3) a running story's giver or cast person stays too: emigration or a raid would stall its stage
  const uint64_t key = npcKeyOf(si, -1, slot);
  // (fixer M6b r1) a saga's census resident is bound by its census id (life::npcId), not its street key
  const uint64_t rkey = life::npcId(world.sites[(size_t)si].id, idx);
  for (const story::Instance& in : story.running()) {
    if (in.done || in.failed) continue;
    for (const story::Binding& b : in.cast) if (b.id == key || b.id == rkey) return true;
  }
  return false;
}

void Game::lifeKill(const Actor& victim, int killer) {
  (void)killer;
  if (victim.critter || victim.resident < 0) return;
  const int si = victim.site >= 0 ? victim.site
                 : (victim.bldg >= 0 && victim.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)victim.bldg].site : -1);
  if (si < 0 || si >= (int)world.sites.size()) return;
  life.residentDied(world.sites[(size_t)si].id, victim.resident, day);
  // (M5) a giver's own open jobs die with them: nobody is left to hand them in to (no reward; the log says why)
  for (Quest& q : quests)
    if (q.state != QState::Done && q.type != QType::Main && isGiver(q, victim)) {
      q.state = QState::Done;
      for (int i = (int)inv.size() - 1; i >= 0; i--) if (inv[(size_t)i].kind == ItemKind::Quest && inv[(size_t)i].questId == q.id) dropItem(i);
      emit(Ev::QuestUpdate, pl().p, q.id, 2, "QUEST FAILED: " + q.title + " (" + victim.name + " IS DEAD)");
      if (trackedQuest == q.id) trackedQuest = -1;
    }
}

bool Game::lifeUseProp(art::Prop p, int tx, int ty) {
  (void)tx; (void)ty;
  LifeOps::RT& R = LifeOps::rt(*this);
  // no sleeping with a fight close by (the same rule as travel: a bedroll by the road, a bed in a raided house)
  if (p == Prop::Bedroll || p == Prop::Bed || p == Prop::Hammock || p == Prop::SleepingMat)
    for (const Actor& a : actors)
      if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - pl().p) < 120 * 120) {
        say("YOU CANNOT SLEEP WITH ENEMIES NEARBY");
        return true;
      }
  if (p == Prop::Bedroll) {   // a wayfarer's bedroll by the embers: the night passes, rested (a little less than a bed)
    R.sleepQ = 7.0f;
    rest(hoursToMorningLife(hour));
    say("YOU SLEEP BY THE EMBERS UNDER THE STARS.");
    return true;
  }
  if (p == Prop::Bed) R.sleepQ = inside && subBldg >= 0 && world.over.bldgs[(size_t)subBldg].type == art::Building::Inn ? 12.0f : 10.0f;
  if (p == Prop::Hammock || p == Prop::SleepingMat) R.sleepQ = 9.0f;
  return false;
}

void Game::lifeAte(const Item& food) {
  // a snack (an apple, bread) gives a short Well Fed; a hearty dish longer (15.2: the inn's cooked meal is best)
  life::PlayerNeeds& n = life.player;
  const int q = food.power >= 28 ? 2 : 1;
  n.sinceMealH = 0;
  n.mealQuality = (uint8_t)std::max<int>(q, n.fedH > 0 ? n.mealQuality : 0);
  n.fedH = std::max(n.fedH, q >= 2 ? 6.0f : 3.5f);
}

void Game::lifeSlept(int hours, bool bed) {
  // rest() has already moved the clock: charge the night to the meal clock, then start the day rested
  LifeOps::RT& R = LifeOps::rt(*this);
  life::PlayerNeeds& n = life.player;
  n.fedH = std::max(0.0f, n.fedH - (float)hours);
  n.sinceMealH = std::min(1000.0f, n.sinceMealH + (float)hours);
  life.resync(day, hour);
  // (an inn's rented room upstairs is the best night's sleep there is)
  if (inside && subBldg >= 0 && world.over.bldgs[(size_t)subBldg].type == art::Building::Inn && subFloor > 0) R.sleepQ = std::max(R.sleepQ, 12.0f);
  if (bed && hours >= 4) { n.sinceSleepH = 0; n.restedH = std::max(n.restedH, R.sleepQ); }
  R.sleepQ = 10.0f;
}

float Game::lifeStaminaRegenMul() const {
  const uint8_t b = life.player.buffs();
  float m = 1.0f;
  if (b & life::BUFF_WELLFED) m *= life.player.mealQuality >= 3 ? 1.3f : 1.25f;
  if (b & life::BUFF_HUNGRY) m *= 0.85f;
  if (b & life::BUFF_WEARY) m *= 0.9f;
  return m;
}
float Game::lifeHealthRegenMul() const { return (life.player.buffs() & life::BUFF_WELLFED) ? 1.2f : 1.0f; }
float Game::lifeXpMul() const { return (life.player.buffs() & life::BUFF_RESTED) ? 1.1f : 1.0f; }

const Game::LifeFolkStats& Game::lifeFolkStats() const {
  static const LifeFolkStats none;
  return lifeRt_ ? lifeRt_->stats : none;
}

// (M8 lead) a census resident's street look on any actor (a companion recruited from the census keeps the face, the dress
// and the name the town knew): the TOWNSFOLK rules of spawnResident / dressResident, without spawning anyone
bool Game::dressAsResident(Actor& a, ew::Gid site, int idx) {
  const int si = world.siteHandle(site);
  const life::Census* c = si >= 0 ? life.find(site) : nullptr;
  if (!c || idx < 0 || idx >= (int)c->res.size()) return false;
  const life::Resident& r = c->res[(size_t)idx];
  const int keepSite = a.site, keepSlot = a.slot, keepBldg = a.bldg;
  a.site = si;   // makeLook dresses by the site's culture
  // (M8 integration) the street body's identity (spawnResident: bldg -1, slot LIFE_SLOT0 + idx): the culture dress is
  // seeded by them, so without it a recruit's face, people and hat changed the moment they joined
  a.slot = LIFE_SLOT0 + idx;
  a.bldg = -1;
  a.role = life::jobRole(r.job);
  if (a.role == Role::Guard) a.role = Role::Villager;
  LifeOps::dressResident(*this, a, r);
  a.site = keepSite; a.slot = keepSlot; a.bldg = keepBldg;
  return true;
}

// ================================================================= (M8 FELLOWS) a companion's body on the furniture
// of the player's own house (game_internal.h): the TOWNSFOLK fit rules (the head on the pillow, the seat line on the
// seat), without the townsfolk's seat reservations (nobody of the census lives in the player's house)
bool lifeLayInBed(Game& g, Actor& a, int x, int y) {
  if (!g.inside || !g.sub.in(x, y)) return false;
  const int pr = g.sub.propAt(x, y);
  if (!pr) return false;
  const Prop b = (Prop)(pr - 1);
  if (b != Prop::Bed && b != Prop::BunkBed && b != Prop::SleepingMat && b != Prop::Bedroll && b != Prop::Hammock) return false;
  const bool longBed = g.sub.propAt(x, y - 1) == (int)Prop::Filler + 1;
  a.p = Vec2(x * TILE + 8.0f, y * TILE + 16.0f + (float)sleepAy(b, longBed, LifeOps::kitOf(g)));
  a.home = a.p; a.goal = a.p;
  a.face = 0;
  a.st = AState::Idle;
  a.posture = Posture::Sleep;
  a.postureT = 0;
  a.useX = x; a.useY = y;
  return true;
}

bool lifeSitOn(Game& g, Actor& a, int x, int y, Posture post) {
  if (!g.inside || !g.sub.in(x, y)) return false;
  const int pr = g.sub.propAt(x, y);
  if (!pr) return false;
  const Prop s = (Prop)(pr - 1);
  if (s != Prop::Chair && s != Prop::Stool && s != Prop::Bench && s != Prop::Cushion) return false;
  // facing the table beside it when there is one
  int pref = 0;
  const int tbl = (int)Prop::Table + 1;
  if (g.sub.propAt(x, y - 1) == tbl) pref = 1;
  else if (g.sub.propAt(x + 1, y) == tbl) pref = 2;
  else if (g.sub.propAt(x - 1, y) == tbl) pref = 3;
  const int faces[4] = {pref, 0, 2, 3};
  for (int f : faces) {
    int ax = 0, ay = 0;
    if (!seatAt(s, f, ax, ay)) continue;
    a.p = Vec2(x * TILE + 8.0f + (float)ax, y * TILE + 16.0f + (float)ay);
    a.home = a.p; a.goal = a.p;
    a.face = f;
    a.st = AState::Idle;
    a.posture = seatedPost(s, post);
    a.postureT = (float)(hash32((uint32_t)a.id * 7u) % 100u) * 0.01f;
    a.useX = x; a.useY = y;
    return true;
  }
  return false;
}

// (P1 Fresh Roads; LIFE lane) where a quest giver is now when the map has no actor for them: the census resident's
// spot by their plan this hour (a street, a stall, the field, the square), else the door of the building their plan has
// them in (home, work, the tavern, the temple). Local tiles (questTarget's). False: not a census resident / unknown.
bool Game::lifeGiverSpot(const Quest& q, int& tx, int& ty) const {
  if (q.giverSite < 0 || q.giverSite >= (int)world.sites.size()) return false;
  const Site& st = world.sites[(size_t)q.giverSite];
  if (!st.settlement() || !st.id) return false;
  const life::Census* c = life.find(st.id);
  if (!c) return false;
  int idx = -1;
  if (q.giverBldg < 0 && q.giverSlot >= LIFE_SLOT0 && q.giverSlot < LIFE_SLOT0 + LIFE_SLOTN) idx = q.giverSlot - LIFE_SLOT0;
  else if (q.giverSlot == 0 && q.giverBldg >= st.bldgFirst && q.giverBldg < st.bldgFirst + st.bldgCount) {
    const int off = q.giverBldg - st.bldgFirst;
    for (const life::Resident& r : c->res)
      if (r.keyBldg == off && r.keySlot == 0) { idx = (int)r.idx; break; }
  }
  if (idx < 0 || idx >= (int)c->res.size()) return false;
  const life::Resident& r = c->res[(size_t)idx];
  if (r.flags & (life::RF_DEAD | life::RF_AWAY)) return false;
  const life::Plan p = life.plan(world, *c, r, day, hour);
  if (p.place == life::Place::Away) return false;
  Game& g = const_cast<Game&>(*this);   // (the street's spot lists are the runtime's cache; nothing of the game changes)
  if (p.bldg < 0 && !LifeOps::keyAtPost(g, *c, r)) {
    LifeOps::RT::SiteRt& S = LifeOps::siteRt(g, q.giverSite);
    int32_t gx, gy;
    int face = 0;
    if (LifeOps::outdoorSpot(g, q.giverSite, S, *c, r, p, gx, gy, face, -2)) {
      tx = gx - world.ox; ty = gy - world.oy;
      if (world.over.in(tx, ty)) return true;
    }
  }
  int off = p.bldg >= 0 ? p.bldg : r.home;
  if (LifeOps::keyAtPost(g, *c, r)) off = r.keyBldg;
  const int b = c->bldgHandle(st, off);
  if (b < 0 || b >= (int)world.over.bldgs.size()) return false;
  tx = world.over.bldgs[(size_t)b].doorX(); ty = world.over.bldgs[(size_t)b].doorY();
  return true;
}

// ================================================================= P1 Fresh Roads: the LIFE lane's public hooks (life.h)
namespace life {
const char* taskName(int t) {
  static const char* n[] = {"SPOT", "STALL", "DELIVER", "SWEEP", "READ", "VISIT", "SHOP"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)TK_COUNT, "a name for every street task");
  return t >= 0 && t < (int)TK_COUNT ? n[t] : "-";
}
int taskOf(const Game& g, int actorId) { return LifeOps::taskOfActor(g, actorId); }
const StreetStats& streetStats(const Game& g) { return LifeOps::streetOf(g); }
IncidentView incidentNow(const Game& g) { return LifeOps::incidentView(g); }
bool forceIncident(Game& g, int kind) {
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor((g.pl().p.y - 2) / TILE);
  int si = -1;
  float bd = 1e30f;
  for (int s : g.world.nearSites) {
    if (s < 0 || s >= (int)g.world.sites.size() || !g.world.sites[(size_t)s].settlement()) continue;
    const Site& st = g.world.sites[(size_t)s];
    const float d = (float)((st.ex - px) * (st.ex - px) + (st.ey - py) * (st.ey - py));
    if (d < bd) { bd = d; si = s; }
  }
  if (si < 0 || g.inside) return false;
  return LifeOps::startIncident(g, si, kind);
}
uint32_t forceGossip(Game& g) { return LifeOps::forceGossip(g); }
std::string debugFolk(const Game& g, int actorId) { return LifeOps::debugFolk(g, actorId); }
bool debugTalk(Game& g, int actorId) { return LifeOps::debugTalk(g, actorId); }
bool debugChoose(Game& g, const std::string& labelStart) {
  if (g.mode != Mode::Dialogue) return false;
  for (size_t i = 0; i < g.dlg.opts.size(); i++)
    if (g.dlg.opts[i].label.compare(0, labelStart.size(), labelStart) == 0) { g.dialogueChoose((int)i); return true; }
  return false;
}
}  // namespace life
