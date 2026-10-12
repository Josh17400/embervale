// M8 Fellowship: the companions' talk (rpg/sim/party.h, game.h partyTalk / partyFolkTalk / partyChoose /
// partyGraveTalk; rpg/sim/party_voice.h). VOICE lane. Everything is said at the M7b bar (rpg/sim/talk.h lint, rpg_test
// --dialogue): natural, first person, names introduced by what they are ("MY WIFE ASTRID"), answers that take up the
// option chosen, whole sentences that fit the phone panel, at most six options on a first page.
//
//   a companion's own conversation (partyTalk): what is on their mind first (their personal quest's offer or its next
//   talk, a grievance below APPROVAL_COMPLAIN, their wounds, keeping watch), then the pages: orders (follow, guard me,
//   hold back, wait here, go home / to the player's house, part ways), their story (aboutLine; home, the ones left
//   behind, what drives them, how they learned to fight), what they made of the deeds they remember (by personality),
//   how they are keeping and what they think of the player (approval and bond) and of their gear (their own people's make
//   or a foreign one), their own errand while it runs
//   everyone else (partyFolkTalk, a talkToInner section-7 hook): a willing tavern regular's or friend's pitch from their
//   real history, a hireling's terms and a haggle, refusals by PartyOffer::why, a member at home asked back, one who LEFT
//   asking for amends (another day, an apology), the kin of a companion on the road ("LOOK AFTER MY DAUGHTER"), and the
//   kin, friends and innkeeper of a fallen companion remembering them
//   the grave (partyGraveTalk): the epitaph in their people's rite, flowers, a keepsake, a few words said
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace {
enum PartyDlg : int {
  P_JOIN = DLG_PARTY + 1,   // a willing person: come with me
  P_JOIN_YES,               // ... they say yes: recruit (arg: party::Origin)
  P_REJOIN,                 // a member at home (or waiting): come with me again
  P_STATUS,                 // a companion: how are you holding up?
  P_PART,                   // a companion: we part ways here
  P_PART_YES,               // ... confirmed
  P_GRAVE_FLOWERS,          // at a grave: lay flowers
  P_GRAVE_KEEP,             // at a grave: take a keepsake
  // (VOICE, phase B)
  P_JOIN_ABOUT,             // the recruit's pitch: tell me more about yourself first
  P_HIRE,                   // a hireling: their terms
  P_HIRE_YES,               // ... agreed (arg: the wage)
  P_HAGGLE,                 // ... a lower wage asked (arg: the wage asked)
  P_AMENDS,                 // a member who left: I was wrong
  P_AMENDS_YES,             // ... the apology made properly
  P_KIN_PROMISE,            // a companion's kin: I'll keep them safe (arg: members index)
  P_FALLEN_KIN,             // the fallen's kin: what you tell them (arg: fallen index * 4 + answer)
  P_ORDERS,                 // a companion: the orders page
  P_ABOUT,                  // ... their story
  P_ABOUT_HOME,
  P_ABOUT_KIN,
  P_ABOUT_STAKE,
  P_ABOUT_FIGHT,
  P_DEEDS,                  // ... what they made of what we did (the list)
  P_DEED,                   // ... one memory (arg: members index * 64 + memory index)
  P_FEEL,                   // ... what they think of the player
  P_GEAR,                   // ... their gear
  P_QUEST,                  // ... their personal quest offered (arg: members index)
  P_QUEST_LATER,            // ... put off
  P_QUEST_NEWS,             // ... how their errand goes
  P_GRIEVANCE,              // ... below APPROVAL_COMPLAIN: what's wrong?
  P_GRIEVE_SORRY,           // ... I'll do better
  P_BACK,                   // ... back to the first page
  P_GRAVE_WORDS,            // at a grave: say a few words
  P_CMD = DLG_PARTY + 100,  // + party::Cmd (arg: members index)
};
using party::Member;
using party::State;
using party::voice::Bio;
using party::voice::Kin;

uint32_t mixh(uint32_t a, uint32_t b) { return hash32(a * 0x9E3779B1u ^ hash32(b + 0x632BE5ABu)); }
std::string heShe(bool f) { return f ? "SHE" : "HE"; }
std::string himHer(bool f) { return f ? "HER" : "HIM"; }
// (fixer M8 r3) a Fallen::cause (a standalone phrase: the paper doll's grave page shows it alone) as the end of
// "IN THE END ...": the subject said, the verb agreeing ("SHE WAS STRUCK DOWN WHERE SHE LAY", never "SHE STRUCK DOWN")
std::string fellClause(const std::string& cause, bool f) {
  if (cause == "STRUCK DOWN WHERE THEY LAY") return heShe(f) + " WAS STRUCK DOWN WHERE " + heShe(f) + " LAY";
  if (cause.rfind("FELL", 0) == 0 || cause.rfind("DIED", 0) == 0 || cause.rfind("BLED", 0) == 0 || cause.rfind("WAS ", 0) == 0) return heShe(f) + " " + cause;
  return heShe(f) + " WAS " + cause;   // a past participle ("SLAIN BY ...", "TAKEN BY ...")
}
std::string hisHer(bool f) { return f ? "HER" : "HIS"; }
// a + " " + b when it fits the panel, else a (the line that matters first)
std::string also(const std::string& a, const std::string& b) {
  if (b.empty()) return a;
  const std::string t = a + " " + b;
  return speech::fitsPanel(t) ? t : a;
}
// what the deed was, said as a noun phrase ("THE BANDITS WE PUT DOWN")
std::string deedPhrase(party::Deed d, const std::string& place) {
  using party::Deed;
  const std::string in = place.empty() ? std::string() : " IN " + place;
  switch (d) {
    case Deed::QuestDone: return "THE HELP WE GAVE" + in;
    case Deed::QuestFailed: return "THE JOB WE LET DROP" + in;
    case Deed::Rescued: return "THE ONE WE BROUGHT HOME ALIVE";
    case Deed::HelpedRefugees: return "THE REFUGEES WE HELPED";
    case Deed::Charity: return "WHAT YOU GAVE AWAY" + in;
    case Deed::Befriended: return "THE FRIEND YOU MADE" + in;
    case Deed::Mercy: return "THE ONE YOU SPARED";
    case Deed::Cruelty: return "WHAT YOU DID TO THAT POOR WRETCH";
    case Deed::KilledInnocent: return "THE INNOCENT WHO DIED" + in;
    case Deed::KilledGuard: return "THE GUARD YOU KILLED";
    case Deed::Stole: return "WHAT YOU STOLE" + in;
    case Deed::ClearedBandits: return "THE BANDITS WE PUT DOWN";
    case Deed::SlewBeast: return "THE BEAST WE KILLED";
    case Deed::SlewMonster: return "THE MONSTER WE BROUGHT DOWN";
    case Deed::JoinedWar: return "THE SIDE YOU TOOK IN THE WAR";
    case Deed::DefendedTown: return "THE TOWN WE HELD" + in;
    case Deed::ExploredRuin: return "THE RUINS WE WENT INTO";
    case Deed::Tavern: return "OUR NIGHT AT THE INN" + in;
    case Deed::Prayed: return "YOUR PRAYERS AT THE SHRINE";
    case Deed::Bribed: return "THE BRIBE YOU PAID";
    case Deed::Lied: return "THE LIE YOU TOLD";
    case Deed::Oathbreak: return "THE OATH YOU BROKE";
    case Deed::DefiledGrave: return "THE GRAVE YOU ROBBED";
    case Deed::Abandoned: return "THE DAY YOU LEFT ME BEHIND";
    default: return "WHAT WE DID";
  }
}
// the opinion of a remembered deed, by its sign and size and the speaker's nature
std::string opinion(const Member& m, const party::Memory& mem, const std::string& god) {
  const int d = mem.delta;
  const bool pious = m.p.pious > 0, kind = m.p.kind > 0, lawful = m.p.lawful > 0, brave = m.p.brave > 0;
  if (d >= 6) {
    if (kind) return "IT WAS GOOD. I'VE THOUGHT ABOUT IT SINCE, AND I'D DO IT AGAIN. THAT'S THE KIND OF PERSON I WANT TO BE.";
    if (pious && !god.empty()) return "IT WAS RIGHT, AND " + god + " SAW IT. I THINK OF IT WHEN I PRAY.";
    if (brave) return "THAT WAS A DAY. THAT'S THE SORT OF THING I LEFT HOME FOR.";
    return "IT WAS WELL DONE. I DON'T SAY THAT OFTEN, SO MARK IT.";
  }
  if (d > 0) return "IT WAS FINE. GOOD, EVEN. I DIDN'T THINK MUCH OF IT AT THE TIME, BUT IT STAYED WITH ME.";
  if (d <= -6) {
    if (lawful) return "IT WAS WRONG, AND YOU KNOW IT WAS WRONG. THERE ARE LAWS FOR A REASON. I HAVEN'T FORGOTTEN.";
    if (kind) return "IT STILL MAKES ME SICK TO THINK OF IT. I KEEP HOPING YOU'LL TELL ME YOU REGRET IT.";
    if (pious && !god.empty()) return "IT WAS WICKED. I'VE PRAYED TO " + god + " FOR YOU SINCE. I DON'T KNOW IF IT HELPS.";
    return "I DIDN'T LIKE IT THEN AND I DON'T LIKE IT NOW. LET'S NOT DO IT AGAIN.";
  }
  if (d < 0) return "I WASN'T HAPPY ABOUT IT. I'VE SEEN WORSE, BUT I WASN'T HAPPY.";
  return "I HONESTLY DIDN'T THINK MUCH ABOUT IT EITHER WAY.";
}
}  // namespace

namespace {
const life::Resident* residentOf(const Game& g, const Actor& a) {
  if (a.resident < 0 || a.site < 0 || a.site >= (int)g.world.sites.size()) return nullptr;
  const life::Census* c = g.life.find(g.world.sites[(size_t)a.site].id);
  return c && a.resident < (int)c->res.size() ? &c->res[(size_t)a.resident] : nullptr;
}
std::string godOf(const Game& g, uint64_t culture) {
  if (!culture || !g.world.src) return std::string();
  const cult::Culture& C = g.world.src->culture(culture);
  return C.faith.names.empty() ? std::string() : C.faith.names[0];
}
std::string siteNameOf(const Game& g, ew::Gid id) {
  const int h = id ? g.world.siteHandle(id) : -1;
  return h >= 0 ? g.world.sites[(size_t)h].name : std::string();
}
// what member m is to the resident `idx` of their census (the resident's word for them: "DAUGHTER", "HUSBAND"), ""
// when no kin
std::string kinWordFor(const Game& g, const Member& m, int idx) {
  const Bio b = party::voice::bio(g, m);
  for (const Kin& k : b.household) {
    if (k.idx != idx) continue;
    if (k.word == "MOTHER" || k.word == "FATHER") return m.female ? "DAUGHTER" : "SON";
    if (k.word == "SON" || k.word == "DAUGHTER") return m.female ? "MOTHER" : "FATHER";
    if (k.word == "WIFE" || k.word == "HUSBAND") return m.female ? "WIFE" : "HUSBAND";
    if (k.word == "BROTHER" || k.word == "SISTER") return m.female ? "SISTER" : "BROTHER";
    return "KIN";
  }
  return std::string();
}
}  // namespace

int PartyTalkOps::memberOfResident(const Game& g, const Actor& a) {
  if (a.resident < 0 || a.site < 0 || a.site >= (int)g.world.sites.size()) return -1;
  const ew::Gid id = life::npcId(g.world.sites[(size_t)a.site].id, a.resident);
  for (size_t i = 0; i < g.party.members.size(); i++) if (g.party.members[i].npc == id) return (int)i;
  return -1;
}
void PartyTalkOps::graveTalk(Game& g, int fi) { g.partyGraveTalk(fi); }

// the first page of a companion's own conversation
void PartyTalkOps::front(Game& g, int mi, bool opening) {
  Member& m = g.party.members[(size_t)mi];
  const Bio b = party::voice::bio(g, m);
  const uint32_t h = mixh(m.id * 0x3Fu, (uint32_t)g.day * 7u + (uint32_t)g.hour);
  const int ai = g.findActor(m.actor);
  const bool hurt = ai > 0 && g.actors[(size_t)ai].maxHp > 0 && g.actors[(size_t)ai].hp < g.actors[(size_t)ai].maxHp * 0.5f;
  g.dlg.opts.clear();
  g.dlg.group = 0;
  std::string t;
  // what is on their mind
  const int qs = party::voice::questStatus(g, m);
  std::string spec;
  const bool offer = qs < 2 && !(m.flags & party::MF_QUEST_DONE) && questReady(g, mi, spec);
  if (m.approval < party::APPROVAL_COMPLAIN) {
    static const char* const k[] = {"WHAT NOW? SAY IT QUICKLY.", "YOU WANT SOMETHING FROM ME? AFTER EVERYTHING?", "WHAT IS IT? I'M LISTENING, FOR NOW."};
    t = k[h % 3];
    g.dlg.opts.push_back({"WHAT'S WRONG?", P_GRIEVANCE, mi});
  } else if (offer) {
    const story::dsl::Script* sc = story::scriptById(spec);
    t = sc && !sc->hint.empty() ? sc->hint : std::string("THERE'S SOMETHING I'VE BEEN MEANING TO ASK YOU.");
    g.dlg.opts.push_back({sc ? sc->pitch : std::string("WHAT IS IT?"), P_QUEST, mi});
    m.flags |= party::MF_QUEST_OFFERED;
    if (m.questState == 0) m.questState = 1;
  } else if (m.state == State::Waiting) {
    t = h % 2 ? "YOU'RE BACK. I KEPT WATCH WHILE YOU WERE GONE." : "THERE YOU ARE. I WAS STARTING TO WONDER IF YOU'D FORGOTTEN ME.";
  } else if (hurt || (m.flags & party::MF_WOUNDED)) {
    t = h % 2 ? "I'M STILL HURTING FROM THAT LAST FIGHT, BUT I'M HERE. WHAT DO YOU NEED?" : "I'LL MEND. A NIGHT'S SLEEP AND I'LL BE BETTER. WHAT IS IT?";
  } else if (m.approval >= party::APPROVAL_QUEST) {
    t = h % 2 ? "I'M GLAD IT'S YOU I'M WALKING WITH. WHAT DO YOU NEED?" : "ALWAYS GOOD TO HEAR YOUR VOICE. WHAT IS IT?";
  } else {
    static const char* const k[] = {"I'M HERE. WHAT DO YOU NEED?", "YES? I'M LISTENING.", "WHAT IS IT? I'M READY WHEN YOU ARE."};
    t = k[h % 3];
    if (g.isNight() && h % 4 == 0) t = "IT'S LATE. WHAT IS IT?";
  }
  if (qs == 2) g.dlg.opts.push_back({"HOW IS YOUR OWN ERRAND GOING?", P_QUEST_NEWS, mi});
  g.dlg.opts.push_back({"I HAVE ORDERS FOR YOU.", P_ORDERS, mi});
  g.dlg.opts.push_back({"TELL ME ABOUT YOURSELF.", P_ABOUT, mi});
  bool mem = false;
  for (const party::Memory& x : m.mem) if (x.delta != 0) mem = true;
  if (mem) g.dlg.opts.push_back({"WHAT DID YOU THINK OF WHAT WE DID?", P_DEEDS, mi});
  g.dlg.opts.push_back({"HOW ARE YOU HOLDING UP?", P_STATUS, mi});
  g.dlg.opts.push_back({"NOTHING. LET'S GO ON.", A_BYE, 0});
  if ((int)g.dlg.opts.size() > speech::FIRST_PAGE)   // (a full page: the deeds wait for another time)
    g.dlg.opts.erase(std::remove_if(g.dlg.opts.begin(), g.dlg.opts.end(), [](const DlgOpt& o) { return o.action == P_DEEDS; }), g.dlg.opts.end());
  if (!opening) t = "WHAT ELSE?";
  speech::propose(g.dlg, speech::P_URGENT, speech::Topic::Companion, t);
  if (!opening) g.dlg.text = t;
  (void)b;
}

void PartyTalkOps::orders(Game& g, int mi) {
  const Member& m = g.party.members[(size_t)mi];
  using party::Cmd;
  g.dlg.opts.clear();
  if (m.stance != Cmd::Follow || m.state != State::With) g.dlg.opts.push_back({"FOLLOW ME.", P_CMD + (int)Cmd::Follow, mi});
  if (m.state == State::With) {
    if (m.stance != Cmd::Defend) g.dlg.opts.push_back({"STAY AT MY SIDE AND GUARD ME.", P_CMD + (int)Cmd::Defend, mi});
    if (m.stance != Cmd::Passive) g.dlg.opts.push_back({"HOLD BACK FROM THE FIGHTING.", P_CMD + (int)Cmd::Passive, mi});
    g.dlg.opts.push_back({"WAIT HERE FOR ME.", P_CMD + (int)Cmd::Wait, mi});
    g.dlg.opts.push_back({g.home.plots.empty() ? "GO HOME. I'LL FIND YOU THERE." : "GO TO MY HOUSE AND WAIT FOR ME THERE.", P_CMD + (int)Cmd::Home, mi});
  }
  g.dlg.opts.push_back({"WE PART WAYS HERE.", P_PART, mi});
  g.dlg.opts.push_back({"SOMETHING ELSE.", P_BACK, mi});
}

// is their personal quest ready to be offered? (APPROVAL_QUEST, bond 30, a quest their history can tell)
bool PartyTalkOps::questReady(Game& g, int mi, std::string& spec) {
  Member& m = g.party.members[(size_t)mi];
  if (m.approval < party::APPROVAL_QUEST || m.bond < 30 || m.state != State::With) return false;
  if (m.quest.empty() || !story::scriptById(m.quest)) {
    m.quest = party::voice::questFor(g, m);
    if (m.quest.empty()) return false;
  }
  spec = m.quest;
  return true;
}

bool Game::partyTalk(Actor& a) {
  const int mi = (int)a.comp - 1;
  if (mi < 0 || mi >= (int)party.members.size()) return false;
  Member& m = party.members[(size_t)mi];
  if (m.voc == party::Vocation::Hound) {   // (the animal: a scratch behind the ears, and the orders)
    dlg.speaker = m.name;
    a.face = faceOf(pl().p - a.p);
    static const char* const k[] = {"THE HOUND LEANS ITS WHOLE WEIGHT AGAINST YOUR LEGS AND THUMPS ITS TAIL.",
                                    "IT LOOKS UP AT YOU, EARS PRICKED, WAITING TO SEE WHERE YOU GO NEXT."};
    dlg.text = k[mixh(m.id, (uint32_t)day) % 2];
    PartyTalkOps::orders(*this, mi);
    dlg.opts.back() = {"GOOD DOG. COME ON.", A_BYE, 0};
    dlg.front = dlg.text;
    mode = Mode::Dialogue;
    sfx((int)Sfx::Bark, a.p, 1.1f, 0.6f);
    return true;
  }
  dlg.speaker = m.name;
  a.face = faceOf(pl().p - a.p);
  // their own story waits on a talk with them: it speaks first (the story's choices, then a way to give orders)
  if (m.questInst) {
    story::Instance* in = story.find(m.questInst);
    if (in && !in->done && story.talksTo(*this, *in, a)) {
      story.sagaOffers_.clear();
      story.said_.clear();
      dlg.opts = {{"NOT NOW. I HAVE ORDERS FOR YOU.", P_ORDERS, mi}};
      story.showDialogue(*this, *in);
      dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      dlg.front = dlg.text;
      mode = Mode::Dialogue;
      sfx((int)Sfx::Talk, a.p);
      return true;
    }
  }
  PartyTalkOps::front(*this, mi, true);
  dlg.front = dlg.text;
  mode = Mode::Dialogue;
  sfx((int)Sfx::Talk, a.p);
  return true;
}

// ================================================================ everyone else
void Game::partyFolkTalk(Actor& a) {
  if (!a.npc || !a.human || a.comp) return;
  using speech::Topic;
  const uint64_t key = npcKey(a);
  // ---- a member's own street body (at home, waiting there, or left over the player's deeds)
  const int mi = PartyTalkOps::memberOfResident(*this, a);
  if (mi >= 0) {
    Member& m = party.members[(size_t)mi];
    const uint32_t h = mixh(m.id, (uint32_t)day);
    if (m.state == State::Home) {
      speech::propose(dlg, speech::P_STORY, Topic::Companion,
                      h % 2 ? "YOU'RE BACK. ARE WE TAKING THE ROAD AGAIN, OR IS THIS A VISIT?" : "THERE YOU ARE. I'VE BEEN RESTLESS SINCE YOU LEFT ME HERE.");
      dlg.opts.push_back({"COME WITH ME AGAIN.", P_REJOIN, mi});
    } else if (m.state == State::Left) {
      const bool rebuffed = (m.talkBits & party::voice::TB_REBUFF) && m.lastTalkDay >= 0 && m.lastTalkDay < day && m.leftDay < day;
      speech::propose(dlg, speech::P_STORY, Topic::Companion,
                      rebuffed ? "YOU CAME BACK. I DIDN'T THINK YOU WOULD." : "YOU'VE GOT A NERVE, COMING HERE AFTER WHAT YOU DID.");
      dlg.opts.push_back({"I WAS WRONG. I'M SORRY.", P_AMENDS, mi});
    }
    return;
  }
  const life::Resident* r = residentOf(*this, a);
  const ew::Gid siteId = a.site >= 0 && a.site < (int)world.sites.size() ? world.sites[(size_t)a.site].id : 0;
  // ---- the kin of a companion on the road: look after them (not every day)
  if (r && siteId)
    for (size_t k = 0; k < party.members.size(); k++) {
      const Member& m = party.members[k];
      if (m.site != siteId || (m.state != State::With && m.state != State::Downed)) continue;
      const std::string w = kinWordFor(*this, m, r->idx);
      if (w.empty() || w == "KIN") continue;
      const std::string nm = party::voice::given(m.name);
      std::string line;
      if (w == "SON" || w == "DAUGHTER")
        line = "YOU'RE THE ONE MY " + w + " " + nm + " TRAVELS WITH. LOOK AFTER " + himHer(m.female) + ". I DIDN'T RAISE " + himHer(m.female) +
               " TO DIE IN A DITCH.";
      else if (w == "WIFE" || w == "HUSBAND")
        line = "SO YOU'RE THE ONE MY " + w + " " + nm + " WENT ON THE ROAD WITH. I LIE AWAKE EVERY NIGHT. BRING " + himHer(m.female) + " HOME TO ME.";
      else if (w == "MOTHER" || w == "FATHER")
        line = "MY " + w + " " + nm + " WENT OFF WITH YOU. TELL " + himHer(m.female) + " WE'RE ALL RIGHT, AND THAT WE MISS " + himHer(m.female) + ".";
      else
        line = "MY " + w + " " + nm + " IS OUT THERE WITH YOU, ISN'T " + heShe(m.female) + "? KEEP " + himHer(m.female) + " SAFE FOR ME.";
      if (talkMem.saidRecently(speech::lineHash(line), day, 2)) continue;
      if (speech::propose(dlg, speech::P_NEED, Topic::Companion, line)) dlg.opts.push_back({"I'LL KEEP " + himHer(m.female) + " SAFE.", P_KIN_PROMISE, (int)k});
      break;
    }
  // ---- the fallen remembered: their kin and friends, and the innkeeper of their home town (not every conversation)
  if (siteId)
    for (size_t fi = 0; fi < party.fallen.size(); fi++) {
      const party::Fallen& f = party.fallen[fi];
      if (f.site != siteId || day - f.day > 400) continue;
      const std::string nm = party::voice::given(f.name);
      bool fem = f.female;
      int fidx = -1;
      if (const life::Census* c = life.find(siteId))
        for (const life::Resident& o : c->res) if (life::npcId(siteId, o.idx) == f.npc) { fidx = o.idx; fem = o.female; }
      std::string line;
      int answer = -1;
      if (r && fidx >= 0) {
        const life::Census* c = life.find(siteId);
        const life::Resident& fr = c->res[(size_t)fidx];
        std::string w;
        if (fr.spouse == (int)r->idx || r->spouse == fidx) w = fem ? "WIFE" : "HUSBAND";
        else if (fr.parent == (int)r->idx) w = fem ? "DAUGHTER" : "SON";
        else if (r->parent == fidx) w = fem ? "MOTHER" : "FATHER";
        else if (fr.parent >= 0 && fr.parent == r->parent && (int)r->idx != fidx) w = fem ? "SISTER" : "BROTHER";
        else for (int fr2 : life.friendsOf(*c, r->idx)) if (fr2 == fidx) w = "FRIEND";
        if (w == "FRIEND") {
          line = "MY FRIEND " + nm + " WALKED OUT WITH YOU AND NEVER CAME BACK. I STILL KEEP " + hisHer(fem) + " SEAT BY THE FIRE.";
          answer = 2;
        } else if (!w.empty()) {
          line = "MY " + w + " " + nm + " DIED ON THE ROAD WITH YOU. THEY SAY " + heShe(fem) + " DIED WELL. I NEED TO HEAR IT FROM YOU.";
          answer = 0;
        }
      }
      if (line.empty() && a.role == Role::Innkeeper) {
        line = "WE STILL KEEP A CUP ON THE SHELF FOR " + nm + ", WHO DRANK HERE EVERY NIGHT BEFORE GOING ON THE ROAD WITH YOU. NOBODY DRINKS FROM IT.";
        answer = 2;
      }
      // (the first days after the death it is all they speak of; later now and then, never the same line within days)
      const bool fresh = day - f.day <= 3;
      if (line.empty() || talkMem.saidRecently(speech::lineHash(line), day, fresh ? 1 : 4) || (!fresh && (mixh((uint32_t)key, (uint32_t)day) % 3) == 1)) continue;
      if (speech::propose(dlg, speech::P_STORY + 5, Topic::Companion, line)) {
        if (answer == 0) {
          dlg.opts.push_back({heShe(fem) + " DIED BRAVELY, FOR ALL OF US.", P_FALLEN_KIN, (int)fi * 4 + 0});
          dlg.opts.push_back({"I'M SORRY. I COULDN'T SAVE " + himHer(fem) + ".", P_FALLEN_KIN, (int)fi * 4 + 1});
        } else {
          dlg.opts.push_back({"I MISS " + himHer(fem) + " TOO.", P_FALLEN_KIN, (int)fi * 4 + 2});
        }
      }
      break;
    }
  // ---- would they come along?
  const PartyOffer o = partyOffer(a);
  if (o.origin == party::Origin::Mercenary && (o.ok || o.why == "BROKE" || o.why == "FULL" || o.why == "RECORDS")) {
    // a hireling says what they are before anyone asks (their own trade's line, over a greeting)
    static const char* const k[] = {"YOU LOOK LIKE YOU COULD USE ANOTHER SWORD. MINE IS FOR HIRE, IF THE PRICE IS RIGHT.",
                                    "LOOKING FOR A BLADE TO WALK BESIDE YOU? I'M CHEAPER THAN A FUNERAL.",
                                    "I'VE FOUGHT FOR WORSE PEOPLE THAN YOU, I'D WAGER. IF YOU NEED A HAND ON THE ROAD, I'M FOR HIRE."};
    speech::propose(dlg, speech::P_BARK, Topic::Companion, k[mixh((uint32_t)key, (uint32_t)(day / 2)) % 3]);
    dlg.opts.push_back({"ARE YOU FOR HIRE?", P_HIRE, o.wage});
    return;
  }
  if (o.ok) {
    dlg.opts.push_back({"WOULD YOU TRAVEL WITH ME?", P_JOIN, 0});
    // a regular at the inn after dark says what keeps them there (their own words, before anyone asks)
    if (o.origin == party::Origin::Tavern && isNight() && (mixh((uint32_t)key, (uint32_t)(day / 3)) % 2) == 0) {
      static const char* const k[] = {"ANOTHER NIGHT IN THIS INN. SOMETIMES I THINK I SHOULD JUST WALK OUT THAT DOOR AND KEEP GOING.",
                                      "SAME BENCH, SAME ALE, SAME FACES. I'M SICK OF ALL OF IT, IF I'M HONEST.",
                                      "YOU'VE GOT THE ROAD ON YOUR BOOTS. I ENVY YOU THAT, YOU KNOW."};
      speech::propose(dlg, speech::P_BARK, Topic::Companion, k[mixh((uint32_t)key, 0x1Bu) % 3]);
    }
    return;
  }
  // a friend who cannot come now says why, when asked (a friend would come, but the company is full)
  if ((r && (r->flags & life::RF_BEFRIENDED) && o.why == "FULL") || o.why == "RECORDS") dlg.opts.push_back({"WOULD YOU TRAVEL WITH ME?", P_JOIN, 0});
}

// ================================================================ the grave
void Game::partyGraveTalk(int fi) {
  if (fi < 0 || fi >= (int)party.fallen.size()) return;
  const party::Fallen& f = party.fallen[(size_t)fi];
  dlg = Dialogue();
  dlg.actor = -1;
  dlg.speaker = "THE GRAVE OF " + f.name;
  std::string t = party::epitaph(*this, f);
  for (size_t k = t.find(" / "); k != std::string::npos; k = t.find(" / ")) t.replace(k, 3, "\n");
  dlg.text = t;
  if (!(f.graveFlags & party::GF_FLOWERS)) dlg.opts.push_back({"LAY FLOWERS ON THE GRAVE.", P_GRAVE_FLOWERS, fi});
  dlg.opts.push_back({"SAY A FEW WORDS.", P_GRAVE_WORDS, fi});
  if (!(f.graveFlags & party::GF_KEEPSAKE) && !f.belongings.empty()) dlg.opts.push_back({"TAKE SOMETHING TO REMEMBER THEM BY.", P_GRAVE_KEEP, fi});
  dlg.opts.push_back({"LEAVE THEM IN PEACE.", A_BYE, 0});
  dlg.front = dlg.text;
  partyGraveAct(fi, 3);
  mode = Mode::Dialogue;
}

// ================================================================ the choices
bool Game::partyChoose(const DlgOpt& o) {
  const int ai = findActor(dlg.actor);
  auto member = [&](int mi) -> Member* { return mi >= 0 && mi < (int)party.members.size() ? &party.members[(size_t)mi] : nullptr; };
  auto backTo = [&](int mi) { dlg.opts.push_back({"SOMETHING ELSE.", P_BACK, mi}); };
  if (o.action >= P_CMD && o.action < P_CMD + (int)party::Cmd::COUNT) {
    const party::Cmd c = (party::Cmd)(o.action - P_CMD);
    Member* m = member(o.arg);
    if (!m) return false;
    if (!partyCommand(o.arg, c)) {
      dlg.text = c == party::Cmd::Follow && party.people() >= party::capacity(plLevel) ? "YOU'VE COMPANY ENOUGH ALREADY. I'D ONLY BE IN THE WAY."
                                                                                       : "NOT NOW. LOOK AROUND YOU.";
      dlg.opts = {{"FAIR ENOUGH.", A_BYE, 0}};
      return true;
    }
    const uint32_t h = mixh(m->id, (uint32_t)day + (uint32_t)c);
    switch (c) {
      case party::Cmd::Follow: dlg.text = h % 2 ? "RIGHT BEHIND YOU. I'LL FOLLOW WHEREVER YOU GO." : "I'LL FOLLOW. LEAD ON, AND I'LL KEEP UP."; break;
      case party::Cmd::Wait: dlg.text = h % 2 ? "I'LL WAIT HERE. DON'T BE LONG." : "I'LL BE HERE WHEN YOU COME BACK. COME BACK."; break;
      case party::Cmd::Attack: dlg.text = "THEN I'LL GO WHERE YOU POINT."; break;
      case party::Cmd::Defend: dlg.text = h % 2 ? "I WON'T LEAVE YOUR SIDE." : "ANYTHING THAT WANTS YOU GOES THROUGH ME FIRST."; break;
      case party::Cmd::Passive: dlg.text = h % 2 ? "I'LL HOLD BACK AND KEEP OUT OF THE FIGHTING UNLESS YOU NEED ME." : "I'LL HOLD BACK. SHOUT IF THE FIGHT GOES BADLY."; break;
      case party::Cmd::Home:
        dlg.text = home.plots.empty() ? "HOME, THEN. COME AND FIND ME WHEN THE ROAD CALLS." : "YOUR HOUSE, THEN. I'LL KEEP THE FIRE IN UNTIL YOU'RE BACK.";
        break;
      default: dlg.text = "AS YOU SAY."; break;
    }
    dlg.opts = {{"GOOD.", A_BYE, 0}};
    return true;
  }
  switch (o.action) {
    // ---- recruiting
    case P_JOIN: {
      if (ai < 0) return false;
      Actor& who = actors[(size_t)ai];
      const PartyOffer off = partyOffer(who);
      const life::Resident* r = residentOf(*this, who);
      if (!off.ok) {
        const std::string& w = off.why;
        if (w == "FULL") dlg.text = "I'D COME, BUT YOU'VE COMPANY ENOUGH ALREADY. ASK ME AGAIN WHEN THERE'S ROOM FOR ONE MORE.";
        else if (w == "RECORDS") dlg.text = "I'D COME, BUT TOO MANY FRIENDS ARE WAITING ON YOU ALREADY. PART WAYS WITH ONE OF THEM, THEN ASK ME AGAIN.";
        else if (w == "OLD") dlg.text = "ME? ON THE ROAD? MY KNEES WOULD GIVE OUT BEFORE THE FIRST HILL. THANK YOU FOR ASKING, THOUGH.";
        else if (w == "CHILD") dlg.text = "MY MOTHER WOULD SKIN ME. ASK ME AGAIN WHEN I'M GROWN.";
        else if (w == "DUTY") dlg.text = "I CAN'T. THIS PLACE NEEDS ME, AND I GAVE MY WORD TO STAY.";
        else if (w == "BROKE" || w == "GOLD" || w == "WAGE") dlg.text = "NOT FOR NOTHING, I WON'T. " + std::to_string(off.wage) + " GOLD UP FRONT, OR FIND YOURSELF ANOTHER SWORD.";
        else if (w == "KEY") dlg.text = "I CAN'T. TOO MANY PEOPLE HERE DEPEND ON ME, AND I'D NEVER FORGIVE MYSELF.";
        else if (w == "MEMBER") dlg.text = "WITH YOU AGAIN? YOU ONLY HAVE TO ASK ME TO COME ALONG.";
        else if (w == "ANGRY") dlg.text = "WITH YOU? AFTER WHAT YOU DID? YOU OWE ME AN APOLOGY FIRST.";
        else if (w == "HIRED") dlg.text = "YOU'VE ALREADY GOT MY BLADE, FRIEND. ONE OF ME IS ALL YOU'RE PAYING FOR.";
        else if (w == "DEAD") dlg.text = "NO. I CAN'T, NOT ANY MORE.";
        else if (w == "SLIGHTED" || w == "HATE" || w == "CRIME") dlg.text = "WITH YOU? AFTER WHAT YOU DID HERE? NO.";
        else dlg.text = "TRAVEL? NO. I'VE A LIFE HERE, SMALL AS IT IS, AND I MEAN TO KEEP IT.";
        dlg.opts = {{"I UNDERSTAND.", A_BYE, 0}};
        return true;
      }
      // their pitch in their own words: the life they would leave, and why the road calls
      Member tmp;
      tmp.name = who.name;
      tmp.npc = r && who.site >= 0 ? life::npcId(world.sites[(size_t)who.site].id, r->idx) : 0;
      tmp.site = who.site >= 0 ? world.sites[(size_t)who.site].id : 0;
      tmp.res = r ? (int16_t)r->idx : (int16_t)-1;
      tmp.job = r ? (uint8_t)r->job : 0;
      tmp.female = r ? r->female : false;
      tmp.culture = who.site >= 0 ? world.sites[(size_t)who.site].culture : 0;
      if (r && who.site >= 0) tmp.stake = party::stakeFor(*this, who.site, r->idx);   // (FELLOWS: their real stake)
      const Bio b = party::voice::bio(*this, tmp);
      std::string pitch = off.origin == party::Origin::Resident ? "TRAVEL WITH YOU? YOU'VE BEEN A GOOD FRIEND TO ME. YES, I'LL COME, IF YOU'LL HAVE ME."
                          : off.origin == party::Origin::Rescued ? "TRAVEL WITH YOU? YOU CAME INTO THE DARK FOR ME WHEN NOBODY ELSE WOULD. THE LEAST I CAN DO IS WALK BESIDE YOU."
                                                                 : "TRAVEL WITH YOU? I'VE BEEN SITTING IN THIS INN NIGHT AFTER NIGHT WAITING FOR SOMEONE TO ASK ME THAT.";
      if (!talkMem.introduced(npcKey(who))) {
        pitch = "I'M " + party::voice::given(who.name) + ". " + pitch;
        talkMem.flag(npcKey(who), speech::TM_INTRODUCED, day);
      }
      // why the road calls: their stake, as they would say it (the census's own facts behind it)
      std::string why = tmp.stake.hook != party::Hook::None ? party::voice::stakeLine(*this, tmp, b) : std::string();
      if (why.empty()) why = party::voice::tradeLine(b);
      if (why.empty()) why = "THERE'S NOTHING HERE FOR ME BUT THE SAME FOUR WALLS.";
      dlg.text = also(pitch, why);
      dlg.opts = {{"THEN COME WITH ME.", P_JOIN_YES, (int)off.origin}, {"TELL ME MORE ABOUT YOURSELF FIRST.", P_JOIN_ABOUT, (int)off.origin},
                  {"ANOTHER TIME, PERHAPS.", A_BYE, 0}};
      return true;
    }
    case P_JOIN_ABOUT: {
      if (ai < 0) return false;
      const Actor& who = actors[(size_t)ai];
      const life::Resident* r = residentOf(*this, who);
      Member tmp;
      tmp.npc = 1;
      tmp.site = who.site >= 0 ? world.sites[(size_t)who.site].id : 0;
      tmp.res = r ? (int16_t)r->idx : (int16_t)-1;
      tmp.job = r ? (uint8_t)r->job : 0;
      tmp.female = r ? r->female : false;
      tmp.culture = who.site >= 0 ? world.sites[(size_t)who.site].culture : 0;
      const Bio b = party::voice::bio(*this, tmp);
      std::string t = party::voice::tradeLine(b);
      t = also(t.empty() ? std::string("THERE'S NOT MUCH TO TELL. I'VE LIVED HERE ALL MY LIFE.") : t, party::voice::kinLine(b));
      const party::Vocation v = party::vocationFor(tmp.job, r ? r->traits : 0, r ? r->lookSeed : 0);
      static const char* const can[] = {"I CAN SWING A SWORD AND HOLD A SHIELD.", "I'M HANDY WITH A BOW.", "I KNOW A LITTLE OF THE OLD ARTS.",
                                        "I CAN BIND A WOUND AND SET A BONE.", "I'M QUICK, AND QUIET WHEN I NEED TO BE.", ""};
      t = also(t, can[std::min((int)v, 5)]);
      dlg.text = t;
      dlg.opts = {{"THEN COME WITH ME.", P_JOIN_YES, o.arg}, {"ANOTHER TIME, PERHAPS.", A_BYE, 0}};
      return true;
    }
    case P_JOIN_YES: {
      if (ai < 0) return false;
      const int mi = partyRecruit(actors[(size_t)ai], (party::Origin)o.arg);
      if (mi < 0) { dlg.text = "ON SECOND THOUGHT, I CAN'T LEAVE. FORGIVE ME."; dlg.opts = {{"FAREWELL.", A_BYE, 0}}; return true; }
      Member& m = party.members[(size_t)mi];
      dlg.speaker = m.name;
      m.talkBits |= party::voice::TB_INTRO;
      const Bio b = party::voice::bio(*this, m);
      const Kin* k = nullptr;
      for (const Kin& x : b.household) if (!x.dead && !x.away) { k = &x; break; }
      dlg.text = k ? "THEN I'LL COME. LET ME TELL MY " + k->word + " " + k->name + ", AND FETCH MY PACK. YOU LEAD THE WAY, AND I'LL KEEP UP."
                   : std::string("THEN I'LL COME. LET ME FETCH MY PACK. LEAD THE WAY, AND I'LL KEEP UP.");
      dlg.opts = {{"LET'S GO.", A_BYE, 0}};
      return true;
    }
    case P_HIRE: {
      if (ai < 0) return false;
      dlg.text = party::voice::mercTerms(*this, actors[(size_t)ai].name, o.arg);
      const int ask = std::max(party::WAGE_MIN, o.arg * 4 / 5);
      dlg.opts = {{"AGREED. YOU'RE HIRED.", P_HIRE_YES, o.arg}, {std::to_string(ask) + " GOLD A DAY, AND NOT A COIN MORE.", P_HAGGLE, ask},
                  {"TOO RICH FOR ME.", A_BYE, 0}};
      return true;
    }
    case P_HAGGLE: {
      if (ai < 0) return false;
      const bool yes = mixh((uint32_t)npcKey(actors[(size_t)ai]), (uint32_t)day) % 2 == 0;
      if (yes) {
        dlg.text = "HA. YOU DRIVE A HARD BARGAIN. " + std::to_string(o.arg) + " A DAY, THEN, BUT I EAT FIRST AT EVERY FIRE.";
        dlg.opts = {{"DEAL.", P_HIRE_YES, o.arg}, {"ON SECOND THOUGHT, NO.", A_BYE, 0}};
      } else {
        dlg.text = "THE PRICE IS THE PRICE, NOT A COIN LESS A DAY. YOU WANT CHEAP STEEL, GO AND HIRE A FARMER.";
        dlg.opts = {{"FINE. THE FULL WAGE.", P_HIRE_YES, 0}, {"THEN NO.", A_BYE, 0}};
      }
      return true;
    }
    case P_HIRE_YES: {
      if (ai < 0) return false;
      const PartyOffer off = partyOffer(actors[(size_t)ai]);
      const int mi = partyRecruit(actors[(size_t)ai], party::Origin::Mercenary);
      if (mi < 0) {
        dlg.text = off.why == "BROKE" ? "YOUR PURSE IS LIGHTER THAN YOUR WORD. COME BACK WHEN YOU CAN PAY THE FIRST DAY."
                   : off.why == "RECORDS" ? "YOU'VE MORE PEOPLE WAITING ON YOU THAN YOU CAN FEED. LET ONE OF THEM GO, THEN COME BACK TO ME."
                                          : "YOUR COMPANY'S FULL. I DON'T FIGHT SHOULDER TO SHOULDER IN A CROWD.";
        dlg.opts = {{"FAIR ENOUGH.", A_BYE, 0}};
        return true;
      }
      Member& m = party.members[(size_t)mi];
      if (o.arg > 0 && m.wage > o.arg) { gold += m.wage - o.arg; m.wage = o.arg; }   // (the haggled wage, from the first day)
      dlg.speaker = m.name;
      dlg.text = "DONE. PAY ME EACH MORNING AND POINT ME AT WHATEVER NEEDS KILLING.";
      dlg.opts = {{"LET'S GO.", A_BYE, 0}};
      return true;
    }
    // ---- members at home, and the ones who left
    case P_REJOIN: {
      Member* m = member(o.arg);
      if (!m) return false;
      if (!partyCommand(o.arg, party::Cmd::Follow)) {
        dlg.text = party.people() >= party::capacity(plLevel) ? "YOU'VE COMPANY ENOUGH ALREADY. COME BACK FOR ME WHEN THERE'S ROOM."
                                                             : "NOT YET. I'M NEEDED HERE A WHILE LONGER.";
        dlg.opts = {{"I UNDERSTAND.", A_BYE, 0}};
        return true;
      }
      dlg.text = m->approval >= party::APPROVAL_QUEST ? "I WAS HOPING YOU'D COME FOR ME. MY PACK HAS BEEN READY FOR DAYS."
                                                     : "I WONDERED WHEN YOU'D COME BACK FOR ME. I'M READY.";
      dlg.opts = {{"LET'S GO.", A_BYE, 0}};
      return true;
    }
    case P_AMENDS: {
      Member* m = member(o.arg);
      if (!m) return false;
      const bool another = (m->talkBits & party::voice::TB_REBUFF) && m->lastTalkDay >= 0 && m->lastTalkDay < day && m->leftDay < day;
      if (!another) {
        // the deed that drove them away (their worst memory)
        const party::Memory* worst = nullptr;
        for (const party::Memory& x : m->mem) if (!worst || x.delta < worst->delta) worst = &x;
        std::string what = worst ? deedPhrase(worst->deed, siteNameOf(*this, worst->about)) : std::string("EVERYTHING YOU DID");
        if (what.size() > 40) what = deedPhrase(worst->deed, "");
        dlg.text = "SORRY? I KEEP SEEING " + what + ". ONE WORD DOESN'T MEND THAT. COME BACK ANOTHER DAY, WHEN I'VE HAD TIME TO THINK.";
        m->talkBits |= party::voice::TB_REBUFF;
        m->lastTalkDay = day;
        dlg.opts = {{"I'LL COME BACK.", A_BYE, 0}};
        return true;
      }
      dlg.text = "YOU CAME BACK, AS YOU SAID YOU WOULD. ALL RIGHT. SAY IT, THEN, AND SAY IT PROPERLY THIS TIME.";
      dlg.opts = {{"I WAS WRONG, AND I'LL DO BETTER.", P_AMENDS_YES, o.arg}, {"I NEED YOU AT MY SIDE. I MEAN IT.", P_AMENDS_YES, o.arg},
                  {"FORGET IT.", A_BYE, 0}};
      return true;
    }
    case P_AMENDS_YES: {
      Member* m = member(o.arg);
      if (!m) return false;
      m->state = State::Home;
      m->approval = (int8_t)std::max<int>(m->approval, party::APPROVAL_COMPLAIN + 10);
      m->talkBits = (m->talkBits & ~party::voice::TB_REBUFF) | party::voice::TB_AMENDS;
      m->flags = (uint16_t)(m->flags & ~party::MF_COMPLAINED);
      if (!partyCommand(o.arg, party::Cmd::Follow)) {
        dlg.text = "ALL RIGHT. I FORGIVE YOU. BUT YOU'VE COMPANY ENOUGH FOR NOW. COME BACK FOR ME WHEN THERE'S ROOM.";
        dlg.opts = {{"I WILL.", A_BYE, 0}};
        return true;
      }
      dlg.text = "ALL RIGHT. ONE MORE CHANCE. I'LL GET MY THINGS. DON'T MAKE ME REGRET IT.";
      dlg.opts = {{"YOU WON'T.", A_BYE, 0}};
      return true;
    }
    // ---- the kin of companions, and of the fallen
    case P_KIN_PROMISE: {
      const Member* m = member(o.arg);
      const bool f = m && m->female;
      dlg.text = "SEE THAT YOU DO. AND TELL " + himHer(f) + " TO SEND WORD NOW AND THEN. A LETTER WOULDN'T KILL " + himHer(f) + ".";
      dlg.opts = {{"I'LL TELL " + himHer(f) + ".", A_BYE, 0}};
      return true;
    }
    case P_FALLEN_KIN: {
      const int fi = o.arg / 4, ans = o.arg % 4;
      if (fi < 0 || fi >= (int)party.fallen.size()) return false;
      const party::Fallen& f = party.fallen[(size_t)fi];
      const bool fem = f.female;   // (fixer M8 r3) saved with the grave: right for the rescued, the hired and the hound too
      if (ans == 0) dlg.text = "BRAVELY. FOR ALL OF YOU. ...THANK YOU. THAT'S WHAT I'LL TELL THE OTHERS. THAT'S WHAT I'LL TELL MYSELF.";
      else if (ans == 1) dlg.text = "NO. NOBODY COULD HAVE. I KNOW THAT. I KNEW IT THE DAY " + heShe(fem) + " LEFT. BUT THANK YOU FOR SAYING IT.";
      else dlg.text = "THEN DRINK TO " + himHer(fem) + " WITH ME SOMETIME. I MISS " + himHer(fem) + " TOO, EVERY DAY.";
      dlg.opts = {{"I WILL.", A_BYE, 0}};
      return true;
    }
    // ---- a companion's pages
    case P_BACK: {
      if (!member(o.arg)) return false;
      PartyTalkOps::front(*this, o.arg, false);
      return true;
    }
    case P_ORDERS: {
      if (!member(o.arg)) return false;
      dlg.text = "WHAT DO YOU WANT ME TO DO?";
      PartyTalkOps::orders(*this, o.arg);
      return true;
    }
    case P_ABOUT: {
      Member* m = member(o.arg);
      if (!m) return false;
      // (their trade and the ones they left; what drives them waits for the question that asks it)
      const Bio b0 = party::voice::bio(*this, *m);
      std::string t = m->npc && m->res >= 0 ? also(party::voice::tradeLine(b0), party::voice::kinLine(b0)) : party::aboutLine(*this, *m);
      if (m->npc && m->res >= 0 && m->stake.hook != party::Hook::None) t = also(t, "BUT THAT'S NOT WHY I'M ON THE ROAD.");
      if (t.empty()) t = "THERE'S NOT MUCH TO TELL. I'VE WALKED A LOT OF ROADS, AND THIS ONE IS AS GOOD AS ANY.";
      if (m->talkBits & party::voice::TB_ABOUT) t = also("YOU'VE HEARD SOME OF IT.", t);
      m->talkBits |= party::voice::TB_ABOUT;
      dlg.text = t;
      dlg.opts.clear();
      const Bio b = party::voice::bio(*this, *m);
      if (!b.home.empty()) dlg.opts.push_back({"TELL ME ABOUT " + b.home + ".", P_ABOUT_HOME, o.arg});
      if (!b.household.empty() || !b.friendOf.name.empty()) dlg.opts.push_back({"WHO DID YOU LEAVE BEHIND?", P_ABOUT_KIN, o.arg});
      if (m->stake.hook != party::Hook::None) dlg.opts.push_back({"WHAT REALLY DRIVES YOU?", P_ABOUT_STAKE, o.arg});
      dlg.opts.push_back({"WHERE DID YOU LEARN TO FIGHT?", P_ABOUT_FIGHT, o.arg});
      backTo(o.arg);
      return true;
    }
    case P_ABOUT_HOME: {
      Member* m = member(o.arg);
      if (!m) return false;
      const Bio b = party::voice::bio(*this, *m);
      m->talkBits |= party::voice::TB_HOME;
      const char* kind = b.homeType == (int)SiteType::City ? "A CITY" : b.homeType == (int)SiteType::Town ? "A MARKET TOWN" : "A VILLAGE";
      std::string t = b.home + " IS " + kind + (b.people.empty() ? std::string() : " OF THE " + b.people) + ". WE BUILD " +
                      party::voice::houseWords(b.arch) + ", WE DRINK " + party::voice::drinkWord(b.drink) + ", AND EVERYONE KNOWS EVERYONE ELSE'S BUSINESS.";
      if (!speech::fitsPanel(t)) t = b.home + " IS " + kind + ". EVERYONE THERE KNOWS EVERYONE ELSE'S BUSINESS.";
      if (!b.god.empty()) t = also(t, "THE SHRINE IS TO " + b.god + ", AND MY MOTHER NEVER MISSED A FEAST DAY.");
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_ABOUT_KIN: {
      Member* m = member(o.arg);
      if (!m) return false;
      const Bio b = party::voice::bio(*this, *m);
      m->talkBits |= party::voice::TB_KIN;
      std::string t;
      int said = 0;
      for (const Kin& k : b.household) {
        if (said >= 2) break;
        std::string s;
        if (k.dead) s = "MY " + k.word + " " + k.name + " IS DEAD, AND I MISS " + himHer(k.female) + " EVERY DAY.";
        else if (k.away) s = "MY " + k.word + " " + k.name + " WENT AWAY, AND NOBODY KNOWS WHERE.";
        else if (said == 0) s = "MY " + k.word + " " + k.name + " IS STILL AT HOME, AND WORRIES ABOUT ME MORE THAN " + heShe(k.female) + " LETS ON.";
        else s = "AND MY " + k.word + " " + k.name + ", WHO NEVER WANTED ME TO GO.";
        const std::string next = t.empty() ? s : t + " " + s;
        if (!speech::fitsPanel(next)) break;
        t = next;
        said++;
      }
      if (!b.friendOf.name.empty())
        t = t.empty() ? "MY FRIEND " + b.friendOf.name + ", MOSTLY. WE GREW UP TOGETHER, AND " + heShe(b.friendOf.female) + " STILL OWES ME A DRINK."
                      : also(t, "AND MY FRIEND " + b.friendOf.name + ", WHO OWES ME A DRINK.");
      if (t.empty()) t = "NOBODY, REALLY. THAT'S PART OF WHY IT WAS EASY TO GO.";
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_ABOUT_STAKE: {
      Member* m = member(o.arg);
      if (!m) return false;
      const Bio b = party::voice::bio(*this, *m);
      m->talkBits |= party::voice::TB_STAKE;
      std::string t = party::voice::stakeLine(*this, *m, b);
      const int qs = party::voice::questStatus(*this, *m);
      if (qs == 3) t = also(t, "BUT YOU HELPED ME PUT THAT TO REST. I DON'T THINK ABOUT IT THE WAY I USED TO.");
      else if (qs == 2) t = also(t, "AND NOW WE'RE FINALLY DOING SOMETHING ABOUT IT.");
      else t = also(t, "I MAY ASK FOR YOUR HELP WITH THAT, WHEN THE TIME COMES. NOT YET.");
      if (t.empty()) t = "WHAT DRIVES ME? THE ROAD ITSELF, I THINK. I NEVER COULD SIT STILL.";
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_ABOUT_FIGHT: {
      Member* m = member(o.arg);
      if (!m) return false;
      const Bio b = party::voice::bio(*this, *m);
      switch (m->voc) {
        case party::Vocation::Warrior: dlg.text = "FROM MY FATHER, WITH A STICK, IN THE YARD BEHIND THE HOUSE. LATER WITH STEEL. HE NEVER ONCE LET ME WIN."; break;
        case party::Vocation::Archer: dlg.text = "HUNTING, MOSTLY. YOU LEARN TO BE PATIENT WHEN SUPPER DEPENDS ON ONE ARROW."; break;
        case party::Vocation::Mage:
          dlg.text = b.people.empty() ? std::string("FROM AN OLD WOMAN WHO LIVED ALONE. SHE TAUGHT ME THE FIRE, AND MADE ME PROMISE TO RESPECT IT.")
                                      : "FROM AN OLD " + b.people + " WOMAN WHO LIVED ALONE. SHE TAUGHT ME THE FIRE, AND MADE ME PROMISE TO RESPECT IT.";
          break;
        case party::Vocation::Healer: dlg.text = "I DIDN'T. I LEARNED TO PUT PEOPLE BACK TOGETHER. THE FIGHTING CAME LATER, BECAUSE SOMEONE HAD TO GUARD THE HEALER."; break;
        case party::Vocation::Rogue: dlg.text = "ON THE STREETS. WHERE I GREW UP YOU LEARNED TO BE QUICK OR YOU LEARNED TO BE HUNGRY."; break;
        default: dlg.text = "THE SAME WAY EVERYONE DOES. BADLY AT FIRST."; break;
      }
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_DEEDS: {
      // (arg: members index, + 1000 when asked again from one of the memories)
      const bool again = o.arg >= 1000;
      const int mi = again ? o.arg - 1000 : o.arg;
      Member* m = member(mi);
      if (!m) return false;
      dlg.text = again ? std::string("WHAT ELSE? LET ME THINK.") : std::string("WHICH PART? WE'VE DONE A GOOD DEAL TOGETHER, YOU AND I.");
      dlg.opts.clear();
      std::vector<int> seen;
      for (int i = (int)m->mem.size() - 1; i >= 0 && dlg.opts.size() < 4; i--) {
        const party::Memory& x = m->mem[(size_t)i];
        if (x.delta == 0 || std::find(seen.begin(), seen.end(), (int)x.deed) != seen.end()) continue;
        seen.push_back((int)x.deed);
        std::string lab = "WHAT ABOUT " + deedPhrase(x.deed, siteNameOf(*this, x.about)) + "?";
        if (!speech::fitsOption(lab) || lab.size() > 52) lab = "WHAT ABOUT " + deedPhrase(x.deed, "") + "?";
        dlg.opts.push_back({lab, P_DEED, mi * 64 + i});
      }
      backTo(mi);
      return true;
    }
    case P_DEED: {
      Member* m = member(o.arg / 64);
      const int k = o.arg % 64;
      if (!m || k < 0 || k >= (int)m->mem.size()) return false;
      const party::Memory& x = m->mem[(size_t)k];
      const std::string when = party::voice::ago(day - x.day);
      // what it was, then what they made of it: the deed's own words when it moved them, else their nature's
      std::string t = opinion(*m, x, godOf(*this, m->culture));
      if (x.delta != 0) {
        const std::string own = party::deedLine(*this, *m, x.deed, x.delta > 0 ? std::max(4, (int)x.delta) : std::min(-4, (int)x.delta));
        if (!own.empty() && std::abs(x.delta) >= 4) t = also(own, std::abs(x.delta) >= 6 ? "I STILL THINK ABOUT IT." : "");
      }
      std::string lead = deedPhrase(x.deed, siteNameOf(*this, x.about));
      if (lead.size() > 40) lead = deedPhrase(x.deed, "");
      t = also(lead + "? " + t, "");
      if (!speech::fitsPanel(t)) t = opinion(*m, x, godOf(*this, m->culture));
      if (x.deed == party::Deed::Abandoned) t = "YOU LEFT ME BLEEDING ON THE GROUND, " + when + ". I'M STILL HERE. I HAVEN'T FORGOTTEN.";
      dlg.text = t;
      dlg.opts.clear();
      dlg.opts.push_back({"WHAT ELSE STAYED WITH YOU?", P_DEEDS, o.arg / 64 + 1000});
      backTo(o.arg / 64);
      return true;
    }
    case P_STATUS: {
      Member* m = member(o.arg);
      if (!m) return false;
      const int i = findActor(m->actor);
      const bool hurt = i > 0 && actors[(size_t)i].hp < actors[(size_t)i].maxHp * 0.5f;
      if (hurt) dlg.text = "I'VE BEEN BETTER. A LITTLE REST AND A POTION WOULDN'T GO AMISS, IF YOU'VE ONE TO SPARE.";
      else if (m->flags & party::MF_WOUNDED) dlg.text = "STILL STIFF FROM WHEN I WENT DOWN. A PROPER NIGHT'S SLEEP WILL SORT IT OUT.";
      else if (m->approval < party::APPROVAL_COMPLAIN) dlg.text = "HONESTLY? I DON'T LIKE THE WAY YOU DO THINGS. I'M STILL HERE, FOR NOW.";
      else if (m->bond >= 50) dlg.text = "WELL. BETTER THAN WELL. I'D FOLLOW YOU INTO ANY DARK THERE IS.";
      else if (m->days < 3) dlg.text = "STILL GETTING USED TO THE ROAD. MY FEET HAVE A LOT TO SAY ABOUT IT.";
      else dlg.text = "WELL ENOUGH. THE ROAD SUITS ME MORE THAN I THOUGHT IT WOULD.";
      dlg.opts.clear();
      dlg.opts.push_back({"WHAT DO YOU MAKE OF ME, HONESTLY?", P_FEEL, o.arg});
      if (m->voc != party::Vocation::Hound) dlg.opts.push_back({"HOW DOES YOUR GEAR SIT ON YOU?", P_GEAR, o.arg});
      backTo(o.arg);
      return true;
    }
    case P_FEEL: {
      Member* m = member(o.arg);
      if (!m) return false;
      const int a = m->approval, b = m->bond;
      std::string t;
      if (a >= 60) t = "HONESTLY? YOU'RE THE BEST PERSON I'VE EVER WALKED BESIDE. DON'T LET IT GO TO YOUR HEAD.";
      else if (a >= 25) t = "YOU'RE A GOOD SORT. YOU DO RIGHT MORE OFTEN THAN NOT, AND YOU DON'T WASTE MY TIME.";
      else if (a >= -10) t = "I'M STILL MAKING UP MY MIND ABOUT YOU. ASK ME AGAIN IN A MONTH.";
      else if (a >= party::APPROVAL_COMPLAIN) t = "I'LL BE HONEST, SOME OF WHAT YOU DO SITS BADLY WITH ME.";
      else t = "YOU REALLY WANT TO KNOW? I THINK YOU'RE TURNING INTO SOMEONE I WOULDN'T HAVE FOLLOWED.";
      if (b >= 80) t = also(t, "AND WHATEVER HAPPENS, I'D DIE FOR YOU. I THINK YOU KNOW THAT.");
      else if (b >= 50) t = also(t, "I TRUST YOU WITH MY LIFE. THAT'S NOT NOTHING.");
      else if (b >= 20) t = also(t, "WE'RE GETTING THE MEASURE OF EACH OTHER, YOU AND I.");
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_GEAR: {
      Member* m = member(o.arg);
      if (!m) return false;
      const Bio b = party::voice::bio(*this, *m);
      auto worn = [&](int s) -> const Item* { const int e = m->eq[(size_t)s]; return e >= 0 && e < (int)m->inv.size() ? &m->inv[(size_t)e] : nullptr; };
      const Item* arm = worn(party::S_ARMOR);
      const Item* wpn = worn(m->voc == party::Vocation::Archer ? party::S_BOW : m->voc == party::Vocation::Mage ? party::S_STAFF : party::S_WEAPON);
      const Item* it = wpn ? wpn : arm;
      const char* what = it == wpn ? (m->voc == party::Vocation::Archer ? "THIS BOW" : m->voc == party::Vocation::Mage ? "THIS STAFF" : "THIS BLADE")
                                   : "THIS ARMOUR";
      std::string t;
      if (!it) t = "WHAT GEAR? I'M WEARING MY SECOND-BEST SHIRT AND HOPE. IF YOU'VE ANYTHING BETTER, I'LL TAKE IT.";
      else if (!it->culture || (m->culture && cult::familyOf(it->culture) == cult::familyOf(m->culture)))
        t = std::string(what) + (b.people.empty() ? " IS MADE THE WAY WE MAKE THEM AT HOME." : " IS " + b.people + " WORK, THE WAY WE MAKE THEM AT HOME.") +
            " IT FITS MY HAND LIKE AN OLD FRIEND.";
      else {
        const std::string maker = world.src ? world.src->culture(it->culture).adjective : std::string();
        t = std::string(what) + (maker.empty() ? " IS FOREIGN WORK." : " IS " + maker + " WORK.") +
            " IT HANGS STRANGELY ON ME, BUT IT'S GOOD STEEL, AND I'M LEARNING ITS WAYS.";
      }
      if (!speech::fitsPanel(t)) t = std::string(what) + " WILL DO. I'D NOT SAY NO TO SOMETHING BETTER, MIND.";
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_GRIEVANCE: {
      Member* m = member(o.arg);
      if (!m) return false;
      const party::Memory* worst = nullptr;
      for (const party::Memory& x : m->mem) if (x.delta < 0 && (!worst || x.delta < worst->delta)) worst = &x;
      std::string t;
      if (worst) {
        std::string what = deedPhrase(worst->deed, siteNameOf(*this, worst->about));
        t = "WHAT'S WRONG? I'LL TELL YOU WHAT'S WRONG. " + what + ". " + party::voice::ago(day - worst->day) + ", AND I STILL CAN'T STOP THINKING ABOUT IT.";
        if (!speech::fitsPanel(t)) t = "WHAT'S WRONG? " + deedPhrase(worst->deed, "") + ". I STILL CAN'T STOP THINKING ABOUT IT.";
      } else {
        t = "WHAT'S WRONG IS THE WAY YOU DO THINGS. ALL OF IT. I DIDN'T COME ON THIS ROAD TO BE ASHAMED.";
      }
      dlg.text = t;
      dlg.opts = {{"I'LL DO BETTER. I PROMISE.", P_GRIEVE_SORRY, o.arg}, {"GO, THEN, IF YOU DON'T LIKE IT.", P_PART, o.arg}, {"SOMETHING ELSE.", P_BACK, o.arg}};
      return true;
    }
    case P_GRIEVE_SORRY: {
      Member* m = member(o.arg);
      if (!m) return false;
      if (m->lastTalkDay != day) {   // (one promise a day counts for a little)
        m->lastTalkDay = day;
        m->approval = (int8_t)std::min(100, m->approval + 4);
        dlg.text = "WORDS ARE CHEAP. BUT ALL RIGHT. I'LL WATCH WHAT YOU DO, NOT WHAT YOU SAY.";
      } else {
        dlg.text = "YOU SAID THAT ALREADY TODAY. SHOW ME INSTEAD.";
      }
      dlg.opts = {{"I WILL.", A_BYE, 0}};
      return true;
    }
    case P_QUEST: {
      Member* m = member(o.arg);
      if (!m) return false;
      const uint32_t inst = party::voice::questBegin(*this, o.arg);
      story::Instance* in = inst ? story.find(inst) : nullptr;
      if (!in) {
        dlg.text = "NO. IT'S NOTHING. FORGET I SAID ANYTHING.";
        dlg.opts = {{"ALL RIGHT.", A_BYE, 0}};
        return true;
      }
      story.sagaOffers_.clear();
      story.said_ = dlg.text + " ";
      dlg.opts = {{"NOT NOW. I HAVE ORDERS FOR YOU.", P_ORDERS, o.arg}};
      if (!story.showDialogue(*this, *in)) { dlg.text = "THEN LET'S BE ABOUT IT."; dlg.opts.clear(); }
      dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      return true;
    }
    case P_QUEST_NEWS: {
      Member* m = member(o.arg);
      if (!m) return false;
      story::Instance* in = m->questInst ? story.find(m->questInst) : nullptr;
      std::string t;
      if (in && !in->done) {
        const std::string step = story.nextStepLine(*this, *in, dlg.text, true);
        const std::string j = story.journalText(*this, *in);
        t = !step.empty() ? also("I THINK ABOUT IT ALL THE TIME.", step) : "WE'RE NOT DONE YET. IT'S ALL IN YOUR JOURNAL, AND IT'S ALL IN MY HEAD.";
        (void)j;
      } else {
        t = "IT'S OVER, ONE WAY OR ANOTHER. THANK YOU FOR WALKING IT WITH ME.";
      }
      dlg.text = t;
      dlg.opts.clear();
      backTo(o.arg);
      return true;
    }
    case P_PART: {
      Member* m = member(o.arg);
      if (!m) return false;
      if (m->origin == party::Origin::Mercenary) dlg.text = "PAID UP TO TODAY? THEN WE'RE SQUARE. NO HARD FEELINGS.";
      else if (m->bond >= 50) dlg.text = "YOU MEAN IT? AFTER EVERYTHING? ...THEN I'LL GO HOME. YOU KNOW WHERE TO FIND ME, IF YOU EVER CHANGE YOUR MIND.";
      else dlg.text = "YOU MEAN IT? THEN I'LL GO HOME. YOU KNOW WHERE TO FIND ME.";
      dlg.opts = {{"YES. GO HOME, AND GO WELL.", P_PART_YES, o.arg}, {"NO. STAY WITH ME.", P_BACK, o.arg}};
      return true;
    }
    case P_PART_YES: {
      Member* m = member(o.arg);
      if (!m) return false;
      const bool close = m->bond >= 50;
      partyDismiss(o.arg);
      dlg.text = close ? "THEN THIS IS GOODBYE. I'LL NEVER FORGET THE ROAD WE WALKED. LOOK AFTER YOURSELF." : "GOODBYE, THEN. TAKE CARE OF YOURSELF.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    // ---- the grave
    case P_GRAVE_FLOWERS:
      partyGraveAct(o.arg, 1);
      dlg.text = "YOU LAY THE FLOWERS DOWN AND STAND A WHILE IN SILENCE.";
      dlg.opts = {{"SAY A FEW WORDS.", P_GRAVE_WORDS, o.arg}, {"LEAVE THEM IN PEACE.", A_BYE, 0}};
      return true;
    case P_GRAVE_WORDS: {
      if (o.arg < 0 || o.arg >= (int)party.fallen.size()) return false;
      const party::Fallen& f = party.fallen[(size_t)o.arg];
      const std::string nm = party::voice::given(f.name);
      const std::string span = f.days < 7 ? "ONLY A FEW DAYS" : f.days < 30 ? "MANY DAYS" : f.days < 90 ? "A WHOLE SEASON" : "MORE THAN A YEAR";
      std::string t = "YOU SAY A FEW WORDS FOR " + nm + ": " + span + " ON THE ROAD TOGETHER";
      t += f.kills >= 10 ? ", A STEADY BLADE IN EVERY FIGHT" : ", A FRIEND AT EVERY FIRE";
      t += f.bond >= 50 ? ", AND A BOND THAT DID NOT BREAK." : ".";
      if (!f.cause.empty()) t = also(t, "IN THE END " + fellClause(f.cause, f.female) + ".");
      dlg.text = t;
      dlg.opts.clear();
      if (!(f.graveFlags & party::GF_FLOWERS)) dlg.opts.push_back({"LAY FLOWERS ON THE GRAVE.", P_GRAVE_FLOWERS, o.arg});
      dlg.opts.push_back({"LEAVE THEM IN PEACE.", A_BYE, 0});
      return true;
    }
    case P_GRAVE_KEEP: {
      const std::string what = o.arg >= 0 && o.arg < (int)party.fallen.size() && !party.fallen[(size_t)o.arg].belongings.empty()
                                   ? party.fallen[(size_t)o.arg].belongings.front().name : std::string("IT");
      if (!partyGraveAct(o.arg, 2)) { dlg.text = "THERE IS NOTHING MORE TO TAKE."; dlg.opts = {{"LEAVE THEM IN PEACE.", A_BYE, 0}}; return true; }
      dlg.text = "YOU TAKE THE " + what + " AND PROMISE TO CARRY IT WELL.";
      dlg.opts = {{"LEAVE THEM IN PEACE.", A_BYE, 0}};
      return true;
    }
    default: return false;
  }
}
