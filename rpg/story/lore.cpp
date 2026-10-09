// M4 "Banners": investigable ruins (owner 15.3: "every ruin has a true record behind it (who built it, when, how it
// fell)"; journals, inscriptions, murals, graves with names and dates, toppled statues of named rulers; a "Lost
// History" journal; clues lead onward to map fragments and living descendants). STORY lane.
//
// The record comes from the realm (Realm::ruin, the REALM lane's history pre-roll). Until it has one for a site, a
// stand-in is derived here from the site's own culture and id: a pure function, so the props and their words are the
// same on every visit and after every load.
// (M6b CAMPAIGNS lane) Some ruins hold the tomb of a king buried with a thing that could not be unmade: their third
// clue is the steward's letter about it, and the campaign `burden` begins there (burdenRuin).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {

const char* causeWords(realm::FallCause c) {
  switch (c) {
    case realm::FallCause::War: return "WAR";
    case realm::FallCause::Plague: return "PLAGUE";
    case realm::FallCause::Flood: return "FLOOD";
    case realm::FallCause::Famine: return "FAMINE";
    case realm::FallCause::Collapse: return "COLLAPSE";
    case realm::FallCause::Dragon: return "DRAGONFIRE";
    case realm::FallCause::Curse: return "A CURSE";
    default: return "RUIN";
  }
}

namespace {

const char* const kLordEpithet[] = {"THE PALE", "THE YOUNGER", "OF THE TWO SWORDS", "THE UNBURIED", "THE GENEROUS", "THE LAST",
                                    "IRONBROW", "THE FAITHFUL", "WHO WEPT", "THE BUILDER", "THE GREY", "SILVERHAND"};

// the stand-in record: a pure function of the world seed, the site and its culture
realm::RuinRecord makeRecord(Game& g, ew::Gid site) {
  realm::RuinRecord R;
  R.site = site;
  const int h = siteByIdLoad(g, site);
  if (h < 0 || !g.world.src) return R;
  const Site S = g.world.sites[(size_t)h];
  uint64_t cid = S.culture;
  if (!cid) cid = g.world.src->cultureAt(g.world.ox + S.ex, g.world.oy + S.ey);
  const cult::Culture& C = g.world.src->culture(cid);
  const cult::Society soc = cult::societyOf(C);
  uint64_t k = ew::mix64(site ^ g.seed ^ 0x4C4F5245ull);
  auto nx = [&]() { k = ew::mix64(k + 0x9E3779B97F4A7C15ull); return k; };
  R.valid = true;
  R.culture = cid;
  R.oldName = cult::placeName(C, (uint32_t)nx());
  R.extinct = (nx() % 10) < 7;
  if (R.extinct) R.builtBy = "THE KINGDOM OF " + cult::kingdomName(C, (uint32_t)nx());
  else {
    const ew::Gid land = landAt(g, g.world.ox + S.ex, g.world.oy + S.ey);
    if (land) { R.builtByKingdom = land; R.builtBy = "THE FOREFATHERS OF " + kingdomName(g, land); }
    else { R.extinct = true; R.builtBy = "THE KINGDOM OF " + cult::kingdomName(C, (uint32_t)nx()); }
  }
  const bool fem = (nx() & 1) != 0;
  const std::string title = (nx() % 3 == 0) ? std::string(soc.rulerTitle) : std::string(soc.lordTitle);
  const std::string ttl = fem && title == "KING" ? "QUEEN" : fem && title == "LORD" ? "LADY" : fem && title == "JARL" ? "JARL" : title;
  // (draws sequenced: MSVC/GCC evaluate the right operand of the string + first, clang the left)
  const char* const epithet = kLordEpithet[nx() % (sizeof(kLordEpithet) / sizeof(kLordEpithet[0]))];
  const std::string lordName = cult::personName(C, (uint32_t)nx(), fem);
  R.lastLord = ttl + " " + lordName + " " + epithet;
  R.fellYearsAgo = 60 + (int)(nx() % 340);
  R.foundedYearsAgo = R.fellYearsAgo + 120 + (int)(nx() % 500);
  const int c = (int)(nx() % 100);
  R.cause = c < 34 ? realm::FallCause::War : c < 48 ? realm::FallCause::Plague : c < 58 ? realm::FallCause::Flood : c < 68 ? realm::FallCause::Famine
            : c < 78 ? realm::FallCause::Collapse : c < 89 ? realm::FallCause::Dragon : realm::FallCause::Curse;
  if (R.cause == realm::FallCause::War) R.destroyerName = "THE HOST OF " + cult::kingdomName(g.world.src->culture(cid), (uint32_t)nx());
  // the clues: an inscription, a grave, a journal, a mural, the last lord's statue, the last words
  const std::string lord = R.lastLord, old = R.oldName;
  // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them)
  const bool stewardFem = (nx() & 1) != 0;
  const std::string steward = cult::personName(C, (uint32_t)nx(), stewardFem);
  const bool childFem = (nx() & 1) != 0;
  const std::string child = cult::personName(C, (uint32_t)nx(), childFem);
  const int built = R.foundedYearsAgo, fell = R.fellYearsAgo;
  R.clues.push_back("HERE " + R.builtBy + " RAISED THE HALL OF " + old + ", " + std::to_string(built) +
                    " WINTERS BEFORE YOUR TIME. MAY ITS DOORS OUTLAST ITS ENEMIES.");
  R.clues.push_back(steward + ", STEWARD OF " + old + ". KEPT THE KEYS FOR FORTY WINTERS. DIED IN THE YEAR OF THE " +
                    std::string(causeWords(R.cause)) + ", " + std::to_string(fell) + " YEARS AGO.");
  switch (R.cause) {
    case realm::FallCause::War:
      R.clues.push_back("DAY THIRTY-ONE OF THE SIEGE. " + R.destroyerName + " HAS RAMS AT THE WEST GATE. " + lord + " WILL NOT YIELD, AND SO WE WILL NOT EAT.");
      break;
    case realm::FallCause::Plague:
      R.clues.push_back("THE SWEATING SICKNESS TOOK THE MILLER'S HOUSE TODAY. " + lord + " HAS BARRED THE GATES. NO ONE LEAVES. NO ONE.");
      break;
    case realm::FallCause::Flood:
      R.clues.push_back("THE RAIN HAS NOT STOPPED IN NINE DAYS. THE LOWER HALLS ARE WATER. " + lord + " SAYS THE DAM WILL HOLD. IT GROANS AT NIGHT.");
      break;
    case realm::FallCause::Famine:
      R.clues.push_back("THE THIRD HARVEST HAS FAILED. WE ATE THE HORSES IN THE WINTER AND THE DOGS IN THE SPRING. " + lord + " EATS LAST, THEY SAY.");
      break;
    case realm::FallCause::Collapse:
      R.clues.push_back("THE MASONS WARNED " + lord + " THAT THE DEEP HALLS WERE HOLLOW. THE CRACKS REACH THE THRONE ROOM NOW.");
      break;
    case realm::FallCause::Dragon:
      R.clues.push_back("IT CAME AT DUSK, RED AS A FORGE. THE ROOFS WENT FIRST. " + lord + " RODE OUT WITH TWELVE SPEARS. NONE CAME BACK.");
      break;
    default:
      R.clues.push_back(lord + " TOOK SOMETHING FROM THE BARROW BENEATH THE HALL. SINCE THEN THE CANDLES BURN BLUE AND THE WELLS TASTE OF IRON.");
      break;
  }
  R.clues.push_back("A FADED MURAL: " + lord + " CROWNED BENEATH THE BANNERS OF " + old + ", AND BEHIND, SMALL AND HALF-ERASED, A CHILD NAMED " + child + ".");
  R.clues.push_back(lord + ", LAST LORD OF " + old + ". THE NOSE IS GONE, THE CROWN CHISELLED AWAY. SOMEONE HATED THIS FACE.");
  R.clues.push_back("THE LAST PAGE: \"WE SEND " + child + " SOUTH WITH THE SEAL. IF ANYONE READS THIS, THE BLOOD OF " + old + " STILL LIVES.\"");
  return R;
}

std::map<uint64_t, realm::RuinRecord>& cache() {
  static std::map<uint64_t, realm::RuinRecord> c;
  return c;
}

// a realm clue may say what it is written on ("INSCRIPTION: ...", "GRAVE: ...", "STATUE: ...", "MURAL: ...", "JOURNAL: ...")
struct ClueTag { const char* tag; art::Prop prop; };
const ClueTag kTags[] = {{"INSCRIPTION: ", art::Prop::Inscription}, {"GRAVE: ", art::Prop::NamedGrave}, {"STATUE: ", art::Prop::ToppledStatue},
                         {"MURAL: ", art::Prop::Mural}, {"JOURNAL: ", art::Prop::LostJournal}, {"LETTER: ", art::Prop::LostJournal}};
int taggedProp(const std::string& clue) {
  for (const ClueTag& t : kTags) if (clue.rfind(t.tag, 0) == 0) return (int)t.prop;
  return -1;
}
std::string untagged(const std::string& clue) {
  for (const ClueTag& t : kTags) if (clue.rfind(t.tag, 0) == 0) return clue.substr(std::char_traits<char>::length(t.tag));
  return clue;
}
// the lore prop that tells clue i (when the clue does not say)
art::Prop propForClue(int i) {
  static const art::Prop k[] = {art::Prop::Inscription, art::Prop::NamedGrave, art::Prop::LostJournal, art::Prop::Mural,
                                art::Prop::ToppledStatue, art::Prop::LostJournal};
  return k[i % 6];
}
const char* propWords(art::Prop p) {
  switch (p) {
    case art::Prop::Inscription: return "A CARVED INSCRIPTION";
    case art::Prop::NamedGrave: return "A NAMED GRAVE";
    case art::Prop::LostJournal: return "A LOST JOURNAL";
    case art::Prop::Mural: return "A FADED MURAL";
    case art::Prop::ToppledStatue: return "A TOPPLED STATUE";
    default: return "OLD WORDS";
  }
}
const char* propLead(art::Prop p) {
  switch (p) {
    case art::Prop::Inscription: return "THE CARVING READS: ";
    case art::Prop::NamedGrave: return "THE STONE READS: ";
    case art::Prop::LostJournal: return "YOU TURN THE BRITTLE PAGES: ";
    default: return "";
  }
}

void showLore(Game& g, const std::string& speaker, const std::string& text) {
  g.dlg = Dialogue();
  g.dlg.actor = -1;
  g.dlg.speaker = speaker;
  g.dlg.text = text;
  g.dlg.opts.push_back({"FAREWELL.", 0, 0});
  g.mode = Mode::Dialogue;
}

void loreNotice(Game& g, const std::string& text) {
  Event e;
  e.type = Ev::Notice; e.p = g.pl().p; e.a = (int)rgba(220, 200, 150); e.f = 0; e.s = text;
  g.events.push_back(e);
}

}  // namespace

// (M6b CAMPAIGNS lane) a ruin whose record tells of a made thing its makers could not unmake: the hook of the campaign
// `burden` (rpg/story/campaigns/burden.cpp; chain.cpp pickCampaign asks this). One ruin in four, and every ruin that
// fell to a curse. A pure function of the world seed, the site and its record.
namespace {
bool burdenRecord(const Game& g, ew::Gid site, const realm::RuinRecord& R) {
  return R.valid && (R.cause == realm::FallCause::Curse || ew::mix64(site ^ g.seed ^ 0xB0D3A11Full) % 4 == 0);
}
// the third clue of such a ruin (always among the props a ruin lays out) is the steward's last letter about it
void burdenClue(const Game& g, ew::Gid site, realm::RuinRecord& R) {
  if (!burdenRecord(g, site, R) || R.clues.size() < 3) return;
  static const char* const kThing[3] = {"CROWN", "KEY", "AMULET"};
  const char* thing = kThing[ew::mix64(site ^ 0x7417Aull) % 3];
  R.clues[2] = std::string("JOURNAL: ") + R.lastLord + " WILL NOT LET GO OF THE " + thing +
               ", EVEN IN SLEEP. IT WAS MADE IN THE FORGE OF AN OLDER KING, AND ONLY THAT FIRE CAN UNMAKE IT. WE WILL BURY IT IN THE TOMB, IN THOSE HANDS, AND PRAY NO ONE EVER OPENS IT.";
}
}  // namespace

realm::RuinRecord& ruinCached(Game& g, ew::Gid site) {
  const uint64_t key = ew::mix64(site ^ g.seed);
  auto it = cache().find(key);
  if (it != cache().end()) return it->second;
  if (cache().size() > 512) cache().clear();
  realm::RuinRecord R;
  if (g.world.src) R = g.realm.ruin(*g.world.src, site);
  if (!R.valid || R.clues.size() < 3) {
    // the realm has no record (yet): the stand-in, with whatever the realm knows laid over it
    realm::RuinRecord S = makeRecord(g, site);
    if (R.valid) {
      if (!R.oldName.empty()) S.oldName = R.oldName;
      if (!R.lastLord.empty()) S.lastLord = R.lastLord;
      if (!R.builtBy.empty()) S.builtBy = R.builtBy;
      for (size_t i = 0; i < R.clues.size() && i < S.clues.size(); i++) S.clues[i] = R.clues[i];
    }
    R = S;
  }
  burdenClue(g, site, R);
  return cache()[key] = R;
}

bool burdenRuin(Game& g, ew::Gid site) { return burdenRecord(g, site, ruinCached(g, site)); }

realm::RuinRecord ruinRecordFor(Game& g, ew::Gid site) { return ruinCached(g, site); }

std::vector<Engine::PropRef> placeRuinLore(Game& g, int siteHandle) {
  std::vector<Engine::PropRef> out;
  if (!g.inside || siteHandle < 0 || siteHandle >= (int)g.world.sites.size()) return out;
  const Site& S = g.world.sites[(size_t)siteHandle];
  if (S.type != SiteType::Ruin) return out;
  const realm::RuinRecord& R = ruinCached(g, S.id);
  if (!R.valid || R.clues.empty()) return out;
  const int n = std::min((int)R.clues.size(), 3 + (int)(ew::mix64(S.id ^ 0x10EEull) % 4));
  std::vector<int> props;
  for (int i = 0; i < n; i++) { const int t = taggedProp(R.clues[(size_t)i]); props.push_back(t >= 0 ? t : (int)propForClue(i)); }
  std::vector<Engine::PropRef> placed;
  placeLoreProps(g.sub, S.id, props, placed);
  // monsters already standing where a solid prop went step aside (a body in a slab would be stuck)
  for (const Engine::PropRef& p : placed) {
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      const int ax = (int)std::floor(a.p.x / TILE), ay = (int)std::floor(a.p.y / TILE);
      if (std::abs(ax - p.tx) <= 1 && ay == p.ty) a.p.y += TILE;
    }
  }
  return placed;
}

bool readLore(Game& g, int tx, int ty) {
  if (!g.inside || g.subSite < 0 || g.subSite >= (int)g.world.sites.size()) return false;
  const Site& S = g.world.sites[(size_t)g.subSite];
  if (g.story.ruinPropsSite_ != S.id) return false;
  const Engine::PropRef* P = nullptr;
  for (const Engine::PropRef& p : g.story.ruinProps_) if (p.tx == tx && p.ty == ty) P = &p;
  if (!P) return false;
  const realm::RuinRecord& R = ruinCached(g, S.id);
  if (P->clue < 0 || P->clue >= (int)R.clues.size()) return false;
  const art::Prop prop = (art::Prop)P->prop;
  LoreEntry* E = g.story.loreFor(S.id);
  if (!E) {
    LoreEntry e;
    e.key = S.id;
    e.title = "THE FALL OF " + R.oldName;
    e.total = (int)g.story.ruinProps_.size();
    g.story.lore_.push_back(e);
    E = &g.story.lore_.back();
  }
  const std::string clue = untagged(R.clues[(size_t)P->clue]);
  const uint32_t bit = 1u << std::min(P->clue, 30);
  showLore(g, propWords(prop), std::string(propLead(prop)) + clue);
  if (E->foundMask & bit) return true;
  E->foundMask |= bit;
  E->found++;
  E->lines.push_back(clue);
  loreNotice(g, "LOST HISTORY: " + E->title + " (" + std::to_string(std::min(E->found, E->total)) + "/" + std::to_string(E->total) + ")");
  if (host().sound) host().sound(g, (int)Sfx::Discover, 1.15f, 0.7f);
  if (E->found >= E->total && !E->complete) {
    E->complete = true;
    const int lvl = std::max(1, S.level);
    if (host().gold) host().gold(g, 20 + lvl * 6);
    if (host().xp) host().xp(g, 40 + lvl * 8);
    // the lead (15.3): a living descendant in a present-day city, and a map fragment toward another ruin
    std::string lead;
    if (g.world.src) {
      const int32_t gx = g.world.ox + S.ex, gy = g.world.oy + S.ey;
      const ew::SettlementNode* best = nullptr;
      int64_t bd = INT64_MAX;   // (fixer M6b r2: integer squared distance, no libm)
      const std::vector<ew::SettlementNode> near = g.world.src->settlementsIn(gx - 1500, gy - 1500, gx + 1500, gy + 1500, true);
      for (const ew::SettlementNode& nd : near) {
        if (nd.type != SiteType::City) continue;
        const int64_t dx = (int64_t)nd.x - gx, dy = (int64_t)nd.y - gy, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = &nd; }
      }
      if (best) {
        const uint64_t cid = R.culture ? R.culture : g.world.src->cultureAt(gx, gy);
        const std::string heir = cult::personName(g.world.src->culture(cid), (uint32_t)ew::mix64(S.id ^ 0xD35Cull), (S.id & 1) != 0);
        lead = heir + ", OF THE BLOOD OF " + R.oldName + ", IS SAID TO LIVE IN " + best->name + " TO THIS DAY.";
        g.story.addFact(best->id, heir + " OF " + best->name + " DESCENDS FROM " + R.lastLord + " OF " + R.oldName + ".", g.day);
      }
    }
    const std::string frag = g.hearRumour();
    if (!frag.empty()) lead += (lead.empty() ? "" : " ") + std::string("A MAP FRAGMENT SHOWS ") + frag + ".";
    if (!lead.empty()) E->lines.push_back(lead);
    g.dlg.text += " ... " + std::string(lead.empty() ? "THE STORY OF " + R.oldName + " IS WHOLE AT LAST." : lead);
    loreNotice(g, "LOST HISTORY COMPLETE: " + E->title);
    if (host().sound) host().sound(g, (int)Sfx::QuestDone, 0.9f, 1.0f);
  }
  return true;
}

// ---------------------------------------------------------------- overworld ruins: the toppled statue of the last lord
namespace {
bool statueSpot(const Game& g, const Site& s, int& ox, int& oy) {
  const Map& m = g.world.over;
  // already standing? (a window move keeps it; a region streamed back in has it again only after this places it)
  for (int y = s.ey - 7; y <= s.ey + 7; y++)
    for (int x = s.ex - 8; x <= s.ex + 8; x++)
      if (m.propAt(x, y) == (int)art::Prop::ToppledStatue + 1) { ox = x; oy = y; return true; }
  int best = -1;
  for (int y = s.ey - 6; y <= s.ey + 6; y++)
    for (int x = s.ex - 7; x <= s.ex + 7; x++) {
      const int d = std::abs(x - s.ex) + std::abs(y - s.ey);
      if (d < 3) continue;
      bool ok = true;
      for (int dy = -1; dy <= 1 && ok; dy++)
        for (int dx = -2; dx <= 2 && ok; dx++) {
          const int qx = x + dx, qy = y + dy;
          if (!m.in(qx, qy) || m.blocked(qx, qy) || m.propAt(qx, qy) || m.bldgAt[(size_t)qy * m.w + qx] >= 0) ok = false;
          else {
            const Ground gr = m.at(qx, qy);
            if (groundSolid(gr) || gr == Ground::Water || gr == Ground::DeepWater || gr == Ground::Road || gr == Ground::Bridge) ok = false;
          }
        }
      if (!ok) continue;
      const int score = 1000 - d * 10 + (int)(hash2(x, y, (uint32_t)s.id) % 9);
      if (score > best) { best = score; ox = x; oy = y; }
    }
  return best >= 0;
}
}  // namespace

void placeRuinStatues(Game& g) {
  if (g.inside || !g.world.endless) return;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Site& s = g.world.sites[(size_t)si];
    if (s.type != SiteType::Ruin || (ew::mix64(s.id ^ 0x57A7ull) % 3) == 0) continue;   // two ruins in three keep one
    if (std::fabs(s.ex - px) > 40 || std::fabs(s.ey - py) > 30) continue;
    if (!g.world.over.in(s.ex - 9, s.ey - 8) || !g.world.over.in(s.ex + 9, s.ey + 8)) continue;
    int x = 0, y = 0;
    if (!statueSpot(g, s, x, y)) continue;
    if (g.world.over.propAt(x, y) == (int)art::Prop::ToppledStatue + 1) continue;
    g.world.over.setProp(x, y, art::Prop::ToppledStatue);
    g.world.over.setProp(x - 1, y, art::Prop::Filler);
    g.world.over.setProp(x + 1, y, art::Prop::Filler);
    for (int dx = -1; dx <= 1; dx++) g.world.over.solid[(size_t)y * g.world.over.w + x + dx] = 1;
  }
}

bool readStatue(Game& g, int tx, int ty) {
  if (g.inside) return false;
  const Map& m = g.world.over;
  if (m.propAt(tx, ty) != (int)art::Prop::ToppledStatue + 1) return false;
  int best = -1;
  float bd = 1e9f;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size() || g.world.sites[(size_t)si].type != SiteType::Ruin) continue;
    const float d = std::hypot((float)(g.world.sites[(size_t)si].ex - tx), (float)(g.world.sites[(size_t)si].ey - ty));
    if (d < bd) { bd = d; best = si; }
  }
  if (best < 0 || bd > 14) { showLore(g, "A TOPPLED STATUE", "A STONE LORD LIES ON HIS FACE IN THE GRASS. WHOEVER HE WAS, THE WIND HAS TAKEN HIS NAME."); return true; }
  const Site S = g.world.sites[(size_t)best];
  const realm::RuinRecord& R = ruinCached(g, S.id);
  const std::string line = R.lastLord + ", LAST LORD OF " + R.oldName + ", FELL WITH IT " + std::to_string(R.fellYearsAgo) + " YEARS AGO.";
  showLore(g, "A TOPPLED STATUE", "THE PLINTH IS CRACKED, THE HEAD ROLLED INTO THE BRACKEN. CUT DEEP IN THE BASE: " + line);
  LoreEntry* E = g.story.loreFor(S.id);
  if (!E) {
    LoreEntry e;
    e.key = S.id;
    e.title = "THE FALL OF " + R.oldName;
    e.total = std::min((int)R.clues.size(), 3 + (int)(ew::mix64(S.id ^ 0x10EEull) % 4));
    g.story.lore_.push_back(e);
    E = &g.story.lore_.back();
  }
  if (!(E->foundMask & (1u << 31))) {
    E->foundMask |= 1u << 31;
    E->lines.push_back(line);
    loreNotice(g, "LOST HISTORY: " + E->title);
  }
  return true;
}

}  // namespace story
