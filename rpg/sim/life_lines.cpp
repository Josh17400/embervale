// LifeOps (life_ops.h, TOWNSFOLK lane): hook internals and dialogue lines.
// What the public hooks of life.h read (scripts, tests) and the 15.12 dialogue hooks' lines (meals, barks, moods).
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

// ---- the public hooks' internals (life.h: scripts and tests)
int LifeOps::taskOfActor(const Game& g, int id) {
  if (!g.lifeRt_) return -1;
  auto it = g.lifeRt_->act.find(id);
  const int k = g.findActor(id);
  if (it == g.lifeRt_->act.end() || k <= 0 || g.actors[(size_t)k].resident < 0) return -1;
  return it->second.task;
}

const life::StreetStats& LifeOps::streetOf(const Game& g) {
  static const life::StreetStats none;
  return g.lifeRt_ ? g.lifeRt_->street : none;
}

life::IncidentView LifeOps::incidentView(const Game& g) {
  life::IncidentView v;
  if (!g.lifeRt_) return v;
  const RT::Incident& I = g.lifeRt_->inc;
  v.kind = I.kind; v.victim = I.victim; v.other = I.other; v.quest = I.quest; v.engaged = I.engaged; v.gx = I.gx; v.gy = I.gy; v.ex = I.ex; v.ey = I.ey; v.holdT = I.holdT;
  return v;
}

uint32_t LifeOps::forceGossip(Game& g) {
  if (g.inside) return 0;
  // the nearest resident in the street near the player is the one who lets it slip
  const Actor* best = nullptr;
  float bd = (10.0f * TILE) * (10.0f * TILE);
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!(a.lifeBits & LB_LIFE) || a.resident < 0 || a.st == AState::Dead) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = &a; }
  }
  if (!best) return 0;
  const Actor speaker = *best;
  return overheard(g, speaker, rt(g).rt(speaker.id).partner, true);
}

std::string LifeOps::debugFolk(const Game& g, int id) {
  if (!g.lifeRt_) return "";
  auto it = g.lifeRt_->act.find(id);
  if (it == g.lifeRt_->act.end()) return "";
  const RT::ARt& A = it->second;
  char b[200];
  snprintf(b, sizeof b, "kind %d task %d leg %d arrived %d fails %d retry %d target %d,%d (local %d,%d) dwell %.1f mood %d partner %d path %d",
           (int)A.kind, (int)A.task, (int)A.leg, (int)A.arrived, A.fails, (int)A.retry, A.gx, A.gy, A.gx - g.world.ox, A.gy - g.world.oy, A.dwell, (int)A.mood,
           A.partner, (int)(A.path.size() / 2)); if (A.slip) snprintf(b + strlen(b), sizeof b - strlen(b), " SLIP to %.0f,%.0f", A.slipTo.x / TILE, A.slipTo.y / TILE);
  return b;
}

bool LifeOps::debugTalk(Game& g, int id) {
  const int k = g.findActor(id);
  if (k <= 0) return false;
  g.talkTo(g.actors[(size_t)k]);
  return g.mode == Mode::Dialogue;
}

uint64_t LifeOps::markOf(const Game& g, int si, int idx, int tag) {
  return markKey(life::npcId(g.world.sites[(size_t)si].id, idx), (Mk)(80 + tag));
}

int LifeOps::foodIndex(const Game& g) {
  for (int i = 0; i < (int)g.inv.size(); i++) if (g.inv[(size_t)i].kind == ItemKind::Food) return i;
  return -1;
}

// (fixer M5 r3) the town's food stock (what the inn's pot is filled from) and one serving taken from it
int LifeOps::townFood(const life::Census& c) {
  static const ew::Good foods[5] = {ew::Good::Bread, ew::Good::Meat, ew::Good::Fish, ew::Good::Produce, ew::Good::Grain};
  int n = 0;
  for (ew::Good f : foods) n += c.stock[(size_t)f];
  return n;
}

void LifeOps::takeFood(life::Census& c) {
  static const ew::Good foods[5] = {ew::Good::Bread, ew::Good::Meat, ew::Good::Fish, ew::Good::Produce, ew::Good::Grain};
  size_t best = (size_t)foods[0];
  for (ew::Good f : foods) if (c.stock[(size_t)f] > c.stock[best]) best = (size_t)f;
  if (c.stock[best] > 0) c.stock[best]--;
}

// the inn's dish in the people's own kitchen (cult::Archetype order), thinner in a hungry town
std::string LifeOps::mealLine(int arch, uint16_t mf) {
  static const char* dish[] = {
      "FISH STEW, RYE BREAD AND A HORN OF MEAD.",                  // fjordfolk
      "MUTTON BROTH, OATCAKES AND A DRAM AGAINST THE COLD.",        // highland
      "STEW, BLACK BREAD AND A MUG OF THE GOOD STUFF.",             // heartland
      "LENTILS, A CUT OF ROAST AND A CUP OF WATERED WINE.",         // imperial
      "SPICED LAMB, FLATBREAD, DATES AND MINT TEA.",                // dune
      "BOILED MUTTON, NOODLES IN BROTH AND A BOWL OF KUMIS.",       // steppe
      "EEL STEW, A RICE CAKE AND A CUP OF REED BEER.",              // marsh
      "RICE, PICKLED GREENS, A LITTLE PORK AND A POT OF TEA.",      // jade
      "RIVER FISH, BARLEY BREAD AND A JUG OF BEER.",                // river
      "MAIZE CAKES, BEANS, PEPPERS AND A CUP OF CHOCOLATE.",        // sun temple
      "NUT BREAD, MUSHROOMS, BERRIES AND A CUP OF FLOWER WINE.",    // sylvan
      "WHITE BREAD, HONEYED FRUIT AND A GLASS OF STARWINE."};       // starspire
  const int n = (int)(sizeof(dish) / sizeof(dish[0]));
  std::string t = arch >= 0 && arch < n ? dish[arch] : dish[2];
  if (mf & (life::MF_FAMINE | life::MF_HUNGRY)) return "A SMALL BOWL, I'M AFRAID. " + t + " WHAT THERE IS OF IT. FOOD'S SHORT IN TOWN.";
  return t + " THAT'LL KEEP YOU GOING TILL TONIGHT.";
}

std::string LifeOps::needBark(const life::Census& c, const life::Resident& r) {
  const uint32_t h = mixu(c.seed ^ r.idx, 0xBA4Cull);
  if (r.flags & life::RF_GRIEVING) return "WE BURIED ONE OF OUR OWN THIS WEEK. FORGIVE ME IF I'M POOR COMPANY.";
  if (r.need[(int)life::Need::Hunger] < 25) return h % 2 ? "I HAVEN'T EATEN SINCE YESTERDAY." : "MY BELLY THINKS MY THROAT'S BEEN CUT.";
  if (r.need[(int)life::Need::Money] < 20) return "NOT TWO COINS TO RUB TOGETHER, THESE DAYS.";
  if (r.need[(int)life::Need::Rest] < 20) return "I COULD SLEEP FOR A WEEK.";
  if (r.need[(int)life::Need::Social] < 20) return "NOBODY'S STOPPED TO TALK WITH ME ALL DAY. THANK YOU.";
  if (r.need[(int)life::Need::Faith] < 20) return "I OUGHT TO GET MYSELF TO THE TEMPLE.";
  return "";
}

// the short street bark of a pressing need ("" when content)
std::string LifeOps::shortBark(const life::Census& c, const life::Resident& r) {
  if (r.flags & life::RF_GRIEVING) return "...";
  if (r.need[(int)life::Need::Hunger] < 25) return "HAVEN'T EATEN SINCE YESTERDAY...";
  if (r.need[(int)life::Need::Money] < 15) return "NOT A COIN TO MY NAME...";
  if (r.need[(int)life::Need::Rest] < 15) return "SO TIRED...";
  if (c.moodFlags & life::MF_FAMINE) return "NO BREAD AGAIN TODAY...";
  if (c.moodFlags & life::MF_FESTIVAL) return "FEAST TONIGHT!";
  return "";
}

std::string LifeOps::moodLine(const life::Census& c, const std::string& town) {
  const uint16_t f = c.moodFlags;
  if (f & life::MF_FAMINE) return "THERE'S NO BREAD IN " + town + ". PEOPLE ARE LEAVING.";
  if (f & life::MF_HUNGRY) return "THE STORES ARE NEARLY EMPTY AND BREAD COSTS DOUBLE.";
  if (f & life::MF_WARTORN) return "THE WAR HAS TAKEN SO MUCH FROM US.";
  if (f & life::MF_FESTIVAL) return "IT'S FEAST DAY! COME TO THE HALL TONIGHT.";
  if (f & life::MF_GRIEF) return "THE WHOLE TOWN IS IN MOURNING.";
  if (c.mood >= 70) return "GOOD TIMES IN " + town + ". FULL BELLIES, FULL HALL.";
  return "";
}
