// LifeOps (life_ops.h, TOWNSFOLK lane): the street.
// The street's day (TaskK: stalls, carry routes, sweeping, visits) and one resident actor's street life (streetFolk).
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

// a trade with no workplace at work in the street (WORK@STREET: merchants, tailors, scholars, stablehands, smiths and
// bakers without a shop of their own, servants, priests without a temple)
bool LifeOps::streetTrade(const life::Resident& r, const life::Plan& p) {
  using life::Job;
  if (p.act != life::Act::Work || p.place != life::Place::Street) return false;
  return r.job != Job::Labourer && r.job != Job::Lamplighter && r.job != Job::Beggar && r.job != Job::Guard && r.job != Job::Child;
}

bool LifeOps::leisureAct(const life::Plan& p) {
  using life::Act;
  return p.bldg < 0 && (p.act == Act::Wander || p.act == Act::Socialise || p.act == Act::Shop || p.act == Act::Home || p.act == Act::Eat ||
                        p.act == Act::Pray || p.act == Act::Tavern || p.act == Act::Bathe);
}

// a free place at a building's door (global tile): the step itself, else a tile beside or below it (two people never
// stand on one tile; a busy shop's door has a queue round it)
bool LifeOps::doorNear(Game& g, int b, int32_t& gx, int32_t& gy, int self) {
  int x, y;
  if (!doorStep(g, b, x, y)) return false;
  static const int ox[6] = {0, -1, 1, 0, -1, 1}, oy[6] = {0, 0, 0, 1, 1, 1};
  const Map& m = g.world.over;
  for (int k = 0; k < 6; k++) {
    const int tx = x + ox[k], ty = y + oy[k];
    if (!m.in(tx, ty) || m.blocked(tx, ty) || m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx]) continue;
    if (spotTaken(g, g.world.ox + tx, g.world.oy + ty, self)) continue;
    gx = g.world.ox + tx; gy = g.world.oy + ty;
    return true;
  }
  return false;
}

// where a trade's goods come from: the bakery (or the mill), the smithy, the market hall, the inn's yard, the lord's
// seat, the temple; a quarter of the rounds start from their own house
int LifeOps::sourceOf(Game& g, int si, const life::Census& c, const life::Resident& r) {
  using life::Job;
  int off = -1;
  switch (r.job) {
    case Job::Baker: off = c.bakery >= 0 ? c.bakery : c.mill; break;
    case Job::Smith: off = c.smithy; break;
    case Job::Merchant: case Job::Tailor: off = c.market >= 0 ? c.market : c.inn; break;
    case Job::Stablehand: off = c.inn; break;
    case Job::Servant: off = c.seat >= 0 ? c.seat : c.inn; break;
    case Job::Scholar: off = c.temple >= 0 ? c.temple : c.seat; break;
    case Job::Priest: off = c.temple; break;
    default: break;
  }
  if (off < 0 || mixu(c.seed ^ r.idx, 0x50Cull) % 4u == 0) off = r.home >= 0 ? r.home : off;
  int x, y;
  int b = resBldg(g, si, c, off);
  if (doorStep(g, b, x, y)) return b;
  b = resBldg(g, si, c, c.inn);
  return doorStep(g, b, x, y) ? b : -1;
}

// a real door of the census within [4, maxD] tiles of (gx, gy): a resident's home, the inn, the market (not notB);
// the first hashed candidate, or a nearer one half the time (short rounds and long ones)
int LifeOps::customerDoor(Game& g, int si, const life::Census& c, int32_t gx, int32_t gy, uint32_t h, int notB, int maxD, int self) {
  const int n = (int)c.res.size();
  if (n <= 0) return -1;
  int best = -1, bd = 1 << 30;
  for (int k = 0; k < 14; k++) {
    const uint32_t hk = mixu(h, (uint64_t)k * 7u + 1u);
    const uint32_t kind = hk % 10u;
    const int off = kind == 0 && c.inn >= 0 ? c.inn : kind == 1 && c.market >= 0 ? c.market : c.res[(size_t)((hk >> 8) % (uint32_t)n)].home;
    const int b = resBldg(g, si, c, off);
    if (b < 0 || b == notB) continue;
    int32_t dx, dy;
    if (!doorNear(g, b, dx, dy, self)) continue;
    const int d = std::abs(dx - gx) + std::abs(dy - gy);
    if (d < 4 || d > maxD) continue;
    if (best < 0 || (((hk >> 4) & 1u) && d < bd)) { best = b; bd = d; }
    if (k >= 5) break;
  }
  return best;
}

// a free spot of a list within [minD, maxD] tiles (Manhattan) of (gx, gy), by hash
const LifeOps::RT::Spot* LifeOps::pickNear(Game& g, const std::vector<RT::Spot>& v, uint32_t h, int32_t gx, int32_t gy, int minD, int maxD, int self) {
  if (v.empty()) return nullptr;
  for (int k = 0; k < 16; k++) {
    const RT::Spot& s = v[(h + (uint32_t)k * 2654435761u) % v.size()];
    const int d = std::abs(s.x - gx) + std::abs(s.y - gy);
    if (d < minD || d > maxD) continue;
    if (spotOk(g, s) && !spotTaken(g, s.x, s.y, self)) return &s;
  }
  // (P1 fixer r2) a city's lists run to thousands of spots: sixteen picks from the whole list rarely land within a few
  // tiles, and a stall keeper's every trip for stock then failed (stood at the stall for hours: town-motion seed 5).
  // One pass from a hashed start finds the first fitting spot (once a leg: cheap)
  const size_t n = v.size(), s0 = (size_t)(h % (uint32_t)n);
  for (size_t k = 0; k < n; k++) {
    const RT::Spot& s = v[(s0 + k) % n];
    const int d = std::abs(s.x - gx) + std::abs(s.y - gy);
    if (d < minD || d > maxD) continue;
    if (spotOk(g, s) && !spotTaken(g, s.x, s.y, self)) return &s;
  }
  return nullptr;
}

void LifeOps::headTo(Game& g, Actor& a, RT::ARt& A, int32_t gx, int32_t gy) {
  A.gx = gx; A.gy = gy; A.arrived = false; A.path.clear(); A.pi = 0; A.fails = 0; A.want = false; A.stuckT = 0;
  A.watchT = 0; A.watchP = a.p;
  claimSpot(g, A, a.id, gx, gy);
}

// a new segment: what this person does with it (weights by trade; the leisure hours stroll, look round the stalls,
// call on a friend)
void LifeOps::pickTask(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, int32_t seg) {
  using life::Job;
  RT& R = rt(g);
  A.seg = seg; A.leg = 0; A.dwell = 0; A.srcB = -1; A.walkPost = Posture::None; A.retry = 0; A.slip = false;
  breakPair(g, A, a.id);
  const uint32_t h = mixu(c.seed ^ ((uint64_t)r.idx << 12), (uint64_t)(uint32_t)seg * 2654435761u + 0x7A5Cull);
  int w[TK_COUNT] = {};
  if (streetTrade(r, p)) {
    switch (r.job) {
      case Job::Merchant: w[TK_STALL] = 42; w[TK_DELIVER] = 30; w[TK_SHOP] = 6; w[TK_VISIT] = 10; w[TK_NONE] = 12; break;
      case Job::Tailor: w[TK_STALL] = 36; w[TK_DELIVER] = 28; w[TK_VISIT] = 14; w[TK_SHOP] = 10; w[TK_NONE] = 12; break;
      case Job::Baker: w[TK_DELIVER] = 46; w[TK_STALL] = 32; w[TK_SWEEP] = 10; w[TK_NONE] = 12; break;
      case Job::Smith: w[TK_DELIVER] = 46; w[TK_SWEEP] = 22; w[TK_STALL] = 12; w[TK_NONE] = 20; break;
      case Job::Stablehand: w[TK_DELIVER] = 46; w[TK_SWEEP] = 34; w[TK_NONE] = 20; break;
      case Job::Scholar: w[TK_READ] = 38; w[TK_VISIT] = 24; w[TK_SHOP] = 14; w[TK_DELIVER] = 12; w[TK_NONE] = 12; break;
      case Job::Priest: w[TK_READ] = 34; w[TK_VISIT] = 34; w[TK_SWEEP] = 14; w[TK_NONE] = 18; break;
      case Job::Servant: w[TK_DELIVER] = 46; w[TK_SHOP] = 24; w[TK_SWEEP] = 20; w[TK_NONE] = 10; break;
      default: w[TK_DELIVER] = 36; w[TK_SWEEP] = 24; w[TK_VISIT] = 14; w[TK_NONE] = 26; break;
    }
  } else if (leisureAct(p) && r.job != Job::Child && r.job != Job::Beggar) {
    w[TK_NONE] = 52;
    w[TK_SHOP] = p.act == life::Act::Shop ? 56 : 18;
    w[TK_VISIT] = r.job == Job::Elder ? 26 : 18;
  } else w[TK_NONE] = 1;
  if (S.market.empty()) { w[TK_DELIVER] += w[TK_STALL]; w[TK_STALL] = 0; w[TK_NONE] += w[TK_SHOP]; w[TK_SHOP] = 0; }
  int sum = 0;
  for (int k = 0; k < TK_COUNT; k++) sum += w[k];
  int roll = (int)(h % (uint32_t)std::max(1, sum)), task = TK_NONE;
  for (int k = 0; k < TK_COUNT; k++) {
    if (roll < w[k]) { task = k; break; }
    roll -= w[k];
  }
  A.task = (uint8_t)task;
  if (!startLeg(g, a, A, c, r, p, S)) { A.task = TK_NONE; A.leg = 0; startLeg(g, a, A, c, r, p, S); }
  R.street.tasks[std::min<int>(A.task, 7)]++;
}

// the next stop of the task's route (false: no way to do it here: the caller falls back to a plain spot)
bool LifeOps::startLeg(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S) {
  const int si = a.site;
  const int32_t px = g.world.ox + tileX(a.p), py = g.world.oy + tileY(a.p);
  const uint32_t h = mixu(c.seed ^ ((uint64_t)r.idx << 12) ^ 0x1E6ull, (uint64_t)(uint32_t)A.seg * 977u + A.leg);
  int32_t gx = 0, gy = 0;
  switch (A.task) {
    case TK_STALL: {
      if (A.leg == 0) {   // a stall to mind (a market row's front, facing the passers-by)
        const RT::Spot* s = pickNear(g, S.market, h, px, py, 0, 40, a.id);
        if (!s) s = pick(g, S.market, h, a.id);
        if (!s) return false;
        A.ax = s->x; A.ay = s->y;
        A.walkPost = Posture::None;
        headTo(g, a, A, s->x, s->y);
        return true;
      }
      bool fetched = A.leg >= 2;
      if (A.leg & 1) {   // a trip for stock: a store door near the stall (none near: stay on)
        const int b = customerDoor(g, si, c, A.ax, A.ay, h, -1, 18, a.id);
        if (b >= 0 && doorNear(g, b, gx, gy, a.id)) { A.walkPost = Posture::None; headTo(g, a, A, gx, gy); return true; }
        if (const RT::Spot* s = pickNear(g, S.street, h, A.ax, A.ay, 3, 10, a.id)) { A.walkPost = Posture::None; headTo(g, a, A, s->x, s->y); return true; }   // (a handcart down the lane)
        A.leg++;
        fetched = false;
      }
      A.walkPost = fetched ? Posture::Carry : Posture::None;   // back with the stock
      if (spotTaken(g, A.ax, A.ay, a.id)) return false;
      headTo(g, a, A, A.ax, A.ay);
      return true;
    }
    case TK_DELIVER: {
      if (A.leg == 0) A.srcB = sourceOf(g, si, c, r);
      if (A.srcB < 0) return false;
      if ((A.leg & 1) == 0) {   // to the source (the goods to fetch)
        if (!doorNear(g, A.srcB, gx, gy, a.id)) return false;
        A.walkPost = Posture::None;
        headTo(g, a, A, gx, gy);
        return true;
      }
      int sx, sy;
      if (!doorStep(g, A.srcB, sx, sy)) return false;
      const int b = customerDoor(g, si, c, g.world.ox + sx, g.world.oy + sy, h, A.srcB, 30, a.id);
      if (b < 0 || !doorNear(g, b, gx, gy, a.id)) return false;
      A.walkPost = Posture::Carry;   // a sack, a basket, a crate on the shoulder
      headTo(g, a, A, gx, gy);
      return true;
    }
    case TK_SWEEP: {
      if (A.leg == 0) {   // their own lane (the house's, or the trade's door)
        int ox = px, oy = py, x, y;
        const int b = r.home >= 0 ? resBldg(g, si, c, r.home) : sourceOf(g, si, c, r);
        if (doorStep(g, b, x, y)) { ox = g.world.ox + x; oy = g.world.oy + y; }
        A.ax = ox; A.ay = oy;
      }
      const RT::Spot* s = pickNear(g, S.street, h, A.leg == 0 ? A.ax : px, A.leg == 0 ? A.ay : py, A.leg == 0 ? 0 : 1, A.leg == 0 ? 7 : 4, a.id);
      if (!s) s = pickNear(g, S.street, h, A.ax, A.ay, 0, 10, a.id);
      if (!s) return false;
      A.walkPost = Posture::None;
      headTo(g, a, A, s->x, s->y);
      return true;
    }
    case TK_READ: {
      const RT::Spot* s = pickNear(g, S.plaza, h, px, py, 3, 24, a.id);
      if (!s) s = pick(g, S.plaza, h, a.id);
      if (!s) return false;
      A.walkPost = Posture::None;
      headTo(g, a, A, s->x, s->y);
      return true;
    }
    case TK_VISIT: {   // a friend's door (else a neighbour's)
      int b = -1;
      const std::vector<int> fr = g.life.friendsOf(c, r.idx);
      for (size_t k = 0; k < fr.size() && k < 3 && b < 0; k++) {
        const int f = fr[(k + h) % std::min<size_t>(fr.size(), 3)];
        if (f < 0 || f >= (int)c.res.size() || c.res[(size_t)f].home < 0 || c.res[(size_t)f].home == r.home) continue;
        const int fb = resBldg(g, si, c, c.res[(size_t)f].home);
        int32_t dx, dy;
        if (fb >= 0 && doorNear(g, fb, dx, dy, a.id) && std::abs(dx - px) + std::abs(dy - py) <= 40) b = fb;
      }
      if (b < 0) b = customerDoor(g, si, c, px, py, h, r.home >= 0 ? resBldg(g, si, c, r.home) : -1, 26, a.id);
      if (b < 0 || !doorNear(g, b, gx, gy, a.id)) return false;
      A.walkPost = Posture::None;
      headTo(g, a, A, gx, gy);
      return true;
    }
    case TK_SHOP: {   // a round of the stalls
      const RT::Spot* s = pickNear(g, S.market, h, px, py, 2, 16, a.id);
      if (!s && A.leg == 0) s = pick(g, S.market, h, a.id);
      if (!s) return false;
      A.walkPost = Posture::None;
      headTo(g, a, A, s->x, s->y);
      return true;
    }
    default: {   // a plain spot of the plan's kind, somewhere new (never the spot they stand on: 3 tiles off at least)
      int face = 0;
      const uint32_t salt = (uint32_t)A.seg * 2u + 1u + (uint32_t)A.leg * 7919u;
      // (the fields: on along their own furrows, the next woodpile, the next stretch of bank: never across the land)
      const bool lunch = fieldLunch(g, r, p);   // (the midday bread and cheese is eaten out at the field: its spots)
      if ((p.place == life::Place::Field || lunch) && A.arrived) {
        life::Plan fp = p;
        if (lunch) { fp.act = life::Act::Work; fp.place = life::Place::Field; fp.bldg = -1; }
        if (const RT::Spot* s = pickNear(g, spotsFor(S, r, fp), h, px, py, 3, 12, a.id)) {
          A.walkPost = Posture::None;
          headTo(g, a, A, s->x, s->y);
          return true;
        }
      }
      if (!outdoorSpot(g, si, S, c, r, p, gx, gy, face, a.id, salt)) return false;
      for (uint32_t k = 1; k < 4 && std::abs(gx - px) + std::abs(gy - py) < 3; k++) {
        int32_t x2, y2;
        int f2 = 0;
        if (outdoorSpot(g, si, S, c, r, p, x2, y2, f2, a.id, salt + k * 0x5151u) && std::abs(x2 - px) + std::abs(y2 - py) >= 3) { gx = x2; gy = y2; }
      }
      const int far = std::abs(gx - px) + std::abs(gy - py);
      if (far > 36) {   // (a capital's square, a city's woods, are a long way across: somewhere of the kind nearer)
        const std::vector<RT::Spot>& v = spotsFor(S, r, fieldAware(g, r, p));
        if (const RT::Spot* s = pickNear(g, v, h ^ 0x4EA7u, px, py, 3, 30, a.id)) { gx = s->x; gy = s->y; }
      }
      if (std::abs(gx - px) + std::abs(gy - py) < 3 && p.place != life::Place::Field) {
        // (their own lane's spot is the nearest to the door whatever the hash: a few steps along it instead)
        const bool lane = p.place == life::Place::Street || p.place == life::Place::Home || p.place == life::Place::Work;
        const RT::Spot* s = pickNear(g, lane ? S.street : S.plaza, h, px, py, 3, 10, a.id);
        if (!s) s = pickNear(g, lane ? S.plaza : S.street, h, px, py, 3, 12, a.id);
        if (s) { gx = s->x; gy = s->y; }
      }
      A.walkPost = Posture::None;
      headTo(g, a, A, gx, gy);
      return true;
    }
  }
}

// the cry of a stall's wares
std::string LifeOps::wares(const life::Resident& r, uint32_t h) {
  using life::Job;
  static const char* merchant[] = {"FAIR PRICES, FRIEND!", "COME AND SEE, COME AND SEE!", "THE BEST IN TOWN!", "FRESH IN TODAY!"};
  static const char* baker[] = {"HOT BREAD! STILL WARM!", "FRESH LOAVES! HONEY CAKES!", "BREAD! GOOD BREAD!"};
  static const char* tailor[] = {"FINE CLOTH! GOOD WOOL!", "CLOAKS FOR THE COLD!", "MENDING DONE, CHEAP!"};
  static const char* smith[] = {"NAILS, HINGES, KNIVES!", "GOOD IRON, FAIR PRICE!"};
  switch (r.job) {
    case Job::Baker: return baker[h % 3u];
    case Job::Tailor: return tailor[h % 3u];
    case Job::Smith: return smith[h % 2u];
    default: return merchant[h % 4u];
  }
}

// got to the task's stop: the posture and the wait there
void LifeOps::taskArrive(Game& g, Actor& a, RT::ARt& A, const life::Resident& r) {
  (void)g;
  const uint32_t h = mixu((uint64_t)a.id * 31u + A.leg, (uint64_t)(uint32_t)A.seg);
  auto secs = [&](float lo, float hi) { return lo + (hi - lo) * (float)(h % 1000u) * 0.001f; };
  a.st = AState::Idle;
  a.postureT = 0;
  switch (A.task) {
    case TK_STALL:
      a.posture = Posture::None;
      if (A.leg & 1) { a.face = 1; A.dwell = secs(2.0f, 3.5f); }   // at the store's door: the stock taken up
      else { a.face = 0; A.dwell = secs(16.0f, 38.0f); A.hawkT = secs(1.5f, 6.0f); }
      break;
    case TK_DELIVER:
      a.posture = Posture::None;
      a.face = 1;
      if (A.leg & 1) { A.dwell = secs(1.8f, 3.2f); bubble(a, (h >> 7) % 3u == 0 ? Bubble::Coin : Bubble::Talk, 1.4f); }   // handed over
      else A.dwell = secs(2.4f, 4.5f);
      break;
    case TK_SWEEP: a.posture = Posture::Sweep; a.face = (int)((h >> 10) % 4u); A.dwell = secs(14.0f, 28.0f); break;
    case TK_READ: a.posture = r.job == life::Job::Priest ? Posture::Pray : Posture::Read; a.face = 0; A.dwell = 1e6f; break;
    case TK_VISIT:
      a.posture = Posture::None; a.face = 1; A.dwell = secs(6.0f, 11.0f);
      bubble(a, (h >> 3) & 1u ? Bubble::Talk : Bubble::Question, 1.6f);
      break;
    case TK_SHOP:
      a.posture = Posture::None; a.face = 1; A.dwell = secs(5.0f, 12.0f);
      if ((h >> 5) & 1u) bubble(a, Bubble::Coin, 1.5f);
      break;
    default: break;
  }
}

// at the stop: the wait (a stall's cries), then the next leg
void LifeOps::taskTick(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, float dt) {
  RT& R = rt(g);
  a.st = AState::Idle;
  A.dwell -= dt;
  if (A.task == TK_STALL && !(A.leg & 1)) {
    A.hawkT -= dt;
    if (A.hawkT <= 0) {
      const uint32_t hh = mixu((uint64_t)a.id, (uint64_t)(g.time * 3.0f));
      A.hawkT = 8.0f + (float)(hh % 1000u) * 0.01f;
      a.posture = Posture::Wave; a.postureT = 0;
      if ((hh >> 12) & 1u) bubble(a, Bubble::Talk, 1.2f);
      if (R.barkT <= 0 && len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE) && onScreen(g, a.p)) {
        R.barkT = 7.0f;
        say(g, a, wares(r, hh));
      }
    } else if (a.posture == Posture::Wave && a.postureT > 1.1f) a.posture = Posture::None;
  }
  if (A.dwell > 0) return;
  A.leg++;
  if ((A.task == TK_SHOP && A.leg >= 3) || (A.task == TK_VISIT && A.leg >= 1)) { A.task = TK_NONE; A.leg = 0; }
  if (!startLeg(g, a, A, c, r, p, S)) {
    A.task = TK_NONE; A.leg = 0;
    if (!startLeg(g, a, A, c, r, p, S)) A.dwell = 4.0f;
  }
}

// the kind of spots a plan's place uses (outdoorSpot's lists, roughly): for the plain-sight fallback
// a field hand's midday meal is out at the field (outdoorSpot's rule): the plan as the street's spots see it
life::Plan LifeOps::fieldAware(const Game& g, const life::Resident& r, const life::Plan& p) {
  life::Plan q = p;
  if (fieldLunch(g, r, p)) { q.act = life::Act::Work; q.place = life::Place::Field; q.bldg = -1; }
  return q;
}

const std::vector<LifeOps::RT::Spot>& LifeOps::spotsFor(RT::SiteRt& S, const life::Resident& r, const life::Plan& p) {
  using life::Place;
  using life::Job;
  switch (p.place) {
    case Place::Field:
      switch (r.job) {
        case Job::Fisher: if (!S.water.empty()) return S.water; break;
        case Job::Woodcutter: if (!S.wood.empty()) return S.wood; break;
        case Job::Miner: if (!S.mine.empty()) return S.mine; break;
        case Job::Herder: if (!S.pasture.empty()) return S.pasture; break;
        default: break;
      }
      return !S.farm.empty() ? S.farm : S.street;
    case Place::Market: return !S.market.empty() ? S.market : S.plaza;
    case Place::Plaza: case Place::Gathering: case Place::Gathering2: case Place::Temple: return S.plaza;
    default: return S.street;
  }
}

// a spot of the plan's kind (else any street or square spot) 2..8 tiles off in plain sight: walked straight to
bool LifeOps::plainSight(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, const life::Resident& r, const life::Plan& p) {
  const int32_t px = g.world.ox + tileX(a.p), py = g.world.oy + tileY(a.p);
  const uint32_t h = mixu((uint64_t)a.id * 7u + A.leg, (uint64_t)(uint32_t)A.seg);
  const std::vector<RT::Spot>* lists[3] = {&spotsFor(S, r, fieldAware(g, r, p)), &S.street, &S.plaza};
  for (const std::vector<RT::Spot>* v : lists) {
    for (int k = 0; k < 24; k++) {
      if (v->empty()) break;
      const RT::Spot& s = (*v)[(h + (uint32_t)k * 2654435761u) % v->size()];
      const int d = std::abs(s.x - px) + std::abs(s.y - py);
      if (d < 2 || d > 8 || !spotOk(g, s) || spotTaken(g, s.x, s.y, a.id)) continue;
      if (!clearLine(g, a.p, tileCentre(s.x - g.world.ox, s.y - g.world.oy), a.radius)) continue;
      A.task = TK_NONE;
      headTo(g, a, A, s.x, s.y);
      return true;
    }
  }
  return false;
}

// the nearest open tile (its centre free for a body) other than the one they are stuck in, within 3 tiles
bool LifeOps::slipOut(Game& g, const Actor& a, Vec2& out) {
  const Map& m = g.world.over;
  const int tx = tileX(a.p), ty = tileY(a.p);
  for (int rr = 1; rr <= 3; rr++)
    for (int oy = -rr; oy <= rr; oy++)
      for (int ox = -rr; ox <= rr; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != rr) continue;
        const int x = tx + ox, y = ty + oy;
        if (!m.in(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0 || m.wall[(size_t)y * m.w + x]) continue;
        const Vec2 c = tileCentre(x, y);
        if (!g.bodyFree(c, a.radius + 1.0f, false)) continue;
        out = c;
        return true;
      }
  return false;
}

// a fresh street's walker part-way along: a walkable tile 3..9 tiles from the target in plain sight of it
// (from inside the town's own streets: someone bound for the fields is met coming out of town, never out of the wild)
bool LifeOps::midRoute(Game& g, const Actor& a, Vec2 tgt, uint32_t h, Vec2& out) {
  const Map& m = g.world.over;
  const IRect town = a.site >= 0 && a.site < (int)g.world.sites.size() ? g.world.sites[(size_t)a.site].r : IRect();
  const int tx = tileX(tgt), ty = tileY(tgt);
  for (int k = 0; k < 12; k++) {
    const uint32_t hk = mixu(h, (uint64_t)k + 0x3D1ull);
    const int dx = (int)(hk % 15u) - 7, dy = (int)((hk >> 5) % 11u) - 5;
    const int d = std::abs(dx) + std::abs(dy);
    if (d < 3 || d > 9) continue;
    const int x = tx + dx, y = ty + dy;
    if (!m.in(x, y) || !town.contains(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0 || m.wall[(size_t)y * m.w + x]) continue;
    const Vec2 c = tileCentre(x, y);
    if (!g.bodyFree(c, a.radius, false) || !clearLine(g, c, tgt, a.radius)) continue;
    out = c;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- the street life of one resident actor
bool LifeOps::streetFolk(Game& g, Actor& a, float dt) {
  RT& R = rt(g);
  const int si = a.site;
  life::Census* c = si >= 0 ? g.life.findMut(g.world.sites[(size_t)si].id) : nullptr;
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) { a.lifeBits |= LB_GONE; return true; }
  life::Resident& r = c->res[(size_t)a.resident];
  // (M7) something solid was put up where they stand (a notice board, a FOR SALE sign, a yard object): step off it
  if (a.posture == Posture::None && g.world.over.blocked(tileX(a.p), tileY(a.p))) { a.p = snapWalk(g, a.p); a.home = a.goal = a.p; }
  RT::ARt& A = R.rt(a.id);
  RT::SiteRt* S = siteRtById(g, c->site);
  if (!S || S->plans.size() != c->res.size()) return true;
  const life::Plan& p = S->plans[(size_t)a.resident];
  if (a.postureT >= 0) a.postureT += dt;
  if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
  // (P1) held by a street incident (the robbed, a cart's owner, a brawler): incidentTick drives them
  if (A.mood == MOOD_INCIDENT) return incidentHeld(g, a, A, dt);
  if (A.mood == MOOD_PARTED) return partedHeld(g, a, A, dt);
  // the plan changed: a new target (time-sliced: 40 thinkers per step)
  if (A.kind != T_AWAY && (A.kind == T_NONE || A.act != p.act || A.place != p.place || A.pbldg != p.bldg) && R.thinkBudget > 0) {
    R.thinkBudget--;
    if (A.mood == 2 && A.partner >= 0) { auto it = R.act.find(A.partner); if (it != R.act.end()) it->second.partner = -1; }
    if (A.mood) A.partner = -1;
    A.mood = 0;
    retarget(g, a, A, *c, r, p);
  }
  switch (A.kind) {
    case T_DOOR: {
      a.posture = Posture::None;
      int dx, dy;
      if (!doorStep(g, A.bldg, dx, dy)) { a.lifeBits |= LB_GONE; return true; }
      // far off screen: they are simply indoors already
      if (!onScreen(g, a.p, 6.0f) && len2(a.p - g.pl().p) > (30.0f * TILE) * (30.0f * TILE)) { a.lifeBits |= LB_GONE; return true; }
      A.gx = g.world.ox + dx; A.gy = g.world.oy + dy;
      // husband and wife going home together: the one walks beside the other (a heart now and then)
      if (r.spouse >= 0 && r.spouse < (int)c->res.size() && c->res[(size_t)r.spouse].actor >= 0 && c->res[(size_t)r.spouse].actor < a.id) {
        const int k = g.findActor(c->res[(size_t)r.spouse].actor);
        auto it = k > 0 ? R.act.find(g.actors[(size_t)k].id) : R.act.end();
        if (k > 0 && it != R.act.end() && it->second.kind == T_DOOR && it->second.bldg == A.bldg &&
            len2(g.actors[(size_t)k].p - a.p) < (4.0f * TILE) * (4.0f * TILE) && len2(g.actors[(size_t)k].p - tileCentre(dx, dy)) > 12.0f * 12.0f) {
          const Actor& o = g.actors[(size_t)k];
          const Vec2 side = Vec2(-o.aim.y, o.aim.x) * 11.0f;
          const Vec2 to = o.p + side - o.aim * 3.0f;
          if (len2(to - a.p) > 4.0f) stepToward(g, a, to, a.speed * kWalk * 1.15f, dt);
          else { a.face = o.face; a.st = o.st; }
          if (a.bubbleT <= 0 && mixu(a.id, (uint64_t)(g.time / 8.0f)) % 5u == 0) bubble(a, Bubble::Heart, 1.2f);
          return true;
        }
      }
      if (walkOut(g, a, A, a.speed * kWalk, dt) || len2(a.p - tileCentre(dx, dy)) < 7.0f * 7.0f) {
        a.lifeBits |= LB_GONE;
        if (onScreen(g, a.p)) g.sfx((int)Sfx::Door, a.p, 1.1f, 0.25f);
        return true;
      }
      watch(g, a, A, dt);
      return true;
    }
    case T_AWAY: {   // an emigrant on the road out, a pack on the back
      a.posture = Posture::Carry;
      if (walkOut(g, a, A, a.speed * 0.55f, dt) || (!onScreen(g, a.p, 4.0f) && len2(a.p - g.pl().p) > (24.0f * TILE) * (24.0f * TILE))) a.lifeBits |= LB_GONE;
      watch(g, a, A, dt);
      return true;
    }
    case T_SPOT: break;
    default: a.st = AState::Idle; return true;
  }
  // (P1) wedged: the short step out (a walk, never a jump), then the next stop of the day
  if (A.slip) {
    const Vec2 d = A.slipTo - a.p;
    const float l = len(d);
    A.slipT += dt;
    if (l > 0.6f && A.slipT < 3.0f) {
      a.p += d * (std::min(l, a.speed * kWalk * 0.8f * dt) / l);
      a.face = faceOf(d); a.st = AState::Walk; a.posture = Posture::None;
      return true;
    }
    A.slip = false; A.path.clear(); A.fails = 0;
    A.leg++;
    if (!startLeg(g, a, A, *c, r, p, *S)) { A.arrived = true; settleOutdoor(g, a, A, *c, r); }
    return true;
  }
  // (P1) the street's rhythm: a new segment of the block, a new task (time-sliced with the plan changes)
  if (rhythmic(r, p, A) && A.act == p.act && A.place == p.place && A.retry != 255) {
    const int32_t seg = segOf(g, *c, r);
    if (A.seg != seg && R.thinkBudget > 0) {
      R.thinkBudget--;
      pickTask(g, a, A, *c, r, p, *S, seg);
    }
  }
  if (!A.arrived) {
    // (P1) everyone at their own pace (a little slower under a load)
    const float pace = ((p.act == life::Act::Play) ? 0.9f : kWalk) * A.pace * (A.task != TK_NONE && A.walkPost == Posture::Carry ? 0.93f : 1.0f);
    a.posture = p.act == life::Act::Play ? Posture::Play : (r.job == life::Job::Labourer && p.act == life::Act::Work ? Posture::Carry : Posture::None);
    if (A.task != TK_NONE) a.posture = A.walkPost;
    if (walkOut(g, a, A, a.speed * pace, dt)) {
      A.arrived = true;
      A.path.clear();
      A.watchT = 0;
      settleOutdoor(g, a, A, *c, r);
      if (A.task != TK_NONE) taskArrive(g, a, A, r);
    } else {
      watch(g, a, A, dt);
      if (A.fails >= 3) {   // no way there: another spot of the same kind (or stay put)
        A.fails = 0;
        if (A.task != TK_NONE) {   // (P1) a stop of the route out of reach: the next one
          A.arrived = true;
          A.dwell = 0;
          settleOutdoor(g, a, A, *c, r);
        } else if (rhythmic(r, p, A)) {   // (P1) a spot out of reach: another one (never the same unreachable spot again)
          // (twice in a row: one in plain sight, walked straight to without a search; none: they stay where they are)
          A.leg++;
          if (++A.retry >= 2 ? !plainSight(g, a, A, *S, r, p) : !startLeg(g, a, A, *c, r, p, *S)) {
            // nowhere to go from here: wedged on a corner or in a gap too tight for a body: a step out to the nearest
            // open tile (through the snag), then on with the day
            // (once a block; trapped for good, a yard fenced all round: they keep to their spot until the block ends)
            Vec2 to;
            if (A.retry >= 2 && !A.slipped && slipOut(g, a, to)) { A.slip = true; A.slipped = true; A.slipT = 0; A.slipTo = to; A.retry = 0; }
            else { A.retry = 255; A.task = TK_NONE; A.arrived = true; settleOutdoor(g, a, A, *c, r); }
          }
        } else {
          A.kind = T_NONE;
          A.arrived = true;
          settleOutdoor(g, a, A, *c, r);
        }
      }
    }
    if (!A.arrived) { greetings(g, a, *c, r); return true; }
  }
  // (P1) at a stop of the segment's task
  if (A.task != TK_NONE && A.mood == 0) {
    taskTick(g, a, A, *c, r, p, *S, dt);
    greetings(g, a, *c, r);
    return true;
  }
  // ---- at the spot: what they do there
  a.st = AState::Idle;
  A.t -= dt;
  if (A.mood == 1) {   // (stageMood) begging by the stalls
    a.posture = Posture::Beg;
    if (len2(g.pl().p - a.p) < (2.5f * TILE) * (2.5f * TILE) && R.barkT <= 0) {
      R.barkT = 9.0f;
      bubble(a, Bubble::Bread, 2.5f);
      say(g, a, "A CRUST, FRIEND? THE CHILDREN HAVEN'T EATEN.");
    }
    return true;
  }
  if (A.mood == 2) {   // (stageMood) a scuffle in the street
    if (A.partner < 0 || g.findActor(A.partner) <= 0) { A.mood = 0; A.partner = -1; }
    else { brawl(g, a, A, *c, dt); return true; }
  }
  using life::Act;
  switch (p.act) {
    case Act::Work:
      if (p.place == life::Place::Field && (r.job == life::Job::Farmer || r.job == life::Job::Herder) && A.t <= 0) {
        // along the furrow: a few tiles on to the next patch
        const std::vector<RT::Spot>& v = r.job == life::Job::Herder && !S->pasture.empty() ? S->pasture : S->farm;
        const RT::Spot* s = pick(g, v, mixu(c->seed ^ (uint64_t)r.idx, (uint64_t)(g.time * 0.1f)), a.id);
        if (s && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 10) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
        A.t = 14.0f + (float)(mixu(r.idx, (uint64_t)g.time) % 1600u) * 0.01f;
      } else if (r.job == life::Job::Labourer && A.t <= 0) {
        // (P1: a load carried a few lanes on, not across the whole city: a route that is walked, never a search that fails)
        const uint32_t lh = mixu(c->seed ^ (uint64_t)r.idx, (uint64_t)(g.time * 0.1f));
        const RT::Spot* s = pickNear(g, S->street, lh, g.world.ox + tileX(a.p), g.world.oy + tileY(a.p), 3, 14, a.id);
        if (!s) s = pick(g, S->street, lh, a.id);
        if (s) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
        A.t = 6.0f;
      }
      break;
    case Act::Play:
      // a child sometimes tags along after a parent passing by (the rest play tag on the green)
      if (r.parent >= 0 && r.parent < (int)c->res.size() && c->res[(size_t)r.parent].actor >= 0 && (mixu(c->seed ^ r.idx, (uint64_t)g.day) % 3u) == 0) {
        const int k = g.findActor(c->res[(size_t)r.parent].actor);
        if (k > 0 && g.actors[(size_t)k].st == AState::Walk && len2(g.actors[(size_t)k].p - a.p) < (6.0f * TILE) * (6.0f * TILE)) {
          const Actor& o = g.actors[(size_t)k];
          const Vec2 to = o.p - o.aim * 13.0f + Vec2(-o.aim.y, o.aim.x) * 6.0f;
          a.posture = Posture::None;
          if (len2(to - a.p) > 9.0f) stepToward(g, a, to, a.speed * 0.95f, dt);
          break;
        }
      }
      playTag(g, a, A, *S, p.place);
      break;
    case Act::LightLamps: lampRound(g, a, A, *S, dt); break;
    case Act::Eat:
      if (fieldLunch(g, r, p)) { a.posture = Posture::Eat; break; }
      chatOrStroll(g, a, A, *c, r, *S, dt);
      break;
    case Act::Socialise: case Act::Tavern: case Act::Wander: case Act::Shop: case Act::Home: case Act::Pray: case Act::Bathe:
      chatOrStroll(g, a, A, *c, r, *S, dt);
      break;
    case Act::Beg:
      if (len2(g.pl().p - a.p) < (2.5f * TILE) * (2.5f * TILE) && R.barkT <= 0) {
        R.barkT = 9.0f;
        bubble(a, Bubble::Coin, 2.5f);
        say(g, a, "SPARE A COIN, FRIEND?");
      }
      break;
    case Act::Perform:
      if (a.bubbleT <= 0 && mixu(a.id, (uint64_t)(g.time * 2)) % 7 == 0) bubble(a, Bubble::Note, 1.4f);
      break;
    case Act::Brawl: brawl(g, a, A, *c, dt); break;
    default: break;
  }
  if (p.act != Act::Play) greetings(g, a, *c, r);
  // (P1) the street's talk as the player passes close: a grown-up at leisure (alone or in a pair) lets a line slip now
  // and then (the site's gossip clock: one line in 24 s at most), sometimes naming someone's need (overheard: a lead)
  if (r.job != life::Job::Child && r.job != life::Job::Beggar && A.mood == 0 && S->gossipT <= g.time &&
      (A.partner >= 0 || p.act == Act::Socialise || p.act == Act::Wander || p.act == Act::Shop || p.act == Act::Tavern || p.act == Act::Eat) &&
      len2(g.pl().p - a.p) < (3.0f * TILE) * (3.0f * TILE)) {
    gossip(g, a, *c, *S);
    bubble(a, Bubble::Talk, 1.6f);
  }
  // a word of need as the player passes (15.12: "HAVEN'T EATEN SINCE YESTERDAY"), now and then
  if (R.barkT <= 0 && r.job != life::Job::Child && r.job != life::Job::Beggar && len2(g.pl().p - a.p) < (2.2f * TILE) * (2.2f * TILE)) {
    R.barkT = 12.0f;
    const std::string b = shortBark(*c, r);
    if (!b.empty()) {
      a.face = faceOf(g.pl().p - a.p);
      bubble(a, r.need[(int)life::Need::Hunger] < 25 ? Bubble::Bread : (r.flags & life::RF_GRIEVING) ? Bubble::Tear : Bubble::Talk, 2.0f);
      say(g, a, b, rgba(232, 222, 196));
    }
  }
  return true;
}

// the stuck watchdog: a walker that moved less than a tile in 30 s
void LifeOps::watch(Game& g, Actor& a, RT::ARt& A, float dt) {
  A.watchT += dt;
  if (A.watchT >= 30.0f) {
    A.stuck = len2(a.p - A.watchP) < (float)(TILE * TILE);
    if (A.stuck) {
      // re-route; failing that, off screen they are where they were going
      A.path.clear();
      A.fails++;
      if (!onScreen(g, a.p)) {
        if (A.kind == T_DOOR || A.kind == T_AWAY) a.lifeBits |= LB_GONE;
        else { a.p = snapWalk(g, tileCentre(A.gx - g.world.ox, A.gy - g.world.oy)); A.stuck = false; }
      } else {
        // in sight: off the corner it is caught on (the next route starts from a clean tile; the spot walkers
        // give up after three failed routes)
        unwedge(g, a);
      }
    }
    A.watchT = 0;
    A.watchP = a.p;
  }
}

void LifeOps::retarget(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p) {
  const int si = a.site;
  RT::SiteRt& S = siteRt(g, si);
  A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
  A.path.clear(); A.pi = 0; A.fails = 0; A.arrived = false; A.want = false;
  breakPair(g, A, a.id);
  A.task = TK_NONE; A.seg = INT32_MIN; A.leg = 0; A.retry = 0; A.slipped = false; A.slip = false;   // (P1: the new block's first segment picks its task at once)
  a.posture = Posture::None;
  int32_t gx, gy;
  int face;
  if (p.act == life::Act::Emigrate && !S.edge.empty()) {
    const RT::Spot* s = pick(g, S.edge, mixu(c.seed, (uint64_t)r.idx));
    if (s) { A.kind = T_AWAY; A.gx = s->x; A.gy = s->y; return; }
  }
  if (targetSpot(g, si, S, c, r, p, gx, gy, face, a.id)) {
    A.kind = T_SPOT; A.gx = gx; A.gy = gy;
    claimSpot(g, A, a.id, gx, gy);
    return;
  }
  releaseSpot(g, A, a.id);
  // indoors: walk to the door and go in
  const Site& st = g.world.sites[(size_t)si];
  int b = p.bldg >= 0 ? c.bldgHandle(st, p.bldg) : -1;
  if (b < 0 && r.home >= 0) b = c.bldgHandle(st, r.home);
  int dx, dy;
  if (b >= 0 && doorStep(g, b, dx, dy)) { A.kind = T_DOOR; A.bldg = b; A.gx = g.world.ox + dx; A.gy = g.world.oy + dy; return; }
  // no door to go to (a burned house): off along the street
  const RT::Spot* s = pick(g, S.edge.empty() ? S.street : S.edge, mixu(c.seed, (uint64_t)r.idx + 5u));
  if (s) { A.kind = T_AWAY; A.gx = s->x; A.gy = s->y; return; }
  A.kind = T_NONE;
  a.lifeBits |= LB_GONE;
}

// children at tag on the green: run from spot to spot, the one who is "it" after the nearest
// children at play: each child's game changes every half minute or so: tag (short dashes between nearby spots, the
// run frames while moving, a breath stood still between dashes), knucklebones sat on the ground, hopscotch (the
// hopping step on the spot). Never the whole lot frozen with their arms up.
void LifeOps::playTag(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, life::Place place) {
  const std::vector<RT::Spot>& v = place == life::Place::Plaza || S.street.empty() ? S.plaza : S.street;
  const uint32_t game = mixu((uint64_t)a.id * 7u + 3u, (uint64_t)(g.time / 28.0f)) % 5u;
  if (game == 3) { a.posture = Posture::SitFloor; return; }   // knucklebones / marbles in the dust
  if (game == 4) { a.posture = Posture::Dance; return; }      // hopscotch
  a.posture = Posture::None;                                  // tag: catching a breath between dashes
  if (A.t > 0) return;
  for (int k = 0; k < 4; k++) {
    // (P1: a dash to a spot near, not a hashed one anywhere in town that is never near enough to run to)
    const RT::Spot* s = pickNear(g, v, mixu((uint64_t)a.id * 131u + (uint64_t)k, (uint64_t)(g.time * 3.0f)), A.gx, A.gy, 2, 6, a.id);
    if (s && (s->x != A.gx || s->y != A.gy) && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 7) {
      A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y);
      break;
    }
  }
  A.t = 0.8f + (float)(mixu(a.id, (uint64_t)(g.time * 5.0f)) % 220u) * 0.01f;
  // a near miss: a shriek
  for (const Actor& o : g.actors)
    if (o.id != a.id && o.resident >= 0 && o.st == AState::Walk && o.posture == Posture::Play && len2(o.p - a.p) < 14.0f * 14.0f) { bubble(a, Bubble::Exclaim, 0.8f); break; }
}

// the lamplighter walks the round: each lamp is lit at its hour (life::lampLightHour), put out at dawn
void LifeOps::lampRound(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, float dt) {
  (void)dt;
  if (S.lamps.empty()) return;
  if (A.t > 0) { a.posture = Posture::Lamp; return; }
  a.posture = Posture::None;
  // the next lamp due (not lit yet by the clock, or the nearest when it is still too early)
  int best = -1;
  float bh = 1e9f;
  const bool dusk = g.hour >= 12.0f;
  for (size_t i = 0; i < S.lamps.size(); i++) {
    const RT::Spot& s = S.lamps[i];
    const float lh = dusk ? life::lampLightHour(s.x, s.y - 1) : life::lampOutHour(s.x, s.y - 1);
    if ((int)i == A.lampIdx || lh < g.hour - 0.25f) continue;
    if (lh < bh) { bh = lh; best = (int)i; }
  }
  if (best < 0) return;
  A.lampIdx = best;
  A.gx = S.lamps[(size_t)best].x; A.gy = S.lamps[(size_t)best].y;
  claimSpot(g, A, a.id, A.gx, A.gy);
  A.arrived = false; A.path.clear();
  A.t = 2.5f;   // the pole raised once there (settleOutdoor sets the posture)
}

// chatting in pairs (facing each other, bubbles), else a slow stroll round the spot
void LifeOps::chatOrStroll(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, RT::SiteRt& S, float dt) {
  RT& R = rt(g);
  (void)dt;
  if (A.partner >= 0) {
    const int k = g.findActor(A.partner);
    if (k <= 0 || len2(g.actors[(size_t)k].p - a.p) > (2.2f * TILE) * (2.2f * TILE)) A.partner = -1;
    else {
      Actor& o = g.actors[(size_t)k];
      if (A.pairStep) {   // (P1 fixer r2) a step or two into place beside them (never a hop)
        A.pairT += dt;
        if (len2(A.dest - a.p) > 1.0f && A.pairT < 2.5f) { stepToward(g, a, A.dest, a.speed * kWalk * 0.8f, dt); return; }
        A.pairStep = false;
      }
      a.face = faceOf(o.p - a.p);
      a.st = AState::Idle;
      if (A.t <= 0) {
        A.t = 2.4f + (float)(mixu(a.id, (uint64_t)(g.time * 4)) % 200u) * 0.01f;
        static const Bubble chat[] = {Bubble::Talk, Bubble::Talk, Bubble::Exclaim, Bubble::Question, Bubble::Talk, Bubble::Heart};
        Bubble b = chat[mixu(a.id ^ o.id, (uint64_t)(g.time * 2)) % 6u];
        if ((r.flags & life::RF_GRIEVING)) b = Bubble::Tear;
        if (b == Bubble::Heart && (r.spouse < 0 || o.resident != r.spouse)) b = Bubble::Talk;
        if (r.need[(int)life::Need::Hunger] < 25 && (A.t > 3.3f)) b = Bubble::Bread;
        bubble(a, b, 1.6f);
        gossip(g, a, c, S);
      }
      return;
    }
  }
  if (A.t > 0) return;
  A.t = 5.0f + (float)(mixu(a.id, (uint64_t)(g.time * 3)) % 600u) * 0.01f;
  // a partner: another resident actor at leisure nearby (a friend first)
  int best = -1;
  float bd = (4.0f * TILE) * (4.0f * TILE);
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& o = g.actors[k];
    if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.site != a.site || o.st == AState::Dead || o.st == AState::Walk) continue;
    auto it = R.act.find(o.id);
    if (it == R.act.end() || !it->second.arrived || it->second.partner >= 0) continue;
    const life::Act oa = it->second.act;
    if (!(oa == life::Act::Socialise || oa == life::Act::Tavern || oa == life::Act::Wander || oa == life::Act::Shop || oa == life::Act::Eat)) continue;
    float d = len2(o.p - a.p);
    for (int f : g.life.friendsOf(c, r.idx)) if (f == o.resident) d *= 0.5f;
    if (d < bd) { bd = d; best = (int)k; }
  }
  if (best > 0) {
    Actor& o = g.actors[(size_t)best];
    RT::ARt& B = R.rt(o.id);
    // (P1 fixer r2) side by side, facing: west and east of the middle, 9 px each way, walked to. (They were put there
    // in one jump, along the line between them: a pair met north-south stood in one column, a head painted over the
    // other's body, and with a wall at one side one of them stayed where they were, the two in one heap)
    Vec2 mid = (a.p + o.p) * 0.5f;
    const Vec2 dir(o.p.x >= a.p.x ? 1.0f : -1.0f, 0.0f);
    bool room = false;
    for (float sx : {0.0f, 6.0f, -6.0f, 12.0f, -12.0f}) {
      const Vec2 m2 = mid + Vec2(sx, 0);
      if (g.bodyFree(m2 - dir * 9.0f, a.radius, false) && g.bodyFree(m2 + dir * 9.0f, o.radius, false)) { mid = m2; room = true; break; }
    }
    if (!room) return;   // (no room for two here: no chat this time)
    A.partner = o.id; B.partner = a.id;
    A.dest = mid - dir * 9.0f; B.dest = mid + dir * 9.0f;
    A.pairStep = B.pairStep = true; A.pairT = B.pairT = 0;
    A.t = 0.2f; B.t = 1.4f;
    return;
  }
  // a stroll round the spot
  const std::vector<RT::Spot>& v = (A.place == life::Place::Market && !S.market.empty()) ? S.market : (S.plaza.empty() ? S.street : S.plaza);
  const RT::Spot* s = pick(g, v, mixu((uint64_t)a.id * 977u, (uint64_t)(g.time * 0.5f)), a.id);
  if (s && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 12) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
}

// two neighbours at it in the street (MF_BRAWLS): fists, an angry bubble, until the watch comes or they tire
void LifeOps::brawl(Game& g, Actor& a, RT::ARt& A, life::Census& c, float dt) {
  (void)c;
  if (A.partner < 0) return;
  const int k = g.findActor(A.partner);
  if (k <= 0) { A.partner = -1; return; }
  Actor& o = g.actors[(size_t)k];
  a.face = faceOf(o.p - a.p);
  A.t2 += dt;
  if (a.st == AState::Idle && fmodf(A.t2, 1.4f) < dt) { a.st = AState::Windup; a.stT = 0; }
  if (a.st == AState::Windup && a.stT > 0.3f) {
    a.st = AState::Strike; a.stT = 0; g.sfx((int)Sfx::Hit, a.p, 1.3f, 0.3f);
    // (P1 fixer) a scuffle reads at a glance: a kick of dust between them on every blow
    if (len2(o.p - a.p) < (1.6f * TILE) * (1.6f * TILE)) g.emit(Ev::Dust, (a.p + o.p) * 0.5f + Vec2(0, 1), 0, 0, "");
  }
  if (a.st == AState::Strike && a.stT > 0.25f) a.st = AState::Idle;
  if (a.bubbleT <= 0) bubble(a, Bubble::Anger, 1.2f);
}

// gossip near the player: a pair chatting within a few tiles lets a line slip (a real rumour now and then)
void LifeOps::gossip(Game& g, Actor& a, life::Census& c, RT::SiteRt& S) {
  if (len2(g.pl().p - a.p) > (4.0f * TILE) * (4.0f * TILE) || S.gossipT > g.time) return;
  S.gossipT = g.time + 24.0f;
  const uint32_t h = mixu(c.seed ^ (uint64_t)a.id, (uint64_t)g.day * 24u + (uint64_t)g.hour);
  rt(g).street.gossipLines++;
  // (P1) overheard: now and then the talk is of someone in town who needs help (a journal lead); close by only
  if (h % 3 == 1 && len2(g.pl().p - a.p) < (3.5f * TILE) * (3.5f * TILE) && overheard(g, a, rt(g).rt(a.id).partner, false)) return;
  std::string line;
  if (h % 3 == 0) {
    // (the places the session already holds near the window: nothing is loaded for a line of gossip)
    const int tx = tileX(a.p), ty = tileY(a.p);
    int rs = -1;
    float bd = 160.0f * 160.0f;
    for (int si : g.world.nearSites) {
      if (si < 0 || si >= (int)g.world.sites.size()) continue;
      const Site& s = g.world.sites[(size_t)si];
      if (s.discovered || s.rumoured || !(s.type == SiteType::Cave || s.type == SiteType::Ruin || s.type == SiteType::BanditCamp)) continue;
      const float d = (float)((s.ex - tx) * (s.ex - tx) + (s.ey - ty) * (s.ey - ty));
      if (d > 14.0f * 14.0f && d < bd) { bd = d; rs = si; }
    }
    if (rs >= 0) {
      const Site& s = g.world.sites[(size_t)rs];
      const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
      line = "...THEY SAY THERE'S " + std::string(s.type == SiteType::Cave ? "A CAVE" : s.type == SiteType::Ruin ? "AN OLD RUIN" : s.type == SiteType::BanditCamp ? "A BANDIT CAMP" : "SOMETHING ODD") +
             " OFF TO THE " + dirWord(g.world.ox + s.ex - gx, g.world.oy + s.ey - gy, false) + "...";
    }
  }
  if (line.empty()) {
    const uint16_t mf = c.moodFlags;
    static const char* content[] = {"...BEST HARVEST IN YEARS...", "...AND THEN HE FELL IN THE TROUGH...", "...THE BARD'S NEW SONG...",
                                    "...MARRIED BY SPRING, YOU'LL SEE...", "...WOLVES OUT BY THE OLD MILL..."};
    static const char* hungry[] = {"...BREAD'S DEAR AS SILVER NOW...", "...THE STORES ARE NEARLY EMPTY...", "...MY BOYS GO TO BED HUNGRY..."};
    static const char* war[] = {"...THE SOLDIERS TOOK OUR CART...", "...WILL THEY COME BACK, YOU THINK?...", "...MY BROTHER WENT TO THE WAR..."};
    if (mf & (life::MF_FAMINE | life::MF_HUNGRY)) line = hungry[h % 3];
    else if (mf & life::MF_WARTORN) line = war[h % 3];
    else line = content[h % 5];
  }
  say(g, a, line, rgba(210, 214, 230));
}

// friends greet each other by name when they pass ("EVENING, HALLA")
void LifeOps::greetings(Game& g, Actor& a, life::Census& c, const life::Resident& r) {
  RT& R = rt(g);
  RT::ARt& A = R.rt(a.id);
  if (A.greetT > g.time || r.job == life::Job::Child) return;
  A.greetT = g.time + 1.5f;
  for (int f : g.life.friendsOf(c, r.idx)) {
    if (f < 0 || f >= (int)c.res.size()) continue;
    const int oid = c.res[(size_t)f].actor;
    if (oid < 0) continue;
    const int k = g.findActor(oid);
    if (k <= 0) continue;
    Actor& o = g.actors[(size_t)k];
    if (len2(o.p - a.p) > (2.5f * TILE) * (2.5f * TILE)) continue;
    // once a day per pair (marks: no; the runtime keeps it)
    const uint64_t key = ((uint64_t)(uint32_t)std::min(r.idx, (uint16_t)f) << 32 | (uint32_t)std::max((int)r.idx, f)) ^ ((uint64_t)c.site * 31u) ^ ((uint64_t)g.day << 48);
    if (R.greeted.count(key)) continue;
    RT::ARt& B = R.rt(o.id);
    if (B.greetT > g.time + 6.0f) continue;   // just greeted by someone else: one greeting at a time
    if (R.greeted.size() > 4096) R.greeted.clear();
    R.greeted.insert(key);
    B.greetT = g.time + 8.0f;
    a.face = faceOf(o.p - a.p);
    bubble(a, Bubble::Exclaim, 1.2f);
    a.posture = a.posture == Posture::None ? Posture::Wave : a.posture;
    a.postureT = 0;
    if (onScreen(g, a.p)) say(g, a, std::string(timeGreeting(g.hour)) + ", " + firstName(c.res[(size_t)f].name));
    return;
  }
}
