// EMBERVALE software synth: SFX voices + generative nordic-folk score + reverb/echo + limiter.
// M3 (CULTURE lane): Town, Wild and Night can play in a culture's MusicStyle (engine/music_style.h): any of 12 scales,
// its tempo and meter (3/4, 4/4, 5/4, 6/8, 7/8), its lead instrument (lute, flute, horn, pipes, oud, reed, fiddle,
// bells, marimba, harp, brass, voice), pad, bass, percussion family, swing, ornament (grace notes, slides, trills) and
// drone, with motifs from its own seed. Combat and Boss keep their structure but take the culture's drums. Without a
// style every classic piece plays exactly as before.
//
// Threading: play() / setMusic() only touch a lock-free queue and an atomic. Everything else (voice
// allocation, sequencing, DSP) happens on the audio thread inside render(), so render() also works
// standalone (no init(), no device) for offline previews. Nothing on the audio thread allocates or
// uses std::function: fixed voice pool, fixed string and delay buffers.
//
// Signal flow, per 32-sample block:
//   voices -> sfx bus -------------------------------------------------------\
//          -> music bus x layer crossfade x musicVol x ducking under sfx ----+-> + reverb/echo -> master -> limiter
//          -> reverb / echo sends -------------------------------------------/
//
// Where it lives (engine/audio/audio_internal.h has the map): this file keeps the main-thread API (play, the queue,
// setMusic); the synth, the sound effects, the score, the beds and the mixer are in engine/audio/.
#include "engine/audio.h"

// ---- (Witness 0.6 sound events) the names a listener hears (engine/audio.h setPlayListener)
namespace {
Audio::PlayListener g_playListener = nullptr;
}  // namespace
void Audio::setPlayListener(PlayListener f) { g_playListener = f; }

void Audio::play(Sfx s, float pitch, float vol) {
  if ((int)s >= (int)Sfx::COUNT || !(vol > 0.0f)) return;
  if (!(pitch > 0.0f)) pitch = 1.0f;
  if (g_playListener) g_playListener(sfxName(s), vol);
  push(Event{s, clampf(pitch, 0.25f, 4.0f), clampf(vol, 0.0f, 2.0f), 0});
}
void Audio::playMotif(uint8_t motif, uint8_t scale, uint8_t timbre, int root, float vol) {
  if (!(vol > 0.0f)) return;
  if (g_playListener) g_playListener("spell_motif", vol);
  const uint32_t arg = (uint32_t)motif | (uint32_t)(scale % (uint8_t)Scale::COUNT) << 8 | (uint32_t)(timbre & 15) << 12 |
                       (uint32_t)(root < 36 ? 36 : root > 96 ? 96 : root) << 16 | 1u << 31;
  push(Event{Sfx::SpellMotif, 1.0f, clampf(vol, 0.0f, 2.0f), arg});
}
void Audio::playSting(Sting s, float pitch, float vol) {
  if ((int)s >= (int)Sting::COUNT || !(vol > 0.0f)) return;
  if (g_playListener) g_playListener(stingName(s), vol);
  if (!(pitch > 0.0f)) pitch = 1.0f;
  push(Event{Sfx::SpellMotif, clampf(pitch, 0.25f, 4.0f), clampf(vol, 0.0f, 2.0f), (uint32_t)s | 1u << 30});
}
void Audio::push(const Event& e) {
  while (qLock_.test_and_set(std::memory_order_acquire)) {}
  uint32_t head = qHead_.load(std::memory_order_relaxed);
  if (head - qTail_.load(std::memory_order_acquire) < (uint32_t)QN) {   // full queue: drop (never block)
    q_[head % QN] = e;
    qHead_.store(head + 1, std::memory_order_release);
  }
  qLock_.clear(std::memory_order_release);
}

void Audio::setMusic(Music m) { setMusic(m, nullptr, false); }
void Audio::setMusic(Music m, const MusicStyle* style) { setMusic(m, style, false); }
// M3 PHASE A: the style is stored for the audio thread; the AUDIO lane makes updateMusic / compose play it (and
// crossfade when only the style changes). M5: the tavern's festive variant rides in bit 62 of the packed style (pack()
// uses bits 0..59 and 63), so a festival starting crossfades like a new piece.
void Audio::setMusic(Music m, const MusicStyle* style, bool festive) {
  if ((int)m >= (int)Music::COUNT) return;
  uint64_t packed = style ? style->pack() : 0;
  if (m == Music::Tavern && festive) packed |= (1ull << 62) | (1ull << 63);
  const uint32_t s = wantSeq_.load(std::memory_order_relaxed);
  wantSeq_.store(s + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  wantStyle_.store(packed, std::memory_order_relaxed);
  wantMusic_.store((int)m, std::memory_order_relaxed);
  wantSeq_.store(s + 2, std::memory_order_release);
}

