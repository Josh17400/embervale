// The STORY lane's hooks into the core loop (SOLID 4.1: they were Game::story*): story quests and campaigns, rumours
// and news, lore in ruins, notice boards, heralds. The core (sim_schedule.cpp, game_dialogue.cpp, game_combat.cpp,
// game_rpg.cpp ...) calls them where phase A put the calls; they are DEFINED in rpg/story/story_game.cpp. StoryOps is
// a friend of Game (game.h).
#pragma once
#include "rpg/sim/game.h"

// API (SOLID 5.6): no stable test/script API. Every member is internal to the lane: only rpg/sim and rpg/story
// call it (tests and scripts go through Game's public members), and it may change with the lane.
struct StoryOps {
  //   STORY (rpg/story/story_game.cpp): story quests and campaigns, rumours and news, lore in ruins, notice boards,
  //   heralds
  static void step(Game& g, float dt);
  static void talk(Game& g, Actor& a);   // may add options (DLG_STORY..) and set dlg.text; called by talkTo before FAREWELL
  // (P1b, lead) "ANY WORK GOING?" with a radiant job (story::radiant::offerFor): start its composed script on giver `a`
  // and show its opening talk (the pitch, ACCEPT / DECLINE) in the open dialogue; false: it could not be cast
  static bool beginJob(Game& g, Actor& a, const std::string& specId);
  static bool choose(Game& g, const DlgOpt& o);   // an option in [DLG_STORY, DLG_WAR): true handled
  static void kill(Game& g, const Actor& victim, bool byPlayer);
  static void entered(Game& g, int site, int bldg);   // the player just entered a site's map (site >= 0) or a building (bldg >= 0)
  static bool useProp(Game& g, art::Prop p, int tx, int ty);   // the player used a prop (a notice board, an inscription...): true handled
};
