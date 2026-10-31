// LifeOps (life_ops.h, TOWNSFOLK lane): overheard talk and street incidents.
// P1 stumble-on hooks: overheard gossip naming a real need, and a thief, a broken cart or a brawl and their leads.
// (overheardP1b, the P1b items' gossip, is defined in life_game.cpp: it reads rpg/story/story_internal.h.)
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

Actor* LifeOps::actorById(Game& g, int id) {
  const int k = id >= 0 ? g.findActor(id) : -1;
  return k > 0 ? &g.actors[(size_t)k] : nullptr;
}

std::string LifeOps::callName(const life::Resident* r, const std::string& name, uint32_t h) {
  const std::string fn = firstName(name);
  if (!r) return fn;
  if (r->age >= 58 && h % 2u == 0) return "OLD " + fn;
  if (h % 3u == 1 && r->job != life::Job::Child && r->job != life::Job::Elder && r->job != life::Job::None) {
    std::string j = life::jobName(r->job);
    return fn + " THE " + j;
  }
  return fn;
}

const life::Resident* LifeOps::residentOf(Game& g, const Actor& a) {
  if (a.resident < 0 || a.site < 0 || a.site >= (int)g.world.sites.size()) return nullptr;
  const life::Census* c = g.life.find(g.world.sites[(size_t)a.site].id);
  return c && a.resident < (int)c->res.size() ? &c->res[(size_t)a.resident] : nullptr;
}

// the lead's place: the person's own door (a resident), else where they stand (global tile)
void LifeOps::leadPlace(Game& g, const Actor& a, jobs::Lead& L) {
  L.hasPos = true;
  L.gx = g.world.ox + tileX(a.p); L.gy = g.world.oy + tileY(a.p);
  const life::Resident* r = residentOf(g, a);
  if (r && r->home >= 0) {
    const life::Census* c = g.life.find(g.world.sites[(size_t)a.site].id);
    int x, y;
    if (c && doorStep(g, c->bldgHandle(g.world.sites[(size_t)a.site], r->home), x, y)) { L.gx = g.world.ox + x; L.gy = g.world.oy + y; }
  }
}

// (5a) overheard: a chatting pair near the player lets slip a real need of someone in town ("...OLD BRANDR'S BEEN
// ASKING FOR SOMEONE TO CULL THE WOLVES..."): a person in play in this settlement with work to give, not overheard this
// week; the line becomes a journal lead (LeadFrom::Gossip) pointing at their door. At most two a town a day.
// (P1b "New Roads Content", REQUESTS lane; VISION_PLAN 15.35) overheard, the new items' way: while a P1b request or tale
// that carries `gossip` lines (dsl.h) is on offer in this settlement (story::tellerOffer would offer it to someone here:
// the very person who raises it), a chatting pair lets one of its lines slip, and the journal keeps a LeadFrom::Gossip
// lead pointing at that teller's door. The P1 rules: a teller once a week (Game::marks MK_P1B_GOSSIP), two overheard
// leads a town a day (the P1 counter, shared). Out of a forced test, a town's or city's notice board that holds a request
// with gossip may be the subject too (no teller: the lead points at the board). A teller's P1b gossip lead closes when
// their story starts (it names no job, so JobOps never takes it: the sweep below does).
// the line as the street hears it, between "..." and "...": without its own closing full stop
std::string LifeOps::spoken(std::string s) {
  while (!s.empty() && s.back() == '.') s.pop_back();
  return s;
}

uint32_t LifeOps::overheard(Game& g, const Actor& speaker, int partnerId, bool force) {
  RT& R = rt(g);
  const int si = speaker.site;
  if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) return 0;
  const ew::Gid sid = g.world.sites[(size_t)si].id;
  const int week = g.day / 7;
  int32_t today = 0;
  {
    auto it = g.marks.find(markKey(sid, mkLife(1)));
    if (it != g.marks.end() && it->second / 4 == g.day) today = it->second % 4;
  }
  if (!force && today >= 2) return 0;
  // (P1b) the new items first, on one overheard line in two (always when forced, falling back to a job when none is)
  if (force || (mixu(sid ^ (uint64_t)speaker.id, (uint64_t)g.day * 24u + (uint64_t)g.hour) & 1u) == 0u) {
    const Actor sp = speaker;   // (a copy: the P1b offers may stream places in)
    if (const uint32_t id = overheardP1b(g, sp, partnerId, force, today)) return id;
  }
  std::vector<std::pair<uint32_t, int>> cand;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& o = g.actors[k];
    if (!o.npc || !o.human || o.hostile || o.comp || o.critter || o.st == AState::Dead || o.site != si || o.id == speaker.id || o.id == partnerId) continue;
    if (o.lifeBits & LB_THIEF) continue;
    if (!g.offersWork(o)) continue;
    const uint64_t key = g.npcKey(o);
    auto it = g.marks.find(markKey(key, mkLife(0)));
    if (it != g.marks.end() && it->second == week + 1) continue;
    cand.push_back({mixu(key, (uint64_t)g.day * 24u + (uint64_t)g.hour), (int)k});
  }
  if (cand.empty()) return 0;
  std::sort(cand.begin(), cand.end());
  const Actor giver = g.actors[(size_t)cand[0].second];   // (a copy: offerFor may stream places in)
  // (P1 fixer) the job named is the one this person offers when asked (makeOffer: their pool job this week), so the
  // lead's words, its qtype and the lead closing on taking the job all agree
  bool ok = false;
  const Quest q = g.makeOffer(giver, &ok);
  if (!ok) return 0;
  const life::Resident* gr = residentOf(g, giver);
  const uint32_t h = mixu(g.npcKey(giver), (uint64_t)g.day);
  const std::string who = callName(gr, giver.name, h);
  const std::string fn = firstName(giver.name);
  const std::string town = g.world.sites[(size_t)si].name;
  const std::string subj = q.subject.empty() ? std::string() : q.subject;
  const std::string mons = monsterPlural(q.mon);
  const std::string there = q.target >= 0 && q.target < (int)g.world.sites.size() ? g.world.sites[(size_t)q.target].name : std::string("THE NEXT TOWN");
  // the line said (short: the street's floating words; the first that fits 30 letters) and the journal's lead
  std::vector<std::string> lines;
  std::string lead;
  const std::string sj = subj.empty() ? std::string() : subj;
  switch (q.type) {
    case QType::Hunt:
      lines = {who + " WANTS THE " + mons + " CULLED", fn + " WANTS " + mons + " CULLED", fn + " NEEDS A HUNTER"};
      lead = fn + " OF " + town + " WANTS " + mons + " CULLED";
      break;
    case QType::Clear: lines = {fn + " WANTS " + there + " CLEARED", fn + " WANTS A DEN CLEARED"}; lead = fn + " OF " + town + " WANTS " + there + " CLEARED"; break;
    case QType::Deliver: lines = {who + " NEEDS A COURIER", fn + " NEEDS A COURIER"}; lead = fn + " OF " + town + " NEEDS A COURIER"; break;
    case QType::Missing:
      lines = {sj.empty() ? std::string() : sj + " WENT MISSING, POOR " + fn, fn + " HAS SOMEONE MISSING"};
      lead = fn + " OF " + town + ": SOMEONE IS MISSING";
      break;
    case QType::Retrieve: case QType::Heirloom:
      lines = {sj.empty() ? std::string() : fn + " LOST " + sj, fn + " LOST SOMETHING DEAR"};
      lead = fn + " OF " + town + " LOST " + (sj.empty() ? std::string("AN HEIRLOOM") : sj);
      break;
    case QType::Bounty: case QType::NamedBandit:
      lines = {sj.empty() ? std::string() : fn + " WANTS " + sj + " DEAD", fn + " WANTS BANDITS DEAD", fn + " OFFERS A BOUNTY"};
      lead = fn + " OF " + town + " OFFERS A BOUNTY";
      break;
    case QType::Protect: lines = {who + " FEARS FOR THE FARM", fn + " FEARS FOR THE FARM"}; lead = fn + " OF " + town + " FEARS FOR THE FARM"; break;
    case QType::Supply: lines = {who + " IS SHORT OF GOODS", fn + " IS SHORT OF GOODS"}; lead = fn + " OF " + town + " IS SHORT OF GOODS"; break;
    default: lines = {who + " COULD USE A HAND", fn + " NEEDS A HAND"}; lead = fn + " OF " + town + " NEEDS A HAND"; break;
  }
  std::string line;
  for (const std::string& c2 : lines) if (!c2.empty() && c2.size() <= 30) { line = c2; break; }
  if (line.empty()) line = (fn.size() > 18 ? fn.substr(0, 18) : fn) + " NEEDS HELP";
  jobs::Lead L;
  L.from = jobs::LeadFrom::Gossip;
  L.qtype = (uint8_t)q.type;
  L.site = sid;
  L.who = g.npcKey(giver);
  leadPlace(g, giver, L);
  L.text = lead;
  const uint32_t id = g.addLead(L);
  if (!id) return 0;
  g.marks[markKey(L.who, mkLife(0))] = week + 1;
  g.marks[markKey(sid, mkLife(1))] = g.day * 4 + std::min(3, today + 1);
  say(g, speaker, "..." + line + "...", rgba(232, 214, 150));
  R.street.gossipLeads++;
  return id;
}

// ---- (5b) street incidents: a thief running with a purse, a broken cart in the road, a brawl. Decided by the site and
// the hour (one an hour at most, by day, a fair way from rare in a city), staged near the player with the people in the
// street; the player steps in (talks to the victim, catches the thief, walks between the brawlers) and an Errand opens
// (Game::openErrand) and ends on the spot (finishErrand); ignored, the incident leaves a lead (LeadFrom::Incident).
bool LifeOps::incidentFit(Game& g, const Actor& a, int si) {
  if (!(a.lifeBits & LB_LIFE) || (a.lifeBits & (LB_GONE | LB_KEY)) || a.site != si || a.st == AState::Dead || a.resident < 0) return false;
  const life::Resident* r = residentOf(g, a);
  if (!r || r->job == life::Job::Child || r->job == life::Job::Guard || r->job == life::Job::Beggar || r->job == life::Job::Noble) return false;
  auto it = rt(g).act.find(a.id);
  if (it == rt(g).act.end() || it->second.kind != T_SPOT || it->second.mood) return false;
  if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) return false;
  return onScreen(g, a.p, -2.0f);
}

void LifeOps::holdForIncident(Game& g, Actor& a) {
  RT::ARt& A = rt(g).rt(a.id);
  breakPair(g, A, a.id);
  A.mood = MOOD_INCIDENT;
  A.task = TK_NONE;
  A.path.clear();
  A.arrived = true;
  A.t = 0; A.fails = 0;
  a.posture = Posture::None;
  a.st = AState::Idle;
}

bool LifeOps::startIncident(Game& g, int si, int kind) {
  RT& R = rt(g);
  RT::Incident& I = R.inc;
  if (I.kind || si < 0 || si >= (int)g.world.sites.size()) return false;
  const Site& st = g.world.sites[(size_t)si];
  RT::SiteRt& S = siteRt(g, si);
  const Vec2 pp = g.pl().p;
  const uint32_t h = mixu(st.id ^ 0x1AC2ull, (uint64_t)g.day * 24u + (uint64_t)g.hour);
  // the people in the street fit for it, nearest the player first (a little hash so it is not always the nearest)
  std::vector<std::pair<float, int>> cand;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!incidentFit(g, a, si)) continue;
    const float d = len(a.p - pp) / TILE;
    if (d < 2.5f || d > 13.0f) continue;
    cand.push_back({d + (float)(mixu(h, (uint64_t)a.id) % 300u) * 0.01f, a.id});
  }
  std::sort(cand.begin(), cand.end());
  if (cand.empty()) return false;
  if (kind == life::INC_THIEF) {
    Actor* v = actorById(g, cand[0].second);
    if (!v || S.edge.empty()) return false;
    Vec2 side = Vec2(v->p.x < pp.x ? -12.0f : 12.0f, 3.0f);
    {   // (P1 fixer r2) off from the victim on the open side when the far side is behind a stall or under an eave
      const Vec2 s2 = v->p + side;
      if (!openTile(g.world.over, tileX(s2), tileY(s2)) && openTile(g.world.over, tileX(v->p - Vec2(side.x, -side.y)), tileY(v->p - Vec2(side.x, -side.y))))
        side.x = -side.x;
    }
    const Vec2 at = snapWalk(g, v->p + side);
    // the way out: an edge of town on the thief's side, away from the player (never past them), of a few by hash.
    // (P1 integration fixer) only an edge the thief can actually reach from where they stand: thiefRun counts a failed
    // route as a fail and gives up after three, and a failed route is cached, so a way out with no road to it ended
    // the run within three frames of the snatch (the thief "escaped" standing still, and a catch found no thief)
    std::vector<int32_t> route;
    auto reachable = [&](const RT::Spot& s) { return findPath(g, tileX(at), tileY(at), s.x - g.world.ox, s.y - g.world.oy, route, true); };
    std::vector<std::pair<float, const RT::Spot*>> ways;
    for (int k = 0; k < 32; k++) {
      const RT::Spot& s = S.edge[(h + (uint32_t)k * 2654435761u) % S.edge.size()];
      const Vec2 e = tileCentre(s.x - g.world.ox, s.y - g.world.oy);
      const float dp = len(e - pp), dv = len(e - v->p);
      if (dp < 18.0f * TILE || dv > dp) continue;
      ways.push_back({-(dp - 1.3f * dv), &s});
    }
    std::stable_sort(ways.begin(), ways.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const RT::Spot* esc = nullptr;
    int tries = 0;
    for (const auto& w : ways)
      if (++tries <= 6 && reachable(*w.second)) { esc = w.second; break; }
    if (!esc) {   // (no way out on their side: the edges farthest from the player)
      ways.clear();
      for (int k = 0; k < 32; k++) {
        const RT::Spot& s = S.edge[(h + (uint32_t)k * 2654435761u) % S.edge.size()];
        const float dp = len(tileCentre(s.x - g.world.ox, s.y - g.world.oy) - pp);
        if (dp >= 18.0f * TILE) ways.push_back({-dp, &s});
      }
      std::stable_sort(ways.begin(), ways.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
      tries = 0;
      for (const auto& w : ways)
        if (++tries <= 6 && reachable(*w.second)) { esc = w.second; break; }
    }
    if (!esc) return false;
    Spawn sp;
    sp.npc = true; sp.role = Role::Villager; sp.site = si; sp.slot = THIEF_SLOT; sp.x = tileX(at); sp.y = tileY(at);
    const int tid = g.spawnHuman(sp, at);
    Actor& t = g.actors.back();
    t.lifeBits = LB_THIEF;
    t.name = "CUTPURSE";
    t.militia = false;
    t.fromMap = true;
    t.resident = -1;
    t.bldg = -1;
    t.p = at; t.home = at; t.goal = at;
    t.speed = 56.0f;
    t.look.topColor = rgba(58, 52, 50);
    t.look.bottomColor = rgba(44, 40, 38);
    // (P1 fixer) told from the crowd at a glance: a hooded charcoal travel cloak (nobody in town walks hooded by day)
    t.look.cloak = 1;
    t.look.cloakColor = rgba(40, 36, 42);
    t.look.hood = true;
    t.posture = Posture::Carry;   // the purse clutched to the chest
    RT::ARt& T = R.rt(tid);
    T = RT::ARt();
    T.kind = T_AWAY; T.gx = esc->x; T.gy = esc->y; T.pace = 1.0f;
    v = actorById(g, cand[0].second);   // (spawning moved the actors)
    if (!v) { t.lifeBits |= LB_GONE; return false; }
    holdForIncident(g, *v);
    v->face = faceOf(t.p - v->p);
    bubble(*v, Bubble::Exclaim, 2.5f);
    say(g, *v, "STOP, THIEF! MY PURSE!", rgba(250, 170, 130));
    g.sfx((int)Sfx::Hit, v->p, 1.6f, 0.25f);
    I = RT::Incident();
    I.kind = life::INC_THIEF; I.si = si; I.site = st.id; I.victim = v->id; I.other = tid;
    I.gx = g.world.ox + tileX(t.p); I.gy = g.world.oy + tileY(t.p); I.ex = esc->x; I.ey = esc->y;
  } else if (kind == life::INC_CART) {
    // a cart standing in the street near the player (the world's own cart props: nothing new is put down)
    const Map& m = g.world.over;
    const int px = tileX(pp), py = tileY(pp);
    int cx = -1, cy = -1, sx = -1, sy = -1;
    float best = 1e9f;
    for (int y = py - 12; y <= py + 12; y++)
      for (int x = px - 14; x <= px + 14; x++) {
        if (!m.in(x, y) || m.propAt(x, y) != (int)Prop::Cart + 1 || !st.r.contains(x, y)) continue;
        static const int ox[3] = {0, 1, -1}, oy[3] = {1, 0, 0};   // (P1 fixer: in front first, beside the loose wheel)
        for (int k = 0; k < 3; k++) {
          const int tx = x + ox[k], ty = y + oy[k];
          if (!m.in(tx, ty) || m.blocked(tx, ty) || m.bldgAt[(size_t)ty * m.w + tx] >= 0 || spotTaken(g, g.world.ox + tx, g.world.oy + ty, -1)) continue;
          const float d = (float)(std::abs(x - px) + std::abs(y - py)) + (float)(mixu(h, (uint64_t)(y * 4096 + x)) % 400u) * 0.01f;
          if (d >= 3.0f && d < best) { best = d; cx = x; cy = y; sx = tx; sy = ty; }
          break;
        }
      }
    if (cx < 0) return false;
    // its owner: the nearest fit person to the cart
    Actor* o = nullptr;
    float od = (16.0f * TILE) * (16.0f * TILE);
    for (auto& cd : cand) {
      Actor* a = actorById(g, cd.second);
      if (!a) continue;
      const float d = len2(a->p - tileCentre(cx, cy));
      if (d < od) { od = d; o = a; }
    }
    if (!o) return false;
    holdForIncident(g, *o);
    RT::ARt& A = R.rt(o->id);
    A.arrived = false;
    A.gx = g.world.ox + sx; A.gy = g.world.oy + sy;
    claimSpot(g, A, o->id, A.gx, A.gy);
    bubble(*o, Bubble::Exclaim, 2.0f);
    g.emit(Ev::Dust, tileCentre(cx, cy) + Vec2(-11, 11), 0, 0, "");   // (P1 fixer) the wheel just came off
    g.sfx((int)Sfx::Hit, tileCentre(cx, cy), 0.7f, 0.3f);
    I = RT::Incident();
    I.kind = life::INC_CART; I.si = si; I.site = st.id; I.victim = o->id;
    I.gx = g.world.ox + cx; I.gy = g.world.oy + cy;
  } else if (kind == life::INC_BRAWL) {
    // two who fall out: the first fit person and the nearest other within a few tiles of them
    for (size_t i = 0; i < cand.size(); i++) {
      Actor* a = actorById(g, cand[i].second);
      if (!a) continue;
      Actor* b = nullptr;
      float bd2 = (3.5f * TILE) * (3.5f * TILE);   // (neighbours already close: a few steps to square up, not a march)
      for (size_t j = 0; j < cand.size(); j++) {
        if (j == i) continue;
        Actor* o = actorById(g, cand[j].second);
        if (!o) continue;
        const float d = len2(o->p - a->p);
        if (d < bd2) { bd2 = d; b = o; }
      }
      if (!b) continue;
      holdForIncident(g, *a);
      holdForIncident(g, *b);
      RT::ARt& A = R.rt(a->id);
      RT::ARt& B = R.rt(b->id);
      A.partner = b->id; B.partner = a->id; A.t2 = 0; B.t2 = 0.7f;
      // a step apart, squared up
      // (P1 fixer) always side by side, face to face (west / east of the middle, 11 px each way): two people squared
      // up north-south were drawn in one column, one head painted over the other's body
      Vec2 mid = (a->p + b->p) * 0.5f;
      const Vec2 dir(b->p.x >= a->p.x ? 1.0f : -1.0f, 0.0f);
      {   // (a wall or a stall at one side: the pair shift along the street until both feet are free)
        static const float sh[5] = {0.0f, 6.0f, -6.0f, 12.0f, -12.0f};
        for (float sx : sh) {
          const Vec2 m2 = mid + Vec2(sx, 0);
          if (g.bodyFree(m2 - dir * 11.0f, a->radius, false) && g.bodyFree(m2 + dir * 11.0f, b->radius, false)) { mid = m2; break; }
        }
      }
      A.dest = mid - dir * 11.0f; B.dest = mid + dir * 11.0f;
      A.arrived = false; B.arrived = false;
      bubble(*a, Bubble::Anger, 2.0f);
      static const char* opener[] = {"SAY THAT AGAIN, I DARE YOU!", "YOU CHEATED ME, YOU DOG!", "THAT'S MY WIFE YOU'RE TALKING ABOUT!", "GIVE IT BACK!"};
      say(g, *a, opener[h % 4u], rgba(240, 150, 120));
      I = RT::Incident();
      I.kind = life::INC_BRAWL; I.si = si; I.site = st.id; I.victim = a->id; I.other = b->id;
      I.gx = g.world.ox + tileX(mid); I.gy = g.world.oy + tileY(mid);
      // the street stops to watch
      for (size_t k = 1; k < g.actors.size(); k++) {
        Actor& w = g.actors[k];
        if (w.id == a->id || w.id == b->id || !(w.lifeBits & LB_LIFE) || w.site != si || len2(w.p - mid) > (7.0f * TILE) * (7.0f * TILE)) continue;
        if (w.st == AState::Walk) continue;
        w.face = faceOf(mid - w.p);
        bubble(w, Bubble::Exclaim, 1.4f);
      }
      break;
    }
    if (I.kind != life::INC_BRAWL) return false;
  } else return false;
  if (Actor* v = actorById(g, I.victim)) { I.vSite = v->site; I.vBldg = v->bldg; I.vSlot = v->slot; I.vKey = g.npcKey(*v); I.vName = v->name; }
  R.street.incidents++;
  return true;
}

// an incident's resident (the robbed, the cart's owner, a brawler): what they do while it lasts
bool LifeOps::incidentHeld(Game& g, Actor& a, RT::ARt& A, float dt) {
  RT& R = rt(g);
  RT::Incident& I = R.inc;
  if (!I.kind || (a.id != I.victim && a.id != I.other)) {   // it is over: back to their day
    A.mood = 0; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN;
    a.posture = Posture::None; a.st = AState::Idle;
    return true;
  }
  life::Census* c = g.life.findMut(I.site);
  switch (I.kind) {
    case life::INC_THIEF: {
      Actor* t = actorById(g, I.other);
      a.st = AState::Idle;
      a.face = faceOf((t ? t->p : g.pl().p) - a.p);
      if (!I.engaged && len2(g.pl().p - a.p) < (4.0f * TILE) * (4.0f * TILE) && I.sayT <= 0) {
        I.sayT = 6.0f;
        bubble(a, Bubble::Exclaim, 1.5f);
        a.face = faceOf(g.pl().p - a.p);
        say(g, a, "HE'S GOT MY PURSE! STOP HIM!", rgba(250, 190, 150));
      }
      return true;
    }
    case life::INC_CART: {
      if (!A.arrived) {
        a.posture = Posture::None;
        if (walkOut(g, a, A, a.speed * kWalk * 1.1f, dt) || A.fails >= 3) { A.arrived = true; A.path.clear(); }
        else { watch(g, a, A, dt); return true; }
      }
      a.st = AState::Idle;
      a.face = faceOf(tileCentre(I.gx - g.world.ox, I.gy - g.world.oy) - a.p);
      a.posture = I.engaged && I.holdT > 0 ? Posture::Hammer : Posture::Kneel;
      if (!I.engaged && len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE) && I.sayT <= 0) {
        I.sayT = 8.0f;
        bubble(a, Bubble::Question, 2.0f);
        say(g, a, "MY WHEEL'S OFF! A HAND, PLEASE?", rgba(232, 222, 196));
      }
      return true;
    }
    case life::INC_BRAWL: {
      if (!A.arrived) {   // square up (a step, never a jump)
        if (len2(A.dest - a.p) > 4.0f && g.bodyFree(A.dest, a.radius, false)) { stepToward(g, a, A.dest, a.speed * 0.9f, dt); A.t += dt; if (A.t < 4.0f) return true; }
        A.arrived = true; A.t = 0;
      }
      if (c) brawl(g, a, A, *c, dt);
      return true;
    }
    default: break;
  }
  return true;
}

// (P1 fixer r2) parted: the two brawlers back off from whoever stepped between them to a body and a half on their own
// side (the player stands where the fight was: left there, all three were drawn in one heap), glare a moment each (not
// in step), then go back to their day
void LifeOps::partFrom(Game& g, Actor& a, Vec2 from, float side, float vy) {
  RT::ARt& A = rt(g).rt(a.id);
  breakPair(g, A, a.id);
  A.mood = MOOD_PARTED; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.path.clear();
  A.t = 0; A.arrived = false; A.fails = 0;
  // their own side first; a wall, a post or a hedge there: off at a slant, then up (or down) the street, the two
  // always on opposite sides of the one who parted them
  const Vec2 dirs[5] = {Vec2(side, 0), Vec2(side * 0.7f, vy * 0.7f), Vec2(side * 0.7f, -vy * 0.7f), Vec2(0, vy), Vec2(-side * 0.45f, vy * 0.9f)};
  A.dest = a.p;
  bool found = false;
  for (const Vec2& dv : dirs) {
    for (float d : {28.0f, 22.0f, 17.0f}) {
      const Vec2 c = from + dv * d;
      if (len2(c - from) <= len2(a.p - from) + 4.0f) continue;   // (never closer than they already are)
      if (g.bodyFree(c, a.radius, false)) { A.dest = c; found = true; break; }
    }
    if (found) break;
  }
  A.t2 = 2.1f + (float)(mixu((uint64_t)a.id, 0xB4A1ull) % 9u) * 0.15f;
  a.posture = Posture::None; a.st = AState::Idle;
}

void LifeOps::partBrawlers(Game& g, int ida, int idb, Vec2 from) {
  Actor* a = actorById(g, ida);
  Actor* b = actorById(g, idb);
  float sa = -1.0f, sb = 1.0f;
  if (a && b) { sa = a->p.x <= b->p.x ? -1.0f : 1.0f; sb = -sa; }
  else if (a) sa = a->p.x < from.x ? -1.0f : 1.0f;
  else if (b) sb = b->p.x < from.x ? -1.0f : 1.0f;
  if (a) partFrom(g, *a, from, sa, -1.0f);
  if ((b = actorById(g, idb))) partFrom(g, *b, from, sb, 1.0f);
}

bool LifeOps::partedHeld(Game& g, Actor& a, RT::ARt& A, float dt) {
  A.t += dt;
  const Vec2 pp = g.pl().p;
  if (!A.arrived) {
    if (len2(A.dest - a.p) > 1.5f && A.t < 1.8f) {
      // backing off with their eyes on the one who parted them: a startled quick step that slows (ease-out)
      const float ease = 1.6f - 1.1f * std::min(1.0f, A.t / 0.7f);
      stepToward(g, a, A.dest, a.speed * ease, dt);
      a.face = faceOf(pp - a.p);
      return true;
    }
    A.arrived = true;
  }
  a.st = AState::Idle;
  a.face = faceOf(pp - a.p);
  if (A.t >= A.t2) {   // over: back to their day
    A.mood = 0; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN; A.arrived = false;
    a.posture = Posture::None;
  }
  return true;
}

// the cutpurse: runs for the edge of town, a look back over the shoulder now and then (catchable on foot); caught or
// away, off out of town
bool LifeOps::thiefFolk(Game& g, Actor& a, float dt) {
  RT& R = rt(g);
  RT::ARt& T = R.rt(a.id);
  if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
  const bool running = R.inc.kind == life::INC_THIEF && R.inc.other == a.id;
  if (running) {
    T.t += dt;
    a.posture = Posture::Carry;
    if (std::fmod(T.t, 5.5f) > 4.9f) { a.st = AState::Idle; a.face = faceOf(g.pl().p - a.p); return true; }
    if (thiefRun(g, a, T, a.speed, dt) || T.fails >= 3) T.arrived = true;
    watch(g, a, T, dt);
    // (P1 fixer) running, not walking: a puff of dust at the heels every few strides
    if (a.st == AState::Walk && std::fmod(T.t, 0.32f) < dt) g.emit(Ev::Dust, a.p + Vec2(0, 1), 0, 0, "");
    return true;
  }
  // (P1 fixer r2) caught: down on one knee, the purse given up, eyes on the one who caught them, a moment before
  // slinking off (they ran on at once, and the catch never read as one)
  if (T.t2 > 0) {
    T.t2 -= dt;
    if (T.t2 > 1.45f && len2(T.dest - a.p) > 1.0f) {   // the stumble (fast, slowing)
      stepToward(g, a, T.dest, a.speed * (0.6f + 2.0f * (T.t2 - 1.45f)), dt);
      a.face = faceOf(g.pl().p - a.p);
      return true;
    }
    a.st = AState::Idle;
    a.posture = T.t2 > 0.35f ? Posture::Kneel : Posture::None;
    a.face = faceOf(g.pl().p - a.p);
    return true;
  }
  a.posture = Posture::None;
  if (walkOut(g, a, T, a.speed * 0.85f, dt) || T.fails >= 3 || !onScreen(g, a.p, 1.0f)) a.lifeBits |= LB_GONE;
  return true;
}

// (P1 fixer r2) the cutpurse's run: a route that keeps to the open street (findPath inView), followed waypoint by
// waypoint, a corner cut only where the straight way is open too. (walkOut's own look-ahead cut through the gap behind
// a stall's counter, so the thief ran, and was caught, under the awning: a ghost in the stall)
bool LifeOps::openLine(Game& g, Vec2 a, Vec2 b, float r) {
  if (!clearLine(g, a, b, r)) return false;
  const Vec2 d = b - a;
  const float l = len(d);
  for (float t = 4.0f; t < l; t += 6.0f) {
    const Vec2 q = a + d * (t / l);
    if (!openTile(g.world.over, tileX(q), tileY(q))) return false;
  }
  return true;
}

bool LifeOps::thiefRun(Game& g, Actor& a, RT::ARt& T, float speed, float dt) {
  RT& R = rt(g);
  const int lx = T.gx - g.world.ox, ly = T.gy - g.world.oy;
  const Vec2 dest = tileCentre(lx, ly);
  if (len2(dest - a.p) < 3.0f * 3.0f) return true;
  if (T.path.empty()) {
    if (R.pathBudget > 0) R.pathBudget--;   // (one route a run: never held up by the street's budget)
    std::vector<int32_t> p;
    if (!findPath(g, tileX(a.p), tileY(a.p), lx, ly, p, true)) { T.fails++; a.st = AState::Idle; return false; }
    T.path = std::move(p);
    T.pi = 0; T.lookT = 0; T.stuckT = 0;
  }
  T.lookT -= dt;
  if (T.lookT <= 0) {
    T.lookT = 0.3f;
    for (size_t k = std::min(T.path.size() / 2, T.pi + 4); k > T.pi + 1; k--) {
      const Vec2 w = tileCentre(T.path[(k - 1) * 2] - g.world.ox, T.path[(k - 1) * 2 + 1] - g.world.oy);
      if (openLine(g, a.p, w, a.radius)) { T.pi = k - 1; break; }
    }
  }
  while (T.pi < T.path.size() / 2) {
    const Vec2 w = tileCentre(T.path[T.pi * 2] - g.world.ox, T.path[T.pi * 2 + 1] - g.world.oy);
    if (len2(w - a.p) < 2.5f * 2.5f) { T.pi++; continue; }
    const Vec2 before = a.p;
    stepToward(g, a, w, speed, dt);
    if (len2(a.p - before) < 0.0004f) {   // wedged on a corner: back to the tile's middle and plan again
      T.stuckT += dt;
      if (T.stuckT > 1.0f) { T.stuckT = 0; T.path.clear(); T.fails++; unwedge(g, a); }
    } else T.stuckT = std::max(0.0f, T.stuckT - dt);
    return false;
  }
  stepToward(g, a, dest, speed, dt);
  return len2(dest - a.p) < 3.0f * 3.0f;
}

int LifeOps::incidentTier(const Game& g, int si) {
  const SiteType t = g.world.sites[(size_t)si].type;
  return t == SiteType::Village ? 0 : t == SiteType::Town ? 1 : 2;
}

// the errand the player took on (the victim is the giver: their identity, their thanks)
int LifeOps::openIncidentErrand(Game& g, const Actor& giver) {
  RT& R = rt(g);
  RT::Incident& I = R.inc;
  if (I.quest) return I.quest;
  Quest q;
  q.type = QType::Errand;
  q.stage = (int)(I.kind == life::INC_THIEF ? jobs::ErrandKind::Thief : I.kind == life::INC_CART ? jobs::ErrandKind::Cart : jobs::ErrandKind::Brawl);
  q.giverSite = giver.site; q.giverBldg = giver.bldg; q.giverSlot = giver.slot; q.giverName = giver.name;
  q.giverId = I.site;
  const std::string fn = firstName(giver.name);
  const std::string town = g.world.sites[(size_t)I.si].name;
  const int tier = incidentTier(g, I.si);
  const uint32_t h = mixu(I.site, (uint64_t)g.day * 24u + (uint64_t)g.hour);
  q.gold = 8 + tier * 6 + (int)(h % 7u);
  q.xp = 15 + tier * 6;
  switch (I.kind) {
    case life::INC_THIEF:
      q.title = "STOP, THIEF!"; q.subject = "CATCH THE THIEF";
      q.desc = "A CUTPURSE SNATCHED " + fn + "'S PURSE IN " + town + ". CATCH THEM BEFORE THEY'RE OUT OF TOWN.";
      break;
    case life::INC_CART:
      q.title = "A BROKEN CART"; q.subject = "HELP " + fn + " WITH THE CART";
      q.desc = fn + "'S CART LOST A WHEEL IN THE STREETS OF " + town + ". HOLD IT UP WHILE THE WHEEL GOES BACK ON.";
      break;
    default:
      q.title = "A BRAWL IN THE STREET"; q.subject = "BREAK UP THE BRAWL";
      q.desc = "TWO OF " + town + "'S FOLK ARE AT EACH OTHER'S THROATS. STEP IN BEFORE SOMEONE GETS HURT.";
      break;
  }
  q.hasPos = true; q.tgx = I.gx; q.tgy = I.gy;
  I.quest = g.openErrand(q);
  I.engaged = true;
  R.street.errandsOpened++;
  return I.quest;
}

void LifeOps::finishIncident(Game& g, bool success) {
  RT& R = rt(g);
  if (R.inc.quest) {
    g.finishErrand(R.inc.quest, success);
    if (success) R.street.errandsWon++; else R.street.errandsLost++;
  }
}

// ignored: the incident leaves a lead (the person to see; their job when they have one to give)
void LifeOps::incidentLead(Game& g, Actor* victim) {
  RT& R = rt(g);
  const RT::Incident& I = R.inc;
  if (I.engaged || I.si < 0) return;
  jobs::Lead L;
  L.from = jobs::LeadFrom::Incident;
  L.site = I.site;
  L.qtype = (uint8_t)QType::COUNT;
  L.hasPos = true; L.gx = I.gx; L.gy = I.gy;
  const std::string town = g.world.sites[(size_t)I.si].name;
  std::string fn = I.vName.empty() ? std::string("SOMEONE") : firstName(I.vName);
  L.who = I.vKey;
  if (victim) {
    L.who = g.npcKey(*victim);
    leadPlace(g, *victim, L);
    fn = firstName(victim->name);
    if (g.offersWork(*victim)) {
      bool ok = false;
      const Quest q = g.makeOffer(*victim, &ok);   // (P1 fixer: the job they really offer when asked)
      if (ok) L.qtype = (uint8_t)q.type;
    }
  }
  L.text = I.kind == life::INC_THIEF ? "SEEN: A THIEF ROBBED " + fn + " IN " + town
           : I.kind == life::INC_CART ? "SEEN: " + fn + "'S CART BROKE DOWN IN " + town
                                      : "SEEN: " + fn + " IN A BRAWL IN " + town;
  if (g.addLead(L)) R.street.incidentLeads++;
}

void LifeOps::endIncident(Game& g) {
  RT& R = rt(g);
  for (int id : {R.inc.victim, R.inc.other}) {
    Actor* a = actorById(g, id);
    if (!a || (a->lifeBits & LB_THIEF)) continue;
    auto it = R.act.find(id);
    if (it == R.act.end()) continue;
    RT::ARt& A = it->second;
    if (A.mood != MOOD_INCIDENT) continue;
    A.mood = 0; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN;
    a->posture = Posture::None; a->st = AState::Idle;
  }
  R.inc = RT::Incident();
}

// the player stepped in (dialogue's I'LL HELP, or caught up with it)
void LifeOps::engageIncident(Game& g, Actor& by) {
  RT& R = rt(g);
  RT::Incident& I = R.inc;
  if (!I.kind) return;
  Actor* v = actorById(g, I.victim);
  if (I.kind == life::INC_BRAWL) {
    // the giver: someone watching (the brawlers do not pay to be parted), else the calmer of the two
    const Actor* giver = nullptr;
    float bd = (8.0f * TILE) * (8.0f * TILE);
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& w = g.actors[k];
      if (w.id == I.victim || w.id == I.other || !(w.lifeBits & LB_LIFE) || w.site != I.si || w.resident < 0 || w.st == AState::Dead) continue;
      const life::Resident* r = residentOf(g, w);
      if (!r || r->job == life::Job::Child) continue;
      const float d = len2(w.p - by.p);
      if (d < bd) { bd = d; giver = &w; }
    }
    Actor* o = actorById(g, I.other);
    const Actor gv = giver ? *giver : (o ? *o : by);
    openIncidentErrand(g, gv);
    if (v) { bubble(*v, Bubble::Question, 1.6f); say(g, *v, "ALL RIGHT, ALL RIGHT! HE STARTED IT.", rgba(232, 222, 196)); }
    if ((o = actorById(g, I.other))) { o->face = faceOf(g.pl().p - o->p); bubble(*o, Bubble::Tear, 1.6f); }
    finishIncident(g, true);
    const int ia = I.victim, ib = I.other;
    endIncident(g);
    partBrawlers(g, ia, ib, by.p);
    return;
  }
  if (v) openIncidentErrand(g, *v);
}

// once a step outdoors: the hour's incident decided and staged; the live one followed to its end
void LifeOps::incidentTick(Game& g, float dt) {
  RT& R = rt(g);
  RT::Incident& I = R.inc;
  if (I.sayT > 0) I.sayT -= dt;
  for (size_t k = 0; k < R.sayLater.size();) {   // (P1 fixer r2) the lines said a beat later
    RT::LaterSay& ls = R.sayLater[k];
    if ((ls.t -= dt) > 0) { k++; continue; }
    if (Actor* sp = actorById(g, ls.actor)) say(g, *sp, ls.s, ls.col);
    R.sayLater.erase(R.sayLater.begin() + (std::ptrdiff_t)k);
  }
  // an errand left running with no incident behind it (a load, the clock jumped): it passes once the player is well
  // away, or after a little while
  if (!I.kind) {
    bool orphan = false;
    for (Quest& q : g.quests) {
      if (q.type != QType::Errand || q.state == QState::Done) continue;
      orphan = true;
      R.orphanT += dt;
      const float d = (float)(std::abs(q.tgx - (g.world.ox + tileX(g.pl().p))) + std::abs(q.tgy - (g.world.oy + tileY(g.pl().p))));
      if (R.orphanT > 25.0f || d > 48.0f) { g.finishErrand(q.id, false); R.orphanT = 0; break; }
    }
    if (!orphan) R.orphanT = 0;
  }
  if (!I.kind) {
    if ((R.incidentCheckT -= dt) > 0) return;
    R.incidentCheckT = 1.0f;
    if (g.mode != Mode::Play || g.inside || g.hour < 8.0f || g.hour >= 20.0f) return;
    const int px = tileX(g.pl().p), py = tileY(g.pl().p);
    int si = -1;
    for (int s : g.activeSites_)
      if (s >= 0 && s < (int)g.world.sites.size() && g.world.sites[(size_t)s].settlement() && g.world.sites[(size_t)s].r.contains(px, py)) si = s;
    if (si < 0 || warEmpty(g, si)) return;
    for (int hi : g.hostiles_)
      if (hi >= 0 && hi < (int)g.actors.size() && g.actors[(size_t)hi].aggro && g.actors[(size_t)hi].st != AState::Dead) return;   // (not in a fight)
    const Site& st = g.world.sites[(size_t)si];
    const int32_t slot = g.day * 24 + (int)g.hour;
    const uint64_t key = st.id * 31ull ^ (uint64_t)(uint32_t)slot;
    if (R.incidentSlots.count(key)) return;
    const uint32_t h = mixu(st.id ^ 0x1AC1Dull, (uint64_t)(uint32_t)slot);
    const int minute = 6 + (int)((h >> 8) % 40u);
    if ((g.hour - std::floor(g.hour)) * 60.0f < (float)minute) return;
    if (R.incidentSlots.size() > 2048) R.incidentSlots.clear();
    R.incidentSlots.insert(key);
    const int tier = incidentTier(g, si);
    if ((int)(h % 100u) >= (tier == 0 ? 10 : tier == 1 ? 24 : 32)) return;
    int kind = (int)((h >> 16) % 3u) + 1;
    if (tier == 0 && kind == life::INC_THIEF) kind = life::INC_CART;
    for (int k = 0; k < 3; k++, kind = kind % 3 + 1)
      if (startIncident(g, si, kind)) return;
    return;
  }
  I.t += dt;
  Actor* v = actorById(g, I.victim);
  const Vec2 pp = g.pl().p;
  if (I.kind == life::INC_THIEF) {
    Actor* t = actorById(g, I.other);
    if (t) {
      I.gx = g.world.ox + tileX(t->p); I.gy = g.world.oy + tileY(t->p);
      if (I.quest)
        for (Quest& q : g.quests) if (q.id == I.quest) { q.tgx = I.gx; q.tgy = I.gy; }
    }
    if (t && len2(t->p - pp) < (1.4f * TILE) * (1.4f * TILE) && g.mode == Mode::Play) {   // caught
      if (v) openIncidentErrand(g, *v);
      else if (I.vSlot >= 0) {   // (the robbed is out of sight: their identity still pays, through the errand)
        Actor who;
        who.npc = true; who.site = I.vSite; who.bldg = I.vBldg; who.slot = I.vSlot; who.name = I.vName;
        openIncidentErrand(g, who);
      }
      t->face = faceOf(pp - t->p);
      bubble(*t, Bubble::Exclaim, 1.5f);
      say(g, *t, "ALL RIGHT! ALL RIGHT! TAKE IT!", rgba(220, 200, 170));
      // (P1 fixer r2) the robbed calls their thanks only when near enough to be heard (left behind down the street:
      // a heart, no second plate over the catch)
      if (v) {
        hush(g, *v);   // (their "STOP, THIEF!" still up from the snatch: done with)
        bubble(*v, Bubble::Heart, 2.0f);
        if (len2(v->p - pp) < (6.0f * TILE) * (6.0f * TILE)) { RT::LaterSay ls; ls.actor = v->id; ls.t = 1.2f; ls.s = "MY PURSE! BLESS YOU, FRIEND!"; ls.col = rgba(240, 220, 170); R.sayLater.push_back(ls); }
      }
      finishIncident(g, I.quest != 0);
      RT::ARt& T = R.rt(t->id);
      T.path.clear(); T.fails = 0;
      T.t2 = 1.8f;   // (P1 fixer r2) the caught moment (thiefFolk): a stumble a step on from the hand that caught them
      {   // (to the side of the one who caught them, both seen whole: one behind the other in 3/4 hides the thief)
        const float sx = t->p.x >= pp.x ? 1.0f : -1.0f;
        const Vec2 d = t->p - pp;
        const float l = std::max(len(d), 0.5f);
        const Vec2 cands[5] = {pp + Vec2(sx * 19.0f, 2.0f), pp + Vec2(-sx * 19.0f, 2.0f), pp + Vec2(sx * 15.0f, -6.0f), pp + Vec2(-sx * 15.0f, -6.0f), pp + d * (18.0f / l)};
        T.dest = t->p;
        bool got = false;
        for (int pass = 0; pass < 2 && !got; pass++)   // (out in the open first: never under a stall's awning)
          for (const Vec2& c : cands)
            if ((pass || openTile(g.world.over, tileX(c), tileY(c))) && g.bodyFree(c, t->radius, false) && clearLine(g, t->p, c, t->radius)) { T.dest = c; got = true; break; }
      }
      endIncident(g);
      return;
    }
    const bool away = !t || R.rt(t->id).arrived || len2(t->p - pp) > (28.0f * TILE) * (28.0f * TILE) || I.t > 80.0f;
    if (away) {
      if (I.quest) {
        if (v && onScreen(g, v->p)) { bubble(*v, Bubble::Tear, 2.0f); say(g, *v, "GONE... EVERY COIN I HAD.", rgba(220, 200, 190)); }
        finishIncident(g, false);
      } else incidentLead(g, v);
      if (t) t->lifeBits |= LB_GONE;
      endIncident(g);
    }
    return;
  }
  if (!v) { if (!I.engaged) incidentLead(g, nullptr); else finishIncident(g, false); endIncident(g); return; }
  if (I.kind == life::INC_CART) {
    const Vec2 cart = tileCentre(I.gx - g.world.ox, I.gy - g.world.oy);
    const float d = len(pp - cart) / TILE;
    if (I.engaged) {
      if (d < 2.3f) {
        if (I.holdT <= 0) { say(g, *v, "HOLD IT STEADY... THAT'S IT...", rgba(232, 222, 196)); g.sfx((int)Sfx::Hit, cart, 0.8f, 0.3f); }
        I.holdT += dt;
        if (I.holdT >= 6.0f) {
          bubble(*v, Bubble::Heart, 2.0f);
          say(g, *v, "GOOD AS NEW! THANK YOU, FRIEND.", rgba(240, 220, 170));
          finishIncident(g, true);
          endIncident(g);
        }
      } else if (d > 14.0f) {
        say(g, *v, "...I'LL MANAGE ON MY OWN, THEN.", rgba(220, 200, 190));
        finishIncident(g, false);
        endIncident(g);
      }
      return;
    }
    if (I.t > 150.0f || d > 30.0f) { incidentLead(g, v); endIncident(g); }
    return;
  }
  if (I.kind == life::INC_BRAWL) {
    Actor* o = actorById(g, I.other);
    if (!o) { if (!I.engaged) incidentLead(g, v); endIncident(g); return; }
    if (len2(v->p - pp) < (1.8f * TILE) * (1.8f * TILE) || len2(o->p - pp) < (1.8f * TILE) * (1.8f * TILE)) {   // stepped between them
      engageIncident(g, g.pl());
      return;
    }
    if (I.t > 42.0f) {   // they tire of it
      say(g, *v, "HMPH. THIS ISN'T OVER.", rgba(220, 200, 190));
      incidentLead(g, v);
      endIncident(g);
    }
  }
}
