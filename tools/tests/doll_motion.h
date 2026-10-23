// doll_motion.h: the code contract between GATES-LIMBS (test_doll_motion.cpp, test_doll.cpp) and GATES-WEAR
// (test_doll_wear.cpp), docs/P2B_MOTION_ROLLOUT.md 5.8. B11 limbs, B11 wear and B12 boots measure rig 2's side walk / run
// frames; the meaning of every limbs check is .tmp/judge/proto2/check.py's (the oracle).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art/doll/doll.h"

namespace dollmotion {
struct Look { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; };  // walk run dwalk drun carry carry1 carry2 play dance
struct Img { int w = 0, h = 0, px = 0, py = 0; std::vector<uint32_t> c; };   // pivot at (px, py); probe codes or RGBA
struct Frame {
  std::string gait; int facing = 2, i = 0;                        // facing 2 R, 3 L
  int plant0 = -32768, plant1 = -32768, bob = 0, lean = 0, hipX = 0, hipY = 0, headX = 0, headY = 0;   // the clip frame's moves (kNoPlant when none)
  std::map<std::string, std::string> keys;                         // channel -> key
  std::vector<std::pair<std::string, Img>> parts;                  // each part of the recipe ALONE on its layer, probe codes
  Img raw, fin;                                                    // the whole figure: probe codes; finished in the look's ramps
};
struct LookFrames { Look look; art::doll::DollRecipe recipe; std::vector<Frame> idle, frames; };   // idle: R then L
struct Fail { std::string check, where, why; };
std::vector<Look> readLooks(const char* file);                         // GATES-LIMBS (the existing format, new gaits and fields)
LookFrames shoot(const Look& l, const char* poolFile);                // GATES-LIMBS
void limbsChecks(const LookFrames& f, std::vector<Fail>& out, bool report);   // GATES-LIMBS: B11 limbs, B12 boots
int  limbsSelftest(bool report);                                      // GATES-LIMBS
std::vector<Look> matrixLooks(const std::string& filter);             // GATES-WEAR: section 5.5, part coverage
void wearChecks(const LookFrames& f, std::vector<Fail>& out, bool report);    // GATES-WEAR: B11 wear, T1, T2
int  wearSelftest(bool report);                                       // GATES-WEAR
int  freezeCmd(const char* file, bool write);                         // GATES-WEAR
void b10Watch(std::vector<Fail>& out, bool report);                   // GATES-WEAR: the M7 sweep (called by test_doll.cpp's B10)
void writeStrips(const LookFrames& f, const char* dir);               // GATES-WEAR: --frames

// Notes for both sides (GATES-LIMBS):
// - Img codes: probe code (slot * 5 + step + 2) in the low byte, 0 clear; leg regions recoded as check.py's dump does
//   (Thigh -> Glow, Knee -> Field, Shin -> Fx, Boot -> Plume, Sole -> Device). fin holds RGBA (0 clear).
// - Frame::parts names are the pool's part names; a part that draws nothing in the frame is left out.
// - poolFile may be null (the pool already loaded); shoot() never reloads a pool it has already loaded.
}  // namespace dollmotion
