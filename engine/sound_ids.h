// The sound vocabulary: which sounds and pieces exist (Sfx, Music, Sting), with no synth and no SDL behind them.
// Who may include it: anyone that names a sound (rpg/sim, rpg/story, rpg/view, tools). Only the code that plays sounds
// needs engine/audio.h (which includes this). Values and order are saved and named by tools: append only.
#pragma once
#include <cstdint>

enum class Sfx : uint8_t {
  Swing, Hit, HitHeavy, Block, Hurt, PlayerHurt, EnemyDie, Death,
  Arrow, ArrowHit, Fireball, Explode, Frost, Heal, Roll, Step,
  Pickup, Coin, Chest, Door, Buy, Talk,
  LevelUp, QuestStart, QuestDone, Discover,
  MenuMove, MenuSelect, MenuBack,
  Roar, Splash,
  Bell,      // town alarm bell (M0 town defence): two quick strikes of a bronze bell
  // M5 Hearth and Hall (AMBIENCE lane; phase A stand-ins): village animals and the tavern's crowd
  Bark,      // a dog's bark (a short double woof)
  Cluck,     // a hen's clucking
  Meow,      // a cat
  Cheer,     // a tavern / festival crowd's cheer and clapping (a toast, the bard's last chord)
  // M6 Steel (BEASTS lane): the new families and the ranked foes. The view plays them from what it sees (the harpy's
  // swoop wind-up, the golem's heavy slam, an elite or champion first coming into view, a world boss's aggro and every
  // boss's phase change); the sim may also play them with Game::sfx.
  HarpyShriek,   // a wavering, raking bird-woman's scream (the swoop)
  GolemSlam,     // stone fists into the ground: a deep thump, grinding crunch, rubble pattering down
  EliteSting,    // an ominous sting (a dissonant swell under a cold bell): something stronger is here
  BossRoar,      // a world boss's roar: deeper and longer than the dragon's, a wall of breath and a sub drop
  // M7 Home (VIEW lane): the ridden horse's hooves (the view plays them at the walk's four-beat and the gallop's
  // three-beat cadence, pitched by the ground)
  Hoof,          // one hoof on packed earth: a hollow, woody clop with a little grit
  // M8 Fellowship (lead, phase A stand-ins; the ART lane, owner of the generated audio in M8, voices them)
  Revive,        // a friend's wounds bound: cloth drawn tight, a caught breath, a warm rising swell
  Knell,         // a companion has died: one slow, low funeral bell over a hushed choir
  Howl,          // the bonded hound's or wolf's howl (a rising, wavering call)
  // M8b Treasure (lead, phase A stand-ins). Zap and Rebirth: the POWERS lane voices them (a power's crackle / the
  // phoenix); LootChime: the VIEW lane plays it when an Epic or better piece drops or is picked up (k: the pitch by
  // rarity), the POWERS lane voices it (it owns engine/audio.* in M8b)
  Zap,           // chain lightning / a falling star's strike: a bright electric crack and a sizzle
  Rebirth,       // the phoenix: a rush of fire rising into a bright chord
  LootChime,     // a fine piece found: a glassy rising arpeggio (higher and longer for Legendary and Mythic)
  // M9 Arcana & Mastery (SPELLFX lane): the spells' voices. The view plays them from the Ev::Spell phases
  // (rpg/view/render_magic.cpp); the sim plays none for a spell. k: the school's / the size's pitch.
  SpellHum,      // the wind-up: a breathy hum swelling under a glassy shimmer (soft by design: a bed under the cast)
  SpellCharge,   // an upcast step: a rising swell and a bright tick (k rises with the level)
  SpellFizzle,   // not enough magicka / broken: a damp pfft and a falling sputter
  FireCast,      // fire leaves the hands: a roaring whoosh, a throaty rumble, crackle
  FireBurst,     // fire lands: a deep whump, a boiling roar, crackle and debris
  FrostCast,     // frost: a crisp crystalline zing and a breath of cold air
  FrostShatter,  // frost lands: glass shattering, tinkling shards, an icy hiss
  StormCast,     // lightning leaves the hands: a bright electric snap and a buzz
  Thunder,       // lightning strikes: a white crack, then thunder rolling away
  HolyCast,      // restoration: a rising choir swell under bells
  HolyBloom,     // a heal blooms: a warm major chord and a soft shimmer
  RiftTear,      // conjuration: the air tearing (a ripping, downward groan into a reversed swell)
  SummonRise,    // something climbs out of the rift: a low growl rising over whispers
  Shimmer,       // illusion: a detuned glassy warble, wavering in pitch
  DreamPulse,    // illusion lands: a reversed swell into a soft sub thump and an echoing bell
  StoneGrind,    // alteration: stone grinding over a low pulse
  StoneSlam,     // alteration lands: a rock slam, rubble pattering, a slosh of water
  StarChime,     // starcalling: high bells drawn out like a constellation
  MeteorFall,    // a meteor falls: a descending shriek of air building into a roar (the landing is a FireBurst)
  WardRing,      // a ward raised or struck: a glassy ring over a resonant hum
  Sizzle,        // a surface combo: steam hissing out of a short whoomph
  SpellMotif,    // a school's / tradition's motif: 3-4 notes in the culture's scale (Audio::playMotif; play() gives motif 0)
  // M10 Beyond the Mist (ART lane): the sea's moments. The view plays them from the Ev::Sea events (render_sea.cpp);
  // the sea's continuous sound (waves, surf, gulls, wind in the sails, rigging, oars, the storm, the Mist) is the sea
  // bed (Audio::setSea). k: a pitch / size.
  WaveCrash,     // a wave breaking on the hull or a rock: a thump of water, then the hiss of spray
  SailFlap,      // canvas filling or luffing: two or three quick flaps and a taut thrum
  OarStroke,     // an oar's blade dipping and pulling through, the knock of its lock
  ReelClick,     // the reel's ratchet clicking as line is won (k: faster with the tension)
  LineSnap,      // the line parting under strain: a twang and a whip
  FishPlop,      // a fish taking the float under: a small plop
  SerpentRoar,   // the sea serpent rising: a hissing, bubbling roar that rolls into a deep bellow
  KrakenRumble,  // the kraken's arm breaking the surface: a deep groan, water streaming off it
  MerrowChatter, // merrow calling to each other: clicking, gurgling chirps
  HullCrack,     // the hull struck: timber cracking, a groan of the frames
  MistWhisper,   // the Mist closing in: breathy whispers over a cold, beating drone
  MistChime,     // the Mist parting: a rising shimmer of bells into a warm chord
  ShipBell,      // a ship's bell rung twice (a harbour, a turn of the watch)
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
