// M7 "Home" (VIEW lane): script commands that drive the build / decorate / shell / storage / cooking screens for
// screenshots and checks (View::buildCommand / buildExpect in rpg/view/buildmode.cpp do the work). They never touch the
// player's save. The current plot is the one the player stands in (or whose house he is in), else the last one made.
//   build tab yard|decorate|shell|storage|cook|buy [store index | station]   open that screen (Mode::Build); cook takes
//                                      campfire|hearth|cookpot|inn
//   build sel <object> | <shell> | <row>   the selected piece (yard / decorate), shell (shell), meal or item row
//   build cat <0..4>                   the catalogue's category
//   build ghost <x> <y>                the ghost's tile (plot tiles outdoors, the floor's tiles indoors)
//   build rotate | place | remove      as the buttons (remove = SELL 50 %)
//   build style next|prev|<n>          the shell screen's culture style
//   build side 0|1                     storage: the pack (0) or the store (1); build move: move the selected row across
//   build cook | build done
//   build stand <x> <y>                (staging) the player onto tile (x, y) of the last plot made (outdoors)
//   build enterhome [floor]            (staging) the player into the last plot's house
//   build styles [n]                   (staging) n (6) more cultures' styles discovered: the settlements' in the window
//   build pantry                       (staging) cooking ingredients in the pack: grain, flour, bread, veg, fruit, meat,
//                                      fish, eggs, milk, cheese, herbs, honey (3 of each)
//   expect build mode <none|yard|decorate|shell|storage|cook|buy>
//   expect build fits 0|1              the selected piece fits at the ghost
//   expect build objects <n>           the plot's yard (decorate: indoor) objects
//   expect build at <x> <y> <object|nothing>
//   expect build why <WORDS>           the last refusal contains the words
//   expect build store <n>             stacks in the open store
//   expect build state owned|building|built
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/home.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {
bool cmdBuild(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (op == "styles") {
    int n = c.arg(2).empty() ? 6 : std::atoi(c.arg(2).c_str());
    for (size_t i = 0; i < g.world.sites.size() && n > 0; i++) {
      const cult::Culture* C = g.world.cultureOf((int)i);
      if (!C || g.home.knowsStyle(C->id)) continue;
      g.home.styles.push_back(C->id);
      n--;
    }
    return true;
  }
  if (op == "pantry") {
    const home::Crop crops[] = {home::Crop::Wheat, home::Crop::Potato, home::Crop::Carrot, home::Crop::Grapes, home::Crop::Herbs, home::Crop::Cabbage};
    for (home::Crop k : crops) g.inv.push_back(home::makeCropItem(k, 3, 2));
    const home::Product prods[] = {home::Product::Egg, home::Product::Milk, home::Product::Cheese, home::Product::Honey, home::Product::Flour};
    for (home::Product k : prods) g.inv.push_back(home::makeProduct(k, 3));
    Item bread = makeFood(0); bread.count = 3; g.inv.push_back(bread);
    Item meat = makeFood(1); meat.count = 3; g.inv.push_back(meat);
    Item fish = makeFood(4); fish.count = 3; g.inv.push_back(fish);
    return true;
  }
  std::string err;
  if (!c.view.buildCommand(c.game, c.a, err)) c.fail(err);
  return true;
}
bool expBuild(ScriptCtx& c) {
  std::string err;
  if (!c.view.buildExpect(c.game, c.a, err)) c.fail(err);
  return true;
}
}  // namespace

EMB_SCRIPT_CMD("build", "build tab|sel|cat|ghost|rotate|place|remove|style|side|move|cook|done: drive the M7 build screens", cmdBuild);
EMB_SCRIPT_CMD("expect:build", "expect build mode|fits|objects|at|why|store|state ...: the M7 build screens", expBuild);
