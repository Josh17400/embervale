// A culture's musical style (VISION_PLAN 5.2 MusicStyle, 5.5, M3). Engine-level so the synth (engine/audio.cpp) can take
// it without knowing anything about the game; the culture engine (rpg/culture/culture.h) fills one per culture.
// Audio::setMusic(Music, const MusicStyle*) plays the Town / Wild / Night pieces in this style; Combat and Boss keep
// their structure but take the style's percussion. Plain data, packable into 64 bits so the audio thread can read it
// from one atomic.
#pragma once
#include <cstdint>

// The 12 scales (VISION_PLAN 5.2). Today's three pieces use Major (Town), Dorian (Wild) and Minor (Night/Combat).
enum class Scale : uint8_t { Major, Minor, Dorian, Phrygian, Mixolydian, Lydian, HarmonicMinor, Hijaz, PentaMajor, PentaMinor,
                             Hirajoshi, InSen, COUNT };
// The lead instrument a culture's pieces are carried by (VISION_PLAN 5.3 table)
enum class LeadInst : uint8_t { Lute, Flute, Horn, Pipes, Oud, Reed, Fiddle, Bells, Marimba, Harp, Brass, Voice, COUNT };
// The bed under it
enum class PadInst : uint8_t { Strings, Drone, Organ, Choir, Bowed, Shimmer, None, COUNT };
enum class BassInst : uint8_t { Plucked, Bowed, Drone, Horn, Hand, None, COUNT };
// Percussion family
enum class PercKind : uint8_t { None, Frame, Bodhran, Taiko, Hand, Tabla, Gong, Wood, Bells, Kettle, COUNT };

struct MusicStyle {
  Scale scale = Scale::Major;
  uint8_t bpm = 0;          // 0 = the piece's own tempo; else 50..180
  uint8_t meter = 0;        // beats per bar: 0 = the piece's own (4); 3, 4, 5, 6 (6/8), 7 (7/8)
  LeadInst lead = LeadInst::Lute;
  PadInst pad = PadInst::Strings;
  BassInst bass = BassInst::Plucked;
  PercKind perc = PercKind::Frame;
  uint8_t swing = 0;        // 0..15: 0 straight .. 15 heavy swing
  uint8_t ornament = 0;     // 0..15: grace notes, trills, slides
  uint8_t drone = 0;        // 0..15: how present a held drone is
  uint16_t seed = 0;        // the culture's own motif seed (its tunes differ from a neighbour's in the same scale)

  // 64-bit identity / transport (the audio thread reads it from one atomic). 0 never comes out of a real style
  // (bit 63 is set), so 0 means "no style: the classic pieces".
  uint64_t pack() const {
    return (1ull << 63) | (uint64_t)scale | (uint64_t)bpm << 4 | (uint64_t)(meter & 15) << 12 | (uint64_t)lead << 16 |
           (uint64_t)pad << 20 | (uint64_t)bass << 24 | (uint64_t)perc << 28 | (uint64_t)(swing & 15) << 32 |
           (uint64_t)(ornament & 15) << 36 | (uint64_t)(drone & 15) << 40 | (uint64_t)seed << 44;
  }
  static MusicStyle unpack(uint64_t v) {
    MusicStyle s;
    s.scale = (Scale)(v & 15);
    s.bpm = (uint8_t)(v >> 4);
    s.meter = (uint8_t)((v >> 12) & 15);
    s.lead = (LeadInst)((v >> 16) & 15);
    s.pad = (PadInst)((v >> 20) & 15);
    s.bass = (BassInst)((v >> 24) & 15);
    s.perc = (PercKind)((v >> 28) & 15);
    s.swing = (uint8_t)((v >> 32) & 15);
    s.ornament = (uint8_t)((v >> 36) & 15);
    s.drone = (uint8_t)((v >> 40) & 15);
    s.seed = (uint16_t)((v >> 44) & 0xFFFF);
    return s;
  }
};
