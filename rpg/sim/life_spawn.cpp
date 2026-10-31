// LifeOps (life_ops.h, TOWNSFOLK lane): spawning residents on the street.
// Dress, spawn and remove actors, reconcile a street with the census, staged moods, settling an outdoor walker.
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

// ---------------------------------------------------------------- spawning residents on the street
void LifeOps::dressResident(Game& g, Actor& a, const life::Resident& r) {
  // the look from the resident's own seed (its sex as the census has it), its census name
  for (uint32_t k = 0; k < 12; k++) {
    Rng rr(r.lookSeed + k * 7919u);
    Rng probe = rr;
    const bool fem = probe.f() < 0.45f;
    if (fem != r.female && k + 1 < 12) continue;
    const Role role = a.role;
    g.makeLook(a, role, rr);
    break;
  }
  a.name = r.name;
  // the role they are talked to as: farmers (the fields' jobs), children, else plain townsfolk (the shop keepers'
  // roles stay with the buildings' key persons; a baker at the tavern sells nothing)
  a.role = r.job == life::Job::Child ? Role::Child : (r.job == life::Job::Farmer ? Role::Farmer : Role::Villager);
  if (r.job == life::Job::Beggar) { a.look.outfit = art::Outfit::Rags; a.look.topColor = rgba(120, 104, 84); }
  if (r.job == life::Job::Child) { a.radius = 3.5f; a.look.outfit = art::Outfit::Tunic; a.look.beard = false; }
  if (r.job == life::Job::Elder) a.speed *= 0.8f;
  // (M5 fixer, review: "a row of identical clones outside the gate") the working folk of one trade wear their own
  // homespun: a tunic and breeches of their own dye (earth tones, never a uniform), and hat or none by their own seed
  if (r.job == life::Job::Hunter || r.job == life::Job::Farmer || r.job == life::Job::Herder || r.job == life::Job::Woodcutter ||
      r.job == life::Job::Miner || r.job == life::Job::Labourer || r.job == life::Job::Fisher)
    homespun(a, r);
  else if (r.job != life::Job::Guard && r.job != life::Job::Noble && r.job != life::Job::Beggar && r.job != life::Job::Priest) {
    // everyone else keeps the culture's dress, in their own dye lot: a shade lighter or darker, warmer or cooler
    const uint32_t h = (uint32_t)mixu(r.lookSeed, 0x7EA1ull);
    static const uint32_t tints[6] = {rgba(40, 30, 30), rgba(230, 220, 200), rgba(150, 70, 40), rgba(70, 100, 60), rgba(60, 70, 120), rgba(120, 90, 50)};
    a.look.topColor = tint(a.look.topColor, tints[h % 6u], 0.12f + (float)((h >> 8) % 20u) * 0.01f);
  }
  if (r.job == life::Job::Lamplighter) a.look.weapon = 4;   // the pole
  if (r.job == life::Job::Smith) a.look.weapon = 6;
  // (P2b fix TRADES-DRAWN-ON-V1) the trades that carry a tool never fight with it: the doll draws it at rest (no ACTION)
  if (r.job == life::Job::Smith || r.job == life::Job::Hunter || r.job == life::Job::Scholar || r.job == life::Job::Lamplighter)
    a.look.civil = 1;
  // (P2b FOLK-SIM) the doll's census look: the age (child, youth 10-14, elder from 60), the trades' work garments by
  // job (a labourer's smock, a woodcutter's jerkin, a smith's apron), a beggar's gaunt face (v1 ignores every one)
  census::ageLook(a.look, r.age, r.job == life::Job::Child, r.job == life::Job::Elder, (uint64_t)r.lookSeed);
  if (r.job == life::Job::Labourer || r.job == life::Job::Herder) a.look.garb = 2;
  else if (r.job == life::Job::Woodcutter) a.look.garb = 3;
  else if (r.job == life::Job::Smith) a.look.garb = 1;
  else if (r.job == life::Job::Miner || ((r.job == life::Job::Stablehand || r.job == life::Job::Servant) && !a.look.garb)) a.look.garb = 5;   // shirt and trousers
  if (r.job == life::Job::Beggar && (mixu(r.lookSeed, 0x6A07ull) & 1u)) a.look.marks = (uint8_t)(a.look.marks | 32);
  // (P2b fix, B6) at rig 2 a small child's patterned smock is now and then a spotted one, like the grown-ups' market
  // cloth (looks.cpp dress; only once the census age says child, not youth: a youth has no dots tile)
  // (a striped, checked or vined one half the time; a plain one one time in six)
  if (art::rigVersion() >= 2 && a.look.age == 1) {
    const uint32_t roll = (uint32_t)(mixu(r.lookSeed, 0xD075ull) & 255u);
    const bool tiled = a.look.pattern == 1 || a.look.pattern == 2 || a.look.pattern == 6;
    if ((tiled && roll < 128u) || (a.look.pattern == 0 && roll < 42u)) a.look.pattern = 4;
  }
}

uint32_t LifeOps::tint(uint32_t c, uint32_t to, float t) {
  auto ch = [&](int sh) {
    const float a = (float)((c >> sh) & 255u), b = (float)((to >> sh) & 255u);
    return (uint32_t)std::clamp((int)std::lround(a + (b - a) * t), 0, 255) << sh;
  };
  return (c & 0xFF000000u) | ch(16) | ch(8) | ch(0);
}

void LifeOps::homespun(Actor& a, const life::Resident& r) {
  static const uint32_t tops[8] = {rgba(118, 92, 62), rgba(92, 104, 64), rgba(132, 84, 58), rgba(84, 92, 104),
                                   rgba(146, 120, 80), rgba(104, 70, 60), rgba(70, 88, 70), rgba(150, 132, 104)};
  static const uint32_t legs[4] = {rgba(80, 64, 50), rgba(64, 56, 48), rgba(96, 82, 60), rgba(58, 60, 66)};
  const uint32_t h = (uint32_t)mixu(r.lookSeed, 0xD7E5ull);
  a.look.topColor = tops[h % 8u];
  a.look.bottomColor = legs[(h >> 4) % 4u];
}

int LifeOps::spawnResident(Game& g, int si, life::Census& c, int idx, Vec2 at, bool interior) {
  life::Resident& r = c.res[(size_t)idx];
  Spawn sp;
  sp.npc = true;
  sp.role = life::jobRole(r.job);
  if (sp.role == Role::Guard) sp.role = Role::Villager;
  sp.site = si;
  sp.slot = LIFE_SLOT0 + idx;
  sp.x = tileX(at); sp.y = tileY(at);
  const int id = g.spawnHuman(sp, at);
  Actor& a = g.actors.back();
  a.bldg = -1;   // (one identity indoors and out: npcKey(site, -1, slot))
  // a keyed resident (a workplace's keeper, a home's head) is also its building's interior slot-0 spawn: a quest given
  // by either body is turned in at either (isGiver)
  if (r.keyBldg >= 0 && r.keySlot >= 0) { a.altBldg = g.world.sites[(size_t)si].bldgFirst + r.keyBldg; a.altSlot = r.keySlot; }
  a.stallKeeper = false;   // (spawnHuman makes a merchant beside a stall its keeper: residents keep their own ways)
  a.p = at;
  a.site = si;
  a.resident = idx;
  a.lifeBits = LB_LIFE | LB_FRESH;
  a.fromMap = true;
  dressResident(g, a, r);
  // home is where they live (the militia defend their own street; when trouble comes they run for its door, or for
  // the nearest door when home is far: ai.cpp homeDoor)
  const int hb = r.home >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], r.home) : -1;
  a.homeBldg = -1;
  int hx, hy;
  a.home = !interior && doorStep(g, hb, hx, hy) ? tileCentre(hx, hy) : at;
  a.goal = at;
  // the militia by the resident's own trade (a farmer's pitchfork; others by courage: war_game.cpp's rule, keyed
  // like spawnHuman's)
  if (!interior) {
    // (a farmer with a pitchfork is no braver than his neighbours: courage decides, as for any villager)
    const Role keep = a.role;
    a.role = Role::Villager;
    warArmMilitia(g, a, ((uint64_t)(si + 1) << 24) ^ (uint64_t)(LIFE_SLOT0 + idx));
    a.role = keep;
  } else a.militia = false;
  r.actor = id;
  RT& R = rt(g);
  R.act.erase(id);
  R.stats.spawned++;
  g.perf.spawnedNpcs++;
  return id;
}

void LifeOps::removeActorAt(Game& g, size_t k) {
  Actor& a = g.actors[k];
  release(g, a);
  RT& R = rt(g);
  { auto it = R.act.find(a.id); if (it != R.act.end()) releaseSpot(g, it->second, a.id); }
  R.act.erase(a.id);
  if (a.resident >= 0) {
    const int si = siteOfActor(g, a);
    if (si >= 0)
      if (life::Census* c = g.life.findMut(g.world.sites[(size_t)si].id))
        if (a.resident < (int)c->res.size() && c->res[(size_t)a.resident].actor == a.id) c->res[(size_t)a.resident].actor = -1;
  }
  R.stats.despawned++;
  g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
}

// the street folk of an active settlement by the census (fresh: the site just came in, or the clock jumped: everyone
// is placed where their plan has them, part-way along their way when their block began only minutes ago)
void LifeOps::reconcile(Game& g, int si, bool fresh) {
  RT& R = rt(g);
  life::Census* c = censusOfSite(g, si);
  if (!c || c->res.empty()) return;
  RT::SiteRt& S = siteRt(g, si);
  const Site& st = g.world.sites[(size_t)si];
  refreshPlans(g, S, *c);
  const bool empty = warEmpty(g, si);
  // who is out now (in play or sheltering indoors from a threat)
  std::vector<int> present(c->res.size(), -1);
  int folk = 0;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.npc && a.fromMap && a.site == si && a.role != Role::Guard && !(a.lifeBits & LB_CRITTER)) folk++;
    if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
    present[(size_t)a.resident] = a.id;
  }
  for (const Actor& a : g.sheltered_) {
    if (a.site == si && a.role != Role::Guard) folk++;
    if ((a.lifeBits & LB_LIFE) && a.site == si && a.resident >= 0 && a.resident < (int)c->res.size()) present[(size_t)a.resident] = a.id;
  }
  for (size_t i = 0; i < c->res.size(); i++) c->res[i].actor = present[i];
  // (M4) a home burned out from under them: its people leave town (refugees on the road out)
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
    if (!homeBurned(g, st, *c, c->res[(size_t)a.resident])) continue;
    RT::ARt& A = R.rt(a.id);
    if (A.kind == T_AWAY) continue;
    const RT::Spot* e = pick(g, S.edge.empty() ? S.street : S.edge, mixu(c->seed, (uint64_t)a.resident), a.id);
    if (!e || !onScreen(g, a.p)) { a.lifeBits |= LB_GONE; continue; }
    A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.path.clear(); A.arrived = false;
    a.home = tileCentre(e->x - g.world.ox, e->y - g.world.oy);
  }
  if (empty) {   // an abandoned or ruined place: nobody (its key quest givers are the war lane's)
    for (size_t k = g.actors.size(); k-- > 1;)
      if ((g.actors[k].lifeBits & LB_LIFE) && g.actors[k].site == si) removeActorAt(g, k);
    return;
  }
  // fresh: everyone out of place goes (a jump of the clock: the 2:00 street must not keep the noon crowd)
  if (fresh) {
    for (size_t k = g.actors.size(); k-- > 1;) {
      Actor& a = g.actors[k];
      if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
      if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) continue;
      int32_t gx, gy;
      int face;
      const life::Resident& r = c->res[(size_t)a.resident];
      // (P1) everyone in the street is placed afresh, as a street walked into is (staggered: some on their way, some at
      // their stop part-way through it): kept as they were, the whole street would turn to its new block at once
      auto ai = R.act.find(a.id);
      const bool held = ai != R.act.end() && ai->second.mood == MOOD_INCIDENT;
      if (!held || !targetSpot(g, si, S, *c, r, S.plans[(size_t)a.resident], gx, gy, face, a.id) || !keepOut(g, si, r)) {
        c->res[(size_t)a.resident].actor = -1;
        present[(size_t)a.resident] = -1;
        removeActorAt(g, k);
        folk--;
      }
    }
  }
  const float ptx = g.pl().p.x / TILE, pty = g.pl().p.y / TILE;
  const bool whole = c->res.size() <= 60 || g.lifeFolkCap > 0;
  int cap = g.lifeFolkCap > 0 ? g.lifeFolkCap : std::min(Game::FOLK_CAP, whole ? 60 : Game::FOLK_CAP);
  // (M5 fixer r2) a hungry, war-torn or emptying town has thinner streets: people stay in, or have gone
  if (g.lifeFolkCap <= 0 && (c->moodFlags & (life::MF_FAMINE | life::MF_WARTORN | life::MF_EMIGRATING))) cap = cap * 3 / 5;
  else if (g.lifeFolkCap <= 0 && (c->moodFlags & life::MF_HUNGRY)) cap = cap * 4 / 5;
  struct Cand { float d; int idx; int32_t gx, gy; int face; };
  std::vector<Cand> want;
  for (size_t i = 0; i < c->res.size(); i++) {
    if (present[i] >= 0 || i >= (size_t)LIFE_SLOTN) continue;
    const life::Resident& r = c->res[i];
    if (r.job == life::Job::Guard || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
    if (homeBurned(g, st, *c, r)) continue;   // (M4) the people of a burned-out home are gone with it
    if (keyAtPost(g, *c, r)) continue;   // at their post indoors (lifeInterior)
    int32_t gx, gy;
    int face;
    if (!targetSpot(g, si, S, *c, r, S.plans[i], gx, gy, face)) continue;
    if (!keepOut(g, si, r)) continue;
    const int lx = gx - g.world.ox, ly = gy - g.world.oy;
    if (!g.world.over.in(lx, ly)) continue;
    const float dx = lx + 0.5f - ptx, dy = ly + 0.5f - pty;
    const float d2 = dx * dx + dy * dy;
    if (!whole && d2 > (float)(Game::FOLK_IN * Game::FOLK_IN)) continue;
    if (!warSpawnAllowed(g, si, lx, ly, LIFE_SLOT0 + (int)i)) continue;
    if (g.felled_.count(si) && g.felled_[si].count(LIFE_SLOT0 + (int)i)) continue;
    want.push_back({d2, (int)i, gx, gy, face});
  }
  std::sort(want.begin(), want.end(), [](const Cand& a, const Cand& b) { return a.d < b.d || (a.d == b.d && a.idx < b.idx); });
  int budget = fresh ? 1 << 30 : 6;   // (a few a step: a schedule window's turn must not land in one frame)
  // children at play take at most an eighth of the street (the square must show the town's working day, not a
  // schoolyard): the ones already out count
  const int playCap = std::max(3, cap / 8);
  int playing = 0;
  for (size_t i = 0; i < c->res.size() && i < S.plans.size(); i++)
    if (present[i] >= 0 && S.plans[i].act == life::Act::Play) playing++;
  // the watch changes at 6:00 and 18:00: the night's (or the day's) men come off their posts and walk home to the
  // barracks, a word to the relief at the post (the posts themselves stay manned: the war lane's watch)
  {
    const bool morning = g.hour >= 5.8f && g.hour < 6.4f, evening = g.hour >= 17.8f && g.hour < 18.4f;
    const int key = g.day * 2 + (evening ? 1 : 0);
    if ((morning || evening) && S.shiftDone != key && !warFrontier(g, si)) {
      S.shiftDone = key;
      int shown = 0;
      for (size_t k = 1; k < g.actors.size() && shown < 2; k++) {
        const Actor post = g.actors[k];   // (a copy: spawning moves the actors)
        if (!post.npc || post.role != Role::Guard || post.site != si || post.resident >= 0 || post.st == AState::Dead) continue;
        if (len2(post.p - g.pl().p) > (float)(Game::FOLK_IN * TILE) * (float)(Game::FOLK_IN * TILE)) continue;
        // a guard of the outgoing shift who lives in the barracks or the lodge
        int gi = -1;
        for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN; i++) {
          const life::Resident& r = c->res[i];
          if (r.job != life::Job::Guard || present[i] >= 0 || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
          if ((int)r.shift != (morning ? 1 : 0)) continue;   // the night watch goes off at dawn, the day watch at dusk
          if ((mixu(c->seed ^ i, (uint64_t)key) % 2u) != 0) continue;
          gi = (int)i;
          break;
        }
        if (gi < 0) break;
        const Vec2 at = snapWalk(g, post.p + Vec2(14.0f, 0.0f));
        const int id = spawnResident(g, si, *c, gi, at, false);
        present[(size_t)gi] = id;
        Actor& a = g.actors.back();
        // off duty: the watch's own look, no weapon drawn, a plain townsman in the AI's eyes for the walk home
        Rng rr(c->res[(size_t)gi].lookSeed);
        g.makeLook(a, Role::Guard, rr);
        a.name = c->res[(size_t)gi].name;
        a.militia = false;
        a.face = faceOf(post.p - a.p);
        say(g, a, "YOUR WATCH, FRIEND.", rgba(220, 226, 240));
        bubble(a, Bubble::Exclaim, 1.2f);
        RT::ARt& A = R.rt(id);
        const life::Plan& p = S.plans[(size_t)gi];
        A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
        int hb = c->res[(size_t)gi].home >= 0 ? c->bldgHandle(st, c->res[(size_t)gi].home) : -1;
        int dx, dy;
        if (!doorStep(g, hb, dx, dy)) { a.lifeBits |= LB_GONE; continue; }
        A.kind = T_DOOR; A.bldg = hb; A.gx = g.world.ox + dx; A.gy = g.world.oy + dy;
        shown++;
      }
    }
  }
  // (15.12) a town people are leaving: those who packed up these last days are seen on the road out, packs on their
  // backs (two a day at most, by day)
  if ((c->moodFlags & life::MF_EMIGRATING) && g.hour >= 8.0f && g.hour < 17.0f && S.emigrantDay != g.day) {
    S.emigrantDay = g.day;
    int shown = 0;
    for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN && shown < 2; i++) {
      const life::Resident& r = c->res[i];
      if (!(r.flags & life::RF_AWAY) || (r.flags & (life::RF_DEAD | life::RF_REFUGEE)) || present[i] >= 0 || r.job == life::Job::Child) continue;
      if ((mixu(c->seed ^ i, (uint64_t)g.day) % 3u) != 0) continue;
      int hx, hy;
      const int hb = r.home >= 0 ? c->bldgHandle(st, r.home) : -1;
      if (!doorStep(g, hb, hx, hy) || S.edge.empty()) continue;
      const int id = spawnResident(g, si, *c, (int)i, tileCentre(hx, hy), false);
      Actor& a = g.actors.back();
      RT::ARt& A = R.rt(id);
      const RT::Spot* e = pick(g, S.edge, mixu(c->seed, i), id);
      if (!e) { a.lifeBits |= LB_GONE; continue; }
      A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.act = life::Act::Emigrate; A.place = life::Place::Away; A.pbldg = -1;
      shown++;
    }
    // (M5 fixer r2) nobody has gone yet (the town has only now begun to empty): a household in the street packs up and
    // takes the road out before the player's eyes (they come back with the next plan, as the census has them)
    if (shown == 0 && !S.edge.empty()) {
      for (size_t k = 1; k < g.actors.size() && shown < 2; k++) {
        Actor& a = g.actors[k];
        if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size() || (a.lifeBits & LB_KEY)) continue;
        const life::Resident& r = c->res[(size_t)a.resident];
        if (r.job == life::Job::Guard || r.job == life::Job::Noble || r.job == life::Job::Child || (r.flags & life::RF_KEY) || !onScreen(g, a.p, 8.0f)) continue;
        RT::ARt& A = R.rt(a.id);
        if (A.kind != T_SPOT) continue;
        const RT::Spot* e = pick(g, S.edge, mixu(c->seed, (uint64_t)a.resident), a.id);
        if (!e) continue;
        releaseSpot(g, A, a.id);
        A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.path.clear(); A.arrived = false; A.mood = 0;
        A.act = life::Act::Emigrate; A.place = life::Place::Away; A.pbldg = -1;
        a.home = tileCentre(e->x - g.world.ox, e->y - g.world.oy);
        if (onScreen(g, a.p)) say(g, a, "THERE'S NOTHING LEFT FOR US HERE.", rgba(220, 210, 190));
        shown++;
        // the spouse and the children go with them
        for (size_t j = 1; j < g.actors.size(); j++) {
          Actor& o = g.actors[j];
          if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.site != si || o.resident < 0 || o.resident >= (int)c->res.size()) continue;
          const life::Resident& q = c->res[(size_t)o.resident];
          if ((int)q.idx != r.spouse && q.parent != (int16_t)r.idx) continue;
          RT::ARt& B = R.rt(o.id);
          if (B.kind != T_SPOT) continue;
          releaseSpot(g, B, o.id);
          B.kind = T_AWAY; B.gx = e->x; B.gy = e->y; B.path.clear(); B.arrived = false; B.mood = 0;
          B.act = life::Act::Emigrate; B.place = life::Place::Away; B.pbldg = -1;
        }
      }
    }
  }
  for (Cand w : want) {
    if (folk >= cap || budget <= 0) break;
    const life::Resident& r = c->res[(size_t)w.idx];
    const life::Plan& p = S.plans[(size_t)w.idx];
    if (p.act == life::Act::Play && playing >= playCap) continue;
    // the spot again, now that the ones spawned before have claimed theirs
    if (!targetSpot(g, si, S, *c, r, p, w.gx, w.gy, w.face)) continue;
    Vec2 at = tileCentre(w.gx - g.world.ox, w.gy - g.world.oy);
    // where from: the door they came out of, part-way along the way (VISION_PLAN 10.2)
    life::Plan before;
    const float age = blockAge(g, *c, r, p, before);
    int fromB = -1;
    if (before.bldg >= 0) fromB = c->bldgHandle(st, before.bldg);
    int fx, fy;
    const bool haveDoor = doorStep(g, fromB, fx, fy);
    if (fresh) {
      if (haveDoor && age < 0.3f) {
        const Vec2 door = tileCentre(fx, fy);
        const float tripH = len(at - door) / (g.actors[0].speed * kWalk) * (14.5f / 1200.0f);   // in-game hours
        const float t = tripH > 0 ? std::clamp(age / tripH, 0.0f, 1.0f) : 1.0f;
        at = snapWalk(g, door + (at - door) * t);
      }
    } else {
      // never out of thin air on screen: out of the door they left, else their home door, else (off screen) there
      int hx, hy;
      const int hb = r.home >= 0 ? c->bldgHandle(st, r.home) : -1;
      if (haveDoor && len2(tileCentre(fx, fy) - at) < (40.0f * TILE) * (40.0f * TILE)) at = tileCentre(fx, fy);
      else if (onScreen(g, at) && doorStep(g, hb, hx, hy)) at = tileCentre(hx, hy);
      else if (onScreen(g, at)) {
        // the nearest door of the settlement to the spot
        int best = -1, bd = 1 << 30;
        for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
          int x, y;
          if (!doorStep(g, b, x, y)) continue;
          const int d = std::abs(x - tileX(at)) + std::abs(y - tileY(at));
          if (d < bd) { bd = d; best = b; }
        }
        int x, y;
        if (best < 0 || !doorStep(g, best, x, y)) continue;
        at = tileCentre(x, y);
      }
    }
    const int id = spawnResident(g, si, *c, w.idx, at, false);
    Actor& a = g.actors.back();
    RT::ARt& A = R.rt(id);
    A.kind = T_SPOT; A.gx = w.gx; A.gy = w.gy; A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
    claimSpot(g, A, id, w.gx, w.gy);
    A.pace = personPace(*c, r);
    // (P1) the segment's task: its first stop is where they are bound (a stall, a door on a round, a new spot)
    const bool tasked = rhythmic(r, p, A);
    if (tasked) pickTask(g, a, A, *c, r, p, S, segOf(g, *c, r));
    if (fresh && tasked) {
      // a street walked into is mid-day (P1 staggered arrival): those whose block began minutes ago are part-way from
      // the door they left by; of the rest, some are on their way somewhere (a few tiles off, walking), the others at
      // their stop, part-way through its wait
      const Vec2 tgt = tileCentre(A.gx - g.world.ox, A.gy - g.world.oy);
      const uint32_t hh = mixu(c->seed ^ ((uint64_t)w.idx << 4), (uint64_t)(absHour(g) * 12.0));
      Vec2 from;
      if (haveDoor && age < 0.3f) {
        const Vec2 door = tileCentre(fx, fy);
        const float tripH = len(tgt - door) / (g.actors[0].speed * kWalk) * (14.5f / 1200.0f);
        const float t = tripH > 0 ? std::clamp(age / tripH, 0.0f, 1.0f) : 1.0f;
        at = snapWalk(g, door + (tgt - door) * t);
      } else if (hh % 100u < 42u && midRoute(g, a, tgt, hh, from)) at = from;
      else at = tgt;
      a.p = at; a.goal = at;
    }
    if (p.act == life::Act::Play) playing++;
    A.arrived = len2(a.p - tileCentre(A.gx - g.world.ox, A.gy - g.world.oy)) < 4.0f;
    A.watchP = a.p;
    a.face = w.face;
    if (fresh && A.arrived) {
      settleOutdoor(g, a, A, *c, c->res[(size_t)w.idx]);
      if (A.task != TK_NONE) {
        taskArrive(g, a, A, r);
        if (A.dwell < 1e5f) A.dwell *= 0.15f + (float)(mixu(c->seed ^ (uint64_t)w.idx, (uint64_t)g.day) % 85u) * 0.01f;
      }
    }
    folk++;
    budget--;
  }
  stageMood(g, si, *c, S);
}

// (M5 fixer r2, 15.12 visible mood) a hungry town's street shows it: a few of the people idling near the player sit
// down to beg by the stalls and the square (a bowl held out, a bread bubble); a restless one's has a scuffle (two at
// it with their fists, angry bubbles). Staged once an in-game hour among the people at leisure near the player.
void LifeOps::stageMood(Game& g, int si, life::Census& c, RT::SiteRt& S) {
  RT& R = rt(g);
  const uint16_t mf = c.moodFlags;
  const int key = g.day * 24 + (int)g.hour;
  // (topped up at every reconcile: people just spawned are still walking to their spots; the choice is keyed by the
  // hour, so the same few are picked while it lasts)
  S.moodStage = key;
  int begN = 0, brawlN = 0;
  const bool hungry = (mf & (life::MF_HUNGRY | life::MF_FAMINE)) != 0;
  const bool brawls = (mf & life::MF_BRAWLS) != 0 && g.hour >= 8.0f && g.hour < 23.0f;
  struct C { float d; int k; };
  std::vector<C> cand;
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c.res.size() || a.st == AState::Dead) continue;
    auto it = R.act.find(a.id);
    if (it == R.act.end()) continue;
    RT::ARt& A = it->second;
    // the last hour's vignettes end when the town is better (a beggar gets up, a brawl breaks off)
    if (A.mood == 1 && !hungry) { A.mood = 0; a.posture = Posture::None; }
    if (A.mood == 2 && !brawls) { A.mood = 0; A.partner = -1; a.posture = Posture::None; }
    if (A.mood == 1) begN++;
    if (A.mood == 2) brawlN++;
    if (A.mood || A.kind != T_SPOT || !A.arrived || A.partner >= 0) continue;
    const life::Resident& r = c.res[(size_t)a.resident];
    if (r.job == life::Job::Child || r.job == life::Job::Guard || r.job == life::Job::Noble || (r.flags & life::RF_KEY) || (a.lifeBits & LB_KEY)) continue;
    const life::Act pa = A.act;
    if (!(pa == life::Act::Wander || pa == life::Act::Socialise || pa == life::Act::Shop || pa == life::Act::Eat || pa == life::Act::Tavern)) continue;
    const float d = len2(a.p - g.pl().p);
    if (d > (14.0f * TILE) * (14.0f * TILE)) continue;
    cand.push_back({d + (float)(mixu(c.seed ^ (uint64_t)a.resident, (uint64_t)key) % 4096u), (int)k});
  }
  std::sort(cand.begin(), cand.end(), [](const C& a, const C& b) { return a.d < b.d || (a.d == b.d && a.k < b.k); });
  size_t ci = 0;
  if (hungry) {
    const int want = ((mf & life::MF_FAMINE) ? 3 : 2) - begN;
    for (int n = 0; n < want && ci < cand.size(); ci++, n++) {
      Actor& a = g.actors[(size_t)cand[ci].k];
      RT::ARt& A = R.rt(a.id);
      A.mood = 1;
      a.posture = Posture::Beg;
      a.postureT = 0;
      a.face = 0;
      bubble(a, Bubble::Bread, 3.0f);
      begN++;
    }
    // too few out at leisure (a famine noon: the rest are in the fields): the hungriest kept at home come out to beg
    // at the square or the market, the nearest free spots to the player
    const int wantAll = (mf & life::MF_FAMINE) ? 3 : 2;
    while (begN < wantAll) {
      const int i = homeBody(g, si, c, S, key, 60, begN);
      if (i < 0) break;
      const RT::Spot* sp = nearSpot(g, S, -1, -1);
      if (!sp || spawnMood(g, si, c, S, i, sp->x, sp->y, 1) < 0) break;
      begN++;
    }
  }
  if (brawls && brawlN == 0) {
    bool staged = false;
    for (; ci + 1 < cand.size(); ci++) {
      Actor& a = g.actors[(size_t)cand[ci].k];
      // the nearest other candidate within a few tiles is the one they fall out with
      size_t bj = 0;
      float bd = (6.0f * TILE) * (6.0f * TILE);
      for (size_t j = ci + 1; j < cand.size(); j++) {
        const float d = len2(g.actors[(size_t)cand[j].k].p - a.p);
        if (d < bd) { bd = d; bj = j; }
      }
      if (!bj) continue;
      Actor& o = g.actors[(size_t)cand[bj].k];
      RT::ARt& A = R.rt(a.id);
      RT::ARt& B = R.rt(o.id);
      A.mood = B.mood = 2;
      A.partner = o.id; B.partner = a.id;
      A.t2 = 0; B.t2 = 0.7f;
      a.posture = o.posture = Posture::None;
      const Vec2 mid = (a.p + o.p) * 0.5f;
      const Vec2 dir = len2(o.p - a.p) > 1.0f ? norm(o.p - a.p) : Vec2(1, 0);
      const Vec2 pa = mid - dir * 8.0f, pb = mid + dir * 8.0f;
      if (g.bodyFree(pa, a.radius, false)) a.p = pa;
      if (g.bodyFree(pb, o.radius, false)) o.p = pb;
      a.face = faceOf(o.p - a.p); o.face = faceOf(a.p - o.p);
      bubble(a, Bubble::Anger, 1.5f);
      if (onScreen(g, a.p)) say(g, a, "THAT WAS MY BREAD, YOU THIEF!", rgba(240, 150, 120));
      staged = true;
      break;
    }
    // nobody idle near enough: two neighbours come out and fall to it on the square (a spot and the next one by it)
    if (!staged) {
      const int i = homeBody(g, si, c, S, key, 101, 7);
      const int j = i >= 0 ? homeBody(g, si, c, S, key, 101, 8, i) : -1;
      const RT::Spot* s1 = j >= 0 ? nearSpot(g, S, -1, -1) : nullptr;
      const RT::Spot* s2 = s1 ? nearSpot(g, S, s1->x, s1->y) : nullptr;
      if (s1 && s2) {
        const int ia = spawnMood(g, si, c, S, i, s1->x, s1->y, 2);
        const int ib = ia >= 0 ? spawnMood(g, si, c, S, j, s2->x, s2->y, 2) : -1;
        if (ia >= 0 && ib >= 0) {
          RT::ARt& A = R.rt(ia);
          RT::ARt& B = R.rt(ib);
          A.partner = ib; B.partner = ia; A.t2 = 0; B.t2 = 0.7f;
        }
      }
    }
  }
}

// (stageMood) a resident kept at home this hour who may come out for a vignette: not in play, a grown-up of the
// street (no keeper, guard or noble), hungrier than `hungerBelow`, picked by the hour (`salt` varies the pick)
int LifeOps::homeBody(Game& g, int si, life::Census& c, RT::SiteRt& S, int key, int hungerBelow, int salt, int not1) {
  if (S.plans.size() != c.res.size() || g.hour < 8.0f || g.hour >= 20.0f) return -1;
  int best = -1;
  uint32_t bh = 0xFFFFFFFFu;
  for (size_t i = 0; i < c.res.size() && i < (size_t)LIFE_SLOTN; i++) {
    const life::Resident& r = c.res[i];
    if ((int)i == not1 || r.actor >= 0 || (r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_KEY)) || r.job == life::Job::Child ||
        r.job == life::Job::Guard || r.job == life::Job::Noble || r.job == life::Job::Beggar)
      continue;
    bool out = false;
    for (size_t k = 1; k < g.actors.size() && !out; k++) {
      const Actor& a = g.actors[k];
      if ((a.lifeBits & LB_LIFE) && a.site == si && a.resident == (int)i) out = true;
    }
    if (out) continue;
    const life::Act pa = S.plans[i].act;
    if (!(pa == life::Act::Home || pa == life::Act::Eat || pa == life::Act::Wander)) continue;
    if (r.need[(int)life::Need::Hunger] >= hungerBelow) continue;
    const uint32_t h = mixu(c.seed ^ (i * 977u), (uint64_t)key * 31u + (uint64_t)salt);
    if (h < bh) { bh = h; best = (int)i; }
  }
  return best;
}

// (stageMood) the free square or market spot nearest the player (beside (nx, ny) when given: within two tiles of it)
const LifeOps::RT::Spot* LifeOps::nearSpot(Game& g, RT::SiteRt& S, int32_t nx, int32_t ny) {
  const RT::Spot* best = nullptr;
  float bd = (16.0f * TILE) * (16.0f * TILE);
  for (const std::vector<RT::Spot>* v : {&S.market, &S.plaza, &S.street})
    for (const RT::Spot& s : *v) {
      if (!spotOk(g, s) || spotTaken(g, s.x, s.y, -1)) continue;
      if (nx != -1 || ny != -1) {
        if ((s.x == nx && s.y == ny) || std::abs(s.x - nx) > 2 || std::abs(s.y - ny) > 1) continue;
        const float d = (float)(std::abs(s.x - nx) + std::abs(s.y - ny));
        if (d < bd) { bd = d; best = &s; }
        continue;
      }
      const Vec2 at = tileCentre(s.x - g.world.ox, s.y - g.world.oy);
      const float d = len2(at - g.pl().p);
      if (d < bd && d > (2.0f * TILE) * (2.0f * TILE)) { bd = d; best = &s; }
    }
  return best;
}

// (stageMood) bring resident i out to spot (gx, gy) for a vignette (mood 1 begging, 2 brawling): out of their own door
// when the spot is on screen (never out of thin air), already there when it is not. Returns the actor id or -1.
int LifeOps::spawnMood(Game& g, int si, life::Census& c, RT::SiteRt& S, int i, int32_t gx, int32_t gy, uint8_t mood) {
  RT& R = rt(g);
  const life::Resident& r = c.res[(size_t)i];
  const Vec2 at = tileCentre(gx - g.world.ox, gy - g.world.oy);
  Vec2 from = at;
  if (onScreen(g, at, -1.0f)) {
    int hx, hy;
    const int hb = r.home >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], r.home) : -1;
    if (doorStep(g, hb, hx, hy) && len2(tileCentre(hx, hy) - at) <= (16.0f * TILE) * (16.0f * TILE)) from = tileCentre(hx, hy);
    else {   // home is across town: out of the nearest door to the spot (a neighbour's, a shop's)
      const Site& st = g.world.sites[(size_t)si];
      int bd = 1 << 30;
      for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
        int x, y;
        if (!doorStep(g, b, x, y)) continue;
        const int d = std::abs(x - tileX(at)) + std::abs(y - tileY(at));
        if (d < bd) { bd = d; from = tileCentre(x, y); }
      }
      if (bd > 16) return -1;
    }
  }
  const int id = spawnResident(g, si, c, i, from, false);
  Actor& a = g.actors.back();
  RT::ARt& A = R.rt(id);
  const life::Plan& p = S.plans[(size_t)i];
  A.kind = T_SPOT; A.gx = gx; A.gy = gy; A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
  A.arrived = from.x == at.x && from.y == at.y; A.mood = mood;
  claimSpot(g, A, id, gx, gy);
  if (A.arrived && mood == 1) { a.posture = Posture::Beg; a.face = 0; }
  return id;
}

bool LifeOps::homeBurned(const Game& g, const Site& st, const life::Census& c, const life::Resident& r) {
  const int hb = r.home >= 0 ? c.bldgHandle(st, r.home) : -1;
  return hb >= 0 && hb < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)hb].charred == 2;
}

// a resident who should stay out whatever the hour: the giver of an open quest or a story's person (daytime)
bool LifeOps::keepOut(Game& g, int si, const life::Resident& r) {
  if ((r.flags & life::RF_GRIEVING) && r.job != life::Job::Child && r.job != life::Job::Beggar) {
    // the grieving stay home (out only for work)
    (void)si;
  }
  return true;
}

// a resident who just got to their outdoor spot: the posture of what they do there
void LifeOps::settleOutdoor(Game& g, Actor& a, RT::ARt& A, const life::Census& c, const life::Resident& r) {
  using life::Act;
  using life::Job;
  a.st = AState::Idle;
  a.posture = Posture::None;
  a.postureT = 0;
  A.t = 4.0f + (float)(mixu(c.seed, (uint64_t)a.id * 31u + (uint64_t)g.day) % 1000u) * 0.02f;
  switch (A.act) {
    case Act::Work:
      if (A.place == life::Place::Field) {
        switch (r.job) {
          case Job::Fisher: a.posture = Posture::Fish; break;
          case Job::Woodcutter: a.posture = Posture::Chop; break;
          case Job::Miner: a.posture = Posture::Hammer; break;
          case Job::Herder: case Job::Hunter: a.posture = Posture::None; break;
          default: a.posture = Posture::Hoe; break;
        }
        // face the work: the water, the woodpile, the ore (the spot's facing)
        if (RT::SiteRt* S = siteRtById(g, c.site))
          for (const auto* v : {&S->water, &S->wood, &S->mine})
            for (const RT::Spot& s : *v)
              if (s.x == A.gx && s.y == A.gy) { a.face = s.face; break; }
      } else if (r.job == Job::Labourer) a.posture = Posture::Carry;
      else if (r.job == Job::Merchant || r.job == Job::Tailor) a.posture = Posture::None;
      else a.posture = Posture::Sweep;
      break;
    case Act::Beg: a.posture = Posture::Beg; a.face = 0; break;
    case Act::Sleep:   // a beggar dozing in a doorway (anyone else out at this hour simply waits)
      if (r.job == Job::Beggar) { a.posture = Posture::Beg; bubble(a, Bubble::Zzz, 3); }
      break;
    case Act::Perform: a.posture = bardPosture(g.world.cultureOf(siteOfActor(g, a))); a.face = 0; break;
    case Act::Pray: a.posture = Posture::Pray; break;
    case Act::LightLamps: a.posture = Posture::Lamp; a.face = 1; A.t = 2.5f; break;
    case Act::Play: a.posture = Posture::None; break;   // (playTag picks the game)
    case Act::Eat: a.posture = Posture::Eat; break;
    default: break;
  }
}
