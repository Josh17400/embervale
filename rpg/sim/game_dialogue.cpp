// EMBERVALE RPG: greetings and the dialogue box: talking, the option list and its choices (SOLID 1.1).
// Public: Game::greeting, Game::speakerTitle, Game::talkTo, Game::talkToInner, Game::closeDialogue,
//   Game::dialogueChoose, Game::dialogueChooseInner
#include "rpg/sim/game_rpg_detail.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/foe_hooks.h"
#include "rpg/sim/craft_hooks.h"
#include "rpg/sim/war_ops.h"
#include "rpg/sim/wayside_ops.h"
#include "rpg/sim/quest_ops.h"

// (monsterPlural, dirWord, giverTown, rewardReadyMsg, fitLine, overworldTile, isGiver: rpg/sim/game_internal.h)

std::string Game::greeting(const Actor& a, int salt) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day * 31 + (int)hour) ^ (uint32_t)salt * 0x9E3779B9u);
  // where they live: their site, or for people indoors the site of the building they are in
  const int homeSite = a.site >= 0 ? a.site
                       : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  const std::string town = homeSite >= 0 ? world.sites[(size_t)homeSite].name : "THESE PARTS";
  // M1 kingdom identity: whose land this is, and whether the king holds court here
  const Kingdom* K = world.kingdomOf(homeSite);
  const std::string realm = K ? K->name : std::string();
  const bool capitalHere = homeSite >= 0 && world.sites[(size_t)homeSite].capital && K;
  // (fix) the realm's ruler and court by its society's titles (KHAN in THE GREAT TENT, DOGE in THE GUILDHALL...)
  cult::Society rsoc;
  if (homeSite >= 0)
    if (const cult::Culture* oc = world.cultureOfKingdom(world.sites[(size_t)homeSite].kingdom)) rsoc = cult::societyOf(*oc);
  const std::string ruler = rsoc.rulerTitle, court = rsoc.seatTitle;
  std::string seat;   // the kingdom's capital by name, when known
  if (K) { int ch = world.siteHandle(K->capitalId); if (ch >= 0) seat = world.sites[(size_t)ch].name; }
  // world-aware small talk, so the same handful of lines doesn't repeat in every village
  std::vector<std::string> local;
  {
    const Site* home = a.site >= 0 ? &world.sites[a.site] : nullptr;
    int hx = home ? home->ex : (int)(a.p.x / TILE), hy = home ? home->ey : (int)(a.p.y / TILE);
    int best = -1; float bd = 1e30f;
    for (int i = 0; i < (int)world.sites.size(); i++) {
      const Site& s = world.sites[i];
      if ((s.type != SiteType::Cave && s.type != SiteType::Ruin) || s.cleared) continue;
      float d = std::hypot((float)(s.ex - hx), (float)(s.ey - hy));
      if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) {
      const Site& s = world.sites[best];
      // (fixer M7b r2, review: "WENT INTO SILENT CAVE" twice in one talk: a kinsman lost there is a quest's pitch) a
      // stranger's treasure hunt, not the speaker's family
      local.push_back("A TREASURE-HUNTER WENT DIGGING ABOUT IN " + s.name + " LAST SPRING. ALL HE FOUND WAS " + monsterPlural(s.theme) + ".");
      local.push_back("STAY AWAY FROM " + s.name + ". NOTHING GOOD COMES OUT OF THERE.");
    }
    Biome b = world.over.biomeAt(hx, hy);
    if (b == Biome::Snow || b == Biome::Taiga) local.push_back("COLD ENOUGH TO FREEZE A TROLL'S BEARD. KEEP MOVING OR KEEP WARM.");
    if (b == Biome::Desert) local.push_back("WATER IS WORTH MORE THAN GOLD OUT HERE. AND WATCH FOR SAND RIPPLING WHERE THERE IS NO WIND.");
    if (b == Biome::Swamp) local.push_back("THE MARSH LIGHTS AREN'T LANTERNS, FRIEND. DON'T FOLLOW THEM.");
    if (b == Biome::Forest || b == Biome::Autumn) local.push_back("THE WOODS GIVE US TIMBER AND TAKE A HUNTER OR TWO EVERY WINTER.");
    if (isNight()) local.push_back("YOU SHOULDN'T BE WANDERING THIS LATE. THAT'S WHEN THE DEAD WALK.");
    for (auto& q : quests) {
      if (q.type != QType::Main) continue;
      if (q.stage == 0 && a.site != world.capital) local.push_back("THEY SAY THE " + lordTitleAt(world, world.capital) + " OF " + world.sites[world.capital].name + " WANTS SWORDS AGAINST THE DRAGON.");
      if (q.stage >= 1 && q.stage <= 3) local.push_back("THE WHOLE HOLD IS TALKING ABOUT YOU AND THE DRAGON. THE OLD GODS KEEP YOU.");
      if (q.state == QState::Done) local.push_back("IT'S YOU! THE ONE WHO SLEW ASHFANG! YOUR MEAD IS FREE IN " + town + ".");
    }
    if (plLevel >= 10) local.push_back("BY THE OLD GODS, YOU LOOK LIKE YOU'VE WALKED THROUGH A WAR.");
    if (K && !capitalHere) {
      local.push_back("THIS IS " + realm + " LAND. THE " + ruler + "'S TAX MEN COME EVERY AUTUMN, WHETHER THE HARVEST DOES OR NOT.");
      if (!seat.empty()) local.push_back("THEY SAY THE " + ruler + " OF " + realm + " HOLDS COURT IN " + seat + ". I'VE NEVER SEEN THEM MYSELF.");
    }
    if (capitalHere) local.push_back("THE " + ruler + " OF " + realm + " HOLDS COURT IN " + court + ". MIND YOUR MANNERS NEAR THE ROYAL GUARD.");
    // M4 (STORY lane): the town's own trouble, as the realm has it (15.6.3: hardship and war are felt before they are
    // told). Each counts twice: what a town is living through comes up more than the weather.
    if (homeSite >= 0) {
      const Site& hsite = world.sites[(size_t)homeSite];
      const realm::SettlementState* st = this->realm.settlement(hsite.id);
      const uint16_t f = st ? st->flags : 0;
      std::vector<std::string> trouble;
      if (f & realm::SS_FAMINE) trouble.push_back("THE GRANARY IS EMPTY AND THE PRIEST BLAMES THE SKY. I BLAME THE TAX MEN. BREAD IS DEARER THAN BEER NOW.");
      if (f & realm::SS_BESIEGED) trouble.push_back("NOBODY GETS IN OR OUT OF " + town + " WHILE THE SIEGE LASTS. HOW DID YOU GET IN? NO. DON'T TELL ME.");
      if (f & realm::SS_OCCUPIED) {
        const Kingdom* ok = world.kingdomOf(homeSite);
        trouble.push_back((ok ? ok->name : std::string("FOREIGN")) + " SOLDIERS SLEEP IN OUR BEDS NOW. SMILE AT THEM. SMILING IS FREE, AND THEY HAVE SPEARS.");
      }
      if (f & realm::SS_REFUGEES) trouble.push_back("THE CAMP OUTSIDE " + town + " GROWS EVERY DAY. THEY HAVE NOTHING BUT WHAT THEY CARRIED, AND THEY CARRIED THEIR DEAD.");
      if (f & (realm::SS_BURNED | realm::SS_REBUILDING)) trouble.push_back("HALF THE STREET IS ASH. WE'LL REBUILD. WE ALWAYS DO. WE'RE JUST TIRED OF IT.");
      if (hsite.kingdom >= 0) {
        const ew::Gid kid = world.kingdoms[(size_t)hsite.kingdom].id;
        for (const realm::War& w : this->realm.wars())
          if (!w.endDay && (w.attacker == kid || w.defender == kid)) {
            const ew::Gid foe = w.attacker == kid ? w.defender : w.attacker;
            const realm::KingdomState* fk = this->realm.kingdom(foe);
            trouble.push_back("THE WAR WITH " + (fk ? fk->name : std::string("THEM")) + " TOOK MY NEPHEW FOR THE LEVY. HE WRITES THAT THE FOOD IS BAD AND THE SERGEANT IS WORSE.");
            break;
          }
      }
      for (const std::string& t : trouble) { local.push_back(t); local.push_back(t); }
    }
  }
  auto withLocal = [&](const std::string& fixed) { return !local.empty() && r.f() < 0.5f ? local[r.irange((int)local.size())] : fixed; };
  switch (a.role) {
    case Role::Guard: {
      if (a.name == "ROYAL GUARD") {
        const std::string rg[] = {"THE " + ruler + " IS NOT TO BE TROUBLED WITHOUT CAUSE. STATE YOUR BUSINESS.",
                                  "THE ROYAL GUARD SEES EVERYTHING IN THIS HALL. REMEMBER THAT.", "LONG LIVE THE " + ruler + ". NOW MOVE ALONG.",
                                  "KEEP YOUR HANDS WHERE I CAN SEE THEM IN THIS HALL.", "YOU STAND IN THE COURT OF " + (realm.empty() ? std::string("THE REALM") : realm) + ". BOW WHEN THE " + ruler + " PASSES.",
                                  "WEAPONS STAY SHEATHED IN THIS HALL, STRANGER. NO EXCEPTIONS."};
        return rg[(r.irange(6) + salt) % 6];
      }
      if (capitalHere && r.f() < 0.4f) return "LONG LIVE THE " + ruler + " OF " + realm + ". " + court + " IS OFF LIMITS WITHOUT GOOD CAUSE, TRAVELLER.";
      static const char* g[] = {"I WAS A SELLSWORD ONCE. NOW I WATCH A GATE AND COUNT CARTS.",
                                "KEEP YOUR NOSE CLEAN, TRAVELLER.", "LOST SOMETHING? TRY THE INN. EVERYTHING ENDS UP AT THE INN.",
                                "WATCH THE ROADS AT NIGHT. THE DEAD DON'T STAY BURIED HERE.", "EYES OPEN, BLADE SHARP. THAT'S THE WATCH'S WAY.",
                                "NO TROUBLE IN THIS TOWN, TRAVELLER. NOT ON MY WATCH.", "MOVE ALONG. SOME OF US ARE WORKING.",
                                "MY FEET HURT, MY HELMET LEAKS, AND THE PAY IS LATE. WHAT DO YOU WANT?"};
      if (isNight() && r.f() < 0.4f) return "QUIET NIGHT. TOO QUIET. KEEP YOUR BLADE CLOSE.";
      return g[(r.irange(8) + salt) % 8];
    }
    case Role::Innkeeper: {
      if (capitalHere) {   // a capital is unmistakable: its innkeepers always talk of the king
        const std::string c[] = {"WELCOME TO " + town + ", SEAT OF THE " + ruler + " OF " + realm + ". BEDS ARE 10 GOLD, AND THE COURT GOSSIP IS FREE.",
                                 "THE " + ruler + "'S OWN GUARD DRINKS HERE ON FEAST DAYS. SIT, TRAVELLER: THIS IS THE FINEST INN IN " + realm + ".",
                                 "THE " + ruler + " OF " + realm + " RIDES OUT FROM " + court + " NOW AND THEN. CATCH A GLIMPSE IF YOU CAN. A ROOM IS 10 GOLD."};
        return c[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
      }
      const std::string v[] = {"WELCOME TO THE INN OF " + town + ". A WARM BED IS 10 GOLD, AND THE MEAD IS COLD.",
                               "COME IN OUT OF THE " + std::string(isNight() ? "DARK" : "ROAD DUST") + ". STEW'S ON, AND THE ROOMS ARE CLEAN ENOUGH.",
                               "ANOTHER TRAVELLER! " + town + " DOESN'T SEE MANY. SIT, DRINK, TELL ME WHERE YOU'VE BEEN.",
                               "IF YOU'RE LOOKING FOR WORK, ASK. IF YOU'RE LOOKING FOR TROUBLE, TAKE IT OUTSIDE.",
                               "BEDS UPSTAIRS, ALE DOWN HERE, AND RUMOURS ARE FREE WITH EITHER."};
      return withLocal(v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 5]);
    }
    case Role::Merchant: {
      const std::string v[] = {"COME, LOOK AT MY WARES. FINEST GOODS IN " + town + ".", "POTIONS, ARROWS, A BLADE OR TWO. GOLD TALKS, FRIEND.",
                               "BUYING OR SELLING? I DO BOTH, AND I DO THEM FAIRLY. MOSTLY.", "CARAVAN CAME IN LAST WEEK. GOOD STOCK, WHILE IT LASTS."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Smith: {
      const std::string v[] = {"STEEL AND FIRE, FRIEND. IF IT CUTS OR STOPS A BLADE, I CAN SELL IT TO YOU.",
                               "MIND THE SPARKS. NEED SOMETHING SHARP, OR SOMETHING THICK?", "BEST STEEL THIS SIDE OF " + world.sites[world.capital].name + ". ASK ANYONE.",
                               "BRING ME ORE AND I'LL... WELL, I'LL BUY IT. THEN SELL YOU A SWORD."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Priest: {
      const std::string v[] = {"THE OLD GODS WATCH OVER YOU. SHALL I MEND YOUR WOUNDS?", "YOU LOOK ROAD-WORN, CHILD. THE TEMPLE'S DOOR IS ALWAYS OPEN.",
                               "PEACE IN THESE WALLS, TRAVELLER. IF YOU ARE HURT, THE GODS CAN HEAL IT."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Mage: {
      const std::string v[] = {"MAGIC IS NOT A TOY. BUT IF YOU HAVE GOLD, I HAVE KNOWLEDGE.", "MIND THE CIRCLE ON THE FLOOR. IT TOOK ME A MONTH TO DRAW.",
                               "YOU HAVE THE LOOK OF SOMEONE WHO WANTS TO LEARN. GOOD. LEARNING HAS A PRICE."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Jarl: {
      const std::string v[] = {"SPEAK, STRANGER. WHAT BRINGS YOU BEFORE ME?", "SPEAK. THE PEOPLE OF " + town + " HAVE MY EAR, AND TODAY SO DO YOU.",
                               "SPEAK QUICKLY, TRAVELLER. " + town + " DOES NOT RUN ITSELF."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::King: {
      const std::string pre = ruler + " ";
      std::string nm = a.name.rfind(pre, 0) == 0 ? a.name.substr(pre.size()) : (a.name.rfind("KING ", 0) == 0 ? a.name.substr(5) : a.name);
      std::string t = "WELCOME TO MY HALL, TRAVELLER. I AM " + nm + ", " + ruler + " OF " + (realm.empty() ? town : realm) + ".";
      for (const Quest& q : quests) {
        if (q.type != QType::Main) continue;
        if (q.state == QState::Done) t += " ALL " + (realm.empty() ? std::string("THE REALM") : realm) + " SINGS OF THE ONE WHO SLEW ASHFANG. YOU HONOUR MY HALL.";
        else if (q.stage >= 1) t += " MY " + lordTitleAt(world, homeSite) + "S WRITE OF YOU AND THE DRAGON. THE THRONE STANDS WITH YOU.";
        else if (homeSite == world.capital) t += " A DRAGON STIRS IN THE NORTH, THEY TELL ME. MY " + lordTitleAt(world, homeSite) + " GATHERS SWORDS AGAINST IT: GO TO HIM.";
        else t += " A DRAGON STIRS IN THE NORTH, THEY TELL ME. THE " + lordTitleAt(world, world.capital) + " OF " + world.sites[(size_t)world.capital].name + " GATHERS SWORDS AGAINST IT.";
      }
      return t;
    }
    // M2 wayside people (wayside.cpp)
    case Role::Hunter: {
      const std::string v[] = {"QUIET, NOW. YOU'LL SPOOK EVERY DEER FROM HERE TO " + town + ".",
                               "PELTS, MEAT, ARROWS. TAKE YOUR PICK, OR TAKE A BOW AND HELP ME THIN THE WOLVES.",
                               "THE TRACKS SAY THREE WOLVES PASSED AT DAWN. THE TRACKS ARE NEVER WRONG.",
                               "I SLEEP UNDER THE STARS AND EAT WHAT I CATCH. FEW TOWNSFOLK CAN SAY THE SAME."};
      return withLocal(v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4]);
    }
    case Role::Fisher: {
      const std::string v[] = {"THE FISH ARE BITING TODAY. THEY USUALLY ARE, IF YOU'RE PATIENT.",
                               "TROUT AT DAWN, EEL AT DUSK. SMOKED, IF YOU'RE TRAVELLING FAR.",
                               "SIT A WHILE. THE WATER TELLS YOU THINGS IF YOU LISTEN."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Herbalist: {
      const std::string v[] = {"MIND THE NIGHTSHADE. THE BLUE FLOWERS YOU MAY TOUCH.",
                               "EVERY HERB IN THIS GARDEN HEALS SOMETHING, OR KILLS SOMETHING. OFTEN BOTH.",
                               "A TEA FOR THE COLD, A SALVE FOR THE CUT, A DRAUGHT FOR THE ROAD. WHAT AILS YOU?"};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Traveller: {
      const std::string v[] = {"WARM YOURSELF BY THE FIRE, FRIEND. THE ROAD IS LONG IN BOTH DIRECTIONS.",
                               "I'VE WALKED FROM ONE END OF " + (realm.empty() ? std::string("THIS LAND") : realm) + " TO THE OTHER. ASK ME WHAT I'VE SEEN.",
                               "THE ROADS ARE QUIET TONIGHT. THAT'S WHEN I WORRY.",
                               "SIT. SHARE THE FIRE. NEWS IS THE ONLY COIN I CARRY."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Child: { static const char* c[] = {"WANNA PLAY TAG?", "MY DAD SAYS DRAGONS AREN'T REAL.", "ARE YOU AN ADVENTURER? WHOA!"}; return c[r.irange(3)]; }
    default: {
      const std::string v[] = {"FINE WEATHER TODAY. GOOD FOR THE CROPS.", "HAVE YOU HEARD? SOMETHING BIG WAS FLYING OVER THE MOUNTAINS.",
                                "BANDITS HAVE BEEN HITTING THE CARAVANS LATELY.", "MIND YOURSELF IN THE WOODS. WOLVES ARE BOLD THIS YEAR.",
                                "MY GRANDMOTHER SWORE THE OLD BARROWS ARE HAUNTED.", "WELCOME TO " + town + ". NOT MUCH HAPPENS HERE.",
                                "THE MEAD AT THE INN IS THE BEST IN THE HOLD."};
      return withLocal(v[r.irange(7)]);
    }
  }
}

// ------------------------------------------------------------------ dialogue
namespace {
const char* talkRoleName(Role r) {
  static const char* n[] = {"VILLAGER", "GUARD", "MERCHANT", "SMITH", "INNKEEPER", "PRIEST", "JARL", "FARMER", "CHILD", "MAGE", "BANDIT", "KING",
                            "HUNTER", "FISHER", "HERBALIST", "TRAVELLER", "HERALD", "SOLDIER", "CAPTAIN", "REFUGEE",
                            "HARBOURMASTER", "SHIPWRIGHT", "SAILOR", "PIRATE",
                            "STEWARD", "WARLORD", "ENVOY"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Role::COUNT, "a name for every role");
  return (int)r < (int)Role::COUNT ? n[(int)r] : "?";
}

// the site a person belongs to: their own, or for people indoors the site of the building they are in
int talkSiteOf(const Game& g, const Actor& a) {
  if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) {
    const int s = g.world.over.bldgs[(size_t)a.bldg].site;
    if (s >= 0 && s < (int)g.world.sites.size()) return s;
  }
  return -1;
}

// a census resident's job (Job::None for anyone else)
life::Job talkJobOf(const Game& g, const Actor& a) {
  const int si = talkSiteOf(g, a);
  if (si < 0 || !g.world.sites[(size_t)si].settlement() || a.resident < 0) return life::Job::None;
  const life::Census* c = g.life.find(g.world.sites[(size_t)si].id);
  if (!c || a.resident >= (int)c->res.size()) return life::Job::None;
  return c->res[(size_t)a.resident].job;
}

// a person with a name of their own (not "GUARD", "ROYAL GUARD", a soldier of the line, a herald on the road)
bool talkNamed(const Actor& a) {
  if (!a.npc || !a.human) return false;
  if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Herald || a.role == Role::Bandit) return false;
  if (a.name.empty() || a.name == talkRoleName(a.role) || a.name.find("GUARD") != std::string::npos) return false;
  return true;
}

// what a person is to the player, as the panel's title says it ("INNKEEPER", "BAKER", "SPEAKER OF VOND")
std::string talkTrade(const Game& g, const Actor& a) {
  const life::Job j = talkJobOf(g, a);
  if (a.role == Role::Villager && j != life::Job::None) return life::jobName(j);
  if (a.role == Role::Jarl) {
    const int si = talkSiteOf(g, a);
    return si >= 0 ? lordTitleAt(g.world, si) : std::string("JARL");
  }
  if (a.role == Role::King) return "RULER";
  return talkRoleName(a.role);
}

// the first words a people say to a stranger (light culture flavour, cult::Archetype order)
const char* welcomeWord(const Game& g, int site) {
  static const char* const w[] = {"HAIL, STRANGER.", "WELL MET, STRANGER.", "WELL MET.", "GOOD DAY TO YOU.", "PEACE BE UPON YOU.",
                                  "THE SKY KEEP YOU, TRAVELLER.", "MIND THE MUD, STRANGER.", "BE WELCOME, HONOURED GUEST.", "WELL MET, FRIEND.",
                                  "THE SUN SEES YOU, STRANGER.", "THE LEAVES WELCOME YOU.", "THE STARS KEEP YOU, STRANGER."};
  static_assert(sizeof(w) / sizeof(w[0]) == (size_t)cult::Archetype::COUNT, "a welcome for every people");
  const cult::Culture* c = site >= 0 ? g.world.cultureOf(site) : nullptr;
  const int k = c ? (int)c->archetype : (int)cult::Archetype::Heartland;
  return k >= 0 && k < (int)cult::Archetype::COUNT ? w[k] : "WELL MET.";
}

// a person telling the player who they are ("I'M SUSUT, AND THIS IS MY INN."); "" for those without a name of their own
std::string introPhrase(const Game& g, const Actor& a) {
  if (!talkNamed(a) || a.role == Role::King || a.role == Role::Captain || a.role == Role::Refugee) return "";
  const std::string n = speech::givenName(a.name);
  const int si = talkSiteOf(g, a);
  const std::string town = si >= 0 ? g.world.sites[(size_t)si].name : std::string();
  switch (a.role) {
    case Role::Innkeeper: return "I'M " + n + ", AND THIS IS MY INN.";
    case Role::Merchant: return "I'M " + n + ", AND THIS IS MY SHOP.";
    case Role::Smith: return "I'M " + n + ", THE SMITH HERE.";
    case Role::Priest: return "I AM " + n + ", PRIEST OF THIS TEMPLE.";
    case Role::Mage: return "I AM " + n + ". I STUDY THE ARTS HERE.";
    case Role::Jarl: return "I AM " + n + ", " + talkTrade(g, a) + (town.empty() ? std::string(".") : " OF " + town + ".");
    case Role::Farmer: return "I'M " + n + ". I WORK THE FIELDS HERE.";
    case Role::Hunter: return "THEY CALL ME " + n + ". I HUNT THESE WOODS.";
    case Role::Fisher: return "I'M " + n + ". I FISH THESE WATERS.";
    case Role::Herbalist: return "I'M " + n + ". I GROW WHAT HEALS.";
    case Role::Traveller: return "THE NAME'S " + n + ". I'M ONLY PASSING THROUGH.";
    case Role::Child: return "I'M " + n + "!";
    default: break;
  }
  switch (talkJobOf(g, a)) {
    case life::Job::None: return "I'M " + n + ".";
    case life::Job::Beggar: case life::Job::Guard: return "";
    case life::Job::Child: return "I'M " + n + "!";
    case life::Job::Elder: return "I'M " + n + ". I'VE LIVED HERE ALL MY LIFE.";
    case life::Job::Noble: return "I AM " + n + ", OF ONE OF THE GOOD HOUSES HERE.";
    case life::Job::Labourer: return "I'M " + n + ". I TAKE WHATEVER WORK COMES.";
    case life::Job::Servant: return "I'M " + n + ". I'M IN SERVICE HERE.";
    case life::Job::Server: return "I'M " + n + ". I SERVE AT THE INN.";
    case life::Job::Bard: return "I'M " + n + ", THE BARD" + (town.empty() ? std::string(".") : " OF " + town + ".");
    case life::Job::Lamplighter: return "I'M " + n + ". I LIGHT THE LAMPS AT DUSK.";
    default: {
      const std::string j = life::jobName(talkJobOf(g, a));
      const bool vowel = !j.empty() && std::string("AEIOU").find(j[0]) != std::string::npos;
      return "I'M " + n + (vowel ? ", AN " : ", A ") + j + " HERE.";
    }
  }
}

// a line that already says who the speaker is ("I'M ARN", "NAME'S ARN", "THEY CALL ME ARN", "I AM QUAVION")
bool introducesSelf(const std::string& text, const std::string& name) {
  const std::string n = speech::givenName(name);
  for (const char* lead : {"I'M ", "I AM ", "NAME'S ", "CALL ME ", "NAME IS "})
    if (text.find(std::string(lead) + n) != std::string::npos) return true;
  return text.find(n + ", THEY CALL ME") != std::string::npos;
}

// the line opens on a greeting of its own (no second "HELLO" in front of it)
bool opensWithGreeting(const std::string& t) {
  static const char* const k[] = {"WELCOME", "GOOD ", "MORNING", "EVENING", "NIGHT", "HAIL", "GREETINGS", "PEACE", "AH,", "AH.", "ANOTHER ",
                                  "COME ", "WELL MET", "YOU'RE BACK", "BACK AGAIN", "YOU AGAIN", "IT'S YOU", "HELLO", "SPEAK,"};
  for (const char* s : k) if (t.rfind(s, 0) == 0) return true;
  return false;
}

// the kind of option a first page can do without: the services (grouped under one header) and the kindnesses
// (offered once the player asks how someone is keeping)
bool serviceOpt(const DlgOpt& o) {
  if (o.action == A_TRADE || o.action == A_REST || o.action == A_RUMOR || o.action == A_HEAL || o.action == A_LEARN) return true;
  if (o.action >= DLG_HOME && o.action < DLG_HOME_END) return true;    // deeds, horses, recipes, the kitchen, the mill
  if (o.action >= DLG_MAGIC && o.action < DLG_MAGIC_END) return true;  // (M9) a teacher's spells, attunement
  if (o.action >= DLG_SEA && o.action < DLG_SEA_END && o.action != DLG_SEA + 95) return true;   // (M10) boats, the yard, passages
  if (o.action >= DLG_SKILLS && o.action < DLG_SKILLS_END) return true;  // (M9 integration) lessons, prices, unlearning
  if (o.action >= DLG_CRAFT && o.action < DLG_M6_END) return true;     // the forge, the craft
  return lifeOptKind(o.action) == 1;                                    // the inn's meal, food for the stores
}
}  // namespace

std::string Game::speakerTitle(const Actor& a) const {
  if (a.comp) return a.name;   // (M8) a companion is known by name (the VOICE lane may add their calling)
  if (!a.npc || !a.human) return a.name;
  // (P1b fixer) a story's stand-in is called by their part ("BERGBO, COURIER"), not VILLAGER whatever their trade
  const std::string part = a.role == Role::Villager ? story.personTitle(a) : std::string();
  const std::string trade = part.empty() ? talkTrade(*this, a) : part;
  if (!talkNamed(a)) return a.name;
  if (a.role == Role::King) return a.name;   // a ruler is known by name and title alike
  if (talkMem.introduced(npcKey(a))) return speech::givenName(a.name) + ", " + trade;
  return "THE " + trade;
}

std::string Game::speakerTitle() const {
  for (const Actor& a : actors)
    if (a.id == dlg.actor && dlg.actor >= 0) {
      // (M7b) a line told as a story (a narrated ending) carries the tale's title as its speaker: show that, not the person
      if (!dlg.speaker.empty() && dlg.speaker != a.name) return dlg.speaker;
      return speakerTitle(a);
    }
  return dlg.speaker;
}

// M7b (rpg/sim/talk.h): the opening, then the talk memory (a person spoken to is met: the day, how often) and, in tests
// and the dialogue review, the recorder
void Game::talkTo(Actor& a) {
  talkToInner(a);
  if (mode != Mode::Dialogue || dlg.actor != a.id) return;
  const uint64_t key = a.npc && a.human ? npcKey(a) : 0;
  if (key) {
    speech::Met& m = talkMem.meet(key, day);
    m.lastTopic = dlg.topic;
    m.lastLine = speech::lineHash(dlg.front.empty() ? dlg.text : dlg.front);
  }
  if (talkRec) {
    const int hs = talkSiteOf(*this, a);
    const bool troll = a.npc && !a.human && a.mon == Monster::Troll;
    talkRec->begin(dlg, a.name, troll ? std::string("TROLL") : std::string(talkRoleName(a.role)),
                   hs >= 0 ? world.sites[(size_t)hs].name : std::string(), key);
  }
}

void Game::talkToInner(Actor& a) {
  // M5: a village animal is petted, not talked to (a wag, a purr, a cluck)
  if (a.critter) {
    if (HomeOps::critterUse(*this, a)) return;   // M7: the player's own beasts (pet, groom, ride; home_actors.cpp)
    a.bubble = art::Bubble::Heart; a.bubbleT = 1.6f;
    a.face = faceOf(pl().p - a.p);
    const art::Critter k = (art::Critter)(a.critter - 1);
    sfx((int)(k == art::Critter::Dog ? Sfx::Bark : k == art::Critter::Cat ? Sfx::Meow : Sfx::Cluck), a.p, 1.15f, 0.6f);
    return;
  }
  dlg = Dialogue();
  dlg.actor = a.id;
  dlg.speaker = a.name;
  dlg.role = a.role;
  if (a.comp && PartyTalkOps::talk(*this, a)) return;   // M8: a companion's own conversation (party_talk.cpp, VOICE lane)
  // M2: a toll bridge's troll asks its toll; a missing person found in a cave asks to be led out
  if (a.npc && !a.human && a.mon == Monster::Troll) { WaysideOps::tollTalk(*this, a); return; }
  using speech::Topic;
  if (a.quest > 0) {
    const Quest* q = questById(a.quest);
    if (q && q->state == QState::Active && !(q->flags & QF_FOUND)) {
      speech::propose(dlg, speech::P_URGENT, Topic::QuestActive,
                      "YOU CAME FOR ME? " + q->giverName + " SENT YOU? THANK THE GODS. I CAN'T FIND THE WAY OUT, AND THERE ARE THINGS IN THE DARK.");
      dlg.opts.push_back({"STAY CLOSE. I'LL GET YOU OUT.", A_ESCORT, q->id});
    } else if (q && q->state == QState::Active) speech::propose(dlg, speech::P_URGENT, Topic::QuestActive, "I'M RIGHT BEHIND YOU. JUST GET US OUT OF HERE.");
    else speech::propose(dlg, speech::P_URGENT, Topic::QuestDone, "I'LL FIND MY OWN WAY HOME FROM HERE. TELL " + (q ? q->giverName : std::string("THEM")) + " I'M SAFE!");
    dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    mode = Mode::Dialogue;
    sfx((int)Sfx::Talk, a.p);
    return;
  }
  const uint64_t key = a.npc && a.human ? npcKey(a) : 0;
  const speech::Met* met = key ? talkMem.find(key) : nullptr;
  const bool knownBefore = met && met->times > 0;
  // ---- 1. the greeting (never the same line the player heard from them, or from anyone, lately)
  {
    std::string g = greeting(a);
    for (int salt = 1; salt <= 3; salt++) {
      const uint32_t h = speech::lineHash(g);
      const bool stale = talkMem.saidRecently(h, day, 1) || (met && met->lastLine == h && day - met->last <= 3);
      if (!stale) break;
      g = greeting(a, salt);
    }
    const bool trade = a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Innkeeper || a.role == Role::Priest || a.role == Role::Mage;
    speech::propose(dlg, trade ? speech::P_SERVICE : speech::P_GREETING, trade ? Topic::Service : Topic::Greeting, g);
  }
  // the background shows in how people talk to you (and what they charge)
  {
    bool say1 = hash32((uint32_t)npcKey(a) ^ (uint32_t)(day * 977)) % 2 == 0;
    auto bg = [&](const char* t) { speech::propose(dlg, speech::P_SERVICE, Topic::Smalltalk, t); };
    if (background == Background::Blacksmith && a.role == Role::Smith) bg("A SMITH'S CHILD? I CAN SEE IT IN YOUR HANDS. FOR YOU, A FAIR PRICE: A FIFTH OFF ANYTHING ON MY RACK.");
    if (background == Background::Urchin && a.role == Role::Merchant) bg("YOU'VE GOT THE STREETS IN YOUR EYES, FRIEND. ALL RIGHT, ONE OF OUR OWN: I'LL KNOCK A LITTLE OFF.");
    if (background == Background::Noble && a.role == Role::Guard && say1) bg("FORGIVE ME, I KNOW NOBLE BEARING WHEN I SEE IT. THE WATCH COULD USE SOMEONE OF YOUR... STANDING.");
    if (background == Background::Novice && a.role == Role::Priest) bg("A NOVICE OF THE TEMPLE! THE GODS' BLESSINGS RUN LONGER IN YOU, CHILD. SHALL I MEND YOUR WOUNDS?");
    if (background == Background::Sailor && a.role == Role::Innkeeper && say1) bg("SEA LEGS ON DRY LAND, EH? SAILORS DRINK HERE, AND NOBODY ASKS WHAT SANK.");
    if (background == Background::Farmhand && a.role == Role::Innkeeper && !say1) bg("A FARMHAND, BY THOSE HANDS. THEN YOU KNOW WHAT GOOD BREAD IS WORTH ON THE ROAD.");
    if (background == Background::Hunter && a.role == Role::Villager && say1) bg("YOU WALK LIKE A HUNTER. THE WOLVES WON'T HEAR YOU COMING, WILL THEY?");
  }
  // ---- 2. a reward waiting comes first: the first option collects it
  for (auto& q : quests)
    if (q.state == QState::Complete && q.type != QType::Main && isGiver(q, a)) {
      bool bounty = q.type == QType::Bounty || q.type == QType::Hunt || q.type == QType::NamedBandit;
      dlg.opts.push_back({std::string(bounty ? "COLLECT BOUNTY (+" : "COLLECT REWARD (+") + std::to_string(q.gold) + " GOLD)", A_TURNIN, q.id});
      speech::propose(dlg, speech::P_REWARD, Topic::QuestDone, QuestOps::turnInLine(*this, q));
    }
  // ---- 3. the main quest's hooks (the lord of the capital)
  for (auto& q : quests) {
    if (q.type != QType::Main || a.role != Role::Jarl || a.site != world.capital) continue;
    if (q.stage == 0) {
      speech::propose(dlg, speech::P_QUEST + 5, Topic::QuestOffer,
                      "SO YOU'VE HEARD THE RUMOURS. THEY ARE TRUE: A DRAGON, ASHFANG, HAS WOKEN BENEATH " + lairName() + ". STEEL ALONE CANNOT KILL IT.");
      dlg.opts.push_back({"HOW CAN IT BE STOPPED?", A_MAIN, 1});
    }
    if (q.stage == 2) {
      speech::propose(dlg, speech::P_QUEST + 5, Topic::QuestDone, "THE THREE EMBER SHARDS! I NEVER THOUGHT I WOULD SEE THEM WHOLE AGAIN. LET THE SMITHS FORGE THE CROWN.");
      dlg.opts.push_back({"I WILL HUNT THE DRAGON.", A_MAIN, 3});
    }
  }
  // ---- 4. a job of theirs under way: what is still to do, in their own words (the journal keeps q.desc)
  for (auto& q : quests)
    if (q.state == QState::Active && q.type != QType::Main && q.type != QType::Retrieve && isGiver(q, a)) {
      speech::propose(dlg, speech::P_QUEST, Topic::QuestActive, QuestOps::questReminder(*this, q));
      break;
    }
  // ---- 5. the opening: the start village innkeeper hands over the old blade (VISION_PLAN 15.1)
  for (auto& q : quests)
    if (q.type == QType::Retrieve && q.state == QState::Active && isGiver(q, a)) {
      QuestOps::giveFirstWeapon(*this, q);
      std::string t = "THE ROAD TOOK ALL BUT YOUR SHIRT, EH? HERE, TAKE MY FATHER'S OLD BLADE FROM OVER THE HEARTH, AND A DRAUGHT FOR THE ROAD.";
      // point at the job the journal now tracks, so the advice matches the quest (not a made-up favour)
      const Quest* nxt = nullptr;
      for (const auto& o : quests) if (o.id == trackedQuest) nxt = &o;
      if (nxt && nxt->type == QType::Hunt) t += std::string(" THEY SAY THE ") + monsterPlural(nxt->mon) + " ARE BAD THIS YEAR, IF YOU NEED WORK.";
      else t += " IF YOU NEED WORK, ASK AROUND: SOMEONE HERE ALWAYS NEEDS A HAND.";
      speech::propose(dlg, speech::P_URGENT, Topic::Reward, t);
      break;
    }
  if (std::string t = QuestOps::questDialogue(*this, a); !t.empty()) speech::propose(dlg, speech::P_REWARD, Topic::QuestDone, t);   // M2: a parcel's recipient
  // ---- 6. the options of their trade
  if (hasOffer(a)) dlg.opts.push_back({a.role == Role::Innkeeper ? "ANY WORK GOING?" : "DO YOU NEED HELP?", A_ASK, 0});
  if (a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Mage || a.role == Role::Priest || a.role == Role::Innkeeper ||
      a.role == Role::Hunter || a.role == Role::Fisher || a.role == Role::Herbalist)
    dlg.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
  if (a.role == Role::Traveller) dlg.opts.push_back({"ANY NEWS FROM THE ROAD?", A_NEWS, 0});
  if (a.role == Role::Innkeeper) {
    // M0b: a room already paid for is not charged again; in its last hour (from 11:00 on the due day) there is no
    // sleep left in it, so the innkeeper lets you a fresh room instead
    bool mine = lodgingActive() && inside && lodging.bldg == subBldg && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
    dlg.opts.push_back({mine ? "ABOUT MY ROOM..." : "RENT A ROOM (10 GOLD)", A_REST, mine ? 0 : 10});
    dlg.opts.push_back({"BUY A RUMOUR (10 GOLD)", A_RUMOR, 10});
  }
  if (a.role == Role::Priest) dlg.opts.push_back({"BLESS ME.", A_HEAL, 0});
  // (M9) a mage's lessons (MEND, FROST LANCE, the rest of the schools, a land's tradition): MagicOps::talk below
  // ---- 7. the lanes' hooks, in their old order. Each proposes its line (speech::propose); a hook that still writes
  // dlg.text directly is taken at the priority its kind of line has (and gives way to a weightier line already chosen)
  auto hook = [&](int defPri, Topic t, auto&& fn) {
    const std::string before = dlg.text;
    const int bp = dlg.pri;
    const uint8_t bt = dlg.topic;
    fn();
    if (dlg.text != before && dlg.pri == bp && dlg.topic == bt) {
      if (defPri >= bp) { dlg.pri = defPri; dlg.topic = (uint8_t)t; }
      else dlg.text = before;
    }
  };
  hook(speech::P_BARK, Topic::Bark, [&] { LifeHooks::talk(*this, a); });   // M5: barks of need, feed / employ / supply / befriend (life_game.cpp)
  hook(speech::P_NEWS, Topic::War, [&] { WarOps::talk(*this, a); });     // M4: the war's talk (siege commanders, guards' news of the front)
  {   // M4: story quests, gossip, the realm's news (rpg/story/story_game.cpp; the SAGA lane proposes at P_STORY / P_NEWS)
    const std::string before = dlg.text;
    const int bp = dlg.pri;
    const uint8_t bt = dlg.topic;
    storyTalk(a);
    if (dlg.text != before && dlg.pri == bp && dlg.topic == bt) {
      bool scene = false, pitch = false;   // a running story's own scene (its choices), or a story offered (its pitch)
      for (const DlgOpt& o : dlg.opts) {
        if (o.action >= DLG_STORY && o.action < DLG_STORY + story::SA_START) scene = true;
        else if (o.action >= DLG_STORY && o.action < DLG_WAR) pitch = true;
      }
      const int sp = scene ? speech::P_URGENT - 5 : pitch ? speech::P_STORY : speech::P_NEWS;
      if (sp >= bp) { dlg.pri = sp; dlg.topic = (uint8_t)(scene || pitch ? Topic::Story : Topic::News); }
      else dlg.text = before;
    }
  }
  hook(speech::P_NEWS, Topic::Foe, [&] { FoeHooks::talk(*this, a); });         // M6: warnings of named beasts and world bosses (foes_game.cpp)
  hook(speech::P_URGENT, Topic::Craft, [&] { CraftHooks::talk(*this, a); });   // M6: the forge's work and lessons (a thief barred: urgent)
  hook(speech::P_SERVICE, Topic::Home, [&] { HomeOps::talk(*this, a); });    // M7: deeds, builders, horses, recipes and cooking
  hook(speech::P_SERVICE, Topic::Companion, [&] { PartyTalkOps::folkTalk(*this, a); });   // M8: offers to join, kin of companions and the fallen
  hook(speech::P_SERVICE, Topic::Service, [&] { SkillOps::talk(*this, a); });   // M9: trainers, teaching, persuade / intimidate
  hook(speech::P_SERVICE, Topic::Service, [&] { MagicOps::talk(*this, a); });   // M9: mage teachers, tomes, traditions
  hook(speech::P_SERVICE, Topic::Service, [&] { SeaOps::talk(*this, a); });     // M10: harbourmasters, shipwrights, sailors
  hook(speech::P_SERVICE, Topic::Service, [&] { ServiceOps::talk(*this, a); }); // M12: captains (ENLIST, ORDERS), envoys, rulers
  hook(speech::P_SERVICE, Topic::Service, [&] { LordOps::talk(*this, a); });    // M12: the steward, rulers (lordships, charters)
  // ---- 8. one coherent opening: a stranger is told who this is (and gets the people's own hello), an acquaintance is
  // greeted as one; the core line (what matters most to the speaker now) follows
  const std::string core = dlg.text;
  if (key && talkNamed(a)) {
    const int hs = talkSiteOf(*this, a);
    if (!talkMem.introduced(key)) {
      if (introducesSelf(core, a.name)) talkMem.flag(key, speech::TM_INTRODUCED, day);
      else if (const std::string intro = introPhrase(*this, a); !intro.empty()) {
        const bool light = dlg.pri <= speech::P_NEED && !opensWithGreeting(core);
        std::string t = light ? speech::joinFit(std::string(welcomeWord(*this, hs)) + " " + intro, core) : core;
        if (t == core) t = speech::joinFit(intro, core);
        if (t == core) t = speech::joinFit("I'M " + speech::givenName(a.name) + ".", core);
        if (t != core) { dlg.text = t; talkMem.flag(key, speech::TM_INTRODUCED, day); }
      }
    } else if (knownBefore && dlg.pri <= speech::P_NEED && !opensWithGreeting(core)) {
      const uint32_t h = hash32((uint32_t)key ^ (uint32_t)day * 2654435761u);
      std::string hello;
      if (met->flags & speech::TM_SLIGHTED) hello = "YOU AGAIN.";
      else if (met->flags & speech::TM_THANKED) hello = h % 2 ? "GOOD TO SEE YOU AGAIN, FRIEND." : "MY FRIEND! I HAVEN'T FORGOTTEN WHAT YOU DID FOR US.";
      else if (met->last == day) hello = h % 2 ? "BACK AGAIN?" : "YOU AGAIN? WELCOME BACK.";
      else hello = h % 3 == 0 ? "GOOD TO SEE YOU AGAIN." : h % 3 == 1 ? "AH, IT'S YOU AGAIN." : "BACK IN " + (hs >= 0 ? world.sites[(size_t)hs].name : std::string("TOWN")) + ", ARE YOU?";
      dlg.text = speech::joinFit(hello, core);
    }
  }
  talkMem.said(speech::lineHash(core), day);
  dlg.front = core;
  // ---- 9. the first page holds about six options: the services go under one header, the kindnesses come up when the
  // player asks how the speaker is keeping
  {
    auto shown = [&]() { int n = 1; for (const DlgOpt& o : dlg.opts) if (o.grp == 0) n++; return n; };   // (+ FAREWELL)
    if (shown() > speech::FIRST_PAGE) {
      int nserv = 0;
      for (const DlgOpt& o : dlg.opts) if (o.grp == 0 && serviceOpt(o)) nserv++;
      if (nserv >= 2) {
        size_t at = dlg.opts.size();
        for (size_t i = 0; i < dlg.opts.size(); i++)
          if (dlg.opts[i].grp == 0 && serviceOpt(dlg.opts[i])) { if (at == dlg.opts.size()) at = i; dlg.opts[i].grp = 1; }
        dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at, DlgOpt{"WHAT CAN YOU OFFER?", A_GROUP, 1, 0});
        // a keeper with more to offer than one page holds: the house and kitchen business gets its own header
        int n1 = 0, nh = 0;
        for (const DlgOpt& o : dlg.opts) if (o.grp == 1) { n1++; if (o.action >= DLG_HOME && o.action < DLG_HOME_END) nh++; }
        if (n1 > speech::FIRST_PAGE - 1 && nh >= 2 && n1 - nh >= 2) {
          for (DlgOpt& o : dlg.opts) if (o.grp == 1 && o.action >= DLG_HOME && o.action < DLG_HOME_END) o.grp = 3;
          dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at + 1, DlgOpt{"WHAT ELSE DO YOU DO HERE?", A_GROUP, 3, 0});
        }
      }
    }
    bool g1 = false;
    for (const DlgOpt& o : dlg.opts) if (o.action == A_GROUP && o.arg == 1) g1 = true;
    if (shown() > speech::FIRST_PAGE && g1)   // the keeper's word of the beasts about goes with the inn's news
      for (DlgOpt& o : dlg.opts) if (o.grp == 0 && o.action >= DLG_FOES && o.action < DLG_CRAFT) o.grp = 1;
    bool chat = false;
    for (const DlgOpt& o : dlg.opts) if (o.grp == 0 && lifeOptKind(o.action) == 3) chat = true;
    if (shown() > speech::FIRST_PAGE && chat)
      for (DlgOpt& o : dlg.opts) if (o.grp == 0 && lifeOptKind(o.action) == 2) o.grp = 2;   // (L_CHAT shows them: lifeChoose)
  }
  dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  mode = Mode::Dialogue;
  sfx((int)Sfx::Talk, a.p);
}

void Game::closeDialogue() { if (mode == Mode::Dialogue) mode = Mode::Play; }

void Game::dialogueChoose(int oi) {
  if (oi < 0 || oi >= (int)dlg.opts.size()) return;
  dialogueChooseInner(oi);
  // (M7b) an option group whose options were all replaced (an answer with its own options) closes: back to the page
  if (dlg.group) {
    bool any = false;
    for (const DlgOpt& o : dlg.opts) if (o.grp == dlg.group && o.action != A_BACK) any = true;
    if (!any) {
      dlg.group = 0;
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& o) { return o.action == A_BACK; }), dlg.opts.end());
    }
  }
  if (talkRec) talkRec->reply(oi, dlg, mode == Mode::Dialogue);
}

void Game::dialogueChooseInner(int oi) {
  if (oi < 0 || oi >= (int)dlg.opts.size()) return;
  DlgOpt o = dlg.opts[oi];
  int ai = findActor(dlg.actor);
  sfx((int)Sfx::MenuSelect, pl().p);
  switch (o.action) {
    case A_BYE: mode = Mode::Play; return;
    case A_GROUP: {   // (M7b) the services under one header: what they are, in the speaker's words, then the options
      dlg.group = (uint8_t)o.arg;
      std::vector<std::string> what;
      for (const DlgOpt& k : dlg.opts) {
        if (k.grp != dlg.group) continue;
        const char* w = k.action == A_REST ? "A ROOM FOR THE NIGHT" : k.action == A_RUMOR ? "WHATEVER NEWS I'VE HEARD"
                        : k.action == A_TRADE ? (dlg.role == Role::Innkeeper ? "WHAT'S ON MY SHELF" : "MY WARES")
                        : k.action == A_HEAL ? "THE GODS' BLESSING" : k.action == A_LEARN ? "A LESSON OR TWO"
                        : lifeOptKind(k.action) == 1 && dlg.role == Role::Innkeeper ? "A HOT MEAL"
                        : k.label.rfind("PROPERTY", 0) == 0 ? "THE DEEDS OF HOUSES FOR SALE" : k.label.rfind("TEACH ME A RECIPE", 0) == 0 ? "A RECIPE OR TWO"
                        : k.label.rfind("MAY I COOK", 0) == 0 ? "THE USE OF MY KITCHEN" : k.label.rfind("HORSES", 0) == 0 ? "GOOD HORSES"
                        : k.label.find("MY HORSE") != std::string::npos ? "A STALL FOR YOUR HORSE" : k.label.rfind("PAY MY TAXES", 0) == 0 ? "THE TAX BOOKS"
                        : k.label.rfind("GRIND", 0) == 0 ? "THE MILLSTONES" : nullptr;
        if (w && std::find(what.begin(), what.end(), w) == what.end()) what.push_back(w);
      }
      std::string list;
      for (size_t k = 0; k < what.size() && k < 4; k++) list += (k ? (k + 1 == std::min<size_t>(what.size(), 4) ? " AND " : ", ") : "") + what[k];
      dlg.text = list.empty() ? "HAVE A LOOK. WHAT WILL IT BE?" : (dlg.group == 3 ? "I CAN ALSO OFFER YOU " : "I CAN OFFER YOU ") + list + ". WHAT WILL IT BE?";
      if (!speech::fitsPanel(dlg.text)) dlg.text = "HAVE A LOOK. WHAT WILL IT BE?";
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_BACK; }), dlg.opts.end());
      size_t at = dlg.opts.size();
      for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_BYE) { at = k; break; }
      dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at, DlgOpt{"SOMETHING ELSE.", A_BACK, 0, dlg.group});
      return;
    }
    case A_BACK:
      dlg.group = 0;
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_BACK; }), dlg.opts.end());
      dlg.text = "OF COURSE. WHAT ELSE CAN I DO FOR YOU?";
      return;
    case A_TRADE: if (ai >= 0) openShop(actors[ai]); return;
    case A_REST: {
      // M0b: an inn lets you one of its rooms (until noon tomorrow) and tells you where it is. (M3b: a single-storey
      // inn lets its rooms on the ground floor, behind partitions; a taller one upstairs: the rooms are on whichever
      // floor has them, the first such floor from the ground up)
      int inn = inside && subBldg >= 0 && world.over.bldgs[subBldg].type == art::Building::Inn && world.over.bldgs[subBldg].genVer >= WORLDGEN_V7 ? subBldg : -1;
      Map up;
      std::vector<int> guestRooms;
      int roomFloor = -1;
      if (inn >= 0) {
        const Bldg& B = world.over.bldgs[inn];
        // a room already rented here is on its own floor
        const bool rented = lodgingActive() && lodging.bldg == inn && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
        for (int f = rented ? std::clamp(lodging.floor, 0, B.floors() - 1) : 0; f < B.floors() && guestRooms.empty(); f++) {
          up = Map();
          genInterior(up, B, B.seed, f);
          for (int i = 0; i < (int)up.rooms.size(); i++) if (up.rooms[(size_t)i].kind == RoomKind::GuestRoom && up.rooms[(size_t)i].bedX >= 0) guestRooms.push_back(i);
          if (!guestRooms.empty()) roomFloor = f;
          if (rented) break;
        }
      }
      if (inn < 0 || guestRooms.empty()) {   // an old inn: beds in the common room, sleep where you stand
        if (gold < o.arg) { dlg.text = "YOU DON'T HAVE ENOUGH GOLD."; return; }
        gold -= o.arg;
        rest(8);
        dlg.text = "YOU WAKE WELL RESTED. GOOD MORNING!";
        dlg.opts = {{"FAREWELL.", A_BYE, 0}};
        return;
      }
      bool mine = lodgingActive() && lodging.bldg == inn && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
      if (!mine) {
        if (gold < o.arg) { dlg.text = "YOU DON'T HAVE ENOUGH GOLD."; return; }
        gold -= o.arg;
        int pickR = guestRooms[hash32((uint32_t)inn * 2654435761u ^ (uint32_t)day * 40503u) % guestRooms.size()];
        lodging.bldg = inn; lodging.floor = roomFloor; lodging.room = pickR; lodging.untilDay = day + 1;
        sfx((int)Sfx::Coin, pl().p);
      }
      if (std::find(guestRooms.begin(), guestRooms.end(), lodging.room) == guestRooms.end()) lodging.room = guestRooms[0];
      lodging.floor = roomFloor;
      // where it is, as you come up the stairs (or, on the ground floor, as you stand at the bar): doors to the left /
      // right of the stairwell (the counter), nearest first
      const RoomInfo& R = up.rooms[(size_t)lodging.room];
      const bool ground = roomFloor == 0;
      int sx = up.down.valid() ? up.down.x : (ground ? (int)std::floor(pl().p.x / TILE) : up.w / 2);
      // (fix round 2: left and right are counted from the middle of the flight, both of its tiles: a door in front of
      // the flight's second tile is the first one on that side, as the player sees it)
      int sx0 = sx, sx1 = sx;
      if (up.down.valid()) {
        const int dn = (int)Prop::StairsDown + 1;
        while (up.propAt(sx0 - 1, up.down.y) == dn) sx0--;
        while (up.propAt(sx1 + 1, up.down.y) == dn) sx1++;
      }
      auto sideOf = [&](int x) { int d = 2 * x - (sx0 + sx1); return d < 0 ? -1 : (d > 0 ? 1 : 0); };
      auto distOf = [&](int x) { return std::abs(2 * x - (sx0 + sx1)); };
      int side = sideOf(R.doorX);
      std::vector<int> rows;
      for (const RoomInfo& Q : up.rooms) if (Q.doorX >= 0 && std::find(rows.begin(), rows.end(), Q.doorY) == rows.end()) rows.push_back(Q.doorY);
      int nth = 1;
      for (const RoomInfo& Q : up.rooms) {
        if (Q.doorX < 0 || &Q == &R || Q.doorY != R.doorY) continue;
        if (sideOf(Q.doorX) == side && distOf(Q.doorX) < distOf(R.doorX)) nth++;
      }
      static const char* ord[] = {"FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH", "SIXTH"};
      std::string where = side == 0 ? "THE DOOR STRAIGHT AHEAD" : std::string("THE ") + ord[std::min(nth, 6) - 1] + " DOOR ON THE " + (side < 0 ? "LEFT" : "RIGHT");
      if (rows.size() >= 2) {
        int minRow = *std::min_element(rows.begin(), rows.end());
        where += R.doorY == minRow ? ", ON THE BACK SIDE" : ", ON THE FRONT SIDE";
      }
      std::string num = std::to_string((int)R.guest + 1);
      dlg.text = (mine ? "YOUR ROOM IS STILL YOURS UNTIL NOON: ROOM " : "ROOM ") + num + (ground ? ". THROUGH THE HALL, " : ". UP THE STAIRS, ") + where + "." +
                 (mine ? "" : " IT'S YOURS UNTIL NOON TOMORROW.");
      dlg.opts = {{ground ? "GO TO BED" : "GO UP TO BED", A_BED, 0}, {"LATER.", A_BYE, 0}};
      return;
    }
    case A_BED: {
      // straight up to your room: wake beside your own bed
      mode = Mode::Play;
      if (!lodgingActive() || !inside || subBldg != lodging.bldg) return;
      const int h = lodgingSleepHours(day, hour, lodging.untilDay);
      if (h <= 0) { say(kRoomDue); return; }
      if (subFloor != lodging.floor) changeFloor(lodging.floor);
      if (lodging.room >= 0 && lodging.room < (int)sub.rooms.size()) {
        const RoomInfo& R = sub.rooms[(size_t)lodging.room];
        int bx = R.bedX >= 0 ? R.bedX : R.r.cx(), by = R.bedX >= 0 ? R.bedY : R.r.cy();
        int best = -1, bd = 1 << 30;
        for (int y = R.r.y; y < R.r.y + R.r.h; y++)
          for (int x = R.r.x; x < R.r.x + R.r.w; x++) {
            if (sub.roomIndexAt(x, y) != lodging.room || sub.blocked(x, y)) continue;
            int pr = sub.propAt(x, y);
            if (pr == (int)Prop::DoorH + 1 || pr == (int)Prop::DoorV + 1) continue;
            int d = std::abs(x - bx) * 2 + std::abs(y - by) * 2 + (y < by ? 3 : 0);
            if (d < bd) { bd = d; best = y * sub.w + x; }
          }
        if (best >= 0) { pl().p = Vec2((best % sub.w) * TILE + 8.0f, (best / sub.w) * TILE + 10.0f); pl().vel = Vec2(); pl().knock = Vec2(); }
        // face the bed
        Vec2 to = Vec2(bx * TILE + 8.0f, by * TILE + 10.0f) - pl().p;
        pl().aim = len2(to) > 1 ? norm(to) : Vec2(0, -1);
        pl().face = std::fabs(to.x) > std::fabs(to.y) ? (to.x > 0 ? 2 : 3) : (to.y < 0 ? 1 : 0);
      }
      rest(h);
      say("YOU SLEEP IN YOUR ROOM.");
      return;
    }
    case A_RUMOR: {
      // M2 rumours (wayside.cpp): the nearest place the player has not heard of goes on the map as rumoured
      if (gold < o.arg) {
        dlg.text = "NO GOLD, NO GOSSIP. THE ALE ISN'T FREE EITHER.";
        // (M9 integration) the refused offer goes from the page: asking again would only hear the same words
        dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_RUMOR; }), dlg.opts.end());
        return;
      }
      // M4 (VISION_PLAN 4.6): news of the realm that has reached this inn and the player has not heard yet comes first
      {
        int px = 0, py = 0;
        overworldTile(*this, px, py);
        const uint64_t who = ai >= 0 ? npcKey(actors[(size_t)ai]) : 0;
        const realm::WorldEvent* e = story::pickRumour(*this, world.ox + px, world.oy + py, false, who);
        if (e && !e->heard) {
          const int hs = ai >= 0 ? (actors[(size_t)ai].site >= 0 ? actors[(size_t)ai].site : (actors[(size_t)ai].bldg >= 0 ? world.over.bldgs[(size_t)actors[(size_t)ai].bldg].site : -1)) : -1;
          gold -= o.arg;
          sfx((int)Sfx::Coin, pl().p);
          dlg.text = story::hearEvent(*this, *e, hs >= 0 ? world.sites[(size_t)hs].culture : 0);
          for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_RUMOR) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k); break; }
          return;
        }
      }
      const std::string line = hearRumour();
      if (line.empty()) dlg.text = "CAN'T SAY I'VE HEARD ANYTHING NEW. KEEP YOUR COIN.";
      else {
        gold -= o.arg;
        sfx((int)Sfx::Coin, pl().p);
        dlg.text = "THEY SAY THERE'S " + line + ". I'VE MARKED IT ON YOUR MAP.";
      }
      for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_RUMOR) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k); break; }
      return;
    }
    case A_NEWS: {
      // a traveller's news: one rumour a day, free
      const uint64_t k = markKey(ai >= 0 ? npcKey(actors[(size_t)ai]) : 0, Mk::News);
      auto it = marks.find(k);
      if (it != marks.end() && it->second == day) dlg.text = "THAT'S ALL I KNOW FOR NOW. ASK ME AGAIN TOMORROW, IF WE'RE BOTH STILL ON THE ROAD.";
      else {
        // M4: half the time a traveller carries the realm's news (an event the player has not heard), else directions
        int px = 0, py = 0;
        overworldTile(*this, px, py);
        const realm::WorldEvent* e = (day + (int)(k & 0xFF)) % 2 == 0 ? story::pickRumour(*this, world.ox + px, world.oy + py, false, k) : nullptr;
        const std::string line = e && !e->heard ? std::string() : hearRumour();
        const cult::Culture* tc = world.cultureAtTile(px, py);
        if (e && !e->heard) { marks[k] = day; dlg.text = story::hearEvent(*this, *e, tc ? tc->id : 0); }
        else if (line.empty()) dlg.text = "THE ROADS HAVE BEEN QUIET. NOTHING WORTH THE TELLING.";
        else { marks[k] = day; dlg.text = "ON MY WAY HERE I PASSED " + line + ". MIND HOW YOU GO."; }
      }
      for (size_t k2 = 0; k2 < dlg.opts.size(); k2++) if (dlg.opts[k2].action == A_NEWS) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k2); break; }
      return;
    }
    case A_ASK:
      if (ai < 0) return;
      {
        // (P1b) a radiant job runs on the story engine: its opening talk is the pitch, with its own ACCEPT / DECLINE.
        // Not in the first two days (P1b integrator): the opening's first job stays the plain hunt it teaches with (a
        // smuggler's run is a poor first errand), as requests are not raised on days 1-2 either
        const std::string rj = day <= 2 ? std::string() : story::radiant::offerFor(*this, actors[(size_t)ai]);
        if (!rj.empty() && storyBeginJob(actors[(size_t)ai], rj)) return;
      }
      pendingOffer_ = QuestOps::makeOffer(*this, actors[ai]);
      dlg.text = pendingPitch_.empty() ? "THERE IS SOMETHING. " + pendingOffer_.desc + " I'LL PAY " + std::to_string(pendingOffer_.gold) + " GOLD." : pendingPitch_;
      // (fixer M7b r3) a giver who has not yet said who they are does so before the pitch (the title then names them)
      if (actors[(size_t)ai].npc && actors[(size_t)ai].human && talkNamed(actors[(size_t)ai]) && actors[(size_t)ai].role != Role::King &&
          !talkMem.introduced(npcKey(actors[(size_t)ai]))) {
        const std::string t = speech::joinFit("I'M " + speech::givenName(actors[(size_t)ai].name) + ".", dlg.text);
        if (t != dlg.text) { dlg.text = t; talkMem.flag(npcKey(actors[(size_t)ai]), speech::TM_INTRODUCED, day); }
      }
      dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
      return;
    case A_ACCEPT:
      acceptQuest(pendingOffer_);
      dlg.text = QuestOps::acceptLine(*this, pendingOffer_);
      if (ai >= 0 && actors[(size_t)ai].npc && actors[(size_t)ai].human) {
        const Actor& ga = actors[(size_t)ai];
        talkMem.flag(npcKey(ga), speech::TM_KNOWS_PLAYER, day);
        // (fixer M7b r2, review: "the giver stays THE HERDER through the offer, yet the turn-in names them") a giver who
        // has not said who they are does so now, so the journal's and the return's name is one the player has heard
        if (talkNamed(ga) && ga.role != Role::King && !talkMem.introduced(npcKey(ga))) {
          const bool back = dlg.text.find("COME BACK") != std::string::npos;
          std::string t = dlg.text + " I'M " + speech::givenName(ga.name) + (back ? ", BY THE WAY." : ", BY THE WAY. ASK FOR ME WHEN YOU'RE BACK.");
          if (!speech::fitsPanel(t)) t = dlg.text + " I'M " + speech::givenName(ga.name) + ", BY THE WAY.";
          if (speech::fitsPanel(t)) { dlg.text = t; talkMem.flag(npcKey(ga), speech::TM_INTRODUCED, day); }
        }
      }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_DECLINE: mode = Mode::Play; return;
    case A_DELIVER:
      for (auto& q : quests)
        if (q.id == o.arg && q.type == QType::Deliver && q.state == QState::Active) {
          QuestOps::completeQuest(*this, q);   // the recipient pays (the parcel leaves the pack)
          dlg.text = "AT LAST! YOU HAVE MY THANKS, AND " + std::to_string(q.gold) + " GOLD AS PROMISED. TELL " + q.giverName + " IT CAME SAFE.";
          break;
        }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_ESCORT:
      for (auto& q : quests)
        if (q.id == o.arg && q.type == QType::Missing && q.state == QState::Active) {
          q.flags |= QF_FOUND;
          const std::string place = q.target >= 0 && q.target < (int)world.sites.size() ? world.sites[(size_t)q.target].name : std::string("HERE");
          emit(Ev::QuestUpdate, pl().p, q.id, 2, q.subject + " FOLLOWS YOU: LEAD THEM OUT OF " + place);
          sfx((int)Sfx::QuestStart, pl().p, 1.1f);
        }
      mode = Mode::Play;
      return;
    case A_TOLLPAY: {
      const int si = o.arg, toll = WaysideOps::tollOf(*this, si);
      if (gold < toll) { dlg.text = "NO GOLD, NO CROSSING. COME BACK RICHER."; dlg.opts = {{"FAREWELL.", A_BYE, 0}}; return; }
      gold -= toll;
      if (si >= 0 && si < (int)world.sites.size()) marks[markKey(world.sites[(size_t)si].id, Mk::Toll)] = day;
      sfx((int)Sfx::Coin, pl().p);
      dlg.text = dlg.speaker + " COUNTS THE COINS TWICE. \"CROSS, LITTLE ONE. TODAY ONLY.\"";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    }
    case A_TOLLREFUSE:
      if (ai >= 0) {
        Actor& t = actors[(size_t)ai];
        t.hostile = true; t.npc = false; t.faction = Faction::Monster; t.aggro = true; t.target = pl().id;
        sfx((int)Sfx::Roar, t.p);
        emit(Ev::Shake, t.p, 0, 3);
      }
      mode = Mode::Play;
      say("THE TROLL ROARS AND COMES FOR YOU!");
      return;
    case A_TURNIN:
    {
      // (M7b) the reward handed over, said as it is: the gold, and the piece of gear when the job brought one
      int gold0 = 0;
      const size_t inv0 = inv.size();
      for (auto& q : quests)
        if (q.id == o.arg && q.state == QState::Complete) { gold0 = q.gold; QuestOps::completeQuest(*this, q); break; }
      std::string extra;
      for (size_t k = inv0; k < inv.size(); k++)
        if (inv[k].flags & IF_QUESTREWARD) {
          const std::string& n = inv[k].name;
          const bool many = n.size() > 2 && n.back() == 'S' && n[n.size() - 2] != 'S';   // BOOTS, GAUNTLETS
          extra = std::string(many ? " AND TAKE THESE " : " AND TAKE THIS ") + n + " AS WELL. " + (many ? "THEY" : "IT") + " SHOULD SERVE YOU BETTER THAN ME.";
        }
      dlg.text = gold0 > 0 ? "HERE, COUNT IT IF YOU LIKE." + (extra.empty() ? std::string(" YOU'VE EARNED EVERY COIN.") : extra)
                           : "YOU HAVE MY THANKS. HERE, YOU'VE EARNED THIS.";
      if (!speech::fitsPanel(dlg.text)) dlg.text = "HERE, COUNT IT IF YOU LIKE, AND TAKE A LITTLE SOMETHING MORE.";
      if (ai >= 0 && actors[(size_t)ai].npc && actors[(size_t)ai].human) talkMem.flag(npcKey(actors[(size_t)ai]), speech::TM_THANKED | speech::TM_KNOWS_PLAYER, day);
    }
      for (size_t k = 0; k < dlg.opts.size(); k++)   // more rewards from the same giver stay on offer
        if (k != (size_t)oi && dlg.opts[k].action == A_TURNIN) { const Quest* q2 = questById(dlg.opts[k].arg); if (q2 && q2->state == QState::Complete) { dlg.opts = {dlg.opts[k], {"FAREWELL.", A_BYE, 0}}; return; } }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_MAIN:
      QuestOps::advanceMain(*this, o.arg);
      if (o.arg == 1) dlg.text = "LEGEND SAYS THE EMBER CROWN CAN BIND A DRAGON'S SOUL. ITS THREE SHARDS LIE WITH DRAUGR WARLORDS IN THE OLD RUINS. I HAVE MARKED THEM ON YOUR MAP. BRING THEM TO ME.";
      else dlg.text = "THEN GO, WITH THE BLESSINGS OF THE HOLD. ASHFANG NESTS ON " + lairName() + ". TAKE THIS GOLD FOR SUPPLIES.";
      if (o.arg == 3) giveGold(300);
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_HEAL:
      pl().hp = pl().maxHp;
      blessT = blessingSecs(); blessName = "TEMPLE BLESSING";
      recalcPlayer();
      emit(Ev::Heal, pl().p);
      sfx((int)Sfx::Heal, pl().p);
      dlg.text = "THERE. YOUR WOUNDS ARE MENDED. GO WITH THE GODS, AND WALK IN THEIR LIGHT, CHILD.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_LEARN: {
      int price = o.arg == (int)Spell::Heal ? 120 : 300;
      if (gold < price) { dlg.text = "COME BACK WHEN YOU HAVE THE GOLD."; return; }
      gold -= price;
      spellsKnown |= (uint8_t)(1 << o.arg);
      spell = (Spell)o.arg;
      sfx((int)Sfx::LevelUp, pl().p, 1.2f);
      dlg.text = std::string("YOU HAVE LEARNED ") + spellName((Spell)o.arg) + ". USE IT WISELY.";
      dlg.opts.erase(dlg.opts.begin() + oi);
      return;
    }
    default:
      // M4: the lanes' own option ranges (game.h DLG_*)
      if (o.action >= DLG_STORY && o.action < DLG_WAR && storyChoose(o)) return;
      if (o.action >= DLG_WAR && o.action < DLG_END && WarOps::choose(*this, o)) return;
      if (o.action >= DLG_LIFE && o.action < DLG_LIFE_END && LifeHooks::choose(*this, o)) return;
      if (o.action >= DLG_FOES && o.action < DLG_CRAFT && FoeHooks::choose(*this, o)) return;      // M6
      if (o.action >= DLG_CRAFT && o.action < DLG_M6_END && CraftHooks::choose(*this, o)) return;  // M6
      if (o.action >= DLG_HOME && o.action < DLG_HOME_END && HomeOps::choose(*this, o)) return;  // M7
      if (o.action >= DLG_PARTY && o.action < DLG_PARTY_END && PartyTalkOps::choose(*this, o)) return;  // M8 (party_talk.cpp)
      if (o.action >= DLG_SKILLS && o.action < DLG_SKILLS_END && SkillOps::choose(*this, o)) return;  // M9 (skills_game.cpp)
      if (o.action >= DLG_MAGIC && o.action < DLG_MAGIC_END && MagicOps::choose(*this, o)) return;    // M9 (magic_game.cpp)
      if (o.action >= DLG_SEA && o.action < DLG_SEA_END && SeaOps::choose(*this, o)) return;          // M10 (sea_game.cpp)
      if (o.action >= DLG_SERVICE && o.action < DLG_SERVICE_END && ServiceOps::choose(*this, o)) return;  // M12 (service_game.cpp)
      if (o.action >= DLG_LORD && o.action < DLG_LORD_END && LordOps::choose(*this, o)) return;          // M12 (lord_game.cpp)
      mode = Mode::Play;
      return;
  }
}

