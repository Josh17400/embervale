// EMBERVALE tests: the P2c gear gates on the doll (SLICE-DOLL; docs/PLAN.md "## P2c" Gates C3-C6, docs/P2C_CONTRACTS.md
// 7). They run inside `rpg_test --doll --gates` (lines C3 gear, C4 armour change, C5 families, C6 weapons) and alone:
//
//   rpg_test --doll-arms [--looks FILE] [--pool FILE] [--report] [--selftest]
//
// The looks are the slice's (tools/doll/slice_looks.txt; embedded below when no --looks is given): `tag who spec [gaits]`
// read through the one looks-field parser (art::doll::applyLookSpec). Every check reads art::doll::probeFrame (the composed
// frame with, per pixel, the part that wrote it and the region it was drawn with), so a failure names the look, part, clip,
// facing and frame.
//   C3 gear on every frame (LOCO, RUN, DRAWN, ACTION, DEATH):
//     a no holes: nothing missing (dollMissing) and no enclosed hole inside a gear piece (a clear pocket every one of whose
//       edge pixels is armour, a tabard, a helm, a shield or a pauldron; the bow's string gap exempt)
//     b the helm on the head: anchored on the head; its pixels inside the head box +-1 column (the plume slot up to F-37);
//       no hair above the helm in its own columns
//     c the shield on the off arm: anchored on gripL, touching the gripping hand; behind the torso facing right, in front
//       facing left and down; over no boot pixel and at most 1 column of a shin; in the run inside the figure's box + 1
//     d the grips: a library weapon's grip mark inside the gripping fist (+-1), no weapon pixel over that fist's hand pixels,
//       the right arm grips in every facing and the bow sits in the left; a two-handed weapon's off fist within 1.5 px of
//       the haft line; a tip mark at the far end
//     e lying keeps gear: the helm on the head, the shield within 4 px of the body, the weapon within 6 px of a hand
//   C4 armour shows: the idle frame (facing down and right) changes 25 % (padded and leather 15 %) of its pixels against the
//     same look without armour; plate changes the outline too
//   C5 families (the sword against the spear): strike frames 25 % apart per facing; the spear's tip forward 8 px or more
//     from the loaded frame to the impact with under 4 px of drift (side views); its left strike never the right's mirror;
//     an effect on the impact frame
//   C6 the library: every weapon map at its 16 angles (bows: carry and drop plus their aim views), a tip mark with a bright
//     tip pixel on blades, the lit edge toward the top-left, no orphans, the grip origin on the haft, lengths +-1 px
// Each gate has a selftest that feeds it synthetic defects: `--selftest` prints "<gate> selftest: ALL CAUGHT" or names
// what slipped through.
#include "rpg/art/doll/doll_envelope.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"
#include "tools/tests/tests.h"

using art::Clip;
using art::HumanLook;
using art::RigPart;
using art::RigSheet;
namespace doll = art::doll;
namespace P = art::doll::pool;

namespace dollarms {

bool g_report = false;

std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}

struct Line {
  std::string id;
  int checks = 0, fails = 0;
  std::string first;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    if (!fails) first = why;
    if (g_report || fails < 4) std::printf("  %s: %s\n", id.c_str(), why.c_str());
    fails++;
  }
  void print() const {
    std::printf("%s %-24s %4d checks, %4d failed%s%s\n", fails ? "FAIL:" : "ok   ", id.c_str(), checks, fails, fails ? "; first: " : "", first.c_str());
  }
};

// ---------------------------------------------------------------- the looks
const char* kSliceLooks =   // (P2c ladder) the gates' matrix: tools/doll/ladder_looks.txt (tools/doll/embed_looks.py writes it)
    "hero     man    -  walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "lt1      man    hero=1,kit=heartland:1:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt2      man    hero=1,kit=heartland:2:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt3      man    hero=1,kit=heartland:3:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt4      man    hero=1,kit=heartland:4:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt5      man    hero=1,kit=heartland:5:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt6      man    hero=1,kit=heartland:6:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt6m     man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look  stay,walk,run,didle,hit1\n"
    "lt6w     woman  heroine=1,kit=heartland:6:7,wpn=spear,hairColor=#8C5A2A,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "lt1w     woman  heroine=1,kit=heartland:1:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "jade3    man    kit=jade:3:1,wpn=spear,head=6,hair=1,hairVar=1,hairColor=#1E1A22,skin=#E2B58A,palette=look  stay,walk,run,didle,hit1,hurt,"
    "fall,lying\n"
    "imp3     man    kit=imperial:3:2,wpn=sword,head=2,hair=1,hairVar=0,hairColor=#3A2418,beard=1,beardStyle=2,skin=#C98E64,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "fjord3   man    kit=fjordfolk:3:5,wpn=sword,head=4,hair=2,hairVar=1,hairColor=#B87A3E,beard=1,beardStyle=4,skin=#EAC0A0,palette=look  stay,"
    "walk,run,didle,hit1,hurt,fall,lying\n"
    "nband    man    npc=heartland:bandit:0:7,palette=look  stay,walk,run,hit1,fall,lying\n"
    "nroyal   woman  npc=heartland:guard:2:7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "nking    man    npc=heartland:king:0:7,palette=look  stay,walk\n"
    "guard    man    outfit=8,armorStyle=2,bodyForm=3,tabardColor=#A8322E,helmet=1,helmStyle=2,helmForm=1,shield=1,shieldStyle=3,shieldForm=2,"
    "shieldDevice=#E8C25A,weapon=1,bladeForm=1,gloves=1,boots=1,pauldron=1,neck=0,head=1,hair=1,hairVar=1,hairColor=#4A3022,beard=1,beardStyle=0,"
    "skin=#D9A07A,tier=2,palette=look  walk,run,dwalk,drun,didle,hit1,block,hurt,fall,lying\n"
    "knight   woman  outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,"
    "plumeColor=#B83A32,weapon=8,polearmForm=1,hair=5,hairVar=0,hairColor=#2E1E18,skin=#E8B08A,tier=4,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,block,hurt,fall,lying\n"
    "archer   man    outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,neck=0,"
    "topColor=#6F8F45,bottomColor=#4A3A2C,hair=1,hairVar=2,hairColor=#8A5A30,beard=1,beardStyle=1,skin=#CA8E66,tier=2,palette=look  walk,run,"
    "dwalk,drun,didle,draw,hurt,fall,lying\n"
    "knightm  man    outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,"
    "plumeColor=#B83A32,weapon=1,bladeForm=1,neck=0,head=4,beard=1,beardStyle=2,hairColor=#5A3A26,tier=4,palette=look  walk,run,didle,hit1,hurt,"
    "fall,lying\n"
    "archerw  woman  outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,topColor=#6F8F45,"
    "bottomColor=#4A3A2C,hair=3,hairVar=0,hairColor=#3A2418,tier=2,palette=look  walk,run,didle,draw,hurt,fall,lying\n";
const char* kArmsLooks =   // (P2c wave 2, DOLL-C) tools/doll/arms_looks.txt: the matrices M8-M12 (tools/doll/embed_looks.py writes it)
    "m8_padded_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=1,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=1,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_padded_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=1,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=2,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=2,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leather_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=2,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=3,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=3,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_mail_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=3,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=4,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=4,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_scale_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=4,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=5,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=5,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_lamellar_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=5,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=6,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=6,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_brigandine_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=6,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=7,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=7,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_plate_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=7,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb0_t1 man    hero=1,kit=heartland:1:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb0_t3 man    hero=1,kit=heartland:3:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb0_t4 man    hero=1,kit=heartland:4:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb0_t6 man    hero=1,kit=heartland:6:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb1_t1 man    hero=1,kit=heartland:1:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb1_t3 man    hero=1,kit=heartland:3:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb1_t4 man    hero=1,kit=heartland:4:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb1_t6 man    hero=1,kit=heartland:6:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb2_t1 man    hero=1,kit=heartland:1:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb2_t3 man    hero=1,kit=heartland:3:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb2_t4 man    hero=1,kit=heartland:4:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_mb2_t6 man    hero=1,kit=heartland:6:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb0_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb0_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb0_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb0_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=8,build=0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb1_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb1_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb1_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb1_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=8,build=1,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb2_t1 woman  heroine=1,kit=heartland:1:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb2_t3 woman  heroine=1,kit=heartland:3:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb2_t4 woman  heroine=1,kit=heartland:4:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m8_leaf_wb2_t6 woman  heroine=1,kit=heartland:6:7,bodyForm=8,build=2,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "m9_cone_t4_mh1 man    kit=fjordfolk:4:1,shape=helm:cone,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t4_mh4 man    kit=fjordfolk:4:1,shape=helm:cone,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t4_wh2 woman  kit=fjordfolk:4:1,shape=helm:cone,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t4_wh6 woman  kit=fjordfolk:4:1,shape=helm:cone,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t5_mh1 man    kit=fjordfolk:5:1,shape=helm:cone,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t5_mh4 man    kit=fjordfolk:5:1,shape=helm:cone,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t5_wh2 woman  kit=fjordfolk:5:1,shape=helm:cone,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t5_wh6 woman  kit=fjordfolk:5:1,shape=helm:cone,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t6_mh1 man    kit=fjordfolk:6:1,shape=helm:cone,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t6_mh4 man    kit=fjordfolk:6:1,shape=helm:cone,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t6_wh2 woman  kit=fjordfolk:6:1,shape=helm:cone,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cone_t6_wh6 woman  kit=fjordfolk:6:1,shape=helm:cone,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t4_mh1 man    kit=highland:4:1,shape=helm:kettle,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t4_mh4 man    kit=highland:4:1,shape=helm:kettle,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t4_wh2 woman  kit=highland:4:1,shape=helm:kettle,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t4_wh6 woman  kit=highland:4:1,shape=helm:kettle,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t5_mh1 man    kit=highland:5:1,shape=helm:kettle,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t5_mh4 man    kit=highland:5:1,shape=helm:kettle,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t5_wh2 woman  kit=highland:5:1,shape=helm:kettle,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t5_wh6 woman  kit=highland:5:1,shape=helm:kettle,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t6_mh1 man    kit=highland:6:1,shape=helm:kettle,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t6_mh4 man    kit=highland:6:1,shape=helm:kettle,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t6_wh2 woman  kit=highland:6:1,shape=helm:kettle,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kettle_t6_wh6 woman  kit=highland:6:1,shape=helm:kettle,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t4_mh1 man    kit=imperial:4:1,shape=helm:warhat,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t4_mh4 man    kit=imperial:4:1,shape=helm:warhat,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t4_wh2 woman  kit=imperial:4:1,shape=helm:warhat,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t4_wh6 woman  kit=imperial:4:1,shape=helm:warhat,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t5_mh1 man    kit=imperial:5:1,shape=helm:warhat,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t5_mh4 man    kit=imperial:5:1,shape=helm:warhat,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t5_wh2 woman  kit=imperial:5:1,shape=helm:warhat,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t5_wh6 woman  kit=imperial:5:1,shape=helm:warhat,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t6_mh1 man    kit=imperial:6:1,shape=helm:warhat,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t6_mh4 man    kit=imperial:6:1,shape=helm:warhat,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t6_wh2 woman  kit=imperial:6:1,shape=helm:warhat,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_warhat_t6_wh6 woman  kit=imperial:6:1,shape=helm:warhat,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t4_mh1 man    kit=dune:4:1,shape=helm:great,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t4_mh4 man    kit=dune:4:1,shape=helm:great,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t4_wh2 woman  kit=dune:4:1,shape=helm:great,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t4_wh6 woman  kit=dune:4:1,shape=helm:great,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t5_mh1 man    kit=dune:5:1,shape=helm:great,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t5_mh4 man    kit=dune:5:1,shape=helm:great,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t5_wh2 woman  kit=dune:5:1,shape=helm:great,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t5_wh6 woman  kit=dune:5:1,shape=helm:great,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t6_mh1 man    kit=dune:6:1,shape=helm:great,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t6_mh4 man    kit=dune:6:1,shape=helm:great,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t6_wh2 woman  kit=dune:6:1,shape=helm:great,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_great_t6_wh6 woman  kit=dune:6:1,shape=helm:great,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t4_mh1 man    kit=steppe:4:1,shape=helm:mask,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t4_mh4 man    kit=steppe:4:1,shape=helm:mask,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t4_wh2 woman  kit=steppe:4:1,shape=helm:mask,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t4_wh6 woman  kit=steppe:4:1,shape=helm:mask,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t5_mh1 man    kit=steppe:5:1,shape=helm:mask,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t5_mh4 man    kit=steppe:5:1,shape=helm:mask,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t5_wh2 woman  kit=steppe:5:1,shape=helm:mask,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t5_wh6 woman  kit=steppe:5:1,shape=helm:mask,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t6_mh1 man    kit=steppe:6:1,shape=helm:mask,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t6_mh4 man    kit=steppe:6:1,shape=helm:mask,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t6_wh2 woman  kit=steppe:6:1,shape=helm:mask,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_mask_t6_wh6 woman  kit=steppe:6:1,shape=helm:mask,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t4_mh1 man    kit=marsh:4:1,shape=helm:crested,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t4_mh4 man    kit=marsh:4:1,shape=helm:crested,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t4_wh2 woman  kit=marsh:4:1,shape=helm:crested,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t4_wh6 woman  kit=marsh:4:1,shape=helm:crested,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t5_mh1 man    kit=marsh:5:1,shape=helm:crested,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t5_mh4 man    kit=marsh:5:1,shape=helm:crested,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t5_wh2 woman  kit=marsh:5:1,shape=helm:crested,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t5_wh6 woman  kit=marsh:5:1,shape=helm:crested,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t6_mh1 man    kit=marsh:6:1,shape=helm:crested,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t6_mh4 man    kit=marsh:6:1,shape=helm:crested,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t6_wh2 woman  kit=marsh:6:1,shape=helm:crested,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crested_t6_wh6 woman  kit=marsh:6:1,shape=helm:crested,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t4_mh1 man    kit=jade:4:1,shape=helm:onion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t4_mh4 man    kit=jade:4:1,shape=helm:onion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t4_wh2 woman  kit=jade:4:1,shape=helm:onion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t4_wh6 woman  kit=jade:4:1,shape=helm:onion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t5_mh1 man    kit=jade:5:1,shape=helm:onion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t5_mh4 man    kit=jade:5:1,shape=helm:onion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t5_wh2 woman  kit=jade:5:1,shape=helm:onion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t5_wh6 woman  kit=jade:5:1,shape=helm:onion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t6_mh1 man    kit=jade:6:1,shape=helm:onion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t6_mh4 man    kit=jade:6:1,shape=helm:onion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t6_wh2 woman  kit=jade:6:1,shape=helm:onion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_onion_t6_wh6 woman  kit=jade:6:1,shape=helm:onion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t4_mh1 man    kit=river:4:1,shape=helm:veil,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t4_mh4 man    kit=river:4:1,shape=helm:veil,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t4_wh2 woman  kit=river:4:1,shape=helm:veil,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t4_wh6 woman  kit=river:4:1,shape=helm:veil,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t5_mh1 man    kit=river:5:1,shape=helm:veil,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t5_mh4 man    kit=river:5:1,shape=helm:veil,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t5_wh2 woman  kit=river:5:1,shape=helm:veil,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t5_wh6 woman  kit=river:5:1,shape=helm:veil,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t6_mh1 man    kit=river:6:1,shape=helm:veil,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t6_mh4 man    kit=river:6:1,shape=helm:veil,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t6_wh2 woman  kit=river:6:1,shape=helm:veil,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_veil_t6_wh6 woman  kit=river:6:1,shape=helm:veil,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t4_mh1 man    kit=suntemple:4:1,shape=helm:horned,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t4_mh4 man    kit=suntemple:4:1,shape=helm:horned,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t4_wh2 woman  kit=suntemple:4:1,shape=helm:horned,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t4_wh6 woman  kit=suntemple:4:1,shape=helm:horned,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t5_mh1 man    kit=suntemple:5:1,shape=helm:horned,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t5_mh4 man    kit=suntemple:5:1,shape=helm:horned,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t5_wh2 woman  kit=suntemple:5:1,shape=helm:horned,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t5_wh6 woman  kit=suntemple:5:1,shape=helm:horned,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t6_mh1 man    kit=suntemple:6:1,shape=helm:horned,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t6_mh4 man    kit=suntemple:6:1,shape=helm:horned,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t6_wh2 woman  kit=suntemple:6:1,shape=helm:horned,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_horned_t6_wh6 woman  kit=suntemple:6:1,shape=helm:horned,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t4_mh1 man    kit=sylvan:4:1,shape=helm:winged,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t4_mh4 man    kit=sylvan:4:1,shape=helm:winged,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t4_wh2 woman  kit=sylvan:4:1,shape=helm:winged,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t4_wh6 woman  kit=sylvan:4:1,shape=helm:winged,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t5_mh1 man    kit=sylvan:5:1,shape=helm:winged,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t5_mh4 man    kit=sylvan:5:1,shape=helm:winged,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t5_wh2 woman  kit=sylvan:5:1,shape=helm:winged,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t5_wh6 woman  kit=sylvan:5:1,shape=helm:winged,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t6_mh1 man    kit=sylvan:6:1,shape=helm:winged,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t6_mh4 man    kit=sylvan:6:1,shape=helm:winged,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t6_wh2 woman  kit=sylvan:6:1,shape=helm:winged,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_winged_t6_wh6 woman  kit=sylvan:6:1,shape=helm:winged,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t4_mh1 man    kit=starspire:4:1,shape=helm:sallet,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t4_mh4 man    kit=starspire:4:1,shape=helm:sallet,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t4_wh2 woman  kit=starspire:4:1,shape=helm:sallet,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t4_wh6 woman  kit=starspire:4:1,shape=helm:sallet,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t5_mh1 man    kit=starspire:5:1,shape=helm:sallet,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t5_mh4 man    kit=starspire:5:1,shape=helm:sallet,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t5_wh2 woman  kit=starspire:5:1,shape=helm:sallet,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t5_wh6 woman  kit=starspire:5:1,shape=helm:sallet,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t6_mh1 man    kit=starspire:6:1,shape=helm:sallet,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t6_mh4 man    kit=starspire:6:1,shape=helm:sallet,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t6_wh2 woman  kit=starspire:6:1,shape=helm:sallet,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_sallet_t6_wh6 woman  kit=starspire:6:1,shape=helm:sallet,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t4_mh1 man    kit=fjordfolk:4:1,shape=helm:snout,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t4_mh4 man    kit=fjordfolk:4:1,shape=helm:snout,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t4_wh2 woman  kit=fjordfolk:4:1,shape=helm:snout,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t4_wh6 woman  kit=fjordfolk:4:1,shape=helm:snout,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t5_mh1 man    kit=fjordfolk:5:1,shape=helm:snout,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t5_mh4 man    kit=fjordfolk:5:1,shape=helm:snout,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t5_wh2 woman  kit=fjordfolk:5:1,shape=helm:snout,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t5_wh6 woman  kit=fjordfolk:5:1,shape=helm:snout,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t6_mh1 man    kit=fjordfolk:6:1,shape=helm:snout,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t6_mh4 man    kit=fjordfolk:6:1,shape=helm:snout,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t6_wh2 woman  kit=fjordfolk:6:1,shape=helm:snout,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_snout_t6_wh6 woman  kit=fjordfolk:6:1,shape=helm:snout,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t4_mh1 man    kit=highland:4:1,shape=helm:kabuto,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t4_mh4 man    kit=highland:4:1,shape=helm:kabuto,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t4_wh2 woman  kit=highland:4:1,shape=helm:kabuto,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t4_wh6 woman  kit=highland:4:1,shape=helm:kabuto,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t5_mh1 man    kit=highland:5:1,shape=helm:kabuto,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t5_mh4 man    kit=highland:5:1,shape=helm:kabuto,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t5_wh2 woman  kit=highland:5:1,shape=helm:kabuto,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t5_wh6 woman  kit=highland:5:1,shape=helm:kabuto,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t6_mh1 man    kit=highland:6:1,shape=helm:kabuto,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t6_mh4 man    kit=highland:6:1,shape=helm:kabuto,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t6_wh2 woman  kit=highland:6:1,shape=helm:kabuto,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_kabuto_t6_wh6 woman  kit=highland:6:1,shape=helm:kabuto,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t4_mh1 man    kit=imperial:4:1,shape=helm:morion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t4_mh4 man    kit=imperial:4:1,shape=helm:morion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t4_wh2 woman  kit=imperial:4:1,shape=helm:morion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t4_wh6 woman  kit=imperial:4:1,shape=helm:morion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t5_mh1 man    kit=imperial:5:1,shape=helm:morion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t5_mh4 man    kit=imperial:5:1,shape=helm:morion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t5_wh2 woman  kit=imperial:5:1,shape=helm:morion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t5_wh6 woman  kit=imperial:5:1,shape=helm:morion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t6_mh1 man    kit=imperial:6:1,shape=helm:morion,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t6_mh4 man    kit=imperial:6:1,shape=helm:morion,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t6_wh2 woman  kit=imperial:6:1,shape=helm:morion,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_morion_t6_wh6 woman  kit=imperial:6:1,shape=helm:morion,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t4_mh1 man    kit=dune:4:1,shape=helm:plumed,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t4_mh4 man    kit=dune:4:1,shape=helm:plumed,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t4_wh2 woman  kit=dune:4:1,shape=helm:plumed,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t4_wh6 woman  kit=dune:4:1,shape=helm:plumed,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t5_mh1 man    kit=dune:5:1,shape=helm:plumed,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t5_mh4 man    kit=dune:5:1,shape=helm:plumed,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t5_wh2 woman  kit=dune:5:1,shape=helm:plumed,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t5_wh6 woman  kit=dune:5:1,shape=helm:plumed,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t6_mh1 man    kit=dune:6:1,shape=helm:plumed,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t6_mh4 man    kit=dune:6:1,shape=helm:plumed,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t6_wh2 woman  kit=dune:6:1,shape=helm:plumed,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_plumed_t6_wh6 woman  kit=dune:6:1,shape=helm:plumed,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t4_mh1 man    kit=steppe:4:1,shape=helm:crown,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t4_mh4 man    kit=steppe:4:1,shape=helm:crown,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t4_wh2 woman  kit=steppe:4:1,shape=helm:crown,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t4_wh6 woman  kit=steppe:4:1,shape=helm:crown,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t5_mh1 man    kit=steppe:5:1,shape=helm:crown,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t5_mh4 man    kit=steppe:5:1,shape=helm:crown,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t5_wh2 woman  kit=steppe:5:1,shape=helm:crown,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t5_wh6 woman  kit=steppe:5:1,shape=helm:crown,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t6_mh1 man    kit=steppe:6:1,shape=helm:crown,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t6_mh4 man    kit=steppe:6:1,shape=helm:crown,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t6_wh2 woman  kit=steppe:6:1,shape=helm:crown,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crown_t6_wh6 woman  kit=steppe:6:1,shape=helm:crown,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t4_mh1 man    kit=marsh:4:1,shape=helm:hood,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t4_mh4 man    kit=marsh:4:1,shape=helm:hood,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t4_wh2 woman  kit=marsh:4:1,shape=helm:hood,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t4_wh6 woman  kit=marsh:4:1,shape=helm:hood,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t5_mh1 man    kit=marsh:5:1,shape=helm:hood,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t5_mh4 man    kit=marsh:5:1,shape=helm:hood,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t5_wh2 woman  kit=marsh:5:1,shape=helm:hood,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t5_wh6 woman  kit=marsh:5:1,shape=helm:hood,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t6_mh1 man    kit=marsh:6:1,shape=helm:hood,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t6_mh4 man    kit=marsh:6:1,shape=helm:hood,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t6_wh2 woman  kit=marsh:6:1,shape=helm:hood,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_hood_t6_wh6 woman  kit=marsh:6:1,shape=helm:hood,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t4_mh1 man    kit=jade:4:1,shape=helm:cap,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t4_mh4 man    kit=jade:4:1,shape=helm:cap,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t4_wh2 woman  kit=jade:4:1,shape=helm:cap,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t4_wh6 woman  kit=jade:4:1,shape=helm:cap,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t5_mh1 man    kit=jade:5:1,shape=helm:cap,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t5_mh4 man    kit=jade:5:1,shape=helm:cap,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t5_wh2 woman  kit=jade:5:1,shape=helm:cap,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t5_wh6 woman  kit=jade:5:1,shape=helm:cap,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t6_mh1 man    kit=jade:6:1,shape=helm:cap,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t6_mh4 man    kit=jade:6:1,shape=helm:cap,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t6_wh2 woman  kit=jade:6:1,shape=helm:cap,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_cap_t6_wh6 woman  kit=jade:6:1,shape=helm:cap,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t4_mh1 man    kit=river:4:1,shape=helm:furnace,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t4_mh4 man    kit=river:4:1,shape=helm:furnace,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t4_wh2 woman  kit=river:4:1,shape=helm:furnace,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t4_wh6 woman  kit=river:4:1,shape=helm:furnace,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t5_mh1 man    kit=river:5:1,shape=helm:furnace,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t5_mh4 man    kit=river:5:1,shape=helm:furnace,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t5_wh2 woman  kit=river:5:1,shape=helm:furnace,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t5_wh6 woman  kit=river:5:1,shape=helm:furnace,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t6_mh1 man    kit=river:6:1,shape=helm:furnace,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t6_mh4 man    kit=river:6:1,shape=helm:furnace,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t6_wh2 woman  kit=river:6:1,shape=helm:furnace,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_furnace_t6_wh6 woman  kit=river:6:1,shape=helm:furnace,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t4_mh1 man    kit=suntemple:4:1,shape=helm:crystal,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t4_mh4 man    kit=suntemple:4:1,shape=helm:crystal,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t4_wh2 woman  kit=suntemple:4:1,shape=helm:crystal,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t4_wh6 woman  kit=suntemple:4:1,shape=helm:crystal,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t5_mh1 man    kit=suntemple:5:1,shape=helm:crystal,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t5_mh4 man    kit=suntemple:5:1,shape=helm:crystal,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t5_wh2 woman  kit=suntemple:5:1,shape=helm:crystal,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t5_wh6 woman  kit=suntemple:5:1,shape=helm:crystal,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t6_mh1 man    kit=suntemple:6:1,shape=helm:crystal,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t6_mh4 man    kit=suntemple:6:1,shape=helm:crystal,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t6_wh2 woman  kit=suntemple:6:1,shape=helm:crystal,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_crystal_t6_wh6 woman  kit=suntemple:6:1,shape=helm:crystal,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t4_mh1 man    kit=sylvan:4:1,shape=helm:ladder,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t4_mh4 man    kit=sylvan:4:1,shape=helm:ladder,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t4_wh2 woman  kit=sylvan:4:1,shape=helm:ladder,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t4_wh6 woman  kit=sylvan:4:1,shape=helm:ladder,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t5_mh1 man    kit=sylvan:5:1,shape=helm:ladder,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t5_mh4 man    kit=sylvan:5:1,shape=helm:ladder,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t5_wh2 woman  kit=sylvan:5:1,shape=helm:ladder,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t5_wh6 woman  kit=sylvan:5:1,shape=helm:ladder,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t6_mh1 man    kit=sylvan:6:1,shape=helm:ladder,hair=1,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t6_mh4 man    kit=sylvan:6:1,shape=helm:ladder,hair=4,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t6_wh2 woman  kit=sylvan:6:1,shape=helm:ladder,hair=2,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9_ladder_t6_wh6 woman  kit=sylvan:6:1,shape=helm:ladder,hair=6,hairVar=0,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9s_kettle_mh0 man    kit=steppe:6:1,shape=helm:kettle,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh1 man    kit=steppe:6:1,shape=helm:kettle,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh2 man    kit=steppe:6:1,shape=helm:kettle,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh3 man    kit=steppe:6:1,shape=helm:kettle,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh4 man    kit=steppe:6:1,shape=helm:kettle,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh5 man    kit=steppe:6:1,shape=helm:kettle,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh6 man    kit=steppe:6:1,shape=helm:kettle,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_mh7 man    kit=steppe:6:1,shape=helm:kettle,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh0 woman  kit=steppe:6:1,shape=helm:kettle,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh1 woman  kit=steppe:6:1,shape=helm:kettle,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh2 woman  kit=steppe:6:1,shape=helm:kettle,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh3 woman  kit=steppe:6:1,shape=helm:kettle,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh4 woman  kit=steppe:6:1,shape=helm:kettle,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh5 woman  kit=steppe:6:1,shape=helm:kettle,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh6 woman  kit=steppe:6:1,shape=helm:kettle,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kettle_wh7 woman  kit=steppe:6:1,shape=helm:kettle,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh0 man    kit=steppe:6:1,shape=helm:kabuto,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh1 man    kit=steppe:6:1,shape=helm:kabuto,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh2 man    kit=steppe:6:1,shape=helm:kabuto,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh3 man    kit=steppe:6:1,shape=helm:kabuto,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh4 man    kit=steppe:6:1,shape=helm:kabuto,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh5 man    kit=steppe:6:1,shape=helm:kabuto,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh6 man    kit=steppe:6:1,shape=helm:kabuto,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_mh7 man    kit=steppe:6:1,shape=helm:kabuto,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh0 woman  kit=steppe:6:1,shape=helm:kabuto,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh1 woman  kit=steppe:6:1,shape=helm:kabuto,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh2 woman  kit=steppe:6:1,shape=helm:kabuto,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh3 woman  kit=steppe:6:1,shape=helm:kabuto,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh4 woman  kit=steppe:6:1,shape=helm:kabuto,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh5 woman  kit=steppe:6:1,shape=helm:kabuto,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh6 woman  kit=steppe:6:1,shape=helm:kabuto,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_kabuto_wh7 woman  kit=steppe:6:1,shape=helm:kabuto,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh0 man    kit=steppe:6:1,shape=helm:crown,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh1 man    kit=steppe:6:1,shape=helm:crown,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh2 man    kit=steppe:6:1,shape=helm:crown,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh3 man    kit=steppe:6:1,shape=helm:crown,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh4 man    kit=steppe:6:1,shape=helm:crown,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh5 man    kit=steppe:6:1,shape=helm:crown,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh6 man    kit=steppe:6:1,shape=helm:crown,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_mh7 man    kit=steppe:6:1,shape=helm:crown,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh0 woman  kit=steppe:6:1,shape=helm:crown,hair=0,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh1 woman  kit=steppe:6:1,shape=helm:crown,hair=1,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh2 woman  kit=steppe:6:1,shape=helm:crown,hair=2,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh3 woman  kit=steppe:6:1,shape=helm:crown,hair=3,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh4 woman  kit=steppe:6:1,shape=helm:crown,hair=4,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh5 woman  kit=steppe:6:1,shape=helm:crown,hair=5,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh6 woman  kit=steppe:6:1,shape=helm:crown,hair=6,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9s_crown_wh7 woman  kit=steppe:6:1,shape=helm:crown,hair=7,hairVar=1,palette=look  stay,walk,run,didle,fall,lying\n"
    "m9l_nasal_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_nasal_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_nasal_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_nasal_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_nasal_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_nasal_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=1,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_kettle_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=2,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_greathelm_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=3,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_spangen_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=4,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_horned_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=5,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_plumed_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=6,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_conical_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_masked_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=8,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_crested_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=9,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t2_m man    hero=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t2_w woman  heroine=1,kit=heartland:2:7,helmet=1,helmStyle=2,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t3_m man    hero=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t3_w woman  heroine=1,kit=heartland:3:7,helmet=1,helmStyle=3,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t4_m man    hero=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m9l_winged_t4_w woman  heroine=1,kit=heartland:4:7,helmet=1,helmStyle=3,helmForm=10,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "m10_round_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_round_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_round_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=1,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_kite_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_kite_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_kite_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_kite_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_kite_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=2,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_heater_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_heater_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=3,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_tower_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_tower_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_crescent_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_crescent_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_crescent_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_crescent_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_crescent_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying,block\n"
    "m10_crescent_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying,block\n"
    "m10_crescent_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying,block\n"
    "m10_crescent_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=5,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying,block\n"
    "m10_oval_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_oval_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_oval_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_oval_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_oval_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=6,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_buckler_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_buckler_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_buckler_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_buckler_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_buckler_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=7,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_leaf_sword_m_t3 man    kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_sword_m_t5 man    kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_sword_w_t3 woman  kit=imperial:3:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_sword_w_t5 woman  kit=imperial:5:2,wpn=sword,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_axe_m_t3 man    kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_axe_m_t5 man    kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_axe_w_t3 woman  kit=imperial:3:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_axe_w_t5 woman  kit=imperial:5:2,wpn=axe,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_mace_m_t3 man    kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_mace_m_t5 man    kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_mace_w_t3 woman  kit=imperial:3:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_mace_w_t5 woman  kit=imperial:5:2,wpn=mace,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,"
    "fall,lying,block\n"
    "m10_leaf_dagger_m_t3 man    kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_leaf_dagger_m_t5 man    kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_leaf_dagger_w_t3 woman  kit=imperial:3:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m10_leaf_dagger_w_t5 woman  kit=imperial:5:2,wpn=dagger,shield=1,shieldStyle=3,shieldForm=8,palette=look  stay,walk,run,dwalk,drun,didle,"
    "hit1,fall,lying,block\n"
    "m11_sword_straight_m man    kit=dune:3:1,wpn=sword,bladeForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_straight_m man    kit=dune:3:1,wpn=greatsword,bladeForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_leaf_m man    kit=dune:3:1,wpn=sword,bladeForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_leaf_m man    kit=dune:3:1,wpn=greatsword,bladeForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_falchion_m man    kit=dune:3:1,wpn=sword,bladeForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_falchion_m man    kit=dune:3:1,wpn=greatsword,bladeForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_scimitar_m man    kit=dune:3:1,wpn=sword,bladeForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_scimitar_m man    kit=dune:3:1,wpn=greatsword,bladeForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_khopesh_m man    kit=dune:3:1,wpn=sword,bladeForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_khopesh_m man    kit=dune:3:1,wpn=greatsword,bladeForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_wavy_m man    kit=dune:3:1,wpn=sword,bladeForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_wavy_m man    kit=dune:3:1,wpn=greatsword,bladeForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_broad_m man    kit=dune:3:1,wpn=sword,bladeForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_broad_m man    kit=dune:3:1,wpn=greatsword,bladeForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_curved_m man    kit=dune:3:1,wpn=sword,bladeForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_curved_m man    kit=dune:3:1,wpn=greatsword,bladeForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_glaive_m man    kit=dune:3:1,wpn=sword,bladeForm=9,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_glaive_m man    kit=dune:3:1,wpn=greatsword,bladeForm=9,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_axe_m man    kit=dune:3:1,wpn=axe,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_mace_m man    kit=dune:3:1,wpn=mace,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_dagger_m man    kit=dune:3:1,wpn=dagger,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_staff_m man    kit=dune:3:1,wpn=staff,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_spear_m man    kit=dune:3:1,wpn=spear,polearmForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_glaive_m man    kit=dune:3:1,wpn=spear,polearmForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_halberd_m man    kit=dune:3:1,wpn=spear,polearmForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_classic_m man    kit=dune:3:1,wpn=bow,bowForm=0,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_self_m man    kit=dune:3:1,wpn=bow,bowForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_recurve_m man    kit=dune:3:1,wpn=bow,bowForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_composite_m man    kit=dune:3:1,wpn=bow,bowForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_crossbow_m man    kit=dune:3:1,wpn=bow,bowForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_longbow_m man    kit=dune:3:1,wpn=bow,bowForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_straight_w woman  kit=dune:3:1,wpn=sword,bladeForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_straight_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_leaf_w woman  kit=dune:3:1,wpn=sword,bladeForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_leaf_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_falchion_w woman  kit=dune:3:1,wpn=sword,bladeForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_falchion_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_scimitar_w woman  kit=dune:3:1,wpn=sword,bladeForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_scimitar_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_khopesh_w woman  kit=dune:3:1,wpn=sword,bladeForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_khopesh_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_wavy_w woman  kit=dune:3:1,wpn=sword,bladeForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_wavy_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=6,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_broad_w woman  kit=dune:3:1,wpn=sword,bladeForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_broad_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_curved_w woman  kit=dune:3:1,wpn=sword,bladeForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_curved_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=8,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_sword_glaive_w woman  kit=dune:3:1,wpn=sword,bladeForm=9,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_great_glaive_w woman  kit=dune:3:1,wpn=greatsword,bladeForm=9,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_axe_w woman  kit=dune:3:1,wpn=axe,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_mace_w woman  kit=dune:3:1,wpn=mace,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_dagger_w woman  kit=dune:3:1,wpn=dagger,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_staff_w woman  kit=dune:3:1,wpn=staff,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_spear_w woman  kit=dune:3:1,wpn=spear,polearmForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_glaive_w woman  kit=dune:3:1,wpn=spear,polearmForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_pole_halberd_w woman  kit=dune:3:1,wpn=spear,polearmForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_classic_w woman  kit=dune:3:1,wpn=bow,bowForm=0,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_self_w woman  kit=dune:3:1,wpn=bow,bowForm=1,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_recurve_w woman  kit=dune:3:1,wpn=bow,bowForm=2,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_composite_w woman  kit=dune:3:1,wpn=bow,bowForm=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_crossbow_w woman  kit=dune:3:1,wpn=bow,bowForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m11_bow_longbow_w woman  kit=dune:3:1,wpn=bow,bowForm=5,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t4_m_tower man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_round_t4_m_spear man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t4_m_great man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t4_w_tower woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_round_t4_w_spear woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t4_w_great woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:round+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t6_m_tower man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_round_t6_m_spear man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_round_t6_m_great man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_round_t6_w_tower woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_round_t6_w_spear woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_round_t6_w_great woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:round+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t4_m_tower man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_broad_t4_m_spear man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t4_m_great man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t4_w_tower woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_broad_t4_w_spear woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t4_w_great woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t6_m_tower man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_broad_t6_m_spear man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_broad_t6_m_great man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_broad_t6_w_tower woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_broad_t6_w_spear woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_broad_t6_w_great woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:broad+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_tall_t4_m_tower man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_tall_t4_m_spear man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_tall_t4_m_great man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_tall_t4_w_tower woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_tall_t4_w_spear woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_tall_t4_w_great woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_tall_t6_m_tower man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_tall_t6_m_spear man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying\n"
    "m12_tall_t6_m_great man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_tall_t6_w_tower woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_tall_t6_w_spear woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying\n"
    "m12_tall_t6_w_great woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:tall+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_drop_t4_m_tower man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_drop_t4_m_spear man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_drop_t4_m_great man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_drop_t4_w_tower woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_drop_t4_w_spear woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_drop_t4_w_great woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_drop_t6_m_tower man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_drop_t6_m_spear man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying\n"
    "m12_drop_t6_m_great man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_drop_t6_w_tower woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,shieldForm=4,"
    "palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_drop_t6_w_spear woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,drun,"
    "didle,hit1,fall,lying\n"
    "m12_drop_t6_w_great woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:drop+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_mantle_t4_m_tower man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,"
    "shieldForm=4,crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_mantle_t4_m_spear man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_mantle_t4_m_great man    kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,"
    "walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_mantle_t4_w_tower woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=sword,shield=1,shieldStyle=3,"
    "shieldForm=4,crest=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_mantle_t4_w_spear woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=spear,crest=3,palette=look  stay,walk,"
    "run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_mantle_t4_w_great woman  kit=fjordfolk:4:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=2,wpn=greatsword,crest=3,palette=look  stay,"
    "walk,run,dwalk,drun,didle,hit1,fall,lying\n"
    "m12_mantle_t6_m_tower man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,"
    "shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_mantle_t6_m_spear man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_mantle_t6_m_great man    kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n"
    "m12_mantle_t6_w_tower woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=sword,shield=1,shieldStyle=3,"
    "shieldForm=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,fall,lying,block\n"
    "m12_mantle_t6_w_spear woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=spear,palette=look  stay,walk,run,dwalk,"
    "drun,didle,hit1,fall,lying\n"
    "m12_mantle_t6_w_great woman  kit=fjordfolk:6:1,bodyForm=7,shape=mass:mantle+layer:cape,cloak=3,wpn=greatsword,palette=look  stay,walk,run,"
    "dwalk,drun,didle,hit1,fall,lying\n";

uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
HumanLook a3Hero() {
  HumanLook L;
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0);
  return L;
}
HumanLook a3Woman() {
  HumanLook L;
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.outfit = art::Outfit::Dress;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A);
  return L;
}

struct Look { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; };
std::vector<Look> parseLooks(const std::string& text) {
  std::vector<Look> v;
  size_t a = 0;
  while (a < text.size()) {
    size_t b = text.find('\n', a);
    if (b == std::string::npos) b = text.size();
    const std::string line = text.substr(a, b - a);
    a = b + 1;
    if (line.empty() || line[0] == '#') continue;
    char tag[128] = {}, who[16] = {}, spec[800] = {}, gaits[200] = {};
    if (std::sscanf(line.c_str(), "%127s %15s %799s %199s", tag, who, spec, gaits) < 2) continue;
    Look l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") ? spec : "";
    std::string g = gaits;
    size_t c = 0;
    while (c < g.size()) { size_t d = g.find(',', c); if (d == std::string::npos) d = g.size(); if (d > c) l.gaits.push_back(g.substr(c, d - c)); c = d + 1; }
    v.push_back(l);
  }
  return v;
}
std::vector<Look> readLooks(const char* file) {
  if (!file) return parseLooks(kSliceLooks);
  FILE* f = std::fopen(file, "rb");
  if (!f) { std::printf("FAIL: --looks %s: cannot read\n", file); return {}; }
  std::string t;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) t.append(buf, n);
  std::fclose(f);
  return parseLooks(t);
}
struct Built { HumanLook L; doll::DollRecipe r; bool lookPal = false; };
Built build(const Look& l) {
  Built b;
  b.L = l.who ? a3Woman() : a3Hero();
  std::string bad;
  bool bare = false;
  doll::applyLookSpec(b.L, l.spec, &b.lookPal, &bare, &bad);
  if (!bad.empty()) std::printf("  arms: %s: unknown field(s) %s\n", l.tag.c_str(), bad.c_str());
  b.r = doll::resolve(b.L);
  b.r.ramps = b.lookPal ? doll::rampsOf(b.L) : doll::a3Palette(b.L.body == 1 ? 1 : 0);
  return b;
}
bool armoured(const HumanLook& L) {
  return L.bodyForm || L.armorStyle || L.outfit == art::Outfit::Leather || L.outfit == art::Outfit::Chain || L.outfit == art::Outfit::Plate ||
         L.outfit == art::Outfit::Guard || L.outfit == art::Outfit::Elven || L.outfit == art::Outfit::Ebony;
}
bool fighter(const HumanLook& L) { return L.weapon >= 1 && L.weapon <= 8 && !L.civil; }

// ---------------------------------------------------------------- frame enumeration
struct FrameId { RigPart part; int facing; Clip clip; int variant; int i; };
const char* partName(RigPart p) {
  switch (p) {
    case RigPart::Loco: return "LOCO";
    case RigPart::Run: return "RUN";
    case RigPart::Drawn: return "DRAWN";
    case RigPart::Action: return "ACTION";
    case RigPart::Death: return "DEATH";
    default: return "?";
  }
}
std::string frameName(const std::string& tag, const FrameId& f) {
  return fmt("%s %s %s v%d %c%d", tag.c_str(), partName(f.part), art::clipName(f.clip), f.variant, "DURL"[f.facing & 3], f.i);
}
std::vector<FrameId> framesOf(const doll::DollRecipe& r, RigPart part) {
  std::vector<FrameId> v;
  const RigSheet s = doll::compose(r, part);
  for (int fc = 0; fc < 4; fc++)
    for (int c = 0; c < art::CLIP_COUNT; c++)
      for (int va = 0; va < art::RIG_VARIANTS; va++) {
        const RigSheet::Span& sp = s.spans[RigSheet::slot(fc, (Clip)c, va)];
        if (sp.count <= 0) continue;
        for (int i = 0; i < sp.count; i++) v.push_back({part, fc, (Clip)c, va, i});
      }
  return v;
}
std::vector<RigPart> partsOf(const HumanLook& L) {
  std::vector<RigPart> v{RigPart::Loco, RigPart::Run, RigPart::Death};
  if (fighter(L)) { v.push_back(RigPart::Drawn); v.push_back(RigPart::Action); }
  return v;
}

// ---------------------------------------------------------------- probe helpers
bool opaque(const doll::FrameProbe& p, int k) { return k >= 0 && p.code[(size_t)k] >= 2; }
int ownerOf(const doll::FrameProbe& p, int k) { return k >= 0 ? p.owner[(size_t)k] : -1; }
const doll::FrameProbe::It* itemAt(const doll::FrameProbe& p, int k) {
  const int o = ownerOf(p, k);
  return o >= 0 && o < (int)p.items.size() ? &p.items[(size_t)o] : nullptr;
}
bool isGear(const doll::FrameProbe::It& it) {
  return it.has(doll::Role::Armour) || it.has(doll::Role::Helm) || it.has(doll::Role::Pauldron) ||
         (it.has(doll::Role::Shield) && it.anchor == art::doll::Anchor::GripL) ||
         (it.has(doll::Role::Overlay) && (it.part.find("tabard") != std::string::npos || it.part.find("armour/") == 0));
}
// (wave-2 fix round, D81) every library weapon, the spear included (the probe names a part by its library value wherever its map
// lists it); A3's sword (held/weapons/sword) is contracts 2's one exception, re-cut into the arm, and stays out as before
bool isLibraryHeld(const doll::FrameProbe::It& it) {
  return it.has(doll::Role::Held) && it.held.find('.') != std::string::npos && it.part != "held/weapons/sword";
}
bool isBow(const doll::FrameProbe::It& it) { return it.has(doll::Role::Held) && it.held.rfind("bow", 0) == 0; }
bool isShield(const doll::FrameProbe::It& it) { return it.has(doll::Role::Shield) && it.anchor == art::doll::Anchor::GripL; }
bool isHelm(const doll::FrameProbe::It& it) { return it.has(doll::Role::Helm); }
doll::Slot slotAt(const doll::FrameProbe& p, int k) { return doll::codeSlot(p.code[(size_t)k]); }

// ---------------------------------------------------------------- C3 checks (pure: synthetic probes test them)
// a: enclosed holes inside gear (4-connected clear pockets not reached from the canvas edge, every edge pixel gear)
int holesInGear(const doll::FrameProbe& p, std::string* why) {
  std::vector<uint8_t> seen((size_t)p.w * p.h, 0);
  std::vector<int> st;
  for (int x = 0; x < p.w; x++) for (int y : {0, p.h - 1}) { const int k = y * p.w + x; if (!opaque(p, k)) { seen[(size_t)k] = 1; st.push_back(k); } }
  for (int y = 0; y < p.h; y++) for (int x : {0, p.w - 1}) { const int k = y * p.w + x; if (!opaque(p, k) && !seen[(size_t)k]) { seen[(size_t)k] = 1; st.push_back(k); } }
  auto flood = [&]() {
    while (!st.empty()) {
      const int k = st.back(); st.pop_back();
      const int x = k % p.w, y = k / p.w;
      const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
      for (auto& n : nb) {
        const int q = p.at(n[0], n[1]);
        if (q < 0 || seen[(size_t)q] || opaque(p, q)) continue;
        seen[(size_t)q] = 1;
        st.push_back(q);
      }
    }
  };
  flood();
  int holes = 0;
  for (int k = 0; k < p.w * p.h; k++) {
    if (seen[(size_t)k] || opaque(p, k)) continue;
    // one pocket: collect it, then judge its edge
    std::vector<int> pocket{k};
    seen[(size_t)k] = 1;
    for (size_t j = 0; j < pocket.size(); j++) {
      const int x = pocket[j] % p.w, y = pocket[j] / p.w;
      const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
      for (auto& n : nb) {
        const int q = p.at(n[0], n[1]);
        if (q < 0 || seen[(size_t)q] || opaque(p, q)) continue;
        seen[(size_t)q] = 1;
        pocket.push_back(q);
      }
    }
    bool allGear = true, bowEdge = false;
    for (int q : pocket) {
      const int x = q % p.w, y = q / p.w;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const int n = p.at(x + dx, y + dy);
          if (n < 0 || !opaque(p, n)) continue;
          const doll::FrameProbe::It* it = itemAt(p, n);
          if (it && isBow(*it)) bowEdge = true;
          if ((dx == 0) != (dy == 0) && (!it || !isGear(*it))) allGear = false;
        }
    }
    if (allGear && !bowEdge) {
      holes++;
      if (why && why->empty()) {
        const doll::FrameProbe::It* it = nullptr;
        const int x = pocket[0] % p.w, y = pocket[0] / p.w;
        for (int n : {p.at(x - 1, y), p.at(x + 1, y), p.at(x, y - 1), p.at(x, y + 1)}) if (!it) it = itemAt(p, n);
        *why = fmt("a %d px hole in %s at (%d,%d)", (int)pocket.size(), it ? it->part.c_str() : "gear", x - p.px, y - p.py);
        if (std::getenv("EMB_C3_DEBUG")) {
          for (int dy = -3; dy <= 3; dy++) {
            std::string row;
            for (int dx = -4; dx <= 4; dx++) {
              const int k = p.at(x + dx, y + dy);
              row += (k < 0 || !opaque(p, k)) ? (dx == 0 && dy == 0 ? " O" : " .") : fmt("%2d", ownerOf(p, k));
            }
            std::printf("      hole%+d %s\n", dy, row.c_str());
          }
          for (size_t i = 0; i < p.items.size(); i++) std::printf("      item %zu %s key %s at %d,%d hole %d,%d\n", i, p.items[i].part.c_str(), p.items[i].key.c_str(), p.items[i].ax, p.items[i].ay, x, y);
        }
      }
    }
  }
  return holes;
}
// b: the helm on the head. (P2c envelope, C3b by tier; PLAN "The envelope", kEnvelope) T0-T4 keep today's rules exactly:
// a plume to F-37, every other helm pixel inside the head box + 1 column and 1 row. T5-T6 read the envelope as C12f
// measures it, from the head anchor (so the bob, a hurt or a fall never counts): rows over the crown (a crest or plume part,
// or the plume slot: ornRows; the shell: rows) and columns past the 16 px head box (horns, wings, flares, brims, tails and
// the crest parts: flare; the shell: side).
int crownOffsetArms(int who) {   // the crown (F-31, the woman F-30) less the head anchor's row, at the A3 figure's idle D
  static int off[2] = {9999, 9999};
  who = who ? 1 : 0;
  if (off[who] == 9999) {
    HumanLook L = who ? a3Woman() : a3Hero();
    L.weapon = 0;
    const doll::FrameProbe f = doll::probeFrame(doll::resolve(L), art::RigPart::Loco, 0, art::Clip::Idle, 0, 0);
    off[who] = f.ok ? (f.py - (who ? 30 : 31)) - f.headY : 0;
  }
  return off[who];
}
bool helmOrnPart(const std::string& n) { return n.find("helm/crest/") != std::string::npos || n.find("_crest") != std::string::npos || n.find("_plume") != std::string::npos; }
bool helmFlarePart(const std::string& n) {
  for (const char* w : {"horn", "wing", "kabuto", "warhat", "morion", "sallet", "kettle", "leathercap"}) if (n.find(w) != std::string::npos) return true;
  return false;
}
std::string helmCheck(const doll::FrameProbe& p, int helmTier = 0, int who = 0) {
  if (p.turn) return "";
  int helmItem = -1;
  for (size_t i = 0; i < p.items.size(); i++)
    if (isHelm(p.items[i])) {
      if (p.items[i].anchor != art::doll::Anchor::Head) return p.items[i].part + ": not anchored on the head";
      helmItem = (int)i;
    }
  if (helmItem < 0) return "";
  std::vector<int> top((size_t)p.w, 1 << 20);
  for (int y = 0; y < p.h; y++)
    for (int x = 0; x < p.w; x++) {
      const int k = y * p.w + x;
      const doll::FrameProbe::It* it = itemAt(p, k);
      if (!opaque(p, k) || !it || !isHelm(*it)) continue;
      const bool plume = slotAt(p, k) == doll::Slot::Plume;
      if (helmTier >= 1) {   // (wave 2 r3, DOLL-C; PLAN Gates C3b) every tier reads kEnvelope; untiered keeps today's rules
        const art::doll::TierEnvelope& e = art::doll::envelopeOf(helmTier);
        const bool orn = plume || helmOrnPart(it->part);
        const int up = p.headY + crownOffsetArms(who) - y;
        const int col = x < p.headX ? p.headX - x : x > p.headX + 15 ? x - (p.headX + 15) : 0;
        if (up > (orn ? e.ornRows : e.rows)) return fmt("%s: %s %d rows over the crown (T%d: %d)", it->part.c_str(), orn ? "crest or plume" : "helm", up, helmTier, orn ? e.ornRows : e.rows);
        const int cap = (orn || helmFlarePart(it->part)) ? e.flare : it->part.find("brim") != std::string::npos ? std::max(e.side, e.brim) : e.side;
        if (col > cap) return fmt("%s: helm pixel %d columns past the head box (T%d: %d)", it->part.c_str(), col, helmTier, cap);
        if (!orn && col <= 1) top[(size_t)x] = std::min(top[(size_t)x], y);   // the rim: the shell over the head box + 1, as T0-T4
        continue;
      }
      if (plume) { if (y - p.py < -37) return fmt("%s: plume pixel at F%d (F-37 at most)", it->part.c_str(), y - p.py); continue; }
      if (x < p.headX - 1 || x > p.headX + 16) return fmt("%s: helm pixel %d columns outside the head box", it->part.c_str(), x < p.headX - 1 ? p.headX - 1 - x : x - p.headX - 16);
      if (y < p.headY - 1) return fmt("%s: helm pixel %d rows over the head box", it->part.c_str(), p.headY - 1 - y);
      top[(size_t)x] = std::min(top[(size_t)x], y);
    }
  for (int x = 0; x < p.w; x++) {
    if (top[(size_t)x] == (1 << 20)) continue;
    for (int y = 0; y < top[(size_t)x]; y++) {
      const int k = y * p.w + x;
      const doll::FrameProbe::It* it = itemAt(p, k);
      if (opaque(p, k) && it && it->has(doll::Role::Hair)) return fmt("%s: hair over the helm's rim (column %d)", it->part.c_str(), x - p.px);
    }
  }
  return "";
}
// c: the shield on the off arm. bare: the same frame without the shield (its boot, shin and figure box)
std::string shieldCheck(const doll::FrameProbe& p, const doll::FrameProbe* bare, bool run, int facing) {
  // (INTEGRATE forge i1) a shield is its base view plus its `when` overlays (tier fields, forge boss-/rim- styles): every
  // shield item counts as the shield, the base (the first) names it; before, the last overlay alone (a ring of rivets) was
  // tested against the hand, the shins and the run box
  int si = -1;
  std::vector<char> isSh(p.items.size(), 0);
  for (size_t i = 0; i < p.items.size(); i++)
    if (isShield(p.items[i])) { isSh[i] = 1; if (si < 0) si = (int)i; }
  if (si < 0) return "";
  auto shOwner = [&](int k) { const int o = ownerOf(p, k); return o >= 0 && o < (int)isSh.size() && isSh[(size_t)o]; };
  const doll::FrameProbe::It& sh = p.items[(size_t)si];
  if (sh.anchor != art::doll::Anchor::GripL) return sh.part + ": not on gripL";
  if (!p.turn) {
    // (INTEGRATE i1, SLICE-HELM-SHIELD by eye) facing right the raised shield (guard, block) is brought forward across the
    // chest, still behind the near (sword) arm: layer neck at most; every other key stays behind the torso
    const bool raised = sh.key == "guard" || sh.key == "block";
    if (facing == 2 && (int)sh.layer > (int)(raised ? doll::Layer::Neck : doll::Layer::Torso))
      return sh.part + (raised ? ": in front of the near arm facing right (neck at most)" : ": in front of the torso facing right (behind it)");
    if ((facing == 3 || facing == 0) && (int)sh.layer < (int)doll::Layer::Torso) return sh.part + fmt(": behind the torso facing %c (in front)", "DURL"[facing]);
  }
  // attached: a shield pixel within 1 px of the gripL point
  if (p.gripL) {
    bool near = false;
    for (int dy = -1; dy <= 1 && !near; dy++)
      for (int dx = -1; dx <= 1 && !near; dx++) {
        const int k = p.at(p.gripLX + dx, p.gripLY + dy);
        if (k < 0) continue;
        if (shOwner(k)) near = true;
        // (under the hand: the shield drawn first, the fist over it)
        if (!near && opaque(p, k) && p.gripLFrom >= 0 && ownerOf(p, k) == p.gripLFrom) near = true;
      }
    // (SLICE-DOLL i2, by eye) facing right the far shield arm draws nothing (its block view is empty: the hand behind the
    // raised kite); with no hand drawn there is nothing to touch, the arm is hidden behind the shield it holds
    // (INTEGRATE forge i2, by eye) facing up the T5 surcoat and the noble cloak hang over the far arm: the grip and the
    // shield's inner edge lie under the cloak. Attached when the grip's 3 x 3 is all cloak drawn over the shield and a shield
    // pixel shows within 3 px of the grip (the shield runs on under the cloak to the hand)
    if (!near) {
      bool allCloak = true, shNear = false;
      for (int dy = -1; dy <= 1 && allCloak; dy++)
        for (int dx = -1; dx <= 1 && allCloak; dx++) {
          const int k = p.at(p.gripLX + dx, p.gripLY + dy);
          const int o = k < 0 ? -1 : ownerOf(p, k);
          if (k < 0 || !opaque(p, k) || o < 0 || o <= si || !p.items[(size_t)o].has(doll::Role::Cloak)) allCloak = false;
        }
      for (int dy = -3; dy <= 3 && allCloak && !shNear; dy++)
        for (int dx = -3; dx <= 3 && !shNear; dx++) {
          const int k = p.at(p.gripLX + dx, p.gripLY + dy);
          if (k >= 0 && shOwner(k)) shNear = true;
        }
      if (allCloak && shNear) near = true;
      // (wave-2 fix round, D82) facing U a cloak may cover the grip: attached when the shield's visible pixels touch the cloak
      // part that covers the grip
      if (!near) {
        const int kg = p.at(p.gripLX, p.gripLY);
        const int co = kg < 0 ? -1 : ownerOf(p, kg);
        if (co >= 0 && opaque(p, kg) && p.items[(size_t)co].has(doll::Role::Cloak))
          for (int k = 0; k < p.w * p.h && !near; k++) {
            if (!shOwner(k) || !opaque(p, k)) continue;
            const int x = k % p.w, y = k / p.w;
            for (int d = 0; d < 4 && !near; d++) {
              const int k2 = p.at(x + (d == 0) - (d == 1), y + (d == 2) - (d == 3));
              const int o2 = k2 >= 0 && opaque(p, k2) ? ownerOf(p, k2) : -1;
              if (o2 == co || (o2 >= 0 && p.items[(size_t)co].partIndex >= 0 && p.items[(size_t)o2].partIndex == p.items[(size_t)co].partIndex)) near = true;
            }
          }
      }
    }
    bool handDrawn = p.gripLFrom < 0;
    for (int k = 0; k < p.w * p.h && !handDrawn; k++) if (opaque(p, k) && ownerOf(p, k) == p.gripLFrom) handDrawn = true;
    if (!near && handDrawn && std::getenv("EMB_C3_DEBUG")) {
      for (int dy = -3; dy <= 3; dy++) {
        std::string row;
        for (int dx = -4; dx <= 4; dx++) {
          const int k = p.at(p.gripLX + dx, p.gripLY + dy);
          const int o = k < 0 ? -1 : ownerOf(p, k);
          row += (k < 0 || !opaque(p, k)) ? " ." : o == p.gripLFrom ? " H" : shOwner(k) ? " S" : fmt("%2d", o);
        }
        std::printf("      grip%+d %s\n", dy, row.c_str());
      }
      for (size_t i = 0; i < p.items.size(); i++) std::printf("      item %zu %s\n", i, p.items[i].part.c_str());
    }
    if (!near && handDrawn) return sh.part + ": not touching the gripping hand";
  } else if (!p.turn) {
    return sh.part + ": no gripL mark (the shield arm marks it)";
  }
  if (bare && !p.turn) {
    std::set<int> shinCols;
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
    for (int k = 0; k < p.w * p.h; k++)
      if (opaque(*bare, k)) { const int x = k % p.w, y = k / p.w; x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    for (int k = 0; k < p.w * p.h; k++) {
      if (!shOwner(k) || !opaque(p, k)) continue;
      const uint8_t rg = bare->region[(size_t)k];
      if (opaque(*bare, k) && (rg == (uint8_t)doll::Region::Boot || rg == (uint8_t)doll::Region::Sole)) return sh.part + ": over a boot";
      if (opaque(*bare, k) && rg == (uint8_t)doll::Region::Shin) shinCols.insert(k % p.w);
      if (run) {
        const int x = k % p.w, y = k / p.w;
        if (x < x0 - 1 || x > x1 + 1 || y < y0 - 1 || y > y1 + 1) {
          if (std::getenv("EMB_C3_DEBUG")) {
            std::printf("      runbox %d..%d x %d..%d, shield px %d,%d (pivot %d,%d)\n", x0, x1, y0, y1, x, y, p.px, p.py);
            for (size_t i = 0; i < p.items.size(); i++) std::printf("      item %zu %s key %s at %d,%d\n", i, p.items[i].part.c_str(), p.items[i].key.c_str(), p.items[i].ax, p.items[i].ay);
          }
          return sh.part + ": outside the run box + 1";
        }
      }
    }
    if (shinCols.size() > 1) return sh.part + fmt(": over %d columns of a shin (1 at most)", (int)shinCols.size());
  }
  return "";
}
// (wave-2 fix round 4, D90) the hidden-grip proof, every term on the composed frame p (bare: the same frame without the
// weapon, which tells the body's own outline from the weapon's where the outline pass owns a pixel):
//   h1 covered: the grip mark lies on an opaque pixel of a non-weapon part of the figure (or the outline round them);
//      on a clear pixel "the fist missing in the open", on a weapon pixel "a weapon in front with no fist";
//   h2 attached: every 8-connected group of visible weapon pixels touches a non-weapon pixel of the figure.
// No clearance round the grip is asked (a fist at the silhouette's edge has its blade leave the body right there).
// Returns "" when both hold, else the failing term.
std::string hiddenGripProof(const doll::FrameProbe& p, const doll::FrameProbe* bare, int wi, int gx, int gy) {
  const bool bareOk = bare && bare->w == p.w && bare->h == p.h;
  auto heldLib = [&](int o) { return o >= 0 && o < (int)p.items.size() && isLibraryHeld(p.items[(size_t)o]); };
  // a weapon pixel: written by a library weapon, or an unowned (outline) pixel the weaponless frame does not have
  auto weaponPx = [&](int k) {
    if (k < 0 || !opaque(p, k)) return false;
    const int o = ownerOf(p, k);
    if (o == wi || heldLib(o)) return true;
    return o < 0 && bareOk && !opaque(*bare, k);
  };
  auto figurePx = [&](int k) { return k >= 0 && opaque(p, k) && !weaponPx(k); };
  const int gk = p.at(gx, gy);
  if (gk < 0 || !opaque(p, gk)) return fmt("h1: the grip (%d,%d) on a clear pixel (the fist missing in the open)", gx - p.px, gy - p.py);
  if (weaponPx(gk)) return fmt("h1: the grip (%d,%d) on a weapon pixel (a weapon in front with no fist)", gx - p.px, gy - p.py);
  std::vector<uint8_t> seen((size_t)p.w * p.h, 0);
  for (int k0 = 0; k0 < p.w * p.h; k0++) {
    if (seen[(size_t)k0] || ownerOf(p, k0) != wi || !opaque(p, k0)) continue;
    std::vector<int> st{k0};
    seen[(size_t)k0] = 1;
    bool touch = false;
    int n = 0, fx = k0 % p.w, fy = k0 / p.w;
    while (!st.empty()) {
      const int k = st.back();
      st.pop_back();
      n++;
      const int x = k % p.w, y = k / p.w;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (!dx && !dy) continue;
          const int q = p.at(x + dx, y + dy);
          if (q < 0) continue;
          if (figurePx(q)) touch = true;
          else if (!seen[(size_t)q] && weaponPx(q)) { seen[(size_t)q] = 1; st.push_back(q); }
        }
    }
    if (!touch) return fmt("h2: %d weapon pixel(s) from (%d,%d) clear of the body (a weapon floating)", n, fx - p.px, fy - p.py);
  }
  return "";
}

// d: the grips. bare: the same frame without the weapon (the fist's own hand pixels)
// (wave-2 fix round, D81) twoHand: the off fist is asked (DRAWN and ACTION frames and the guard; never a LOCO, RUN or DEATH
// carry: D5 carries spears and staffs in the right fist); upright: no weapon pixel more than 2 rows below the sole row
std::string gripCheck(const doll::FrameProbe& p, const doll::FrameProbe* bare, bool twoHand, bool upright, int facing = 0) {
  int wi = -1;
  for (size_t i = 0; i < p.items.size(); i++) if (isLibraryHeld(p.items[i])) wi = (int)i;
  if (wi < 0) return "";
  const doll::FrameProbe::It& w = p.items[(size_t)wi];
  if (w.key == "drop") return "";   // (INTEGRATE i1) on the ground beside the body: no hand holds it
  if (upright && !p.turn) {
    int sole = -1, low = -1;
    for (int k = 0; k < p.w * p.h; k++) {
      if (!opaque(p, k)) continue;
      const int o = ownerOf(p, k);
      if (o < 0) continue;
      if (p.items[(size_t)o].has(doll::Role::Legs)) sole = std::max(sole, k / p.w);
      if (o == wi) low = std::max(low, k / p.w);
    }
    if (sole >= 0 && low > sole + 2) return w.part + fmt(": %d rows below the soles (2 at most on an upright frame)", low - sole);
  }
  const bool bow = isBow(w);
  const bool onL = w.anchor == art::doll::Anchor::GripL;
  if (bow && !onL) return w.part + ": a bow not in the left hand (gripL)";
  if (!bow && onL) return w.part + ": a weapon in the left hand";
  const int from = onL ? p.gripLFrom : p.gripFrom;
  if (from < 0 || from >= (int)p.items.size()) return w.part + fmt(": no %s mark on the arm", onL ? "gripL" : "grip");
  const doll::FrameProbe::It& arm = p.items[(size_t)from];
  if (!onL && arm.side != 1) return w.part + ": held by " + arm.part + " (the right arm grips in every facing)";
  if (onL && arm.side != 2) return w.part + ": held by " + arm.part + " (the bow in the left hand)";
  const int gx = onL ? p.gripLX : p.gripX, gy = onL ? p.gripLY : p.gripY;
  // the fist's hand pixels (without the weapon), and the grip inside their box +-1
  int hx0 = 1 << 20, hy0 = 1 << 20, hx1 = -1, hy1 = -1, handPx = 0, covered = 0;
  const doll::FrameProbe& hp = bare ? *bare : p;
  // (INTEGRATE i1) the bare frame has one item fewer: the arm's index there is found by its part (a weapon painted before
  // the arm, the bow facing left, shifted it by one and no hand pixel was found)
  int hfrom = from;
  if (bare && hp.items.size() != p.items.size() && arm.partIndex >= 0) {
    hfrom = -1;
    for (size_t i = 0; i < hp.items.size(); i++) if (hp.items[i].partIndex == arm.partIndex) hfrom = (int)i;
  }
  for (int k = 0; k < p.w * p.h; k++) {
    if (hp.owner[(size_t)k] != hfrom || hp.region[(size_t)k] != (uint8_t)doll::Region::Hand || !opaque(hp, k)) continue;
    const int x = k % p.w, y = k / p.w;
    hx0 = std::min(hx0, x); hx1 = std::max(hx1, x); hy0 = std::min(hy0, y); hy1 = std::max(hy1, y);
    handPx++;
    if (ownerOf(p, k) == wi) covered++;
  }
  // (wave-2 fix round, D81: every library weapon checked, the spear and the greatsword among them) a gripping hand the body
  // hides by design draws no H pixel: facing U (the hands before the chest, the back toward us), or the far arm (the right fist
  // facing L, the left facing R); as C3c's hidden far hand, nothing to touch there
  // (wave-2 fix round 4, D90) and on turned frames (fall, roll, lying: the probe's turn); such a frame with no H pixel passes
  // only on the hidden-grip proof (h1 covered, h2 attached), never by a waiver
  const bool hidden = p.turn || facing == 1 || (facing == 3 && !onL) || (facing == 2 && onL);
  if (!handPx && !hidden) return arm.part + ": no hand pixels (region H) on the gripping arm";
  if (!handPx) {
    const std::string why = hiddenGripProof(p, bare, wi, gx, gy);
    if (!why.empty()) return arm.part + ": no hand pixels (region H) on the hidden gripping arm; " + why;
  } else {
    if (gx < hx0 - 1 || gx > hx1 + 1 || gy < hy0 - 1 || gy > hy1 + 1) return w.part + fmt(": the grip (%d,%d) outside the fist", gx - p.px, gy - p.py);
    if (covered) return w.part + fmt(": %d weapon pixel(s) over the fist", covered);
  }
  if (!bow) {
    if (!p.tip) return w.part + ": no tip mark";
    const double reach = std::hypot(p.tipX - gx, p.tipY - gy);
    double far = 0;
    for (int k = 0; k < p.w * p.h; k++)
      if (ownerOf(p, k) == wi) far = std::max(far, std::hypot(k % p.w - gx, k / p.w - gy));
    if (reach + 1.5 < far) return w.part + fmt(": the tip mark %.0f px from the grip, the weapon reaches %.0f", reach, far);
    if (twoHand) {
      if (!p.gripL) return w.part + ": a two-handed weapon without the off fist (gripL)";
      const double vx = p.tipX - gx, vy = p.tipY - gy, L2 = vx * vx + vy * vy;
      double t = L2 > 0 ? ((p.gripLX - gx) * vx + (p.gripLY - gy) * vy) / L2 : 0;
      // (the haft runs on behind the grip too: the line, not the segment)
      const double dx = p.gripLX - (gx + t * vx), dy = p.gripLY - (gy + t * vy);
      if (std::hypot(dx, dy) > 1.5) return w.part + fmt(": the off fist %.1f px off the haft line", std::hypot(dx, dy));
    }
  }
  return "";
}
// e: lying keeps its gear
// (forge i3) headHelm: the look's helm is a culture alloy's metal head cut from the headwear library (DollRecipe::guiseHead)
std::string lyingCheck(const doll::FrameProbe& p, bool helm, bool shield, bool weapon, bool headHelm = false) {
  int helmPx = 0;
  std::vector<int> body, sh, wp, hand, figure;   // figure: every opaque pixel that is not the weapon or the shield
  for (int k = 0; k < p.w * p.h; k++) {
    if (!opaque(p, k)) continue;
    const doll::FrameProbe::It* it = itemAt(p, k);
    if (!it || !(it->has(doll::Role::Held) || it->has(doll::Role::Shield))) figure.push_back(k);
    if (!it) continue;
    if (isHelm(*it) || (headHelm && it->has(doll::Role::Headwear))) helmPx++;
    // (P2c ladder i1: the lie views lie flat beside the corpse, `anchor pivot`: the shield on the ground still counts)
    if (isShield(*it) || (it->has(doll::Role::Shield) && it->anchor == art::doll::Anchor::Pivot)) sh.push_back(k);
    else if (it->has(doll::Role::Held)) wp.push_back(k);
    else body.push_back(k);
    if (p.region[(size_t)k] == (uint8_t)doll::Region::Hand) hand.push_back(k);
  }
  auto gap = [&](const std::vector<int>& a, const std::vector<int>& b) {
    double best = 1e9;
    for (int i : a) for (int j : b) best = std::min(best, std::hypot(i % p.w - j % p.w, i / p.w - j / p.w));
    return best;
  };
  if (helm && !helmPx) return "the helm is gone";
  if (shield) { if (sh.empty()) return "the shield is gone"; if (gap(sh, body) > 4) return fmt("the shield %.0f px from the body (4)", gap(sh, body)); }
  // (P2c ladder i1, the owner: "gear floating over them": the dropped weapon lies flat along the corpse's lower edge, so
  // it reads on the ground beside the body: within 6 px of a hand or of the figure, never off on its own;
  // the probe measures the lying frame before it is laid down, where the drop sits below the soles)
  if (weapon && !wp.empty() && !hand.empty() && gap(wp, hand) > 6 && gap(wp, figure) > 6)
    return fmt("the weapon %.0f px from a hand (6) and %.0f px from the figure (6)", gap(wp, hand), gap(wp, figure));
  return "";
}

doll::DollRecipe without(const doll::DollRecipe& r, bool (*drop)(int part)) {
  doll::DollRecipe o = r;
  o.parts.erase(std::remove_if(o.parts.begin(), o.parts.end(), drop), o.parts.end());
  return o;
}
bool dropShield(int p) {
  for (const P::RoleRec& rr : P::get().parts[(size_t)p].roles)
    if (rr.cat == (uint8_t)doll::Role::Shield && P::get().s(rr.value) != "arm") return true;
  return false;
}
bool dropLibraryHeld(int p) {
  for (const P::RoleRec& rr : P::get().parts[(size_t)p].roles)
    if (rr.cat == (uint8_t)doll::Role::Held && P::get().s(rr.value).find('.') != std::string::npos) return true;
  return false;
}

struct C3dRes { int n = 0; std::set<std::string> looks; std::vector<std::string> frames; };
std::map<std::string, C3dRes> g_c3dRes;
void printC3dResidual() {
  if (g_c3dRes.empty()) return;
  int tot = 0;
  for (const auto& kv : g_c3dRes) tot += kv.second.n;
  std::printf("  C3d residual (D90): %d frame(s) in %zu group(s), by arm part, key and facing | failing term\n", tot, g_c3dRes.size());
  for (const auto& kv : g_c3dRes) {
    std::string lk;
    int i = 0;
    for (const std::string& t : kv.second.looks) { if (i++ == 8) { lk += fmt(" +%d", (int)kv.second.looks.size() - 8); break; } lk += " " + t; }
    std::string fr;
    for (const std::string& f : kv.second.frames) fr += (fr.empty() ? "" : ", ") + f;
    std::printf("    %4d  %s\n          looks %zu:%s\n          frames: %s\n", kv.second.n, kv.first.c_str(), kv.second.looks.size(), lk.c_str(), fr.c_str());
  }
}
void gateC3(Line& g, const std::vector<Look>& looks) {
  g_c3dRes.clear();
  for (const Look& l : looks) {
    const Built b = build(l);
    if (!armoured(b.L) && !fighter(b.L) && !b.L.helmet && !b.L.helmStyle) continue;
    const bool hasHelm = b.L.helmet || b.L.helmStyle || b.L.helmForm;
    const bool hasShield = b.r.has("shield");
    const bool twoHand = b.r.has("twohand");
    // a: nothing missing for any part
    for (RigPart part : partsOf(b.L)) {
      const std::vector<doll::Missing> m = doll::dollMissing(b.L, part);
      if (m.empty()) g.ok();
      for (const doll::Missing& x : m) g.bad(fmt("C3a %s %s: %s", l.tag.c_str(), partName(part), x.what().c_str()));
    }
    const doll::DollRecipe noShield = without(b.r, dropShield), noWeapon = without(b.r, dropLibraryHeld);
    for (RigPart part : partsOf(b.L))
      for (const FrameId& f : framesOf(b.r, part)) {
        const doll::FrameProbe p = doll::probeFrame(b.r, part, f.facing, f.clip, f.variant, f.i);
        if (!p.ok) continue;
        const std::string nm = frameName(l.tag, f);
        std::string why;
        if (holesInGear(p, &why)) g.bad("C3a " + nm + ": " + why); else g.ok();
        // (wave-2 fix round, D80) the helm's resolved tier (after kitVar and the watch: the tier its views were picked by)
        const std::string hb = helmCheck(p, b.r.pieceTier[HumanLook::TIER_HELM], b.L.body == 1);
        if (!hb.empty()) g.bad("C3b " + nm + ": " + hb); else g.ok();
        if (hasShield) {
          const doll::FrameProbe bare = doll::probeFrame(noShield, part, f.facing, f.clip, f.variant, f.i);
          const std::string sc = shieldCheck(p, bare.ok ? &bare : nullptr, part == RigPart::Run || (part == RigPart::Drawn && f.variant == 2), f.facing);
          if (!sc.empty()) g.bad("C3c " + nm + ": " + sc); else g.ok();
        }
        bool lib = false;
        for (const doll::FrameProbe::It& it : p.items) lib = lib || isLibraryHeld(it);
        if (lib) {
          const doll::FrameProbe bare = doll::probeFrame(noWeapon, part, f.facing, f.clip, f.variant, f.i);
          // (D81) the off fist on DRAWN and ACTION frames and the guard only; the carry is one-handed
          const bool askOff = twoHand && (part == RigPart::Drawn || part == RigPart::Action);
          const std::string gc = gripCheck(p, bare.ok ? &bare : nullptr, askOff, f.clip != Clip::Fall && f.clip != Clip::Lying, p.turn ? 3 : f.facing);
          if (!gc.empty()) {
            g.bad("C3d " + nm + ": " + gc);
            // (D90) the residual list, grouped by arm part, key and facing (the arm keys are shared across looks)
            const int fr = p.gripFrom >= 0 && p.gripFrom < (int)p.items.size() ? p.gripFrom : -1;
            const std::string term = gc.find("h1:") != std::string::npos ? "h1" : gc.find("h2:") != std::string::npos ? "h2"
                                   : gc.find("outside the fist") != std::string::npos ? "grip outside the fist"
                                   : gc.find("over the fist") != std::string::npos ? "weapon over the fist"
                                   : gc.find("no hand pixels") != std::string::npos ? "no H pixel (near arm / facing D)"
                                   : gc.find("off the haft") != std::string::npos ? "off fist off the haft"
                                   : gc.find("tip mark") != std::string::npos ? "tip mark" : gc.find("below the soles") != std::string::npos ? "below the soles" : "other";
            const std::string key = (fr >= 0 ? p.items[(size_t)fr].part + " key " + p.items[(size_t)fr].key : std::string("(no grip mark)")) +
                                    fmt(" facing %c%s", "DURL"[f.facing & 3], p.turn ? " turned" : "") + " | " + term;
            C3dRes& r = g_c3dRes[key];
            r.n++;
            r.looks.insert(l.tag);
            if (r.frames.size() < 6) r.frames.push_back(fmt("%s %s v%d %c%d", partName(part), art::clipName(f.clip), f.variant, "DURL"[f.facing & 3], f.i));
          } else g.ok();
        }
        if (f.clip == Clip::Lying) {
          const std::string lc = lyingCheck(p, hasHelm, hasShield, fighter(b.L), b.r.guiseHead);
          if (!lc.empty()) g.bad("C3e " + nm + ": " + lc); else g.ok();
        }
      }
  }
}

// ---------------------------------------------------------------- C4 armour shows
struct Img { int w = 0, h = 0, px = 0, py = 0; std::vector<uint32_t> c; bool ok = false; };
Img frameImg(const RigSheet& s, int facing, Clip clip, int variant, int i) {
  Img m;
  const art::FrameRect* r = s.frame(facing, clip, variant, i);
  if (!r || s.atlas.w <= 0) return m;
  m.w = r->w; m.h = r->h; m.px = r->px; m.py = r->py; m.ok = true;
  m.c.resize((size_t)m.w * m.h);
  for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) m.c[(size_t)y * m.w + x] = s.atlas.get(r->x + x, r->y + y);
  return m;
}
// pixels that differ (colour or presence) over the union of both figures' pixels, aligned on the pivot; outline: pixels
// present in one and not the other
void diff(const Img& a, const Img& b, int& changed, int& uni, int& outline) {
  changed = uni = outline = 0;
  const int L = std::max(a.px, b.px), R = std::max(a.w - a.px, b.w - b.px), U = std::max(a.py, b.py), D = std::max(a.h - a.py, b.h - b.py);
  for (int y = -U; y < D; y++)
    for (int x = -L; x < R; x++) {
      auto get = [&](const Img& m) -> uint32_t {
        const int X = x + m.px, Y = y + m.py;
        return X >= 0 && Y >= 0 && X < m.w && Y < m.h ? m.c[(size_t)Y * m.w + X] : 0u;
      };
      const uint32_t ca = get(a), cb = get(b);
      const bool oa = ca >> 24, ob = cb >> 24;
      if (!oa && !ob) continue;
      uni++;
      if (oa != ob) { outline++; changed++; }
      else if (ca != cb) changed++;
    }
}
std::string armourOfLook(const HumanLook& L) {
  const doll::DollRecipe r = doll::resolve(L);
  for (const doll::Need& n : r.needs) if (n.role == doll::Role::Armour) return n.value;
  return "";
}
void gateC4(Line& g, const std::vector<Look>& looks) {
  for (const Look& l : looks) {
    const Built b = build(l);
    const std::string form = armourOfLook(b.L);
    if (form.empty()) continue;
    // (INTEGRATE i1, SLICE-ARMOUR's ask) the armour is measured with nothing in hand on both sides: a shield or a weapon
    // over the chest is the same pixels in both figures and only dilutes the share
    HumanLook worn = b.L;
    worn.shield = 0; worn.weapon = 0;
    // (ladder i2) and with no cloak on either side: the ladder's travel and noble cloaks are the same pixels over both
    // figures' backs and edges (lt6 D measured the plate's outline as 0 px moved behind its cloak)
    worn.cloak = 0; worn.cape = 0;
    // (ladder i2, integration) and beardless on both sides: the Fjordfolk's braided beard covers the chest in both figures
    // (fjord3 D measured 13 % with the mail under it), the same pixels the cloak was
    worn.beard = 0; worn.hood = false;   // a cloth hood's mantle over the shoulders: the same (nband R measured 13 %)
    worn.neck = 0;   // the hero's scarf over the collar and chest: the same (lt1w D measured 14 %)
    if (worn.hair == art::Hair::Long || worn.hair == art::Hair::Braids) worn.hair = art::Hair::Short;   // long hair over the shoulders: the same
    HumanLook bare = worn;
    bare.outfit = art::Outfit::Tunic; bare.armorStyle = 0; bare.bodyForm = 0; bare.pauldron = 0; bare.skirt = 0; bare.gloves = 0; bare.boots = 0;
    doll::DollRecipe ra = doll::resolve(worn), rb = doll::resolve(bare);
    ra.ramps = b.r.ramps; rb.ramps = b.r.ramps;
    const RigSheet sa = doll::compose(ra, RigPart::Loco), sb = doll::compose(rb, RigPart::Loco);
    const double need = (form == "padded" || form == "leather") ? 0.15 : 0.25;
    for (int fc : {0, 2}) {
      const Img a = frameImg(sa, fc, Clip::Idle, 0, 0), c = frameImg(sb, fc, Clip::Idle, 0, 0);
      if (!a.ok || !c.ok) { g.bad(fmt("C4 %s %c: no idle frame", l.tag.c_str(), "DURL"[fc])); continue; }
      int ch, un, ol;
      diff(a, c, ch, un, ol);
      const double share = un ? (double)ch / un : 0;
      if (g_report) std::printf("  C4 %-8s %-7s %c: %.1f %% changed (%d of %d), outline %d px\n", l.tag.c_str(), form.c_str(), "DURL"[fc], share * 100, ch, un, ol);
      if (share < need) g.bad(fmt("C4 %s %s %c: %.1f %% of the idle figure changed (%.0f %%)", l.tag.c_str(), form.c_str(), "DURL"[fc], share * 100, need * 100));
      else g.ok();
      if (form == "plate") { if (ol < 6) g.bad(fmt("C4 %s plate %c: the outline moved by %d px (6 or more)", l.tag.c_str(), "DURL"[fc], ol)); else g.ok(); }
    }
  }
}

// ---------------------------------------------------------------- C5 families (the sword against the spear)
bool mirrored(const Img& a, const Img& b) {
  if (!a.ok || !b.ok || a.w != b.w || a.h != b.h) return false;
  for (int y = 0; y < a.h; y++) for (int x = 0; x < a.w; x++) if (a.c[(size_t)y * a.w + x] != b.c[(size_t)y * a.w + (a.w - 1 - x)]) return false;
  return true;
}
// the spear's tip path from the loaded frame (the windup's last) to the impact (the strike's first): forward and drift
std::string tipPath(const std::vector<std::pair<int, int>>& tips, int facing, int* fwdOut = nullptr) {
  if (tips.size() < 2) return "no tip marks on the windup and strike frames";
  const int dir = facing == 3 ? -1 : 1;
  const int fwd = (tips.back().first - tips.front().first) * dir;
  int drift = 0;
  for (const auto& t : tips) drift = std::max(drift, std::abs(t.second - tips.front().second));
  if (fwdOut) *fwdOut = fwd;
  if (fwd < 8) return fmt("the tip goes %d px forward (8 or more)", fwd);
  if (drift >= 4) return fmt("the tip drifts %d px (under 4)", drift);
  return "";
}
void gateC5(Line& g, const std::vector<Look>& looks) {
  const Look *sw = nullptr, *sp = nullptr;
  for (const Look& l : looks) {
    const Built b = build(l);
    if (b.L.weapon == 1 && !sw && b.r.has("shield")) sw = &l;   // the guard first (the slice's sword with the shield)
  }
  for (const Look& l : looks) {
    const Built b = build(l);
    if (b.L.weapon == 1 && !sw) sw = &l;
    if (b.L.weapon == 8 && !sp) sp = &l;
  }
  if (!sw || !sp) { g.bad("C5: the looks need a sword and a spear fighter"); return; }
  const Built bs = build(*sw), bp = build(*sp);
  const RigSheet as = doll::compose(bs.r, RigPart::Action), ap = doll::compose(bp.r, RigPart::Action);
  for (int fc = 0; fc < 4; fc++) {
    const Img a = frameImg(as, fc, Clip::Strike, 0, 0), c = frameImg(ap, fc, Clip::Strike, 0, 0);
    if (!a.ok || !c.ok) { g.bad(fmt("C5 %c: no strike frame (%s %s, %s %s)", "DURL"[fc], sw->tag.c_str(), a.ok ? "ok" : "missing", sp->tag.c_str(), c.ok ? "ok" : "missing")); continue; }
    int ch, un, ol;
    diff(a, c, ch, un, ol);
    const double share = un ? (double)ch / un : 0;
    if (g_report) std::printf("  C5 %c: the %s and %s strikes %.1f %% apart\n", "DURL"[fc], sw->tag.c_str(), sp->tag.c_str(), share * 100);
    if (share < 0.25) g.bad(fmt("C5 %c: the sword's and spear's strikes %.1f %% apart (25 %%)", "DURL"[fc], share * 100)); else g.ok();
    // the effect on the impact frame
    const doll::FrameProbe p = doll::probeFrame(bp.r, RigPart::Action, fc, Clip::Strike, 0, 0);
    bool fx = false;
    for (int k = 0; p.ok && k < p.w * p.h; k++) { const doll::FrameProbe::It* it = itemAt(p, k); if (opaque(p, k) && it && it->has(doll::Role::Fx)) fx = true; }
    if (!fx) g.bad(fmt("C5 %s %c: no effect on the impact frame", sp->tag.c_str(), "DURL"[fc])); else g.ok();
  }
  for (int fc : {2, 3}) {
    std::vector<std::pair<int, int>> tips;
    const int nw = ap.count(fc, Clip::Windup, 0);
    if (nw > 0) {
      const doll::FrameProbe p = doll::probeFrame(bp.r, RigPart::Action, fc, Clip::Windup, 0, nw - 1);
      if (p.ok && p.tip) tips.push_back({p.tipX - p.px, p.tipY - p.py});
    }
    for (int i = 0; i < ap.count(fc, Clip::Strike, 0) && i < 1; i++) {
      const doll::FrameProbe p = doll::probeFrame(bp.r, RigPart::Action, fc, Clip::Strike, 0, i);
      if (p.ok && p.tip) tips.push_back({p.tipX - p.px, p.tipY - p.py});
    }
    int fwd = 0;
    const std::string w = tipPath(tips, fc, &fwd);
    if (g_report) std::printf("  C5 %s %c tip path: %d px forward\n", sp->tag.c_str(), "DURL"[fc], fwd);
    if (!w.empty()) g.bad(fmt("C5 %s %c: ", sp->tag.c_str(), "DURL"[fc]) + w); else g.ok();
  }
  if (mirrored(frameImg(ap, 2, Clip::Strike, 0, 0), frameImg(ap, 3, Clip::Strike, 0, 0))) g.bad("C5 " + sp->tag + ": the left strike is the right's plain mirror");
  else g.ok();
}

// ---------------------------------------------------------------- C6 the library
struct Grid { int w = 0, h = 0, ox = 0, oy = 0; std::vector<uint8_t> c; bool tip = false; int tx = 0, ty = 0; };
Grid gridOf(const P::ViewRec& v) {
  Grid g;
  g.w = v.w; g.h = v.h; g.ox = v.ox; g.oy = v.oy; g.c = v.cells;
  const P::Pool& Pl = P::get();
  for (uint8_t m = 0; m < v.nMarks; m++) {
    const P::MarkRec& mk = Pl.marks[v.firstMark + m];
    if (mk.name == Pl.ids.tip) { g.tip = true; g.tx = mk.x; g.ty = mk.y; }
  }
  return g;
}
// one view's craft: orphans, the lit edge toward the top-left, the grip on the haft, the tip
std::string viewCraft(const Grid& g, bool blade, bool bow) {
  auto at = [&](int x, int y) -> uint8_t { return x >= 0 && y >= 0 && x < g.w && y < g.h ? g.c[(size_t)y * g.w + x] : 0; };
  int lit = 0, dark = 0;
  for (int y = 0; y < g.h; y++)
    for (int x = 0; x < g.w; x++) {
      const uint8_t c = at(x, y);
      if (c < 2) continue;
      bool alone = true;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && at(x + dx, y + dy) >= 2) alone = false;
      if (alone) return fmt("an orphan pixel at (%d,%d)", x - g.ox, y - g.oy);
      const int k = doll::codeStep(c);
      if (k == 0) continue;
      for (auto d : {std::make_pair(1, 0), std::make_pair(0, 1), std::make_pair(1, 1)}) {
        const uint8_t n = at(x + d.first, y + d.second);
        if (n < 2 || doll::codeStep(n) == 0 || doll::codeSlot(n) != doll::codeSlot(c)) continue;
        const int kn = doll::codeStep(n);
        if (k > kn) lit++;
        else if (k < kn) dark++;
      }
    }
  if (dark > lit + 1) return fmt("lit toward the bottom-right (%d pairs lit top-left, %d bottom-right)", lit, dark);
  bool haft = false;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (at(g.ox + dx, g.oy + dy) >= 2) haft = true;
  if (!haft) return "the grip origin is not on the haft";
  if (!bow) {
    if (!g.tip) return "no tip mark";
    if (blade) {
      int best = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) { const uint8_t c = at(g.tx + dx, g.ty + dy); if (c >= 2) best = std::max(best, doll::codeStep(c)); }
      if (best < 3) return "no bright tip pixel";
    }
  }
  return "";
}
int lengthOf(const std::string& held) {   // STYLE_P2 11 (docs/PLAN.md "Constants that interact"): the a000 view's length
  const std::string w = held.substr(0, held.find('.'));
  const std::string form = held.find('.') == std::string::npos ? "" : held.substr(held.find('.') + 1);
  if (w == "sword") return 15;
  if (w == "greatsword") return 24;
  if (w == "spear") return 39;
  if (w == "staff") return 30;
  if (w == "bow") return form == "longbow" ? 28 : (form == "recurve" || form == "composite") ? 20 : form == "crossbow" ? 12 : 22;   // (int1) the crossbow 12 x 10 (STYLE_P2 11)
  return 0;
}
void gateC6(Line& g) {
  const P::Pool& Pl = P::get();
  int libs = 0;
  for (int pi = 0; pi < (int)Pl.parts.size(); pi++) {
    const P::PartRec& p = Pl.parts[(size_t)pi];
    std::string held;
    for (const P::RoleRec& rr : p.roles) if (rr.cat == (uint8_t)doll::Role::Held && Pl.s(rr.value).find('.') != std::string::npos) held = Pl.s(rr.value);
    if (held.empty()) continue;
    libs++;
    const std::string name = Pl.s(p.name);
    const bool bow = held.rfind("bow", 0) == 0;
    const bool blade = held.rfind("sword", 0) == 0 || held.rfind("greatsword", 0) == 0 || held.rfind("dagger", 0) == 0 || held.rfind("spear", 0) == 0;
    if (!bow) {
      for (int k = 0; k < 16; k++) {
        char nm[5];
        P::angleKey(k, nm);
        const uint16_t id = Pl.strId(nm);
        bool any = false;
        for (int fc = 0; fc < 4 && !any; fc++) any = id != 0xFFFF && P::viewOf(p, fc, id) != nullptr;
        if (!any) g.bad(fmt("C6 %s (%s): no view %s", name.c_str(), held.c_str(), nm)); else g.ok();
      }
    } else {
      for (const char* k : {"carry", "drop"}) {
        const uint16_t id = Pl.strId(k);
        bool any = false;
        for (int fc = 0; fc < 4 && !any; fc++) any = id != 0xFFFF && P::viewOf(p, fc, id) != nullptr;
        if (!any) g.bad(fmt("C6 %s (%s): no view %s", name.c_str(), held.c_str(), k)); else g.ok();
      }
    }
    for (uint32_t vi = p.firstView; vi < p.firstView + p.nViews; vi++) {
      const P::ViewRec& v = Pl.views[vi];
      if (v.when) continue;
      const std::string key = Pl.s(v.key);
      if (key == "drop" || key == "carry") { g.ok(); continue; }   // (rest poses: the craft of the angle views)
      // (alloys i1) A3's own hand-drawn sword views (d0..l3 the facing frames, a3* the attack, guard, gmove) predate the
      // angle-view craft and are pinned by B1's parity: the library forms (`role held sword.<form>`, which give the
      // sword its blade metal) are measured on their angle views a000..a337 only
      const bool angle = key.size() == 4 && key[0] == 'a' && std::isdigit((unsigned char)key[1]) && std::isdigit((unsigned char)key[2]) &&
                         std::isdigit((unsigned char)key[3]);
      const bool a3own = !angle && ((key.size() == 2 && std::strchr("durl", key[0]) && std::isdigit((unsigned char)key[1])) ||
                                    key.rfind("a3", 0) == 0 || key == "guard" || key == "gmove");
      if (a3own && name == "held/weapons/sword") { g.ok(); continue; }
      const std::string w = viewCraft(gridOf(v), blade, bow);
      if (!w.empty()) g.bad(fmt("C6 %s %c %s: %s", name.c_str(), "DURL"[v.facing & 3], key.c_str(), w.c_str())); else g.ok();
    }
    const int want = lengthOf(held);
    const uint16_t a0 = Pl.strId("a000");
    const P::ViewRec* v0 = nullptr;
    for (int fc = 0; fc < 4 && !v0; fc++) if (a0 != 0xFFFF) v0 = P::viewOf(p, fc, a0);
    if (want && v0) {
      int x0 = 1 << 20, x1 = -1;
      for (int y = 0; y < v0->h; y++) for (int x = 0; x < v0->w; x++) if (v0->cells[(size_t)y * v0->w + x] >= 2) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
      const int len = x1 - x0 + 1;
      if (g_report) std::printf("  C6 %s length %d px (%d)\n", name.c_str(), len, want);
      if (std::abs(len - want) > 1) g.bad(fmt("C6 %s: %d px long (%d +-1)", name.c_str(), len, want)); else g.ok();
    }
  }
  if (!libs) g.bad("C6: no library weapon in the pool (`role held <weapon>.<form>`)");
}

// ---------------------------------------------------------------- selftests (synthetic defects; every one must be caught)
doll::FrameProbe synth(int w, int h) {
  doll::FrameProbe p;
  p.ok = true; p.w = w; p.h = h; p.px = w / 2; p.py = h - 2;
  p.code.assign((size_t)w * h, 0); p.owner.assign((size_t)w * h, -1); p.region.assign((size_t)w * h, 0);
  return p;
}
int addItem(doll::FrameProbe& p, const char* part, doll::Role role, art::doll::Anchor an, doll::Layer layer, int side = 0, const char* held = "") {
  doll::FrameProbe::It it;
  it.part = part; it.roles = 1u << (int)role; it.anchor = an; it.layer = layer; it.side = side; it.held = held;
  p.items.push_back(it);
  return (int)p.items.size() - 1;
}
void fill(doll::FrameProbe& p, int item, int x0, int y0, int x1, int y1, doll::Slot s = doll::Slot::Metal, int step = 2, doll::Region rg = doll::Region::None) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const int k = p.at(x, y);
      if (k < 0) continue;
      p.code[(size_t)k] = doll::code(s, step); p.owner[(size_t)k] = (int16_t)item; p.region[(size_t)k] = (uint8_t)rg;
    }
}
void clearPx(doll::FrameProbe& p, int x, int y) { const int k = p.at(x, y); p.code[(size_t)k] = 0; p.owner[(size_t)k] = -1; }

int selftestC3(bool report) {
  int missed = 0;
  auto expect = [&](bool caught, const char* what) { if (!caught) { missed++; std::printf("  C3 selftest MISSED: %s\n", what); } else if (report) std::printf("  C3 selftest caught: %s\n", what); };
  using A = art::doll::Anchor;
  {   // a: a hole in a breastplate; a clean plate; a gap between arm and body (not gear)
    doll::FrameProbe p = synth(40, 40);
    const int a = addItem(p, "armour/man/plate", doll::Role::Armour, A::Neck, doll::Layer::Torso);
    fill(p, a, 10, 10, 20, 20);
    expect(holesInGear(p, nullptr) == 0, "a solid plate reads clean");
    clearPx(p, 15, 15);
    expect(holesInGear(p, nullptr) == 1, "a 1 px hole inside the plate");
    doll::FrameProbe q = synth(40, 40);
    const int arm = addItem(q, "arms/adult/std", doll::Role::Arms, A::ShoulderR, doll::Layer::ArmNear);
    fill(q, arm, 10, 10, 20, 20);
    clearPx(q, 15, 15);
    expect(holesInGear(q, nullptr) == 0, "a gap between body parts is not a gear hole");
    const int bow = addItem(p, "held/weapons/bow", doll::Role::Held, A::GripL, doll::Layer::Held, 0, "bow.self");
    fill(p, bow, 14, 14, 14, 14);
    clearPx(p, 15, 15);
    expect(holesInGear(p, nullptr) == 0, "the bow's string gap is exempt");
  }
  {   // b: the helm off the head; plume too high; hair over the rim; anchored elsewhere
    doll::FrameProbe p = synth(60, 60);
    p.headX = 20; p.headY = 10;
    const int h = addItem(p, "headwear/helm/nasal", doll::Role::Helm, A::Head, doll::Layer::Headwear);
    fill(p, h, 22, 10, 33, 14);
    expect(helmCheck(p).empty(), "a helm on the head reads clean");
    doll::FrameProbe q = p;
    fill(q, h, 40, 12, 40, 12);
    expect(!helmCheck(q).empty(), "a helm pixel outside the head box");
    q = p;
    fill(q, h, 25, 2, 25, 2, doll::Slot::Plume);   // F-56: above F-37
    expect(!helmCheck(q).empty(), "a plume above F-37");
    {   // T1-T6 read kEnvelope from the head anchor: the crown at headY + the A3 offset
      const int crown = p.headY + crownOffsetArms(0);
      q = p;
      fill(q, h, 25, crown - 7, 25, crown - 7, doll::Slot::Plume);   // past T4's 6 rows (F-37)
      expect(!helmCheck(q, 4).empty(), "a plume 7 rows over the crown on a T4 helm (ornRows 6)");
      q = p;
      fill(q, h, 25, crown - 6, 25, crown - 6, doll::Slot::Plume);
      expect(helmCheck(q, 4).empty(), "a plume 6 rows over the crown on a T4 helm reads clean");
      q = p;
      fill(q, h, 25, crown - 5, 25, crown - 5);
      expect(!helmCheck(q, 3).empty(), "a T3 helm shell 5 rows over the crown (rows 4)");
      expect(helmCheck(q, 4).empty(), "the same shell on a T4 helm (rows 6) reads clean");
      q = p;
      fill(q, h, 20 + 15 + 1, 12, 20 + 15 + 1, 12);   // the shell 1 column past the box
      expect(helmCheck(q, 4).empty(), "a T4 helm shell 1 column past the head box (side 1) reads clean");
      expect(!helmCheck(q, 2).empty(), "a T2 helm shell 1 column past the head box (side 0)");
      q = p;
      fill(q, h, 20 + 15 + 2, 12, 20 + 15 + 2, 12);   // the shell 2 columns past the box
      expect(!helmCheck(q, 4).empty(), "a T4 helm shell 2 columns past the head box (side 1)");
      expect(helmCheck(q, 5).empty(), "the same shell on a T5 helm (side 2) reads clean");
      q = p;
      const int horn = addItem(q, "headwear/helm/horned_horn", doll::Role::Helm, A::Head, doll::Layer::Headwear);
      fill(q, horn, 20 + 15 + 3, 12, 20 + 15 + 3, 12);
      expect(helmCheck(q, 3).empty(), "a horn 3 columns past the head box at T3 (flare 3) reads clean");
      q = p;
      fill(q, h, 25, crown - 8, 25, crown - 8, doll::Slot::Plume);
      expect(helmCheck(q, 5).empty(), "a plume 8 rows over the crown on a T5 helm (ornRows 8) reads clean");
      expect(!helmCheck(q, 4).empty() || crown - 8 >= q.py - 37, "the same plume on a T4 helm (F-37)");
      q = p;
      fill(q, h, 25, crown - 11, 25, crown - 11, doll::Slot::Plume);
      expect(!helmCheck(q, 6).empty(), "a plume 11 rows over the crown on a T6 helm (ornRows 10)");
      q = p;
      fill(q, h, 20 + 15 + 3, 12, 20 + 15 + 3, 12);   // the shell 3 columns past the box
      expect(!helmCheck(q, 6).empty(), "a T6 helm shell 3 columns past the head box (side 2)");
    }
    q = p;
    const int hair = addItem(q, "hair/adult/tousled", doll::Role::Hair, A::Head, doll::Layer::HairFront);
    fill(q, hair, 26, 8, 26, 8, doll::Slot::Hair);
    expect(!helmCheck(q).empty(), "hair over the helm's rim");
    q = p;
    q.items[(size_t)h].anchor = A::Neck;
    expect(!helmCheck(q).empty(), "a helm not anchored on the head");
  }
  {   // c: the shield's layer by facing, detached, over a boot, outside the run box
    doll::FrameProbe p = synth(60, 60);
    const int arm = addItem(p, "shield/arm", doll::Role::Shield, A::ShoulderL, doll::Layer::ArmFar, 2);
    const int sh = addItem(p, "shield/kite", doll::Role::Shield, A::GripL, doll::Layer::Back);
    fill(p, arm, 30, 20, 31, 35, doll::Slot::Skin, 2, doll::Region::Hand);
    fill(p, sh, 25, 22, 29, 32, doll::Slot::Field);
    p.gripL = true; p.gripLX = 30; p.gripLY = 28; p.gripLFrom = arm;
    doll::FrameProbe bare = synth(60, 60);
    const int legs = addItem(bare, "legs", doll::Role::Legs, A::Pivot, doll::Layer::Legs);
    fill(bare, legs, 24, 20, 34, 50, doll::Slot::Cloth2);
    fill(bare, legs, 24, 50, 34, 57, doll::Slot::Leather, 2, doll::Region::Boot);
    expect(shieldCheck(p, &bare, false, 2).empty(), "a shield behind facing right, on the hand, reads clean");
    expect(!shieldCheck(p, &bare, false, 3).empty(), "a shield behind the torso facing left");
    doll::FrameProbe q = p;
    q.gripLX = 50;
    expect(!shieldCheck(q, &bare, false, 2).empty(), "a shield away from the gripping hand");
    q = p;
    fill(q, sh, 25, 51, 29, 53, doll::Slot::Field);
    expect(!shieldCheck(q, &bare, false, 2).empty(), "a shield over a boot");
    q = p;
    fill(q, sh, 18, 25, 20, 26, doll::Slot::Field);
    fill(q, sh, 21, 25, 24, 25, doll::Slot::Field);
    expect(!shieldCheck(q, &bare, true, 2).empty(), "a shield outside the run box + 1");
    q = p;
    q.items[(size_t)sh].anchor = A::Hip;
    // (no longer counted as the shield: re-mark it as on the hip with the shield role kept)
    expect(shieldCheck(q, &bare, false, 2).empty(), "(a non-gripL shield part is the arm's)");
    // (wave-2 fix round, D82) facing U a cloak covers the grip: attached when the shield touches that cloak part
    q = p;
    const int ck = addItem(q, "cloak/noble", doll::Role::Cloak, A::Neck, doll::Layer::CloakFront);
    fill(q, ck, 30, 22, 37, 34, doll::Slot::Cloak);
    q.gripLX = 36;
    expect(shieldCheck(q, nullptr, false, 1).empty(), "(a shield beside the cloak that covers its grip reads clean)");
    q = p;
    const int ck2 = addItem(q, "cloak/noble", doll::Role::Cloak, A::Neck, doll::Layer::CloakFront);
    fill(q, ck2, 33, 22, 37, 34, doll::Slot::Cloak);
    q.gripLX = 35;
    expect(!shieldCheck(q, nullptr, false, 1).empty(), "a shield 3 px clear of the cloak that covers its grip");
  }
  {   // d: a weapon over the fist, the grip outside the fist, the left hand, the off fist off the haft, no tip
    doll::FrameProbe p = synth(80, 60);
    const int arm = addItem(p, "arms/adult/action/onehand/sw", doll::Role::Arms, A::ShoulderR, doll::Layer::ArmNear, 1);
    const int w = addItem(p, "held/weapons/spear", doll::Role::Held, A::Grip, doll::Layer::Held, 0, "spear.spear");
    fill(p, w, 20, 30, 60, 30, doll::Slot::Leather);
    fill(p, arm, 30, 29, 31, 31, doll::Slot::Skin, 2, doll::Region::Hand);
    p.grip = true; p.gripX = 30; p.gripY = 30; p.gripFrom = arm; p.tip = true; p.tipX = 60; p.tipY = 30;
    const int off = addItem(p, "arms/adult/action/twohand/spL", doll::Role::Arms, A::ShoulderL, doll::Layer::ArmFar, 2);
    fill(p, off, 40, 29, 41, 31, doll::Slot::Skin, 2, doll::Region::Hand);
    p.gripL = true; p.gripLX = 40; p.gripLY = 30; p.gripLFrom = off;
    doll::FrameProbe bare = p;
    expect(gripCheck(p, &bare, true, true).empty(), "a two-handed grip on the haft reads clean");
    doll::FrameProbe q = p;
    fill(q, w, 30, 30, 31, 30, doll::Slot::Leather);
    expect(!gripCheck(q, &bare, true, true).empty(), "weapon pixels over the fist");
    q = p; q.gripX = 36;
    expect(!gripCheck(q, &bare, true, true).empty(), "the grip outside the fist");
    q = p; q.items[(size_t)arm].side = 2;
    expect(!gripCheck(q, &bare, true, true).empty(), "a weapon in the left arm");
    q = p; q.gripLY = 35;
    expect(!gripCheck(q, &bare, true, true).empty(), "the off fist off the haft line");
    q = p; q.tip = false;
    expect(!gripCheck(q, &bare, true, true).empty(), "no tip mark");
    q = p; q.tipX = 45;
    expect(!gripCheck(q, &bare, true, true).empty(), "the tip mark short of the far end");
    // (wave-2 fix round, D81) the carry is one-handed: the off fist is not asked on a LOCO / RUN / DEATH carry
    q = p; q.gripLY = 35;
    expect(gripCheck(q, &bare, false, true).empty(), "(a carry: the off fist off the haft line reads clean)");
    // NEW: no weapon pixel more than 2 rows below the sole row on an upright frame (the spear's blow 3 facing D)
    q = p;
    const int legs = addItem(q, "legs", doll::Role::Legs, A::Pivot, doll::Layer::Legs);
    fill(q, legs, 30, 40, 34, 45, doll::Slot::Cloth2);
    fill(q, w, 50, 31, 50, 48, doll::Slot::Leather);
    expect(!gripCheck(q, &bare, true, true).empty(), "a weapon 3 rows below the soles on an upright frame");
    expect(gripCheck(q, &bare, true, false).empty(), "(the same weapon on a fall frame reads clean)");
    q = p;
    const int legs2 = addItem(q, "legs", doll::Role::Legs, A::Pivot, doll::Layer::Legs);
    fill(q, legs2, 30, 40, 34, 45, doll::Slot::Cloth2);
    fill(q, w, 50, 31, 50, 47, doll::Slot::Leather);
    expect(gripCheck(q, &bare, true, true).empty(), "(a weapon 2 rows below the soles reads clean)");
  }
  {   // (wave-2 fix round 4, D90) the hidden-grip proof: facing U the fist is before the chest, the blade rises over the
      // shoulder; the body covers the grip (h1) and every weapon piece touches the body (h2)
    doll::FrameProbe p = synth(80, 60);
    const int body = addItem(p, "armour/man/plate", doll::Role::Armour, A::Neck, doll::Layer::Torso);
    const int w = addItem(p, "held/weapons/arming_sword", doll::Role::Held, A::Grip, doll::Layer::Back, 0, "sword.straight");
    const int arm = addItem(p, "arms/adult/action/onehand/sw_r", doll::Role::Arms, A::ShoulderR, doll::Layer::ArmNear, 1);
    fill(p, body, 30, 30, 50, 50, doll::Slot::Metal);
    fill(p, arm, 44, 32, 47, 44, doll::Slot::Metal);
    fill(p, w, 46, 14, 46, 29, doll::Slot::Blade);   // the blade above the shoulder, touching it at row 29
    p.grip = true; p.gripX = 46; p.gripY = 36; p.gripFrom = arm; p.tip = true; p.tipX = 46; p.tipY = 14;
    doll::FrameProbe bare = p;
    for (int k = 0; k < bare.w * bare.h; k++) if (bare.owner[(size_t)k] == w) { bare.code[(size_t)k] = 0; bare.owner[(size_t)k] = -1; }
    expect(gripCheck(p, &bare, false, true, 1).empty(), "(a clean facing-U strike, the fist hidden, reads clean)");
    doll::FrameProbe q = p;   // the fist at the silhouette's edge: the blade leaves the body right beside the grip
    fill(q, w, 51, 36, 60, 36, doll::Slot::Blade);
    for (int k = 0; k < q.w * q.h; k++) if (q.owner[(size_t)k] == w && k / q.w < 30) { q.code[(size_t)k] = 0; q.owner[(size_t)k] = -1; }
    q.gripX = 50; q.gripY = 36; q.tipX = 60; q.tipY = 36;
    expect(gripCheck(q, &bare, false, true, 1).empty(), "(a facing-U fist at the silhouette's edge, its blade leaving there, reads clean)");
    q = p; q.gripX = 46; q.gripY = 20;   // the grip up on the blade
    expect(!gripCheck(q, &bare, false, true, 1).empty(), "a hidden grip on the blade (a weapon in front with no fist)");
    q = p; q.gripX = 56; q.gripY = 20;   // the grip in the air
    expect(!gripCheck(q, &bare, false, true, 1).empty(), "a hidden grip on a clear pixel (the fist missing in the open)");
    q = p;   // a 2 px gap between the blade and the shoulder
    clearPx(q, 46, 29); clearPx(q, 46, 28);
    expect(!gripCheck(q, &bare, false, true, 1).empty(), "a 2 px gap between the blade and the shoulder (a weapon floating)");
    expect(!gripCheck(p, &bare, false, true, 0).empty(), "a facing-D frame with no H pixel although h1 and h2 hold");
    expect(!gripCheck(p, &bare, false, true, 2).empty(), "the near arm (the right fist facing R) with no H pixel");
    expect(gripCheck(p, &bare, false, true, 3).empty(), "(the far arm, the right fist facing L, on the proof reads clean)");
  }
  {   // e: lying without the helm, the shield away, the weapon away
    doll::FrameProbe p = synth(80, 40);
    const int body = addItem(p, "wear/tunic", doll::Role::Garment, A::Neck, doll::Layer::Torso);
    const int helm = addItem(p, "headwear/helm/nasal", doll::Role::Helm, A::Head, doll::Layer::Headwear);
    const int sh = addItem(p, "shield/kite", doll::Role::Shield, A::GripL, doll::Layer::Back);
    const int w = addItem(p, "held/weapons/sword", doll::Role::Held, A::Grip, doll::Layer::Held, 0, "sword.straight");
    const int arm = addItem(p, "arms", doll::Role::Arms, A::ShoulderR, doll::Layer::ArmNear, 1);
    fill(p, body, 10, 20, 50, 26, doll::Slot::Cloth1);
    fill(p, helm, 51, 20, 55, 26);
    fill(p, sh, 20, 27, 30, 29, doll::Slot::Field);
    fill(p, arm, 35, 27, 36, 28, doll::Slot::Skin, 2, doll::Region::Hand);
    fill(p, w, 38, 28, 48, 28);
    expect(lyingCheck(p, true, true, true).empty(), "a lying body with its gear reads clean");
    doll::FrameProbe q = p;
    for (int k = 0; k < q.w * q.h; k++) if (q.owner[(size_t)k] == helm) { q.code[(size_t)k] = 0; q.owner[(size_t)k] = -1; }
    expect(!lyingCheck(q, true, true, true).empty(), "the helm gone from a lying body");
    q = p;
    for (int k = 0; k < q.w * q.h; k++) if (q.owner[(size_t)k] == sh) { q.code[(size_t)k] = 0; q.owner[(size_t)k] = -1; }
    fill(q, sh, 20, 36, 30, 38, doll::Slot::Field);
    expect(!lyingCheck(q, true, true, true).empty(), "the shield 7 px from the body");
    q = p;
    for (int k = 0; k < q.w * q.h; k++) if (q.owner[(size_t)k] == w) { q.code[(size_t)k] = 0; q.owner[(size_t)k] = -1; }
    fill(q, w, 60, 38, 70, 38);
    expect(!lyingCheck(q, true, true, true).empty(), "the weapon far from the hand");
  }
  return missed;
}
int selftestC4(bool report) {
  int missed = 0;
  auto expect = [&](bool caught, const char* what) { if (!caught) { missed++; std::printf("  C4 selftest MISSED: %s\n", what); } else if (report) std::printf("  C4 selftest caught: %s\n", what); };
  Img a; a.ok = true; a.w = 10; a.h = 10; a.px = 5; a.py = 9; a.c.assign(100, 0xFF808080u);
  Img b = a;
  int ch, un, ol;
  diff(a, b, ch, un, ol);
  expect(ch == 0 && un == 100, "the same figure changes nothing");
  for (int i = 0; i < 20; i++) b.c[(size_t)i] = 0xFF404040u;
  diff(a, b, ch, un, ol);
  expect(ch == 20 && ol == 0 && (double)ch / un < 0.25, "20 % recoloured falls short of 25 %");
  Img c = a; c.w = 12; c.px = 6; c.c.assign(120, 0xFF808080u);
  diff(a, c, ch, un, ol);
  expect(ol == 20, "two new columns move the outline");
  return missed;
}
int selftestC5(bool report) {
  int missed = 0;
  auto expect = [&](bool caught, const char* what) { if (!caught) { missed++; std::printf("  C5 selftest MISSED: %s\n", what); } else if (report) std::printf("  C5 selftest caught: %s\n", what); };
  expect(tipPath({{0, 0}, {10, 1}}, 2).empty(), "a 10 px thrust reads clean");
  expect(!tipPath({{0, 0}, {5, 0}}, 2).empty(), "a 5 px thrust is short");
  expect(!tipPath({{0, 0}, {12, 5}}, 2).empty(), "a thrust drifting 5 px");
  expect(!tipPath({{0, 0}, {10, 0}}, 3).empty(), "a thrust going backward facing left");
  expect(!tipPath({{0, 0}}, 2).empty(), "no tip marks");
  Img a; a.ok = true; a.w = 4; a.h = 1; a.px = 2; a.py = 0; a.c = {1, 2, 3, 4};
  Img m = a; m.c = {4, 3, 2, 1};
  expect(mirrored(a, m), "a plain mirror is caught");
  m.c = {4, 3, 2, 9};
  expect(!mirrored(a, m), "a repainted left frame is not a mirror");
  return missed;
}
int selftestC6(bool report) {
  int missed = 0;
  auto expect = [&](bool caught, const char* what) { if (!caught) { missed++; std::printf("  C6 selftest MISSED: %s\n", what); } else if (report) std::printf("  C6 selftest caught: %s\n", what); };
  Grid g; g.w = 8; g.h = 3; g.ox = 1; g.oy = 1; g.c.assign(24, 0);
  for (int x = 0; x < 8; x++) { g.c[(size_t)x] = doll::code(doll::Slot::Metal, 3); g.c[(size_t)(8 + x)] = doll::code(doll::Slot::Metal, 1); }
  g.c[7] = doll::code(doll::Slot::Metal, 4);
  g.tip = true; g.tx = 7; g.ty = 0;
  expect(viewCraft(g, true, false).empty(), "a blade lit on its top edge with a white tip reads clean");
  Grid q = g;
  for (int x = 0; x < 8; x++) { q.c[(size_t)x] = doll::code(doll::Slot::Metal, 1); q.c[(size_t)(8 + x)] = doll::code(doll::Slot::Metal, 3); }
  q.c[7] = doll::code(doll::Slot::Metal, 4);
  expect(!viewCraft(q, true, false).empty(), "a blade lit from the bottom-right");
  q = g; q.c[(size_t)(16 + 5)] = 0; q.c[(size_t)(2 * 8 + 0)] = 0;
  q.c.resize(40, 0); q.h = 5; q.c[(size_t)(4 * 8 + 6)] = doll::code(doll::Slot::Metal, 2);
  expect(!viewCraft(q, true, false).empty(), "an orphan pixel");
  q = g; q.tip = false;
  expect(!viewCraft(q, true, false).empty(), "no tip mark");
  q = g; q.c[7] = q.c[6] = doll::code(doll::Slot::Metal, 2);
  expect(!viewCraft(q, true, false).empty(), "no bright tip pixel");
  q = g; q.ox = 1; q.oy = 6; q.h = 3;
  expect(!viewCraft(q, true, false).empty(), "the grip origin off the haft");
  return missed;
}

// ---------------------------------------------------------------- runners
int runGates(const std::vector<Look>& looks, bool selftests, std::vector<Line>* out) {
  Line c3{"C3 gear"}, c4{"C4 armour change"}, c5{"C5 families"}, c6{"C6 weapons"}, st{"C3-C6 selftests"};
  gateC3(c3, looks);
  if (g_report) printC3dResidual();
  gateC4(c4, looks);
  gateC5(c5, looks);
  gateC6(c6);
  if (selftests) {
    const int m3 = selftestC3(g_report), m4 = selftestC4(g_report), m5 = selftestC5(g_report), m6 = selftestC6(g_report);
    std::printf("  C3 selftest: %s\n  C4 selftest: %s\n  C5 selftest: %s\n  C6 selftest: %s\n", m3 ? "MISSED" : "ALL CAUGHT", m4 ? "MISSED" : "ALL CAUGHT",
                m5 ? "MISSED" : "ALL CAUGHT", m6 ? "MISSED" : "ALL CAUGHT");
    if (m3 + m4 + m5 + m6) st.bad(fmt("%d synthetic defect(s) missed", m3 + m4 + m5 + m6)); else st.ok();
  }
  int fails = 0;
  for (Line* l : {&c3, &c4, &c5, &c6, &st}) {
    if (l == &st && !selftests) continue;
    if (out) out->push_back(*l);
    fails += l->fails ? 1 : 0;
  }
  return fails;
}

int armsCmd(int argc, char** argv) {
  const char* looksFile = nullptr;
  const char* pool = nullptr;
  bool self = false, matrixArms = false;   // (P2c wave 2) --matrix arms: the embedded M8-M12 (kArmsLooks)
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--looks") && i + 1 < argc) looksFile = argv[++i];
    else if (!std::strcmp(argv[i], "--limbs-looks") && i + 1 < argc) looksFile = argv[++i];
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) pool = argv[++i];
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--selftest")) self = true;
    else if (!std::strcmp(argv[i], "--matrix") && i + 1 < argc) matrixArms = !std::strcmp(argv[++i], "arms");
  }
  if (pool) {
    std::string err;
    if (!doll::loadPoolFile(pool, &err)) { std::printf("FAIL: --pool %s: %s\n", pool, err.c_str()); return 1; }
  }
  const doll::PoolStats ps = doll::poolStats();
  std::printf("  pool: %d parts, %d views, %d clips (%s)\n", ps.parts, ps.views, ps.clips, ps.embedded ? "embedded" : pool);
  if (self) {
    const int m3 = selftestC3(g_report), m4 = selftestC4(g_report), m5 = selftestC5(g_report), m6 = selftestC6(g_report);
    std::printf("  C3 selftest: %s\n  C4 selftest: %s\n  C5 selftest: %s\n  C6 selftest: %s\n", m3 ? "MISSED" : "ALL CAUGHT", m4 ? "MISSED" : "ALL CAUGHT",
                m5 ? "MISSED" : "ALL CAUGHT", m6 ? "MISSED" : "ALL CAUGHT");
    const int miss = m3 + m4 + m5 + m6;
    std::printf("doll-arms selftest: %d missed\n", miss);
    return miss ? 1 : 0;
  }
  const std::vector<Look> looks = matrixArms && !looksFile ? parseLooks(kArmsLooks) : readLooks(looksFile);
  std::printf("  looks: %zu (%s)\n", looks.size(), looksFile ? looksFile : matrixArms ? "the matrices M8-M12, embedded" : "the gates' matrix, embedded");
  std::vector<Line> lines;
  const int fails = runGates(looks, true, &lines);
  for (const Line& l : lines) l.print();
  std::printf("doll-arms: %d failing line(s)\n", fails);
  return fails ? 1 : 0;
}

}  // namespace dollarms

// (P2c wave 2) the matrices M8-M12 (test_doll_envelope.cpp's --doll-coverage --scope envelope reads them)
std::string dollArmsLooksText() { return dollarms::kArmsLooks; }
// (test_doll.cpp's --doll --gates calls these)
int dollArmsGates(bool report, std::vector<std::string>& printed) {
  dollarms::g_report = report;
  std::vector<dollarms::Line> lines;
  const int fails = dollarms::runGates(dollarms::readLooks(nullptr), true, &lines);
  for (const dollarms::Line& l : lines) {
    char b[600];
    std::snprintf(b, sizeof b, "%s %-24s %4d checks, %4d failed%s%s", l.fails ? "FAIL:" : "ok   ", l.id.c_str(), l.checks, l.fails, l.fails ? "; first: " : "", l.first.c_str());
    printed.push_back(b);
  }
  return fails;
}
std::string dollForgeLooksText();   // test_doll_forge.cpp: tools/doll/forge_looks.txt (embedded)
// (P2c forge) the gates' matrix: the ladder looks, then the forge looks whose tags the ladder file lacks
std::string dollSliceLooksText() {
  std::string out = dollarms::kSliceLooks;
  std::set<std::string> have;
  auto tagOf = [](const std::string& ln) { const size_t e = ln.find_first_of(" \t"); return ln.substr(0, e); };
  auto lines = [](const std::string& t, auto fn) { for (size_t a = 0; a < t.size();) { size_t b = t.find('\n', a); if (b == std::string::npos) b = t.size(); fn(t.substr(a, b - a)); a = b + 1; } };
  lines(out, [&](const std::string& ln) { have.insert(tagOf(ln)); });
  const std::string f = dollForgeLooksText();
  lines(f, [&](const std::string& ln) { if (!ln.empty() && ln[0] != '#' && !have.count(tagOf(ln))) out += ln + "\n"; });
  return out;
}

RPG_TEST_CMD("--doll-arms", "P2c gear gates C3-C6 on the slice looks [--looks f | --matrix arms] [--pool f] [--report] [--selftest]", dollarms::armsCmd);
