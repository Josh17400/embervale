// The sound vocabulary: which sounds and pieces exist (Sfx, Music, Sting), with no synth and no SDL behind them.
// Who may include it: anyone that names a sound (rpg/sim, rpg/story, rpg/view, tools). Only the code that plays sounds
// needs engine/audio.h (which includes this). Values and order are saved and named by tools: append only.
#pragma once
#include <cstdint>

// SFX_LIST(X): every sound effect in order, X(name, trim). It generates the Sfx enum below, Audio::sfxName ("HitHeavy" ->
// "hit_heavy") and the per-effect trims (engine/audio/sfx.cpp), so the three cannot drift apart. Append only. A sound's
// voice: engine/audio/sfx.cpp, Audio::fx<name>.
// The trims are calibrated with tools/audio_preview so single effects peak below the limiter (0.8 at master 0.6):
// impacts ~0.7, spells/fanfares ~0.45-0.65, pickups ~0.35, UI and footsteps deliberately soft. Calibration notes:
//   Bark Cluck Meow Cheer (M5 stand-ins)
//   HarpyShriek GolemSlam EliteSting BossRoar (M6; integration: +2 / +4 dB to clear the music by 6 dB, phone too)
//   Hoof (M7: under the music, a little above a footstep)
//   Revive Knell Howl (M8 ART; calibrated with tools/audio_preview)
//   Zap Rebirth LootChime (M8b POWERS; calibrated with tools/audio_preview: clear of the music on a phone, the rebirth under the limiter)
//   (M9 SPELLFX; calibrated with tools/audio_preview --magic)
//   (M10 ART; calibrated with tools/audio_preview --sea)
#define SFX_LIST(X) \
  X(Swing, 1.3f) X(Hit, 0.87f) X(HitHeavy, 0.68f) X(Block, 0.73f) X(Hurt, 0.88f) X(PlayerHurt, 0.87f) X(EnemyDie, 0.82f) X(Death, 0.76f) \
  X(Arrow, 1.08f) X(ArrowHit, 1.0f) X(Fireball, 0.72f) X(Explode, 0.65f) X(Frost, 0.95f) X(Heal, 1.0f) X(Roll, 1.35f) X(Step, 1.0f) \
  X(Pickup, 1.6f) X(Coin, 2.0f) X(Chest, 1.0f) X(Door, 0.95f) X(Buy, 1.7f) X(Talk, 1.0f) \
  X(LevelUp, 0.88f) X(QuestStart, 1.15f) X(QuestDone, 0.9f) X(Discover, 0.8f) \
  X(MenuMove, 1.0f) X(MenuSelect, 1.0f) X(MenuBack, 1.0f) \
  X(Roar, 0.67f) X(Splash, 1.1f) \
  X(Bell, 0.8f) /* town alarm bell (M0 town defence): two quick strikes of a bronze bell */ \
  /* M5 Hearth and Hall (AMBIENCE lane; phase A stand-ins): village animals and the tavern's crowd */ \
  X(Bark, 0.7f) /* a dog's bark (a short double woof) */ \
  X(Cluck, 0.6f) /* a hen's clucking */ \
  X(Meow, 0.6f) /* a cat */ \
  X(Cheer, 0.6f) /* a tavern / festival crowd's cheer and clapping (a toast, the bard's last chord) */ \
  /* M6 Steel (BEASTS lane): the new families and the ranked foes. The view plays them from what it sees (the harpy's */ \
  /* swoop wind-up, the golem's heavy slam, an elite or champion first coming into view, a world boss's aggro and every */ \
  /* boss's phase change); the sim may also play them with Game::sfx. */ \
  X(HarpyShriek, 0.78f) /* a wavering, raking bird-woman's scream (the swoop) */ \
  X(GolemSlam, 0.8f) /* stone fists into the ground: a deep thump, grinding crunch, rubble pattering down */ \
  X(EliteSting, 1.2f) /* an ominous sting (a dissonant swell under a cold bell): something stronger is here */ \
  X(BossRoar, 0.66f) /* a world boss's roar: deeper and longer than the dragon's, a wall of breath and a sub drop */ \
  /* M7 Home (VIEW lane): the ridden horse's hooves (the view plays them at the walk's four-beat and the gallop's */ \
  /* three-beat cadence, pitched by the ground) */ \
  X(Hoof, 0.6f) /* one hoof on packed earth: a hollow, woody clop with a little grit */ \
  /* M8 Fellowship (lead, phase A stand-ins; the ART lane, owner of the generated audio in M8, voices them) */ \
  X(Revive, 1.75f) /* a friend's wounds bound: cloth drawn tight, a caught breath, a warm rising swell */ \
  X(Knell, 0.95f) /* a companion has died: one slow, low funeral bell over a hushed choir */ \
  X(Howl, 0.7f) /* the bonded hound's or wolf's howl (a rising, wavering call) */ \
  /* M8b Treasure (lead, phase A stand-ins). Zap and Rebirth: the POWERS lane voices them (a power's crackle / the */ \
  /* phoenix); LootChime: the VIEW lane plays it when an Epic or better piece drops or is picked up (k: the pitch by */ \
  /* rarity), the POWERS lane voices it (it owns engine/audio.* in M8b) */ \
  X(Zap, 1.0f) /* chain lightning / a falling star's strike: a bright electric crack and a sizzle */ \
  X(Rebirth, 1.25f) /* the phoenix: a rush of fire rising into a bright chord */ \
  X(LootChime, 1.3f) /* a fine piece found: a glassy rising arpeggio (higher and longer for Legendary and Mythic) */ \
  /* M9 Arcana & Mastery (SPELLFX lane): the spells' voices. The view plays them from the Ev::Spell phases */ \
  /* (rpg/view/render_magic.cpp); the sim plays none for a spell. k: the school's / the size's pitch. */ \
  X(SpellHum, 1.0f) /* the wind-up: a breathy hum swelling under a glassy shimmer (soft by design: a bed under the cast) */ \
  X(SpellCharge, 1.9f) /* an upcast step: a rising swell and a bright tick (k rises with the level) */ \
  X(SpellFizzle, 2.3f) /* not enough magicka / broken: a damp pfft and a falling sputter */ \
  X(FireCast, 1.0f) /* fire leaves the hands: a roaring whoosh, a throaty rumble, crackle */ \
  X(FireBurst, 0.8f) /* fire lands: a deep whump, a boiling roar, crackle and debris */ \
  X(FrostCast, 1.4f) /* frost: a crisp crystalline zing and a breath of cold air */ \
  X(FrostShatter, 1.0f) /* frost lands: glass shattering, tinkling shards, an icy hiss */ \
  X(StormCast, 1.55f) /* lightning leaves the hands: a bright electric snap and a buzz */ \
  X(Thunder, 1.0f) /* lightning strikes: a white crack, then thunder rolling away */ \
  X(HolyCast, 1.6f) /* restoration: a rising choir swell under bells */ \
  X(HolyBloom, 1.1f) /* a heal blooms: a warm major chord and a soft shimmer */ \
  X(RiftTear, 1.0f) /* conjuration: the air tearing (a ripping, downward groan into a reversed swell) */ \
  X(SummonRise, 1.15f) /* something climbs out of the rift: a low growl rising over whispers */ \
  X(Shimmer, 1.35f) /* illusion: a detuned glassy warble, wavering in pitch */ \
  X(DreamPulse, 1.1f) /* illusion lands: a reversed swell into a soft sub thump and an echoing bell */ \
  X(StoneGrind, 1.0f) /* alteration: stone grinding over a low pulse */ \
  X(StoneSlam, 0.9f) /* alteration lands: a rock slam, rubble pattering, a slosh of water */ \
  X(StarChime, 1.25f) /* starcalling: high bells drawn out like a constellation */ \
  X(MeteorFall, 1.25f) /* a meteor falls: a descending shriek of air building into a roar (the landing is a FireBurst) */ \
  X(WardRing, 1.0f) /* a ward raised or struck: a glassy ring over a resonant hum */ \
  X(Sizzle, 1.0f) /* a surface combo: steam hissing out of a short whoomph */ \
  X(SpellMotif, 1.0f) /* a school's / tradition's motif: 3-4 notes in the culture's scale (Audio::playMotif; play() gives motif 0) */ \
  /* M10 Beyond the Mist (ART lane): the sea's moments. The view plays them from the Ev::Sea events (render_sea.cpp); */ \
  /* the sea's continuous sound (waves, surf, gulls, wind in the sails, rigging, oars, the storm, the Mist) is the sea */ \
  /* bed (Audio::setSea). k: a pitch / size. */ \
  X(WaveCrash, 1.0f) /* a wave breaking on the hull or a rock: a thump of water, then the hiss of spray */ \
  X(SailFlap, 2.4f) /* canvas filling or luffing: two or three quick flaps and a taut thrum */ \
  X(OarStroke, 1.2f) /* an oar's blade dipping and pulling through, the knock of its lock */ \
  X(ReelClick, 3.0f) /* the reel's ratchet clicking as line is won (k: faster with the tension) */ \
  X(LineSnap, 1.2f) /* the line parting under strain: a twang and a whip */ \
  X(FishPlop, 1.3f) /* a fish taking the float under: a small plop */ \
  X(SerpentRoar, 0.75f) /* the sea serpent rising: a hissing, bubbling roar that rolls into a deep bellow */ \
  X(KrakenRumble, 0.8f) /* the kraken's arm breaking the surface: a deep groan, water streaming off it */ \
  X(MerrowChatter, 2.0f) /* merrow calling to each other: clicking, gurgling chirps */ \
  X(HullCrack, 1.0f) /* the hull struck: timber cracking, a groan of the frames */ \
  X(MistWhisper, 1.0f) /* the Mist closing in: breathy whispers over a cold, beating drone */ \
  X(MistChime, 1.1f) /* the Mist parting: a rising shimmer of bells into a warm chord */ \
  X(ShipBell, 0.9f) /* a ship's bell rung twice (a harbour, a turn of the watch) */

enum class Sfx : uint8_t {
#define EMB_SFX_ENUM(name, trim) name,
  SFX_LIST(EMB_SFX_ENUM)
#undef EMB_SFX_ENUM
  COUNT
};

// Background score. Changing mode crossfades (~2 s) to the new piece.
enum class Music : uint8_t {
  Silence,
  Title,    // slow, majestic, nordic-folk theme
  Wild,     // calm exploration, day
  Night,    // sparse, mysterious exploration at night
  Town,     // warm, lute/flute-ish, cosy
  Cave,     // dark drones, drips
  Combat,   // driving drums, minor
  Boss,     // epic, heavy
  // M5 Hearth and Hall: the bard's piece in the tavern, in the culture's style (setMusic(Tavern, style, festive)): its
  // scale and lead instrument, its meter a little quicker, the bass and drums up front, a clap on the off-beats; on a
  // festival the dance meter (a 4/4 culture turns to a 6/8 jig) and quicker still. The view picks it inside a gathering
  // place while the bard performs, faintly outside its door at night (setMusicLevel: quieter and muffled by distance)
  // and on the plaza on festival days.
  Tavern,
  COUNT
};

// (M12 VIEW lane) the herald's stings of the player's rise (rpg/view/render_rise.cpp): an oath's drum roll, a rank's
// fanfare, independence's horn call, a warlord's or a beast's alarm, a seal pressed into wax (orders, an envoy's
// letters). They ride the Sfx::SpellMotif slot with their own marker (so the Sfx list and its tools stay as they are).
// Audio::Sting is an alias of this enum.
enum class Sting : uint8_t { DrumRoll, Fanfare, HornCall, WarAlarm, Seal, COUNT };
