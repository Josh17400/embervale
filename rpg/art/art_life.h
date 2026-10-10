// EMBERVALE art API, M5 "Hearth and Hall" (VISION_PLAN 10.2-10.3, 15.12): what townsfolk DO with their bodies (poses:
// sitting, eating and drinking, working with tools, sleeping in bed, waving, playing, dancing, playing an instrument,
// praying, begging, lighting lamps), the village animals (dogs, cats, chickens...), the speech-bubble icons and the
// festival dressing. Part of rpg/art.h (include that). Painted in rpg/art/art_life.cpp (ART lane; phase A stand-ins
// built from the standing sheet). The ART lane may ADD to this header; never rename, remove or change the meaning of
// what phase A put here.
//
// Every frame keeps the game's camera: the high 3/4 top-down view with top-left light, a 1 px dark outline (a darkened
// neighbour colour, never pure black), 3-4 tone shading, the cohesive palette (art_internal.h). No ground shadows in the
// sprites (the renderer draws them).
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "engine/music_style.h"
#include "rpg/art/art_human.h"
#include "rpg/art/art_props.h"

namespace art {

// ---------------------------------------------------------------- postures ("poses"; art::Posture: the rig has its own internal Pose)
// Values are runtime only (Actor::posture); append anyway.
enum class Posture : uint8_t {
  None,       // the standing sheet (humanSheet) decides: idle, walk, attack, hurt
  Sit,        // seated on a chair / bench / stool / cushion (the culture's furniture)
  SitEat,     // seated, eating (a spoon or bread to the mouth)
  SitDrink,   // seated, a mug raised
  Eat,        // standing, eating from the hand
  Drink,      // standing, a mug
  Cheer,      // standing, a mug raised high (a toast, a festival)
  Hammer,     // the smith at the anvil: hammer up / down
  Hoe,        // the farmer in the field
  Sweep,      // a broom: shopkeepers, servants, the innkeeper
  Chop,       // an axe: woodcutters, the woodpile
  Stir,       // a ladle in a pot: the cook, the innkeeper's kitchen
  Carry,      // a sack or basket on the shoulder (walk frames: the view picks them while moving)
  Fish,       // a rod held out over the water
  Sleep,      // lying in bed under the blanket (drawn OVER the bed prop: PostureInfo::lying)
  Wave,       // a hand raised in greeting
  Play,       // children at tag: arms up, a hop (run frames)
  Dance,      // festival / tavern dancing (a turning step)
  Lute,       // the bard plucking a lute (the culture's string instrument)
  Drum,       // a hand drum
  Flute,      // a flute / pipe
  Pray,       // hands joined, head bowed (temples, shrines)
  Beg,        // sitting on the ground, a bowl held out (Outfit::Rags)
  Lamp,       // the lamplighter's pole raised to a lamp
  Read,       // a book or a ledger held open (scholars, the exchange's clerks)
  // ---- ART lane, phase B (appended): seated on the floor (cushions round a low table, sleeping mats, the ground):
  //      cross-legged. seatFit(...).sit / seatedPosture() map Sit / SitEat / SitDrink onto these for floor seats.
  SitFloor,      // cross-legged on a cushion / mat / the ground
  SitFloorEat,   // cross-legged, eating
  SitFloorDrink, // cross-legged, a cup raised
  // ---- M7 Home (lead, phase A; ART lane paints it): astride a horse (art::horseSheet / riderSeat): legs either side,
  //      the reins in the hands; phase A stand-in: the seated sheet
  Ride,
  COUNT
};
constexpr int POSTURE_FRAMES = 4;   // columns per pose sheet
// How the view draws a pose. The sheet has POSTURE_FRAMES columns x 3 rows (0 facing down, 1 up, 2 right; flip for
// left) of HUMAN_W x HUMAN_H cells, anchored like the standing sheet (bottom-centre at the actor's feet) plus dy.
struct PostureInfo {
  uint8_t frames = 1;          // frames used (1..POSTURE_FRAMES), played in order and looped at fps
  float fps = 4;
  int8_t dy = 0;               // px the cell is drawn lower than a standing figure (seated poses sink onto the seat)
  bool seated = false;         // on furniture: the actor stands on the seat's tile (the view sorts it in front of
                               //   the chair back when facing down, behind the table when facing up)
  bool lying = false;          // drawn over a bed (one row only: the bed's direction; the view draws it after the bed)
  bool oneRow = false;         // the same cells for every facing (row 0 only)
};
PostureInfo postureInfo(Posture p);
// the posture sheet of a look (POSTURE_FRAMES x 3 cells). Cached by the view under (look.key(), posture).
Canvas humanPostureSheet(const HumanLook& look, Posture p);

// ---------------------------------------------------------------- village animals (VISION_PLAN 10.3 small life)
enum class Critter : uint8_t {
  Dog,        // a village dog (several coats by variant): trots after its owner, lies by the door, barks at wolves
  Cat,        // a cat on walls, sills and doorsteps; grooms, sleeps in the sun
  Chicken,    // a hen pecking in yards (white / brown / speckled by variant)
  Rooster,    // the yard's cock
  Goat,       // a tethered goat (herding villages)
  Pig,        // a pig in a sty (farming villages)
  Duck,       // ducks by ponds and rivers
  // ---- M7 Home (lead, phase A; ART lane paints them; phase A stand-ins on the goat's body): the farm's beasts
  Cow,        // a dairy cow (dun, black-and-white, red by variant)
  Sheep,      // a woolly sheep (a shorn coat by variant bit 7)
  Horse,      // a loose horse in a paddock or stable (the ridden horse is art::horseSheet)
  COUNT
};
// Sheet: CRITTER_FRAMES columns x 3 rows (0 facing down, 1 up, 2 right; flip for left) of critterCellW x critterCellH.
//   columns: 0 idle, 1-4 walk / trot, 5-6 its action (dog: sit and wag; cat: groom; hen: peck; goat and pig: graze /
//   root; duck: dabble), 7 asleep / lying down
constexpr int CRITTER_FRAMES = 8;
int critterCellW(Critter c);
int critterCellH(Critter c);
// variant: the coat / plumage (hash of the animal's identity; each kind decides how many it has)
Canvas critterSheet(Critter c, uint32_t variant);

// ---------------------------------------------------------------- speech bubbles (VISION_PLAN 10.3 chat pairs)
enum class Bubble : uint8_t {
  None, Talk,   // "..." (chatting)
  Exclaim,      // "!" (greeting a friend, surprise, alarm)
  Note,         // a music note (singing, the bard)
  Mug,          // a mug (ordering, a toast)
  Heart,        // a friend, a couple
  Bread,        // hungry
  Zzz,          // asleep / weary
  Coin,         // a deal, a beggar's plea
  Anger,        // a brawl, a grumble
  Tear,         // grief
  Question,     // a rumour, a question
  COUNT
};
// a small framed bubble (about 11x10 px, its tail at the bottom centre: the view anchors it over the speaker's head)
Canvas bubbleSprite(Bubble b);

// ---------------------------------------------------------------- festival dressing (15.12 content towns)
// a string of pennants `widthPx` long (sagging between two posts / eaves), in the town's colours
Canvas festivalBunting(int widthPx, uint32_t a, uint32_t b, uint32_t seed);
// a paper lantern / garland hung at doors and stalls on festival nights (lit by the view's light pass)
Canvas festivalLantern(uint32_t color, uint32_t seed);

// ================================================================ ART lane, phase B (appended)

// ---------------------------------------------------------------- the bard's instrument by culture
// Lute, Flute and Drum have culture variants: humanPostureSheet(look, p, variant) with variant =
//   Lute:  (uint8_t)LeadInst  -> Lute (default), Oud (round-backed, bent neck), Fiddle (and its bow), Harp (a small
//          frame harp); any other value: the lute
//   Flute: (uint8_t)LeadInst  -> Flute (transverse, default), Pipes (pan pipes), Reed (a shawm, bell down), Horn / Brass
//          (a curved horn), Bells (hand bells), Voice (singing, a hand on the heart); any other: the flute
//   Drum:  (uint8_t)PercKind  -> Frame (default), Bodhran (and its beater), Taiko / Kettle (a barrel drum on a strap,
//          two sticks), Hand / Tabla (a pair of small hand drums), Gong, Wood (clappers), Bells (a jingle ring)
// variant 0 is each one's default look, so humanPostureSheet(look, p) == humanPostureSheet(look, p, 0). The view caches
// the sheet under (look.key(), posture, variant).
Canvas humanPostureSheet(const HumanLook& look, Posture p, uint8_t variant);
// what a culture's bard plays (MusicStyle from the culture): the posture (Lute / Flute / Drum) and its variant
Posture bardPosture(const MusicStyle& s);
uint8_t bardVariant(const MusicStyle& s);
// one cell (row = facing 0 down / 1 up / 2 right, frame 0..POSTURE_FRAMES-1) before the outline: tests and galleries
Canvas humanPostureCellRaw(const HumanLook& look, Posture p, uint8_t variant, int row, int frame);

// ---------------------------------------------------------------- seats: how a sitter sits on each seat kind
// The view draws a posed figure like a standing one: the cell's top-left at (p.x - HUMAN_W/2, p.y - HUMAN_H + 2 + dy)
// (PostureInfo::dy is 0 for every seated posture). seatFit says where the sitter's p goes so its seat line lands on
// the seat's surface, and how to sort it against the seat:
//   seat   Prop::Chair, Stool, Bench, Throne (chair height), Cushion (the floor), SleepingMat / Bedroll / Rug (sitting on
//          the floor beside or on them), or Prop::COUNT for the bare ground (a beggar's corner);
//   kit    the culture archetype (cult::Archetype) when the seat is a culture piece, else -1 (today every seat is the
//          classic prop: the value is accepted for future culture seats);
//   facing 0 down (toward the camera), 1 up, 2 right, 3 left (the figure flips; the fit mirrors).
struct SeatFit {
  bool ok = false;             // the seat takes a sitter in this facing (a Chair: facing down or sideways only, its
                               //   back is to the north; a sitter "facing up" on it would face the chair back)
  int8_t ax = 0, ay = 0;       // the sitter's p relative to the seat tile's bottom-centre (tx * 16 + 8, ty * 16 + 16)
  bool front = true;           // draw the sitter after the seat (true for every seat: the sitter's body covers the
                               //   seat; the chair back is behind it when facing down). false: before it
  int8_t seatY = 0;            // the seat surface's row inside its tile (0 top .. 15 bottom): where the sitter's seat
                               //   line (SEAT_ROW of the posture cell) lands
  Posture sit = Posture::Sit;  // Sit (chair height) or SitFloor (cross-legged: cushions, mats, the ground)
};
constexpr int SEAT_ROW = 19;        // the cell row a chair-height sitter rests on the seat with (all facings)
constexpr int SEAT_ROW_FLOOR = 22;  // the cell row a cross-legged sitter rests on the floor / cushion with
SeatFit seatFit(Prop seat, int kit, int facing);
// Sit / SitEat / SitDrink on a floor seat -> SitFloor / SitFloorEat / SitFloorDrink (and back); anything else unchanged
Posture seatedPosture(Prop seat, Posture p);

// ---------------------------------------------------------------- beds: how a sleeper lies in each bed kind
enum class Berth : uint8_t {
  Bed,          // Prop::Bed, the classic one-tile bed (20x32; a culture bed: cultureInteriorPiece(arch, Bed, 0))
  LongBed,      // the two-tile bed: Piece::Styled 4 (20x48) or cultureInteriorPiece(arch, Bed, 1)
  BunkLow,      // Prop::BunkBed's lower bunk (20x40: the head lies in the upper deck's shade)
  BunkHigh,     // Prop::BunkBed's upper bunk
  Hammock,      // Prop::Hammock (28x22, slung west-east: the sleeper lies across, head west)
  LongHammock,  // Piece::Styled 6 (20x48, head to the wall)
  Mat,          // Prop::SleepingMat (24x14, across, head on the bolster to the west)
  LongMat,      // Piece::Styled 7 (20x48)
  Bedroll,      // Prop::Bedroll (the roadside bedroll, across, head west on its pillow)
  Ground,       // no bed: curled under a blanket / cloak on the floor or the street (a beggar, a refugee)
  COUNT
};
struct BedFit {
  int8_t w = 0, h = 0;           // the berth sprite's canvas (sleeperSprite's size; drawn at the berth's own anchor)
  int8_t headX = 0, headY = 0;   // the sleeper's face centre in that canvas
  bool across = false;           // lying west-east (head west; mirror the canvas for head east)
  int8_t ax = 0, ay = 0;         // the GENERIC Sleep posture cell (humanPostureSheet(look, Sleep), row 0): the actor's p
                                 //   relative to the berth's anchor tile bottom-centre so that its head lies on this
                                 //   pillow (a vertical berth; for an across berth use sleeperSprite)
  int8_t len = 0;                // the length of the body under the blanket (px)
};
// kit: the culture archetype for cultureInteriorPiece beds (Berth::Bed / LongBed), else -1
BedFit bedFit(Berth b, int kit);
// The sleeper fitted to a berth: a canvas of bedFit(b, kit).w x h, drawn right AFTER the berth with the berth's own
// anchor (bottom-centre on its tile). The head on the pillow (hats and helmets off, eyes shut), the shoulders and hands
// over the turned-down sheet, the body a soft mound under the berth's own blanket (shading only, so every quilt colour
// shows through); Ground brings its own blanket (the look's cloak colour, or a drab wool). frame 0..1: breathing.
Canvas sleeperSprite(const HumanLook& look, Berth b, int kit, int frame);
// the berth sprite itself for the galleries / tests (the classic or culture piece, frame 0)
Canvas berthSprite(Berth b, int kit, int variant);

// ---------------------------------------------------------------- lamplight
// a lit lamp's flame and halo, drawn over a lamppost's lamp (or a lantern) when lampLit says so. 4 frames side by side
// (LAMP_FLAME_W wide each, LAMP_FLAME_H tall); the flame's base is the canvas's bottom-centre. lampHead(): where the
// classic Prop::Lamppost (12x32) holds its flame, from the prop canvas's top-left.
constexpr int LAMP_FLAME_W = 9, LAMP_FLAME_H = 11, LAMP_FLAME_FRAMES = 4;
Canvas lampFlame(uint32_t tint = 0);
void lampHead(int& x, int& y);

}  // namespace art
