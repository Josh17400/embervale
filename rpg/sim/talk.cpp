// M7b Voice (rpg/sim/talk.h): the phone panel, propose, the talk memory (the save's talk block), the recorder and the
// dialogue lint. TALK lane file (phase A: the lead's first cut of every part).
#include "rpg/sim/talk.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <utility>
#include "engine/font5x7.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"

namespace speech {

// ---------------------------------------------------------------- 1. the phone panel
std::vector<std::string> wrapLines(const std::string& s, int maxChars) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&]() {
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { lines.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char c : s) {
    if (c == ' ') flush();
    else if (c == '\n') { flush(); lines.push_back(line); line.clear(); }
    else word += c;
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::string joinFit(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  const std::string j = a + " " + b;
  return fitsPanel(j) ? j : b;
}

std::string countWord(int n) {
  static const char* const w[] = {"NO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE"};
  return n >= 0 && n <= 12 ? std::string(w[n]) : std::to_string(n);
}

std::string givenName(const std::string& shown) {
  const size_t e = shown.find_last_not_of(' ');
  if (e == std::string::npos) return shown;
  const size_t sp = shown.rfind(' ', e);
  return sp == std::string::npos ? shown.substr(0, e + 1) : shown.substr(sp + 1, e - sp);
}

// ---------------------------------------------------------------- 2. one line at a time
const char* topicName(Topic t) {
  static const char* n[] = {"none", "greeting", "intro", "smalltalk", "bark", "trade", "service", "need", "rumour", "news", "war",
                            "story", "quest offer", "quest active", "quest done", "reward", "warning", "home", "craft", "foe"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Topic::COUNT, "a name for every topic");
  return (int)t < (int)Topic::COUNT ? n[(int)t] : "?";
}

bool propose(Dialogue& d, int pri, Topic t, const std::string& text) {
  if (text.empty() || pri < d.pri) return false;
  d.text = text;
  d.pri = pri;
  d.topic = (uint8_t)t;
  return true;
}

// ---------------------------------------------------------------- 3. memory
uint32_t lineHash(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h ? h : 1u;
}

const Met* Memory::find(uint64_t key) const {
  auto it = std::lower_bound(met.begin(), met.end(), key, [](const Met& m, uint64_t k) { return m.key < k; });
  return it != met.end() && it->key == key ? &*it : nullptr;
}

Met& Memory::meet(uint64_t key, int day) {
  auto it = std::lower_bound(met.begin(), met.end(), key, [](const Met& m, uint64_t k) { return m.key < k; });
  if (it == met.end() || it->key != key) {
    if ((int)met.size() >= MAX_MET) {   // forget the one met longest ago (the lowest key among equals: deterministic)
      size_t old = 0;
      for (size_t i = 1; i < met.size(); i++) if (met[i].last < met[old].last) old = i;
      met.erase(met.begin() + (std::ptrdiff_t)old);
      it = std::lower_bound(met.begin(), met.end(), key, [](const Met& m, uint64_t k) { return m.key < k; });
    }
    Met m;
    m.key = key;
    m.first = m.last = day;
    it = met.insert(it, m);
  }
  it->last = day;
  if (it->times < 0xFFFF) it->times++;
  return *it;
}

void Memory::flag(uint64_t key, uint8_t f, int day) {
  auto it = std::lower_bound(met.begin(), met.end(), key, [](const Met& m, uint64_t k) { return m.key < k; });
  if (it == met.end() || it->key != key) {
    Met& m = meet(key, day);
    m.times = 0;
    m.flags |= f;
    return;
  }
  it->flags |= f;
}

bool Memory::saidRecently(uint32_t h, int day, int withinDays) const {
  for (const auto& r : recent) if (r.first == h && day - r.second <= withinDays) return true;
  return false;
}

void Memory::said(uint32_t h, int day) {
  recent.push_back({h, day});
  if ((int)recent.size() > RECENT) recent.erase(recent.begin());
}

void Memory::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(VERSION);
  w.u16((uint16_t)met.size());
  for (const Met& m : met) {
    w.u64(m.key); w.i32(m.first); w.i32(m.last); w.u16(m.times); w.u8(m.flags); w.u8(m.lastTopic); w.u32(m.lastLine);
  }
  w.u8((uint8_t)recent.size());
  for (const auto& r : recent) { w.u32(r.first); w.i32(r.second); }
}

bool Memory::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u8() != VERSION || r.bad) return false;
  Memory m;
  const int n = r.u16();
  if (n > MAX_MET) return false;
  uint64_t prev = 0;
  for (int i = 0; i < n && !r.bad; i++) {
    Met e;
    e.key = r.u64(); e.first = r.i32(); e.last = r.i32(); e.times = r.u16(); e.flags = r.u8(); e.lastTopic = r.u8(); e.lastLine = r.u32();
    if (i > 0 && e.key <= prev) return false;   // keys strictly ascending (a damaged block is refused)
    if (e.lastTopic >= (uint8_t)Topic::COUNT) e.lastTopic = 0;
    prev = e.key;
    m.met.push_back(e);
  }
  const int nr = r.u8();
  if (nr > RECENT) return false;
  for (int i = 0; i < nr && !r.bad; i++) {
    const uint32_t h = r.u32();
    const int32_t d = r.i32();
    m.recent.push_back({h, d});
  }
  if (r.bad || r.p != in.size()) return false;
  *this = std::move(m);
  return true;
}

bool Memory::operator==(const Memory& o) const {
  if (met.size() != o.met.size() || recent != o.recent) return false;
  for (size_t i = 0; i < met.size(); i++) {
    const Met &a = met[i], &b = o.met[i];
    if (a.key != b.key || a.first != b.first || a.last != b.last || a.times != b.times || a.flags != b.flags || a.lastTopic != b.lastTopic ||
        a.lastLine != b.lastLine)
      return false;
  }
  return true;
}

// ---------------------------------------------------------------- 4. the recorder
namespace {
// the options the panel shows (the page or option group on show: Dialogue::shows), with their indices in Dialogue::opts
Turn turnOf(const Dialogue& d, std::vector<int>& idx) {
  Turn t;
  t.speaker = d.speaker;
  t.text = d.text;
  idx.clear();
  for (size_t i = 0; i < d.opts.size(); i++)
    if (d.shows(d.opts[i])) { t.opts.push_back(d.opts[i].label); idx.push_back((int)i); }
  return t;
}
}  // namespace

void Recorder::begin(const Dialogue& d, const std::string& who, const std::string& role, const std::string& where, uint64_t key) {
  if (convs.size() >= maxConvs) return;
  Conversation c;
  c.who = who; c.role = role; c.where = where; c.key = key;
  c.turns.push_back(turnOf(d, shownIdx_));
  labels_.clear();
  for (const DlgOpt& o : d.opts) labels_.push_back(o.label);
  convs.push_back(std::move(c));
}

void Recorder::reply(int chosen, const Dialogue& d, bool open) {
  if (convs.empty() || convs.back().turns.empty()) return;
  Turn& last = convs.back().turns.back();
  // the choice as the panel showed it; an option picked by code from a group not on show is added to the row it was
  // picked from (so the record still says what was asked)
  int shown = -1;
  for (size_t k = 0; k < shownIdx_.size(); k++) if (shownIdx_[k] == chosen) shown = (int)k;
  if (shown < 0 && chosen >= 0 && chosen < (int)labels_.size()) {
    last.opts.push_back(labels_[(size_t)chosen]);
    shown = (int)last.opts.size() - 1;
  }
  last.chosen = shown;
  if (open) {
    convs.back().turns.push_back(turnOf(d, shownIdx_));
    labels_.clear();
    for (const DlgOpt& o : d.opts) labels_.push_back(o.label);
  }
}

// ---------------------------------------------------------------- 5. the lint
const char* checkName(uint32_t c) {
  switch (c) {
    case C_EMPTY: return "empty";
    case C_PLACEHOLDER: return "placeholder";
    case C_CASE: return "case or a character the HUD font cannot draw";
    case C_PANEL: return "too long for the phone panel";
    case C_OPTION: return "option too long";
    case C_REPEAT: return "repeated phrase";
    case C_DOUBLE: return "doubled word / broken punctuation";
    case C_STITCH: return "stitched fragments";
    case C_FRAGMENT: return "fragment";
    case C_NAME: return "name not introduced";
    case C_ECHO: return "answer ignores the choice";
    case C_PUNCT: return "unfinished sentence";
    case C_SELF: return "speaker names themselves in the third person";
    case C_ANSWER: return "answer does not follow the option";
    default: return "?";
  }
}

namespace {

std::vector<std::string> words(const std::string& s) {
  std::vector<std::string> w;
  std::string cur;
  for (char ch : s) {
    const bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '\'' || ch == '-';
    if (letter) cur += (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
    else if (!cur.empty()) { w.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) w.push_back(cur);
  return w;
}

bool isStop(const std::string& w) {
  static const char* const k[] = {"THE", "A", "AN", "AND", "OF", "TO", "IN", "IS", "IT", "I", "YOU", "YOUR", "MY", "ME", "WE", "OUR",
                                  "THAT", "THIS", "ON", "AT", "FOR", "WITH", "BE", "ARE", "WAS", "IT'S", "I'M", "BUT", "OR", "SO",
                                  "NOT", "NO", "DO", "HAVE", "HAS", "HE", "SHE", "THEY", "THEM", "HIS", "HER", "AS", "IF", "BY"};
  for (const char* s : k) if (w == s) return true;
  return false;
}

// sentences of a line (split after . ! ? keeping "..." whole)
std::vector<std::string> sentences(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (size_t i = 0; i < s.size(); i++) {
    cur += s[i];
    if (s[i] == '.' || s[i] == '!' || s[i] == '?') {
      while (i + 1 < s.size() && (s[i + 1] == '.' || s[i + 1] == '!' || s[i + 1] == '?' || s[i + 1] == '"' || s[i + 1] == ')')) cur += s[++i];
      size_t a = cur.find_first_not_of(' ');
      if (a != std::string::npos) out.push_back(cur.substr(a));
      cur.clear();
    }
  }
  size_t a = cur.find_first_not_of(' ');
  if (a != std::string::npos) out.push_back(cur.substr(a));
  return out;
}

// the word after a sentence-opening AND / OR that keeps it speech ("AND I SWEAR IT", "AND MIND THE OTHERS", "OR ELSE")
bool spokenAndOpener(const std::string& w) {
  static const char* const k[] = {"I", "I'M", "I'LL", "I'VE", "I'D", "YOU", "YOU'LL", "YOU'RE", "WE", "WE'LL", "MIND", "DON'T", "IF", "WHEN",
                                  "THEN", "NOW", "YET", "SO", "WHAT", "WHO", "WHY", "NOBODY", "NO", "NEVER", "ELSE", "WORSE", "MAYBE",
                                  "PERHAPS", "KEEP", "BRING", "TELL", "WATCH", "COME", "TAKE", "GODS", "MY", "OUR", "YOUR", "BE", "AFTER", "RATHER"};
  for (const char* s : k) if (w == s) return true;
  return false;
}

bool interjection(const std::string& w) {
  static const char* const k[] = {"YES", "NO", "AYE", "NAY", "AH", "OH", "HM", "HMM", "HRRM", "WELL", "GOOD", "FINE", "THANKS", "PLEASE",
                                  "FAREWELL", "GOODBYE", "PEACE", "HALT", "LISTEN", "LOOK", "COME", "GO", "QUIET", "ENOUGH", "STOP",
                                  "MERCY", "GODS", "INDEED", "SO", "RIGHT", "HELLO", "GREETINGS", "WELCOME", "NEVER", "ALWAYS",
                                  "TRUE", "AGREED", "DONE", "HUSH", "WAIT", "HAIL", "BEGONE", "ALAS", "OI", "HEY", "YOU", "AMEN",
                                  "GLADLY", "SORRY", "CERTAINLY", "GONE", "NOTHING", "NOBODY", "EXACTLY", "PERHAPS", "MAYBE", "LATER",
                                  "TOMORROW", "TONIGHT", "HERE", "THERE", "BLESS", "CHEERS", "SPLENDID", "AGAIN", "STRANGE", "DEAD",
                                  "EMPTY", "NONE", "BOTH", "SOMETIMES", "ONCE", "SOON", "TRULY", "AYE", "SHAME", "PITY", "HUMPH",
                                  "BAH", "PAH", "HA", "OUCH", "BRAVO", "SAFE", "THIEF", "LIAR", "COWARD", "FOOL", "FRIEND", "STRANGER",
                                  "TRAVELLER", "UNDERSTOOD", "DONE", "WONDERFUL", "EXCELLENT", "MARVELLOUS", "TERRIBLE"};
  for (const char* s : k) if (w == s) return true;
  return false;
}

// is the mention of `name` at word index i an introduction (who they are said with it)?
bool introducedAt(const std::vector<std::string>& w, size_t i, size_t len) {
  static const char* const before[] = {"MY", "OUR", "YOUR", "CALLED", "NAMED", "I'M", "AM", "IS", "OLD", "YOUNG", "LADY", "LORD", "SIR",
                                       "MASTER", "MISTRESS", "BROTHER", "SISTER", "MOTHER", "FATHER", "SON", "DAUGHTER", "WIFE",
                                       "HUSBAND", "UNCLE", "AUNT", "COUSIN", "NEPHEW", "NIECE", "GRANDSON", "GRANDDAUGHTER", "GRANDMOTHER", "GRANDFATHER", "GRANDAD", "GRAN",
                                       "LAD", "LASS", "BOY", "GIRL", "APPRENTICE", "PARTNER", "SWEETHEART", "BETROTHED", "FRIEND", "NEIGHBOUR", "KING", "QUEEN", "PRIEST", "SMITH",
                                       "CAPTAIN", "ELDER", "WIDOW", "HERMIT", "MERCHANT", "HUNTER", "GUARD", "JARL", "CHIEF"};
  static const char* const after[] = {"THE", "MY", "OUR", "WHO", "A", "AN", "OF", "IS", "WAS"};
  if (i > 0) for (const char* s : before) if (w[i - 1] == s) return true;
  // (fixer M7b r2) "THEY CALL IT NENAOR IRONHIDE", "FOLK CALL HER ..."
  if (i > 1 && (w[i - 2] == "CALL" || w[i - 2] == "CALLED" || w[i - 2] == "NAMED") && (w[i - 1] == "IT" || w[i - 1] == "HIM" || w[i - 1] == "HER" || w[i - 1] == "THEM"))
    return true;
  if (i + len < w.size()) for (const char* s : after) if (w[i + len] == s) return true;
  return false;
}

bool hasName(const std::vector<std::string>& w, const std::vector<std::string>& nw, size_t& at) {
  if (nw.empty() || nw.size() > w.size()) return false;
  for (size_t i = 0; i + nw.size() <= w.size(); i++) {
    bool m = true;
    for (size_t k = 0; k < nw.size() && m; k++) if (w[i + k] != nw[k]) m = false;
    if (m) { at = i; return true; }
  }
  return false;
}

void lineChecks(const std::string& s, int turn, bool option, uint32_t on, std::vector<Issue>& out) {
  auto add = [&](uint32_t c, const std::string& what) { if (on & c) out.push_back({c, turn, what.substr(0, 80)}); };
  if (s.find_first_not_of(' ') == std::string::npos) { add(C_EMPTY, option ? "(empty option)" : "(empty line)"); return; }
  for (char ch : s) if (ch == '{' || ch == '}' || ch == '%' || ch == '<' || ch == '>' || ch == '_' || ch == '#' || ch == '@') { add(C_PLACEHOLDER, s); break; }
  if (s.find("NULL") != std::string::npos || s.find("(NONE)") != std::string::npos) add(C_PLACEHOLDER, s);
  for (char ch : s) if (ch >= 'a' && ch <= 'z') { add(C_CASE, s); break; }
  // (fixer M7b r3) a character the HUD font has no glyph for prints as a blank gap (';' read "BENDS  THE"): C_CASE
  // covers every character the 5x7 font cannot draw
  for (char ch : s)
    if (ch != ' ' && ch != '\n' && !(ch >= 'a' && ch <= 'z') && font5x7::glyphIndex(ch) < 0) { add(C_CASE, std::string("no glyph for '") + ch + "' in " + s); break; }
  if (option) { if (!fitsOption(s)) add(C_OPTION, s); }
  else if (!fitsPanel(s)) add(C_PANEL, s);
  if (!option) {   // (fixer M7b r3) a bare trade given as a name ("I'M GUARD. I STAND WATCH HERE...")
    static const char* const trades[] = {"GUARD", "SOLDIER", "HERALD", "BANDIT", "VILLAGER", "INNKEEPER", "MERCHANT", "PRIEST", "FARMER",
                                         "CAPTAIN", "REFUGEE", "TRAVELLER", "HERBALIST", "ROYAL GUARD", "CITY GUARD"};
    bool hit = false;
    for (const char* t : trades)
      for (const char* end : {".", ",", "!"}) {
        const std::string pat = std::string("I'M ") + t + end;
        const size_t p = s.find(pat);
        if (!hit && p != std::string::npos && (p == 0 || s[p - 1] == ' ')) hit = true;
      }
    if (hit) add(C_SELF, "a trade for a name: " + s);
  }
  {   // doubled words and broken punctuation
    // (TALK lane) a word doubled inside one phrase ("THE THE"); "ABOUT IT. IT SMELLS" and "ANYTHING. ANYTHING." are speech
    std::vector<std::string> tok;
    {
      std::string cur;
      for (char ch : s) { if (ch == ' ') { if (!cur.empty()) tok.push_back(cur); cur.clear(); } else cur += ch; }
      if (!cur.empty()) tok.push_back(cur);
    }
    auto bare = [](const std::string& t) { return !t.empty() && ((t.back() >= 'A' && t.back() <= 'Z') || (t.back() >= '0' && t.back() <= '9')); };
    for (size_t i = 1; i < tok.size(); i++) {
      const std::vector<std::string> a0 = words(tok[i - 1]), a1 = words(tok[i]);
      if (a0.size() != 1 || a1.size() != 1 || !bare(tok[i - 1])) continue;
      const std::string& w0 = a0[0];
      if (w0 == a1[0] && w0.size() > 1 && w0 != "HA" && w0 != "NO" && w0 != "YE" && w0 != "VERY" && w0 != "WELL" && w0 != "HUSH") { add(C_DOUBLE, w0 + " " + w0); break; }
    }
    static const char* const bad[] = {" ,", " .", ",,", ",.", ".,", "?.", "!.", ":.", ";.", ". .", "  "};
    for (const char* b : bad)
      if (s.find(b) != std::string::npos) { add(C_DOUBLE, std::string("\"") + b + "\" in " + s); break; }
    for (size_t p = s.find(".."); p != std::string::npos; p = s.find("..", p + 3))
      if (s.compare(p, 3, "...") != 0) { add(C_DOUBLE, ".. in " + s); break; }
  }
  if (option) return;
  {   // stitched fragments
    const std::vector<std::string> ss = sentences(s);
    for (size_t k = 1; k < ss.size(); k++) {
      const std::vector<std::string> w = words(ss[k]);
      // (TALK lane) "AND I...", "AND MIND THE OTHERS", "OR ELSE": a spoken sentence may open on AND; a fragment glued on
      // after a full stop opens on AND / OR and goes on with a name or a new subject ("AND PRAY ARN KEEPS TO THE SOUTH")
      if (!w.empty() && (w[0] == "AND" || w[0] == "OR") && !(w.size() > 1 && spokenAndOpener(w[1]))) { add(C_STITCH, ss[k]); break; }
    }
    if (s.find(" (") != std::string::npos) add(C_STITCH, "a bracketed note after speech: " + s);
    const size_t e = s.find_last_not_of(' ');
    if (e != std::string::npos && (s[e] == ':' || s[e] == ',' || s[e] == ';')) add(C_STITCH, "ends on '" + std::string(1, s[e]) + "': " + s);
    for (const std::string& se : ss) {
      const std::vector<std::string> w = words(se);
      // (TALK lane) an adverb said on its own ("MOSTLY.", "HONESTLY.") or a bare number answer is speech, not a fragment
      const bool adverb = w.size() == 1 && w[0].size() > 4 && w[0].compare(w[0].size() - 2, 2, "LY") == 0;
      // ... and a word said again for weight ("IT LEAVES THE FLEECE FOLDED. FOLDED.") or an order ("HURRY.", "SPEAK.")
      const std::vector<std::string> all = words(s);
      const bool echo = w.size() == 1 && std::count(all.begin(), all.end(), w[0]) > 1;
      static const char* const orders[] = {"HURRY", "SPEAK", "RUN", "FLEE", "LEAVE", "HIDE", "PRAY", "LISTEN", "WATCH", "EVER", "FUNNY", "ODD",
                                           "QUICKLY", "NOW", "PLEASE", "BURIED", "NOTHING", "ANYTHING", "EVERYTHING", "NOBODY", "SILENCE"};
      bool order = false;
      for (const char* o : orders) if (w.size() == 1 && w[0] == o) order = true;
      // a closing word that ends the line ("THE SAND HAS MARKED YOUR FACE. REST.") is a spoken cadence, not a scrap
      if (w.size() == 1 && ss.size() > 1 && &se == &ss.back()) order = true;
      if (w.size() == 1 && !interjection(w[0]) && !adverb && !echo && !order &&se.back() != '?' && se.back() != '!' && se.find("...") == std::string::npos) { add(C_FRAGMENT, se); break; }
    }
  }
  {   // the last sentence is finished
    const size_t e = s.find_last_not_of(' ');
    const char ch = e == std::string::npos ? 0 : s[e];
    if (!(ch == '.' || ch == '!' || ch == '?' || ch == '"' || ch == ')' || ch == '\'')) add(C_PUNCT, s);
  }
}

}  // namespace

namespace {
// a word's stem for the answer check (WOLVES ~ WOLF, ROOMS ~ ROOM, KEEPING ~ KEEP)
std::string stem(std::string w) {
  if (w.size() > 2 && w.compare(w.size() - 2, 2, "'S") == 0) w.resize(w.size() - 2);
  if (w.size() > 5 && w.compare(w.size() - 3, 3, "ING") == 0) w.resize(w.size() - 3);
  else if (w.size() > 4 && w.compare(w.size() - 3, 3, "VES") == 0) w = w.substr(0, w.size() - 3) + "F";
  else if (w.size() > 4 && w.compare(w.size() - 3, 3, "IES") == 0) w = w.substr(0, w.size() - 3) + "Y";
  else if (w.size() > 4 && w.compare(w.size() - 2, 2, "ED") == 0) w.resize(w.size() - 2);
  else if (w.size() > 3 && w.back() == 'S' && w[w.size() - 2] != 'S') w.pop_back();
  return w;
}
// words that answer for each other (an option's word, then the kinds of words a fitting reply uses)
const char* const kKin[][16] = {
    {"RUMOUR", "NEWS", "HEARD", "HEAR", "SAY", "TALK", "WORD", "TELL", "MARK", "GOSSIP", "QUIET", "KNOW", "PASS", "SEEN", "ROAD", nullptr},
    {"ROOM", "BED", "STAIR", "DOOR", "SLEEP", "NOON", "REST", "WAKE", "UPSTAIRS", nullptr},
    {"WARE", "BUY", "SELL", "GOOD", "STOCK", "PRICE", "SHELF", "LOOK", nullptr},
    {"MEAL", "EAT", "BREAD", "STEW", "POT", "FOOD", "BOWL", "BROTH", "SOUP", "CRUST", "DISH", "HUNGRY", "TONIGHT", nullptr},
    {"WORK", "JOB", "HELP", "NEED", "PAY", "GOLD", "REWARD", "TROUBLE", "HAND", "ASK", "FAVOUR", nullptr},
    {"BEAST", "LAIR", "WOLF", "MONSTER", "CREATURE", "THING", "KILL", "WALK", "BOAR", "BEAR", "PRAY", "NOTHING", nullptr},
    {"KEEP", "GET", "WELL", "FINE", "TIRED", "HUNGRY", "BY", "GOOD", "DAY", "ASK", "EATEN", "SLEEP", "WORK", "LATELY", nullptr},
    {"BLESS", "BLESSING", "GOD", "HEAL", "WOUND", "PRAY", "MEND", nullptr},
    {"TEACH", "LEARN", "LESSON", "TRUST", "PATTERN", "CRAFT", "TRADE", "SHOW", "STRANGER", "FAMILY", nullptr},
    {"SHARE", "KIND", "THANK", "BLESS", "EAT", "FORGET", "FOOD", nullptr},
    {"HIRE", "PAY", "WORK", "GLADLY", "WAGE", "DAY", nullptr},
    {"COLLECT", "REWARD", "BOUNTY", "GOLD", "EARN", "THANK", "PROMISE", "PAY", "OWE", "COUNT", "COIN", nullptr},
    {"PROPERTY", "SALE", "HOUSE", "LAND", "DEED", "PLOT", "HUT", "TOWNHOUSE", "BUY", nullptr},
    {"COOK", "KITCHEN", "HEARTH", "STOVE", "FIRE", "POT", nullptr},
    {"RECIPE", "COOK", "DISH", "MOTHER", "LEARN", nullptr},
    {"SPARE", "COIN", "GOLD", "BLESS", "GOD", "THANK", "KEEP", nullptr},
    {"OFFER", "ROOM", "MEAL", "BED", "WARE", "SELL", "RUMOUR", "WHAT", "GOLD", nullptr},
    {"SUPPLY", "STORE", "FOOD", "PORTION", "FEED", "SELL", nullptr},
    {"HORSE", "STABLE", "BEAST", "SADDLE", "RIDE", "MARE", "SHE", nullptr},
    {"NEWS", "ROAD", "PASS", "SAW", "QUIET", "HEARD", "TOLD", nullptr},
};
bool farewellOrCommit(const std::vector<std::string>& w) {
  if (w.empty()) return true;
  static const char* const lead[] = {"FAREWELL", "GOODBYE", "LATER", "THANK", "THANKS", "I'LL", "I", "WE", "WE'LL", "LET'S", "DEAL", "YES",
                                     "NO", "AYE", "FINE", "GO", "NOT", "NEVER", "STAY", "HAND", "PAY", "GIVE", "TAKE", "AGREED",
                                     "SOMETHING", "BACK", "OK", "MAYBE", "PERHAPS", "VERY", "BREAK", "JOIN", "LEAD"};
  for (const char* s : lead) if (w[0] == s) return true;
  return false;
}
}  // namespace

bool answers(const std::string& option, const std::string& reply) {
  const std::vector<std::string> ow = words(option);
  if (farewellOrCommit(ow)) return true;
  const std::vector<std::string> rw = words(reply);
  // a question that asks for an account (WHO, WHY, WHAT HAPPENED...) or an observation that invites one ("YOU LOOK
  // LIKE YOU HAVEN'T SLEPT.") is answered by the account itself: a whole statement, not a greeting or a brush-off
  static const char* const asks[] = {"WHO", "WHO'S", "WHOSE", "WHY", "WHAT", "WHAT'S", "WHERE", "WHERE'S", "HOW", "WHEN", "IS", "ARE", "DID",
                                     "DO", "YOU", "YOU'VE", "YOU'RE", "THAT'S", "THAT", "THERE'S", "TELL", "NO", "SO"};
  bool account = false;
  for (const char* a : asks) if (ow[0] == a) account = true;
  if (account && rw.size() >= 6 && !(rw[0] == "FAREWELL" || rw[0] == "GOODBYE" || rw[0] == "LATER")) return true;
  // an answer given to a riddle or a choice is judged ("HM. SECOND: ...", "WRONG!", "AH, YOU KNOW IT")
  static const char* const judge[] = {"HM", "HMM", "AH", "OH", "YES", "NO", "WRONG", "RIGHT", "CORRECT", "GOOD", "CLEVER", "HA", "WELL"};
  for (const char* j : judge) if (!rw.empty() && rw[0] == j) return true;
  std::set<std::string> rs;
  for (const std::string& x : words(reply)) rs.insert(stem(x));
  for (const std::string& x0 : ow) {
    if (isStop(x0) || x0.size() < 3) continue;
    const std::string x = stem(x0);
    if (rs.count(x)) return true;
    for (const auto& kin : kKin) {
      bool has = false;
      for (int k = 0; kin[k] && !has; k++) if (x == stem(kin[k])) has = true;
      if (!has) continue;
      for (int k = 0; kin[k]; k++) if (rs.count(stem(kin[k]))) return true;
    }
  }
  return false;
}

std::vector<Issue> lint(const Conversation& c, const LintCtx& ctx) {
  std::vector<Issue> out;
  const uint32_t on = ctx.checks;
  // per line
  for (size_t t = 0; t < c.turns.size(); t++) {
    const Turn& T = c.turns[t];
    lineChecks(T.text, (int)t, false, on, out);
    for (const std::string& o : T.opts) lineChecks(o, (int)t, true, on, out);
    if ((on & C_ECHO) && t > 0 && c.turns[t - 1].chosen >= 0 && T.text == c.turns[t - 1].text)
      out.push_back({C_ECHO, (int)t, T.text.substr(0, 80)});
    if ((on & C_ANSWER) && t > 0 && c.turns[t - 1].chosen >= 0 && c.turns[t - 1].chosen < (int)c.turns[t - 1].opts.size() &&
        T.text != c.turns[t - 1].text) {
      const std::string& asked = c.turns[t - 1].opts[(size_t)c.turns[t - 1].chosen];
      if (!answers(asked, T.text)) out.push_back({C_ANSWER, (int)t, asked + " -> " + T.text.substr(0, 60)});
    }
  }
  // the same 4-word phrase twice in the speaker's lines (not counting a phrase of function words only)
  if (on & C_REPEAT) {
    std::set<std::string> seen;
    bool done = false;
    for (size_t t = 0; t < c.turns.size() && !done; t++) {
      std::set<std::string> mine;
      // (fixer M7b r2) a phrase lies within one sentence: "HERE IN DAR QUR. A NEW VOICE..." and "IN DAR QUR, A SHORT
      // WALK" share no phrase
      const std::string& tx = c.turns[t].text;
      size_t from = 0;
      while (from < tx.size() && !done) {
        size_t to = tx.find_first_of(".!?:;", from);
        if (to == std::string::npos) to = tx.size();
        const std::vector<std::string> w = words(tx.substr(from, to - from));
        from = to + 1;
        for (size_t i = 0; i + 4 <= w.size() && !done; i++) {
          int content = 0;
          std::string g;
          for (size_t k = 0; k < 4; k++) { g += w[i + k] + " "; if (!isStop(w[i + k])) content++; }
          if (content < 2) continue;
          if (seen.count(g) && !mine.count(g)) { out.push_back({C_REPEAT, (int)t, g}); done = true; }
          mine.insert(g);
        }
      }
      for (const std::string& g : mine) seen.insert(g);
    }
  }
  // the speaker named in the third person in their own line
  if ((on & C_SELF) && !c.who.empty() && c.role != "TROLL") {   // (a troll's way of talking is its own)
    const std::vector<std::string> sp = words(c.who);
    for (size_t t = 0; t < c.turns.size(); t++) {
      if (c.turns[t].speaker != c.who) continue;
      const std::vector<std::string> w = words(c.turns[t].text);
      size_t at = 0;
      if (!hasName(w, sp, at)) continue;
      // "I'M ARN", "NAME'S ARN", "CALL ME ARN", "ARN, THEY CALL ME", "ARN, SMITH BY TRADE", "ARN HERE", "THE ROYAL GUARD"
      const size_t e = at + sp.size();
      const bool own = (at >= 1 && (w[at - 1] == "I'M" || w[at - 1] == "ME" || w[at - 1] == "IS" || w[at - 1] == "AM" || w[at - 1] == "NAME'S" ||
                                    w[at - 1] == "THE")) ||
                       (e < w.size() && (w[e] == "HERE" || w[e] == "THEY")) || (e + 1 < w.size() && w[e + 1] == "BY");
      if (!own) { out.push_back({C_SELF, (int)t, c.who + ": " + c.turns[t].text.substr(0, 70)}); break; }
    }
  }
  // names before their introduction
  if (on & C_NAME) {
    const std::vector<std::string> sp = words(c.who);
    for (const std::string& name : ctx.names) {
      const std::vector<std::string> nw = words(name);
      if (nw.empty()) continue;
      if (std::find(ctx.known.begin(), ctx.known.end(), name) != ctx.known.end()) continue;
      size_t at = 0;
      if (hasName(sp, nw, at)) continue;   // the speaker's own name (the panel's title shows it)
      for (size_t t = 0; t < c.turns.size(); t++) {
        const std::vector<std::string> w = words(c.turns[t].text);
        if (!hasName(w, nw, at)) continue;
        if (!introducedAt(w, at, nw.size())) out.push_back({C_NAME, (int)t, name});
        break;   // judged at its first mention
      }
    }
  }
  return out;
}

std::string formatPhone(const Conversation& c) {
  std::string s = "== " + c.who + (c.role.empty() ? "" : " (" + c.role + ")") + (c.where.empty() ? "" : " IN " + c.where) + "\n";
  for (const Turn& T : c.turns) {
    s += "   [" + T.speaker + "]\n";
    for (const std::string& l : wrapLines(T.text, PANEL_CHARS)) s += "   | " + l + "\n";
    for (size_t o = 0; o < T.opts.size(); o++) s += std::string((int)o == T.chosen ? "   > " : "     ") + std::to_string(o + 1) + ". " + T.opts[o] + "\n";
  }
  return s;
}

}  // namespace speech
