// M6b "Sagas": the composer (rpg/story/saga.h). Spec ids, the template expander, composing a tier-2 story from an
// archetype, its twists and the voice, the cache of composed scripts, and (COMPOSER lane) choosing stories from the
// world. Phase A (lead): ids, the expander, compose, the cache and the reports. COMPOSER lane: pick, the offers and the
// guard's notes (what was offered and told).
//
// Determinism: everything here is a pure function of the spec (integer maths only; every random draw is its own
// statement: never two draws in one expression or argument list).
#include "rpg/story/saga.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/common.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/life.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"

namespace story {
namespace saga {

namespace {

uint32_t fnv(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h;
}

bool idChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

std::string trimLeft(const std::string& s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
  return s.substr(i);
}

std::vector<std::string> splitLines(const std::string& s) {
  std::vector<std::string> v;
  size_t p = 0;
  while (p <= s.size()) {
    size_t e = s.find('\n', p);
    if (e == std::string::npos) e = s.size();
    std::string l = s.substr(p, e - p);
    if (!l.empty() && l.back() == '\r') l.pop_back();
    v.push_back(l);
    p = e + 1;
  }
  return v;
}

// the first `stage <name>` of a template (its entry), with %names expanded by `prefix`
std::string firstStage(const std::string& body, const std::string& prefix) {
  for (const std::string& l0 : splitLines(body)) {
    const std::string l = trimLeft(l0);
    if (l.compare(0, 6, "stage ") != 0) continue;
    std::string n = trimLeft(l.substr(6));
    size_t e = 0;
    while (e < n.size() && n[e] != ' ' && n[e] != '\t' && n[e] != '#') e++;
    n = n.substr(0, e);
    if (!n.empty() && n[0] == '%') n = prefix + n.substr(1);
    return n;
  }
  return std::string();
}

// does the template declare `role <name> <kind>` with kind one of the '/' separated words (or any kind for "*")?
bool declaresRole(const std::string& body, const std::string& name, const std::string& kinds) {
  for (const std::string& l0 : splitLines(body)) {
    const std::string l = trimLeft(l0);
    if (l.compare(0, 5, "role ") != 0) continue;
    std::string rest = trimLeft(l.substr(5));
    const size_t sp = rest.find(' ');
    if (sp == std::string::npos || rest.substr(0, sp) != name) continue;
    if (kinds == "*") return true;
    std::string kind = trimLeft(rest.substr(sp + 1));
    const size_t e = kind.find(' ');
    if (e != std::string::npos) kind = kind.substr(0, e);
    size_t p = 0;
    while (p <= kinds.size()) {
      size_t q = kinds.find('/', p);
      if (q == std::string::npos) q = kinds.size();
      if (kinds.substr(p, q - p) == kind) return true;
      p = q + 1;
    }
  }
  return false;
}

std::vector<std::string> words(const char* s) {
  std::vector<std::string> v;
  std::string cur;
  for (const char* p = s ? s : ""; ; p++) {
    if (*p == ' ' || *p == '\t' || *p == 0) { if (!cur.empty()) v.push_back(cur); cur.clear(); if (!*p) break; }
    else cur += *p;
  }
  return v;
}

uint64_t step(uint64_t& rng) {
  rng = ew::mix64(rng + 0x9E3779B97F4A7C15ull);
  return rng;
}

}  // namespace

// ---------------------------------------------------------------- names
const char* sourceName(Source s) {
  static const char* n[] = {"SCRIPTURE", "MYTH", "FOLK", "LEGEND", "LEWIS", "TOLKIEN", "MAAS", "GWYNNE", "WORLD"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Source::COUNT, "a name for every source");
  return (int)s >= 0 && s < Source::COUNT ? n[(int)s] : "?";
}
const char* motiveName(Motive m) {
  static const char* n[] = {"fear", "greed", "grief", "pride", "devotion", "love", "duty", "vengeance", "shame", "hope"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Motive::COUNT, "a name for every motive");
  return m < Motive::COUNT ? n[(int)m] : "duty";
}
int motiveWord(const std::string& w) {
  for (int i = 0; i < (int)Motive::COUNT; i++) if (w == motiveName((Motive)i)) return i;
  return -1;
}

const Archetype* archetype(const std::string& id) {
  for (const Archetype& a : archetypes()) if (id == a.id) return &a;
  return nullptr;
}
const Twist* twist(const std::string& id) {
  for (const Twist& t : twists()) if (id == t.id) return &t;
  return nullptr;
}
const CampaignPlan* campaignPlan(const std::string& id) {
  for (const CampaignPlan& c : campaignPlans()) if (id == c.id) return &c;
  return nullptr;
}

bool twistFits(const Archetype& a, const Twist& t, const std::vector<std::string>& chosen) {
  if (!(a.twists & t.family)) return false;
  for (const std::string& r : words(t.roles)) {
    const size_t c = r.find(':');
    if (c == std::string::npos || !declaresRole(a.body, r.substr(0, c), r.substr(c + 1))) return false;
  }
  const std::vector<std::string> ex = words(t.excludes);
  for (const std::string& o : chosen) {
    if (o.empty()) continue;
    if (o == t.id) return false;
    if (std::find(ex.begin(), ex.end(), o) != ex.end()) return false;
    if (const Twist* ot = twist(o)) {
      const std::vector<std::string> ex2 = words(ot->excludes);
      if (std::find(ex2.begin(), ex2.end(), std::string(t.id)) != ex2.end()) return false;
    }
  }
  return true;
}

uint32_t twistFamilyWord(const std::string& w) {
  static const char* n[] = {"deceit", "identity", "rival", "prophecy", "mercy", "price", "betrayal", "return", "wonder", "world"};
  for (int i = 0; i < 10; i++) if (w == n[i]) return 1u << i;
  return 0;
}

uint32_t slotFamilies(const std::string& body, int slot) {
  const std::string want = "t" + std::to_string(slot + 1);
  for (const std::string& l0 : splitLines(body)) {
    const std::string l = trimLeft(l0);
    if (l.compare(0, 5, "slot ") != 0) continue;
    const std::vector<std::string> w = words(l.c_str());
    if (w.size() < 4 || w[1] != want) continue;
    uint32_t m = 0;
    for (size_t i = 4; i < w.size(); i++) m |= twistFamilyWord(w[i]);
    return m;
  }
  return 0;
}

bool twistFitsSlot(const Archetype& a, const Twist& t, int slot, const std::vector<std::string>& chosen) {
  if (!twistFits(a, t, chosen)) return false;
  const uint32_t m = slotFamilies(a.body, slot);
  return !m || (m & t.family);
}

// ---------------------------------------------------------------- spec ids
std::string specId(const Spec& s) {
  char tail[64];
  std::snprintf(tail, sizeof tail, "~v%d~m%d~%08x", s.voice, (int)s.motive, (unsigned)s.seed);
  std::string id = "saga" + std::to_string(s.ver) + "~";
  if (s.campaign) {
    id += "@" + s.arch + "~";
    for (size_t i = 0; i < s.arcs.size(); i++) { if (i) id += "."; id += s.arcs[i]; }
    id += "~~";
  } else {
    id += s.arch + "~" + s.twist[0] + "~" + s.twist[1] + "~" + s.twist[2];
  }
  return id + tail;
}

bool parseSpecId(const std::string& id, Spec& out) {
  if (!isSagaId(id)) return false;
  std::vector<std::string> f;
  size_t p = 0;
  while (p <= id.size()) {
    size_t q = id.find('~', p);
    if (q == std::string::npos) q = id.size();
    f.push_back(id.substr(p, q - p));
    p = q + 1;
  }
  if (f.size() != 8) return false;
  Spec s;
  s.ver = std::atoi(f[0].c_str() + 4);
  if (f[1].empty()) return false;
  if (f[1][0] == '@') {
    s.campaign = true;
    s.arch = f[1].substr(1);
    size_t a = 0;
    while (!f[2].empty() && a <= f[2].size()) {
      size_t b = f[2].find('.', a);
      if (b == std::string::npos) b = f[2].size();
      s.arcs.push_back(f[2].substr(a, b - a));
      a = b + 1;
    }
  } else {
    s.arch = f[1];
    for (int k = 0; k < 3; k++) s.twist[k] = f[2 + (size_t)k];
  }
  if (f[5].size() < 2 || f[5][0] != 'v' || f[6].size() < 2 || f[6][0] != 'm') return false;
  s.voice = std::atoi(f[5].c_str() + 1);
  const int m = std::atoi(f[6].c_str() + 1);
  if (s.voice < 0 || s.voice > 7 || m < 0 || m >= (int)Motive::COUNT) return false;
  s.motive = (Motive)m;
  s.seed = (uint32_t)std::strtoul(f[7].c_str(), nullptr, 16);
  for (char c : s.arch) if (!idChar(c)) return false;
  out = s;
  return true;
}

// ---------------------------------------------------------------- the expander
std::map<std::string, std::string> slotsOf(const std::string& body) {
  std::map<std::string, std::string> m;
  for (const std::string& l0 : splitLines(body)) {
    const std::string l = trimLeft(l0);
    if (l.compare(0, 5, "slot ") != 0) continue;
    char a[64] = {}, b[128] = {};
    if (std::sscanf(l.c_str() + 5, "%63s -> %127s", a, b) == 2) m[a] = b;
  }
  return m;
}

std::string expand(const std::string& body, Expand& x) {
  std::string out;
  for (const std::string& l0 : splitLines(body)) {
    std::string line = l0;
    const std::string t = trimLeft(line);
    if (t.compare(0, 5, "slot ") == 0) continue;
    // ?flag / !flag line conditions
    if (!t.empty() && (t[0] == '?' || t[0] == '!') && t.size() > 1 && idChar(t[1])) {
      size_t e = 1;
      while (e < t.size() && idChar(t[e])) e++;
      const bool want = t[0] == '?';
      const bool has = x.flags.count(t.substr(1, e - 1)) > 0;
      if (want != has) continue;
      line = std::string(line.size() - t.size(), ' ') + trimLeft(t.substr(e));
    }
    std::string o;
    size_t i = 0;
    bool inQuote = false;
    while (i < line.size()) {
      const char c = line[i];
      if (c == '#' && !inQuote) { o += line.substr(i); break; }   // a comment (markup in it is left alone)
      if (c == '[' && i + 1 < line.size() && line[i + 1] == '[') {
        const size_t e = line.find("]]", i + 2);
        if (e == std::string::npos) { x.errors.push_back("an unclosed [[ in: " + line); o += line.substr(i); break; }
        std::string inner = line.substr(i + 2, e - i - 2);
        // [[=name]]: the alternative a [[name=...]] chose earlier (consistency across lines)
        if (!inner.empty() && inner[0] == '=') {
          auto it = x.picks.find(inner.substr(1));
          if (it == x.picks.end()) { x.errors.push_back("[[" + inner + "]] before its [[" + inner.substr(1) + "=...]]"); line = line.substr(0, i) + line.substr(e + 2); continue; }
          line = line.substr(0, i) + it->second + line.substr(e + 2);
          continue;
        }
        // (fixer M6b r1) [[~name|A|B|...]]: the alternative at the same position a [[name=...]] chose earlier (a line
        // that must match another one, like a pitch and its hint)
        if (!inner.empty() && inner[0] == '~') {
          const size_t bar = inner.find('|');
          const std::string nm = inner.substr(1, bar == std::string::npos ? std::string::npos : bar - 1);
          auto it = x.picks.find("#" + nm);
          std::vector<std::string> alts;
          if (bar != std::string::npos) {
            size_t p = bar + 1;
            while (p <= inner.size()) {
              size_t q = inner.find('|', p);
              if (q == std::string::npos) q = inner.size();
              alts.push_back(inner.substr(p, q - p));
              p = q + 1;
            }
          }
          if (it == x.picks.end() || alts.empty()) {
            x.errors.push_back("[[" + inner + "]] before its [[" + nm + "=...]] or without alternatives");
            line = line.substr(0, i) + line.substr(e + 2);
            continue;
          }
          const size_t ix = (size_t)std::strtoul(it->second.c_str(), nullptr, 10) % alts.size();
          line = line.substr(0, i) + alts[ix] + line.substr(e + 2);
          continue;
        }
        std::string pickName;
        {
          size_t k = 0;
          while (k < inner.size() && idChar(inner[k])) k++;
          if (k > 0 && k < inner.size() && inner[k] == '=') { pickName = inner.substr(0, k); inner = inner.substr(k + 1); }
        }
        std::vector<std::string> alts;
        size_t p = 0;
        while (p <= inner.size()) {
          size_t q = inner.find('|', p);
          if (q == std::string::npos) q = inner.size();
          alts.push_back(inner.substr(p, q - p));
          p = q + 1;
        }
        const uint64_t r = step(x.rng);
        const size_t pickAt = (size_t)(r % alts.size());
        const std::string pickd = alts[pickAt];
        if (!pickName.empty()) { x.picks[pickName] = pickd; x.picks["#" + pickName] = std::to_string(pickAt); }
        // the chosen alternative is scanned again (its voice slots and local names expand); [[ inside is an error
        if (pickd.find("[[") != std::string::npos) x.errors.push_back("nested [[ in: " + line);
        line = line.substr(0, i) + pickd + line.substr(e + 2);
        continue;
      }
      if (c == '<' && i + 1 < line.size() && line[i + 1] == '<') {
        const size_t e = line.find(">>", i + 2);
        if (e == std::string::npos) { x.errors.push_back("an unclosed << in: " + line); o += line.substr(i); break; }
        std::string key = line.substr(i + 2, e - i - 2);
        Motive m = x.motive;
        const size_t colon = key.find(':');
        if (colon != std::string::npos) {
          const int mw = motiveWord(key.substr(colon + 1));
          if (mw < 0) x.errors.push_back("unknown motive in <<" + key + ">>");
          else m = (Motive)mw;
          key = key.substr(0, colon);
        }
        VoiceCtx v;
        v.voice = x.voice;
        v.motive = m;
        v.seed = step(x.rng);
        const Phrase ph = phrase(key, v);
        if (ph.text.empty()) x.errors.push_back("unknown voice key <<" + key + ">>");
        else if (ph.family) x.families.push_back(ph.family);
        o += ph.text;
        i = e + 2;
        continue;
      }
      if ((c == '%' || c == '@') && i + 1 < line.size() && line[i + 1] >= 'a' && line[i + 1] <= 'z') {
        size_t e = i + 1;
        while (e < line.size() && idChar(line[e])) e++;
        const std::string name = line.substr(i + 1, e - i - 1);
        if (c == '%') {
          if (x.prefix.empty()) x.errors.push_back("a local name %" + name + " outside a twist or an arc");
          std::string local = x.prefix + name;
          // {%name} inside a line is a placeholder: placeholders are case-blind and the validator wants no lower case in
          // a text, so the local role's name is written upper case there (as the campaign chainer does)
          if (inQuote && !o.empty() && o.back() == '{')
            for (char& ch : local)
              if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
          o += local;
        } else {
          auto it = x.targets.find("@" + name);
          if (it == x.targets.end()) { x.errors.push_back("no target for @" + name); o += name; }
          else o += it->second;
        }
        i = e;
        continue;
      }
      if (c == '"') inQuote = !inQuote;
      o += c;
      i++;
    }
    out += o;
    out += '\n';
  }
  return out;
}

// ---------------------------------------------------------------- composing
namespace {

// parse, link and validate composed text into `s` (one script); problems into `errors`
bool parseComposed(const std::string& text, dsl::Script& s, std::vector<std::string>& errors) {
  std::vector<dsl::Script> v;
  dsl::parse(text.c_str(), "saga", v, errors);
  if (v.size() != 1) { errors.push_back("composed text holds " + std::to_string(v.size()) + " scripts"); return false; }
  s = std::move(v[0]);
  dsl::link(s);
  for (const std::string& p : dsl::validate(s)) errors.push_back(p);
  return errors.empty();
}

int readableWords(const dsl::Script& s) {
  int n = dsl::wordCount(s.pitch) + dsl::wordCount(s.hint);
  for (const dsl::StageDef& G : s.stages) {
    n += dsl::wordCount(G.say) + dsl::wordCount(G.journal);
    for (const dsl::OptDef& o : G.opts) n += dsl::wordCount(o.label);
  }
  return n;
}

}  // namespace

bool compose(const Spec& s, Composed& out) {
  out = Composed();
  if (s.campaign) return composeCampaign(s, out);
  if (s.ver != SAGA_GEN_VER) { out.errors.push_back("another composer version"); return false; }
  const Archetype* a = archetype(s.arch);
  if (!a) { out.errors.push_back("no archetype '" + s.arch + "'"); return false; }
  if (a->tier != 2) { out.errors.push_back("archetype '" + s.arch + "' is a campaign arc"); return false; }
  Expand x;
  x.rng = ew::mix64(((uint64_t)s.seed << 32) ^ fnv(s.arch) ^ ((uint64_t)s.voice << 8) ^ (uint64_t)s.motive);
  x.voice = s.voice;
  x.motive = s.motive;
  x.flags.insert(std::string("m_") + motiveName(s.motive));
  x.flags.insert("v" + std::to_string(s.voice));
  const std::map<std::string, std::string> slots = slotsOf(a->body);
  std::vector<std::string> chosen;
  for (int k = 0; k < 3; k++) {
    const std::string sn = "t" + std::to_string(k + 1);
    auto it = slots.find(sn);
    if (s.twist[k].empty()) {
      if (it != slots.end()) x.targets["@" + sn] = it->second;
      continue;
    }
    const Twist* t = twist(s.twist[k]);
    if (!t) { out.errors.push_back("no twist '" + s.twist[k] + "'"); continue; }
    if (it == slots.end()) { out.errors.push_back("archetype '" + s.arch + "' has no slot " + sn); continue; }
    if (!twistFitsSlot(*a, *t, k, chosen)) out.errors.push_back("twist '" + s.twist[k] + "' does not fit slot " + sn + " of '" + s.arch + "'");
    chosen.push_back(t->id);
    x.flags.insert(sn);
    x.flags.insert(std::string("tw_") + t->id);
    x.targets["@" + sn] = firstStage(t->body, sn + "_");
  }
  out.text = "script " + specId(s) + "\narchetype " + a->name + "\ntier 2\n";
  out.text += expand(a->body, x);
  for (int k = 0; k < 3; k++) {
    if (s.twist[k].empty()) continue;
    const Twist* t = twist(s.twist[k]);
    const std::string sn = "t" + std::to_string(k + 1);
    auto it = slots.find(sn);
    if (!t || it == slots.end()) continue;
    Expand y;
    y.rng = x.rng;
    y.voice = x.voice;
    y.motive = x.motive;
    y.flags = x.flags;
    y.picks = x.picks;
    y.prefix = sn + "_";
    y.targets["@out"] = it->second;
    out.text += "# twist " + sn + ": " + t->id + "\n" + expand(t->body, y);
    x.rng = y.rng;
    for (uint32_t f : y.families) x.families.push_back(f);
    for (const std::string& e : y.errors) x.errors.push_back("twist " + std::string(t->id) + ": " + e);
  }
  for (const std::string& e : x.errors) out.errors.push_back(e);
  out.families = x.families;
  dsl::Script sc;
  if (!parseComposed(out.text, sc, out.errors)) return false;
  out.words = readableWords(sc);
  return out.errors.empty();
}

// ---------------------------------------------------------------- the cache
namespace {
std::map<std::string, std::unique_ptr<dsl::Script>>& cache() {
  static std::map<std::string, std::unique_ptr<dsl::Script>> c;
  return c;
}
}  // namespace

const dsl::Script* script(const std::string& id) {
  if (!isSagaId(id)) return nullptr;
  auto& C = cache();
  auto it = C.find(id);
  if (it != C.end()) return it->second.get();
  Spec s;
  std::unique_ptr<dsl::Script> sc;
  if (parseSpecId(id, s) && s.ver == SAGA_GEN_VER) {
    Composed c;
    const bool ok = s.campaign ? composeCampaign(s, c) : compose(s, c);
    if (ok) {
      sc.reset(new dsl::Script());
      std::vector<std::string> errs;
      if (!parseComposed(c.text, *sc, errs) || sc->id != id) sc.reset();
    }
  }
  const dsl::Script* p = sc.get();
  C[id] = std::move(sc);   // (a failure is cached as nullptr too: it is not composed again every frame)
  return p;
}

void trimCache(const Engine& e) {
  auto& C = cache();
  for (auto it = C.begin(); it != C.end();) {
    bool used = false;
    for (const Instance& in : e.running()) if (in.script == it->first) used = true;
    if (used) ++it;
    else it = C.erase(it);
  }
}
size_t cacheSize() { return cache().size(); }

// ---------------------------------------------------------------- choosing from the world (COMPOSER lane)
//
// pick(): from the hook (who is speaking, where), the living world (what the hook's surroundings hold: caves, ruins,
// towns, a war, a famine, a grieving household, a named beast...), the giver's own life (their motive), the culture (the
// voice) and the repetition guard, choose an archetype, its twists, a voice, a motive and a seed; then compose it and
// try to cast it, so nothing is ever offered that cannot be told here. Deterministic in (world seed, the hook entity,
// the week (day / 7), the guard): integer maths only, every random draw its own statement.
namespace {

// the hook line of an archetype's template (kind, trade, event type), parsed once
struct ArchHook { int kind = -1; int trade = -1; int ev = -1; };
const ArchHook& archHookOf(const Archetype& a) {
  static std::map<std::string, ArchHook> memo;
  auto it = memo.find(a.id);
  if (it != memo.end()) return it->second;
  ArchHook h;
  for (const std::string& l0 : splitLines(a.body)) {
    const std::string l = trimLeft(l0);
    if (l.compare(0, 5, "hook ") != 0) continue;
    const std::vector<std::string> w = words(l.c_str());
    if (w.size() < 2) break;
    if (w[1] == "npc") { h.kind = (int)dsl::HookKind::Npc; h.trade = w.size() > 2 ? dsl::tradeWord(w[2]) : -2; if (h.trade == -1) h.kind = -1; }
    else if (w[1] == "board") h.kind = (int)dsl::HookKind::Board;
    else if (w[1] == "herald") h.kind = (int)dsl::HookKind::Herald;
    else if (w[1] == "ruin") h.kind = (int)dsl::HookKind::Ruin;
    else if (w[1] == "event") { h.kind = (int)dsl::HookKind::Event; h.ev = w.size() > 2 ? dsl::evWord(w[2]) : -1; if (h.ev < 0) h.kind = -1; }
    break;
  }
  return memo.emplace(a.id, h).first->second;
}

// may a talking person of this Role tell a story of this hook trade? (any: every talking adult but the court, the watch
// on duty and children)
bool tradeFits(int trade, Role r) {
  if (r == Role::Child || r == Role::King || r == Role::Herald) return false;
  if (trade == -2) return r != Role::Guard && r != Role::Jarl;
  return trade == (int)r;
}

struct HookCtx {
  Hook h;
  int site = -1;                 // the hook place (handle)
  ew::Gid siteId = 0;
  int32_t gx = 0, gy = 0;        // its global tile
  uint64_t culture = 0;
  ew::Gid land = 0;
  int voice = 2;
  const Actor* actor = nullptr;
  uint64_t key = 0;              // the hook entity (npcKey or site id)
  uint32_t hookHash = 0;         // what the guard records
  int day = 0, bucket = 0;
};

bool hookCtx(Game& g, const Hook& h, HookCtx& c) {
  c.h = h;
  c.day = g.day;
  c.bucket = g.day / 7;
  if (h.actor >= 0 && host().findActor) {
    const int ai = host().findActor(g, h.actor);
    if (ai >= 0) c.actor = &g.actors[(size_t)ai];
  }
  c.site = h.site;
  if (c.site < 0 && c.actor) c.site = homeSiteOf(g, *c.actor);
  if (c.site < 0 || c.site >= (int)g.world.sites.size() || !g.world.src) return false;
  const Site& S = g.world.sites[(size_t)c.site];
  if (h.kind != (int)dsl::HookKind::Ruin && !S.settlement()) return false;
  c.siteId = S.id;
  c.gx = g.world.ox + S.ex;
  c.gy = g.world.oy + S.ey;
  c.culture = S.culture ? S.culture : g.world.src->cultureAt(c.gx, c.gy);
  c.land = landAt(g, c.gx, c.gy);
  c.voice = voiceOf(g, c.culture);
  c.key = c.actor ? g.npcKey(*c.actor) : ew::mix64(S.id ^ (0xB0A2Dull * (uint64_t)(h.kind + 1)));
  c.hookHash = (uint32_t)(c.key ^ (c.key >> 32)) | 1u;
  return true;
}

// ---- the world's facts at a hook (Need bits), each looked at only when an archetype asks, memoised per hook and day
struct NeedMemo { ew::Gid site = 0; int day = -1; uint64_t seed = 0; uint32_t serial = 0; uint32_t known = 0, have = 0; };

bool siteNear(Game& g, int32_t gx, int32_t gy, SiteType t, int maxD) {
  if (t == SiteType::Village || t == SiteType::Town || t == SiteType::City) {
    for (const ew::SettlementNode& n : g.world.src->settlementsIn(gx - maxD, gy - maxD, gx + maxD + 1, gy + maxD + 1, true)) {
      const bool kind = n.type == t || (t == SiteType::Town && n.type == SiteType::City);
      if (!kind) continue;
      const int64_t dx = n.x - gx, dy = n.y - gy, d2 = dx * dx + dy * dy;
      if (d2 >= 24 * 24 && d2 <= (int64_t)maxD * maxD) return true;
    }
    return false;
  }
  const int rings = (maxD + ew::REGION - 1) / ew::REGION;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != t) continue;
        const int64_t dx = p.ex - gx, dy = p.ey - gy, d2 = dx * dx + dy * dy;
        if (d2 >= 16 * 16 && d2 <= (int64_t)maxD * maxD) return true;
      }
    }
  return false;
}

bool needHolds(Game& g, const HookCtx& c, uint32_t bit) {
  const int32_t gx = c.gx, gy = c.gy;
  switch (bit) {
    case N_CAVE: return siteNear(g, gx, gy, SiteType::Cave, 560);
    case N_CAMP: return siteNear(g, gx, gy, SiteType::BanditCamp, 560);
    case N_RUIN: return siteNear(g, gx, gy, SiteType::Ruin, 640);
    case N_VILLAGE: return siteNear(g, gx, gy, SiteType::Village, 1300);
    case N_TOWN: return siteNear(g, gx, gy, SiteType::Town, 1300);
    case N_CITY: return siteNear(g, gx, gy, SiteType::City, 1300);
    case N_KINGDOM: return c.land && g.realm.kingdom(c.land) != nullptr;
    case N_RIVAL:
      for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != c.land && !K.fallen) return c.land != 0;
      return false;
    case N_WAR:
      for (const realm::War& w : g.realm.wars()) if (!w.endDay && c.land && (w.attacker == c.land || w.defender == c.land)) return true;
      return false;
    case N_FAMINE: {
      if (const realm::SettlementState* st = g.realm.settlement(c.siteId)) if (st->flags & realm::SS_FAMINE) return true;
      if (const life::Census* C = g.life.find(c.siteId)) return C->famineUntil >= g.day || (C->moodFlags & (life::MF_FAMINE | life::MF_HUNGRY)) != 0;
      return false;
    }
    case N_GRIEF: case N_FRIENDS: case N_NEEDY: case N_SMITH: {
      const life::Census* C = c.site >= 0 && g.world.sites[(size_t)c.site].settlement() ? g.life.census(g.world, c.site) : nullptr;
      if (!C) return false;
      if (bit == N_FRIENDS) { for (const life::Tie& t : C->ties) if (t.b != life::PLAYER_TIE) return true; return false; }
      if (bit == N_SMITH && C->smithy >= 0) return true;
      for (const life::Resident& r : C->res) {
        if (r.flags & (life::RF_DEAD | life::RF_AWAY)) continue;
        if (bit == N_GRIEF && (r.flags & life::RF_GRIEVING) && r.age >= 14) return true;
        if (bit == N_NEEDY && r.age >= 14 && (r.need[(size_t)life::Need::Hunger] < 35 || r.need[(size_t)life::Need::Money] < 30)) return true;
        if (bit == N_SMITH && r.job == life::Job::Smith) return true;
      }
      return false;
    }
    case N_UNIQUE: {
      const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
      for (int32_t ry = ry0 - 1; ry <= ry0 + 1; ry++)
        for (int32_t rx = rx0 - 1; rx <= rx0 + 1; rx++)
          for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, rx, ry))
            if (!g.marks.count(storyMarkKey(u.id, foes::MK_FOES_NAMED_SLAIN))) return true;
      return false;
    }
    case N_BOSS: {
      bool ok = false;
      const foes::WorldBoss wb = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(gx), ew::EndlessSource::kcellOf(gy), ok);
      return ok && !g.marks.count(storyMarkKey(wb.id, foes::MK_FOES_BOSS_SLAIN));
    }
    case N_CAPITAL: {
      const realm::KingdomState* K = c.land ? g.realm.kingdom(c.land) : nullptr;
      std::string rn, rt;
      return K && K->capital && g.story.rulerOf(g, c.land, rn, rt);
    }
    case N_FALLEN: {
      // a ruin near whose record names the realm or lord that fell there
      const int rings = 3;
      const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
      for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
        for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
          const ew::RegionPlan R = g.world.regionPlan(rx, ry);
          for (const ew::SitePlan& p : R.sites) {
            if (p.type != SiteType::Ruin) continue;
            const int64_t dx = p.ex - gx, dy = p.ey - gy;
            if (dx * dx + dy * dy > 640 * 640) continue;
            // (fixer M6b r2) every ruin's record names who built it and its last lord (lore.cpp makeRecord fills them
            // when the realm's history has none), so the record (a realm history look-up, up to ~20 ms the first time
            // in a kingdom cell) is not built here: the caster and the text read it when a story binds the ruin
            return true;
          }
        }
      return false;
    }
    case N_COAST: {
      // the sea within reach: a coarse look at the macro elevation around the hook
      for (int32_t dy = -600; dy <= 600; dy += 120)
        for (int32_t dx = -600; dx <= 600; dx += 120) {
          if (dx * dx + dy * dy > 600 * 600) continue;
          const ew::MacroSample m = g.world.src->macroFar(gx + dx, gy + dy);
          if (m.elev < ew::ELEV_SEA) return true;
        }
      return false;
    }
    case N_EVENT:
      for (const realm::WorldEvent& e : g.realm.events())
        if (g.day - (int)e.day <= 20 && rumourReaches(e, g.day, gx, gy)) return true;
      return false;
    default: return true;   // (an unknown bit: the caster decides)
  }
}

NeedMemo& needMemo() { static NeedMemo m; return m; }

void needMemoFor(Game& g, const HookCtx& c) {
  NeedMemo& M = needMemo();
  // (forgotten every day, and whenever the realm has news: a war, a famine)
  if (M.site != c.siteId || M.day != g.day || M.seed != g.seed || M.serial != g.realm.eventSerial()) {
    M = NeedMemo();
    M.site = c.siteId; M.day = g.day; M.seed = g.seed; M.serial = g.realm.eventSerial();
  }
}

bool needsHold(Game& g, const HookCtx& c, uint32_t needs) {
  NeedMemo& M = needMemo();
  needMemoFor(g, c);
  for (uint32_t bit = 1; bit && bit <= N_EVENT; bit <<= 1) {
    if (!(needs & bit)) continue;
    if (!(M.known & bit)) {
      M.known |= bit;
      if (needHolds(g, c, bit)) M.have |= bit;
    }
    if (!(M.have & bit)) return false;
  }
  return true;
}

// an event hook's event type has reached the hook lately
bool eventFresh(Game& g, const HookCtx& c, int ev) {
  for (const realm::WorldEvent& e : g.realm.events())
    if ((int)e.type == ev && g.day - (int)e.day <= 30 && rumourReaches(e, g.day, c.gx, c.gy)) return true;
  return false;
}

// ---- the giver's motives, most fitting first, from their life (M5 census) and trade
std::vector<Motive> motivesOf(Game& g, const HookCtx& c) {
  std::vector<Motive> v;
  auto add = [&](Motive m) { if (std::find(v.begin(), v.end(), m) == v.end()) v.push_back(m); };
  const life::Resident* R = nullptr;
  if (c.actor) {
    ew::Gid rs = 0;
    int ix = -1;
    if (residentOfActor(g, *c.actor, rs, ix)) if (const life::Census* C = g.life.find(rs)) R = &C->res[(size_t)ix];
  }
  if (R) {
    if (R->flags & life::RF_GRIEVING) { add(Motive::Grief); add(Motive::Vengeance); }
    const bool hungry = R->need[(size_t)life::Need::Hunger] < 35, poor = R->need[(size_t)life::Need::Money] < 30;
    if (hungry || poor) { if (R->traits & life::TR_THRIFTY) add(Motive::Greed); else add(Motive::Fear); }
    switch (R->job) {
      case life::Job::Guard: add(Motive::Duty); break;
      case life::Job::Priest: add(Motive::Devotion); break;
      case life::Job::Noble: add(Motive::Pride); break;
      case life::Job::Merchant: add(Motive::Greed); break;
      case life::Job::Smith: add(Motive::Pride); break;
      case life::Job::Innkeeper: case life::Job::Server: add(Motive::Hope); break;
      case life::Job::Elder: add(Motive::Shame); add(Motive::Love); break;
      case life::Job::Beggar: add(Motive::Shame); break;
      case life::Job::Scholar: add(Motive::Hope); break;
      case life::Job::Bard: add(Motive::Love); break;
      case life::Job::Hunter: add(Motive::Pride); break;
      case life::Job::Farmer: case life::Job::Herder: add(Motive::Fear); break;
      default: break;
    }
    if (R->traits & life::TR_DEVOUT) add(Motive::Devotion);
    if (R->traits & life::TR_GRUMPY) add(Motive::Vengeance);
    if (R->traits & life::TR_SOCIABLE) add(Motive::Love);
    if (R->traits & life::TR_BRAVE) add(Motive::Duty);
    if (R->spouse >= 0) add(Motive::Love);
  } else if (c.actor) {
    switch (c.actor->role) {
      case Role::Guard: add(Motive::Duty); break;
      case Role::Priest: add(Motive::Devotion); break;
      case Role::Innkeeper: add(Motive::Hope); break;
      case Role::Merchant: add(Motive::Greed); break;
      case Role::Jarl: add(Motive::Pride); break;
      default: break;
    }
  } else {
    // a place: the town's mood speaks (a hungry or grieving town, else its duty to itself)
    const uint16_t mf = g.life.moodFlags(c.siteId);
    if (mf & (life::MF_FAMINE | life::MF_HUNGRY)) add(Motive::Fear);
    if (mf & life::MF_GRIEF) add(Motive::Grief);
    if (mf & life::MF_WARTORN) add(Motive::Vengeance);
    add(Motive::Duty);
  }
  // the rest, in an order fixed by the hook
  uint64_t r = ew::mix64(c.key ^ 0x307117E5ull);
  std::vector<Motive> rest;
  for (int m = 0; m < (int)Motive::COUNT; m++) rest.push_back((Motive)m);
  for (size_t i = rest.size(); i > 1; i--) {
    r = ew::mix64(r + i);
    const size_t j = (size_t)(r % i);
    std::swap(rest[i - 1], rest[j]);
  }
  for (Motive m : rest) add(m);
  return v;
}

Motive motiveFor(const Archetype& a, const std::vector<Motive>& ranked) {
  for (Motive m : ranked) if (!a.motives || (a.motives & motiveBit(m))) return m;
  return ranked.empty() ? Motive::Duty : ranked[0];
}

uint32_t castShape(const std::vector<Binding>& cast) {
  uint32_t h = 2166136261u;
  for (const Binding& b : cast) { h ^= b.kind; h *= 16777619u; h ^= b.trade; h *= 16777619u; }
  return h;
}

// is this spec running already (the same story twice at once)?
bool runningSpec(const Game& g, const std::string& id) {
  for (const Instance& in : g.story.running()) if (in.script == id && !in.done) return true;
  return false;
}

// the phrase penalty of a composed spec (how much of its voice the player has heard lately)
int phraseCost(const Game& g, const Spec& s) {
  Composed c;
  if (!compose(s, c)) return 1 << 20;
  int sum = 0;
  for (uint32_t f : c.families) sum += g.story.guard_.phrasePenalty(f);
  return sum;
}

bool pickFrom(Game& g, const HookCtx& c, Spec& out) {
  const Hook& h = c.h;
  const std::vector<Motive> ranked = motivesOf(g, c);
  // the candidates and their weights
  struct Cand { const Archetype* a; int w; };
  std::vector<Cand> cands;
  for (const Archetype& a : archetypes()) {
    if (a.tier != 2) continue;
    const ArchHook& ah = archHookOf(a);
    if (ah.kind != h.kind) continue;
    if (h.kind == (int)dsl::HookKind::Npc && (!c.actor || !tradeFits(ah.trade, c.actor->role))) continue;
    if (h.kind == (int)dsl::HookKind::Event && (h.ev >= 0 ? ah.ev != h.ev : !eventFresh(g, c, ah.ev))) continue;
    if (!needsHold(g, c, a.needs)) continue;
    const int pen = g.story.guard_.archPenalty(a.id, false, c.day, c.gx, c.gy, c.hookHash);
    int w = 1000 - pen;
    if (w <= 0) continue;
    // a giver whose own life carries the archetype's motive tells it more readily
    if (!ranked.empty() && a.motives && (a.motives & motiveBit(ranked[0]))) w *= 3;
    else if (ranked.size() > 1 && a.motives && (a.motives & motiveBit(ranked[1]))) w *= 2;
    cands.push_back({&a, w});
  }
  if (cands.empty()) return false;
  uint64_t rng = ew::mix64(g.seed * 0x9E3779B97F4A7C15ull ^ c.key ^ ((uint64_t)(uint32_t)c.bucket << 20) ^ (uint64_t)(h.kind + 1));
  for (int attempt = 0; attempt < 6 && !cands.empty(); attempt++) {
    long total = 0;
    for (const Cand& k : cands) total += k.w;
    const uint64_t r0 = step(rng);
    long at = (long)(r0 % (uint64_t)total);
    size_t ci = 0;
    for (; ci + 1 < cands.size(); ci++) { if (at < cands[ci].w) break; at -= cands[ci].w; }
    const Archetype& a = *cands[ci].a;
    cands.erase(cands.begin() + (std::ptrdiff_t)ci);
    Spec s;
    s.arch = a.id;
    s.voice = c.voice;
    s.motive = motiveFor(a, ranked);
    const std::map<std::string, std::string> slots = slotsOf(a.body);
    bool found = false;
    for (int tries = 0; tries < 4 && !found; tries++) {
      std::vector<std::string> chosen;
      for (int k = 0; k < 3; k++) {
        s.twist[k].clear();
        if (!slots.count("t" + std::to_string(k + 1))) continue;
        const uint64_t rf = step(rng);
        if (rf % 4 == 0) continue;   // a quarter of the slots stay empty (a plain telling is a variety too)
        std::vector<const Twist*> fit;
        for (const Twist& t : twists())
          if (twistFitsSlot(a, t, k, chosen) && needsHold(g, c, t.needs)) fit.push_back(&t);
        if (fit.empty()) continue;
        const uint64_t rt = step(rng);
        const Twist* t = fit[(size_t)(rt % fit.size())];
        s.twist[k] = t->id;
        chosen.push_back(t->id);
      }
      if (g.story.guard_.penalty(s, c.day, c.gx, c.gy, c.hookHash) >= 1000) continue;
      found = true;
    }
    if (!found) continue;
    // two seeds of the same story: the one whose phrases the player has heard less
    const uint64_t rs1 = step(rng);
    const uint64_t rs2 = step(rng);
    s.seed = (uint32_t)rs1;
    Spec s2 = s;
    s2.seed = (uint32_t)rs2;
    if (!g.story.guard_.phrases.empty() && phraseCost(g, s2) < phraseCost(g, s)) s = s2;
    const std::string id = specId(s);
    if (runningSpec(g, id)) continue;
    const dsl::Script* sc = script(id);
    if (!sc) continue;
    // try-cast: nothing is offered that cannot be told here
    std::vector<Binding> cast;
    std::string why;
    const uint64_t key = ew::mix64(g.seed ^ strHash64s(id) ^ c.key);
    if (!castScript(g, *sc, c.actor ? c.actor->id : -1, c.site, key, cast, why)) continue;
    out = s;
    return true;
  }
  return false;
}

// offers already worked out (talking to the same innkeeper twice costs nothing); a stale entry is recomputed
struct OfferMemo { uint64_t seed = 0; int bucket = -1; size_t guardSeen = 0; size_t guardTold = 0; size_t running = 0; std::string id; };
std::map<uint64_t, OfferMemo>& offerMemo() { static std::map<uint64_t, OfferMemo> m; return m; }
size_t toldCount(const Guard& G) { size_t n = 0; for (const Seen& x : G.seen) if (!x.offered) n++; return n; }

std::string offerAt(Game& g, const Hook& h) {
  HookCtx c;
  if (!hookCtx(g, h, c)) return std::string();
  auto& M = offerMemo();
  if (M.size() > 512) M.clear();
  const uint64_t mk = ew::mix64(c.key ^ (uint64_t)(h.kind + 1) * 0x51ull ^ ((uint64_t)(uint32_t)(h.ev + 7) << 48));
  auto it = M.find(mk);
  const size_t told = toldCount(g.story.guard_);
  if (it != M.end() && it->second.seed == g.seed && it->second.bucket == c.bucket && it->second.guardTold == told &&
      it->second.running == g.story.running().size()) {
    // (offers made by other hooks since may forbid it now)
    Spec s;
    if (it->second.id.empty()) return it->second.id;
    if (parseSpecId(it->second.id, s) && g.story.guard_.penalty(s, c.day, c.gx, c.gy, c.hookHash) < 1000) {
      // (fixer M6b r1) the town may have changed since (the tale scan works the offers out ahead of the talk): a
      // resident the cast needs died, left or fell out with their friend. The cast is tried again (cheap) and a story
      // that can no longer be told is picked afresh
      const dsl::Script* sc = script(it->second.id);
      std::vector<Binding> cast;
      std::string why;
      if (sc && castScript(g, *sc, c.actor ? c.actor->id : -1, c.site, ew::mix64(g.seed ^ strHash64s(it->second.id) ^ c.key), cast, why))
        return it->second.id;
    }
  }
  Spec s;
  std::string id;
  if (h.kind != (int)dsl::HookKind::Npc && pickCampaign(g, h, s) && g.story.guard_.penalty(s, c.day, c.gx, c.gy, c.hookHash) < 1000 &&
      !runningSpec(g, specId(s)) && script(specId(s)))
    id = specId(s);
  if (id.empty() && pick(g, h, s)) id = specId(s);
  OfferMemo e;
  e.seed = g.seed; e.bucket = c.bucket; e.guardTold = told; e.running = g.story.running().size(); e.id = id;
  M[mk] = e;
  return id;
}

}  // namespace

bool pick(Game& g, const Hook& h, Spec& out) {
  HookCtx c;
  if (!hookCtx(g, h, c)) return false;
  return pickFrom(g, c, out);
}

std::string offerFor(Game& g, const Actor& a) {
  // who tells stories: as the M4 library's tellers, a little less often (the library's own tale comes first)
  if (!a.npc || !a.human || isStoryPerson(a) || a.role == Role::King || a.role == Role::Child || a.role == Role::Herald) return std::string();
  const int hs = homeSiteOf(g, a);
  if (hs < 0 || !g.world.sites[(size_t)hs].settlement()) return std::string();
  const uint64_t key = g.npcKey(a);
  if (g.marks.count(storyMarkKey(key, MK_STORY_GIVER))) return std::string();
  for (const Instance& in : g.story.running()) if (!in.cast.empty() && in.cast[0].id == key) return std::string();
  // not everyone has a tale this week: innkeepers and priests often, guards and the rest now and then
  const uint32_t h = hash32((uint32_t)key ^ (uint32_t)(key >> 32) ^ (uint32_t)(g.day / 7) * 0x9E37u ^ 0x5A6Au);
  const int pct = a.role == Role::Innkeeper ? 45 : a.role == Role::Priest ? 40 : a.role == Role::Guard ? 8 : 10;
  if ((int)(h % 100) >= pct) return std::string();
  Hook hk;
  hk.kind = (int)dsl::HookKind::Npc;
  hk.actor = a.id;
  hk.site = hs;
  return offerAt(g, hk);
}

std::string offerForPlace(Game& g, int hookKind, int site) {
  if (site < 0 || site >= (int)g.world.sites.size()) return std::string();
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  // one generated story per place a fortnight (accepted: the place's mark), and not every place has one
  const uint64_t mk = storyMarkKey(ew::mix64(sid ^ 0x5A6A9ull * (uint64_t)(hookKind + 1)), MK_STORY_MARK);
  auto it = g.marks.find(mk);
  if (it != g.marks.end() && g.day - it->second < 14) return std::string();
  const uint32_t h = hash32((uint32_t)sid ^ (uint32_t)(sid >> 32) ^ (uint32_t)(g.day / 7) * 0x9E37u ^ (uint32_t)hookKind * 0x1F3u);
  const int pct = hookKind == (int)dsl::HookKind::Board ? 40 : hookKind == (int)dsl::HookKind::Herald ? 35 : hookKind == (int)dsl::HookKind::Ruin ? 35 : 50;
  if ((int)(h % 100) >= pct) return std::string();
  Hook hk;
  hk.kind = hookKind;
  hk.site = site;
  return offerAt(g, hk);
}

bool warmHook(Game& g, int site) {
  Hook h;
  h.kind = (int)dsl::HookKind::Npc;
  h.site = site;
  HookCtx c;
  if (!hookCtx(g, h, c)) return true;
  needMemoFor(g, c);
  NeedMemo& M = needMemo();
  for (uint32_t bit = 1; bit && bit <= N_EVENT; bit <<= 1) {
    if (M.known & bit) continue;
    M.known |= bit;
    if (needHolds(g, c, bit)) M.have |= bit;
    return false;
  }
  return true;
}

void warmComposer() {
  static bool done = false;
  if (done) return;
  done = true;
  // one composition builds the composer's static tables (parsed templates, voice grammars); the result is thrown away
  Spec s;
  s.arch = "shield_wall";
  Composed c;
  (void)compose(s, c);
}

uint32_t needsMissing(Game& g, const Hook& h, uint32_t needs) {
  HookCtx c;
  if (!hookCtx(g, h, c)) return needs ? needs : 1u;
  uint32_t miss = 0;
  for (uint32_t bit = 1; bit && bit <= N_EVENT; bit <<= 1)
    if ((needs & bit) && !needsHold(g, c, bit)) miss |= bit;
  return miss;
}

const char* needName(uint32_t bit) {
  static const char* n[] = {"CAVE", "CAMP", "RUIN", "VILLAGE", "TOWN", "CITY", "KINGDOM", "RIVAL", "WAR", "FAMINE", "GRIEF",
                            "FRIENDS", "NEEDY", "UNIQUE", "BOSS", "SMITH", "CAPITAL", "FALLEN", "COAST", "EVENT"};
  for (int i = 0; i < 20; i++) if (bit == (1u << i)) return n[i];
  return "?";
}

uint32_t hookHashOf(Game& g, const Hook& h, int32_t& gx, int32_t& gy) {
  HookCtx c;
  if (!hookCtx(g, h, c)) { gx = gy = 0; return 0; }
  gx = c.gx; gy = c.gy;
  return c.hookHash;
}

void noteOffered(Game& g, const Hook& h, const std::string& id) {
  Spec s;
  if (!parseSpecId(id, s)) return;
  int32_t gx = 0, gy = 0;
  const uint32_t hh = hookHashOf(g, h, gx, gy);
  if (!hh) return;
  g.story.guard_.noteOffer(s, hh, g.day, gx, gy);
}

void noteTold(Game& g, const std::string& id, uint32_t inst, const Hook& h) {
  Spec s;
  if (!parseSpecId(id, s)) return;
  int32_t gx = 0, gy = 0;
  if (!hookHashOf(g, h, gx, gy)) {
    const Instance* in = g.story.find(inst);
    if (in && !in->cast.empty()) { gx = in->cast[0].gx; gy = in->cast[0].gy; }
  }
  Composed c;
  if (s.campaign) composeCampaign(s, c); else compose(s, c);
  const Instance* in = g.story.find(inst);
  g.story.guard_.note(s, in ? castShape(in->cast) : 0, g.day, gx, gy, c.families);
  // a place that told a story tells no other for a fortnight
  if (h.kind != (int)dsl::HookKind::Npc && h.site >= 0 && h.site < (int)g.world.sites.size()) {
    const ew::Gid sid = g.world.sites[(size_t)h.site].id;
    g.marks[storyMarkKey(ew::mix64(sid ^ 0x5A6A9ull * (uint64_t)(h.kind + 1)), MK_STORY_MARK)] = g.day;
  }
}

// ---------------------------------------------------------------- reports
LibraryReport report() {
  LibraryReport r;
  for (const Archetype& a : archetypes()) {
    r.archetypes++;
    if (a.tier == 2) r.tier2++; else r.arcs++;
    if (a.source < Source::COUNT) r.bySource[(int)a.source]++;
  }
  r.twists = (int)twists().size();
  r.voiceKeys = (int)voiceKeys().size();
  r.campaigns = (int)campaignPlans().size();
  return r;
}

}  // namespace saga
}  // namespace story
