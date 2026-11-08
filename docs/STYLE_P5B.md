# STYLE_P5B: every people's signature on its armour (P5b, planner, 2026-11-05)

**Status: PENDING THE OWNER'S REVIEW. Binding on every P5b art and doll area.** Base `f914d03` (branch `fix-p5b-int`; the
stopped P5 batch, `docs/FIX_BATCH_P5.md`). Read with STYLE_P2 27 (the ladder), 28.5 (the peoples' vocabularies), 29 (the
envelope) and 30 (P5's fingerprint), and PLAN D54, D55, D83, D96, D116-D124.

**The owner (2026-11-04/05):** the P5 pack, "a lot of it looks the same"; the ruling, "every culture's finger print to be on
the armor whether that's color or style, something that makes it unique to the culture"; on the FLUX concepts, "those
look great ... good inspiration to go off of". P5's judge (r1) failed on worn twins: thin T1-T3 dye bands read alike.

**Inputs.** `C:\Users\joshu\Documents\embervale-ref\armour\INDEX.md` (a SIGNATURE per people from about 270 real images;
`contact_<people>.png`) and `...\armour\generated\` (FLUX concepts: `<people>/<people>_full_{levy,veteran,elite}_{man,woman}.png`,
`<people>_helmets_{1,2}.png`, `<people>_shields.png`, `<people>_pixel_{1,2}.png`; `generated_contact_<people>.png`;
`PROMPTS.md`). The concepts are FLUX.1-dev output under a non-commercial licence: **ideas only. Draw our own pixels; never
trace, sample or import them**, and never copy a reference photo's pixels.

---

## 1. The feel

A player names a soldier's people from across a square at 1x, in greyscale, before reading a colour: the Jade by the
flare of the kabuto and the bell of laced skirt, the Imperial by the crimson crest and the tall scutum, the Fjordfolk by
the round iron dome over a fur collar and a shield as big as the man. A levy and a champion of one people are plainly the
same people several rungs apart: the levy's cap is the ancestor of the champion's helm, its colours are the same two
colours, its hem the same cut, only plainer, slimmer and humbler (rags to riches). The material still reads first on every
plane (leather like leather, bronze like bronze, D116 and D117); the people shows in the **outline** of the head,
shoulders and hem, and in **one dominant colour and one accent** on the cloth.

**What changes from P5.** P5 put the people's thread on the edges (1 px bands, a stitched mark). That was the right finish
and the wrong size: at 32 px a 1 px band is noise, so T1-T3 still read alike. P5b moves the fingerprint into the three
biggest reads a 32 px figure has (INDEX "pixel_style": the head is a third of the sprite, then the shoulders, then the
hem) and into one dominant colour on the large cloth areas. P5's thread, marks, filigree, makers and gates stay as the
detail and the plumbing (section 8).

## 2. Rules at 32 px (every area)

1. **Read order: head, shoulders, hem, colour pair, motif.** Spend effort and bytes in that order.
2. **Greyscale first.** At every tier the twelve heads are twelve outlines. Test every head, kit and icon in greyscale and
   as a black silhouette fill before colour.
3. **One dominant and one accent per people** (section 3), on cloth, lacing, binding, linings, crests and painted fields.
   Never on a lit metal or leather plane, never more than 30 % of a figure, never a checker or dither.
4. **The ladder holds** (STYLE_P2 27, 29.3). T1 is simple and slim: a cap or hood inside the hair's 2-row rise (only the
   brims and flares 29.3 names may pass, by up to 3 px), no pauldrons, a short hem, close or bare arms. T2 is a soft cap or
   a plain metal one (3 rows) with small shoulders. From T3 the people's metal helm grows every tier: T4 the crest or
   plume, T5 the legend growth in gilt, T6 the living finish. Areas keep C12f's steps (T3 >= 1.20, T5 >= 1.55 x T1).
5. **One signature family a people from T3 to T6, growing** (where the people has one). Identity comes from consistency:
   the Jade's kabuto at T3 gains its flare at T4 and gilt kuwagata at T5. Consecutive tiers still differ in outline at 1x.
   **The alloys (decision 8, reading 1: the owner's ruling 2026-11-06, built in P5b2 area E; restated by the owner
   2026-11-07, built in P5b4: "no thats what i ment i think t3-t4 is a better representaion of every day armor").** A
   kingdom's **everyday** armour is its **T3 and T4** gear and its **masterwork** its T5-T6 gear; both wear the people's
   signature, of every kingdom of the people (the ladder's kits, the slice and the production sheets the owner approved).
   **What "everyday = T3-T4" is in code (P5b4):** `gear::kitBand(tier)` is 1 (everyday) at T3-T4, 2 (masterwork) at T5-T6, 0
   at T1-T2; `gear::kitAlloy(c, i, t)` is the first alloy (i = 0) in band 1 and the culture's last alloy in band 2. The
   everyday kit's **T3 pieces are material pieces, not alloys**: the people's own **steel** (`ladderKit` at T3: mail, helm,
   kite in steel made by the culture). The census's T3 soldiers keep their **kingdom's livery metal** (see the table below:
   `census::dress` gives the watch and the soldiers the kingdom's tint, `rankTiers` at T3 keeps it) and wear the **people's T3
   signature** as the kit does. An alloy never draws at T3 while FIX_LINES holds (`lookTier`: a
   culture alloy draws T4 or more, D86), so the T3 everyday is signed as every T1-T3 piece is (`sigLookOf`: the people's T3
   row, its head, body axes and colours); were an alloy drawn at T3 (FIX_LINES off) the first is a kit piece there and every
   other alloy an own piece. The everyday kit's **T4 pieces** are the **first alloy**: the ladder kit's T4 and, new in P5b4,
   the census's **common soldiers at T4** (`census liftRank`: a watchman of the capital lifted to T4 in band-4+ land; it calls
   `rankTiers(..., everyday = true)`, which under FIX_SIG dresses him in the realm's first alloy, a kit alloy, so he wears
   the signature; before P5b4 he wore the officers' alloy as an own piece and read as no people). The **officers** at T4
   (captains, jarls, the royal guard: D29) keep the realm's best alloy below the masterwork, an own piece, and the king the
   masterwork (kit). `npc=<people>:<role>:<rank>:<seed>:lift` draws the lifted figure (the looks macros, the sheets).
   **The census's T3 soldiers (P5b5, as built; measured by the judge's `ev_recipe.py` over `rpg_test --doll-recipe`, 12
   peoples x seeds 1-4 x man and woman).** The capital watch (T3 by rank) and the soldiers and village watch lifted to T3 in
   band-4+ land wear the coat (`Outfit::Guard`) at T3. Where the coat's helm or body is made by a people **signed at T3** (its
   T3 row's head or skirt set: 11 of the 12 peoples), it is the kingdom's everyday and draws the people's T3 as the kit does
   (`doll_resolve everydayT3`: `sigLookOf`'s head, body axes, shoulders and shield form; the people's T3 cape by the people's
   line rule); a signed coat with no helm (the Highland T2 soldier's bonnet) wears the people's T3 head at T3. The metal is
   the kingdom's livery, not the kit's steel ("tint" below: metalKind 10, the kingdom's armourTint on the alloy ramp; "brass"
   where the tint reads as brass):

   | People | T3 kit head (steel) | capital watch T3 | soldier lifted T3 | village watch lifted T3 |
   |---|---|---|---|---|
   | Fjordfolk | spangen | spangen, steel | spangen, tint | spangen, iron |
   | Highland | kettle | kettle, tint / brass | kettle (the bonnet gives way), tint / brass | kettle, tint / brass |
   | Heartland | nasal, knife comb | **kettle (D57)**, tint / brass | **kettle (D57)**, tint | **kettle (D57)**, tint / brass |
   | Imperial | crested | crested, brass | crested, brass | crested, brass |
   | Dune | veil | veil, brass | veil, brass | veil, brass |
   | Steppe | plumed | plumed, brass | plumed, brass | plumed, brass |
   | Marsh | kettle | kettle, brass | kettle, brass | kettle, brass |
   | Jade | kabuto | kabuto, tint | kabuto, brass | kabuto, tint |
   | River | morion | morion, tint / brass | morion, tint / brass | morion, tint / brass |
   | Sun Temple | masked | masked, brass | masked, brass | masked, brass |
   | Sylvan | winged | winged, tint / brass | winged, tint | winged, tint / brass |
   | Starspire | crown and hood | crown and hood, tint | crown and hood, tint / brass | crown and hood, tint |

   **D57's fate** (docs/PLAN.md D57; P2C_CONTRACTS "The watch at T3": the kettle, the cop, the cape and the round shield the
   owner saw on the Heartland watch): **retired for the 11 signed peoples** under FIX_SIG (their T3 watch and soldiers draw
   the people's T3 above); **kept for the Heartland** (its T3 row is the hero's road, unsigned: D54-D55) until the owner
   rules whether the Heartland's everyday T3 watch wears the kit's nasal with the knife comb instead. FIX_SIG off: D57 for
   every people, as at f914d03.
   **Every other alloy** (the warriors' at T4, a masterwork worked at T4, an Epic everyday at T5) keeps **its own helmet and cut** (D51; the owner, 2026-10-24/25:
   "every alloy its own helmet"), drawn from the people's STYLE_P2 28.5 menu and dressed in the people's pair (crest, layer,
   cape, cloak, shield field), and never a family the people's signature rules out below T5 (the Fjordfolk: no cone, the
   owner's "overly pointy"; no horns below T5, decision 2): it takes the people's next free family. A material piece
   (leather, bronze, iron, steel, the high metals) at T4-T6 keeps its hero's shape (2.6, D83). The P2c alloy gates (C16k,
   C16o, C16m, C18a-b, C19) hold for every pair **except two kit alloys of one people at one tier**, which are one look by
   design and are not parted by them (P5b4: two kit alloys of one people in **one kit band**, both everyday T3-T4 or both
   masterwork T5-T6; a pair across the T4/T5 boundary is never this exemption). **As built (P5b2 E, P5b4):** `gear::kitAlloy`
   (a kit alloy: the first alloy at T3-T4, the last at T5-T6, as `ladderKit`, the census's lifted common soldiers and the king's
   `rankTiers` choose them); a worn non-kit alloy body or helm carries forgeShape bits 23-24
   (`SHAPE_OWN_BODY`, `SHAPE_OWN_HELM`, set by `fillLadder` and `rankTiers` under FIX_SIG only), and `sigLookOf` then keeps its
   own shape axes and family and lays the people's dominant on its layer cloth and crest; from T4 a material piece (forge base
   not an alloy) keeps its hero's shape the same way. The freed helm (`gear::ownAlloyHelm`): ruled out are horns below T5, the
   Fjordfolk cone, and the people's own signed family at that tier (the kit's head: KISTEEL's kabuto, SANDSTEEL's veil); it takes
   a free family of the people's menu (not another of the culture's middle alloys', not a signed family of its own, another
   people's signed family only where nothing else is left), its variant by the alloy's name; a horns crest below T5 is dropped.
   An own body whose skirt is a kit's (the people's T4 or T5) or the kit of another people whose signed head is its helm family
   takes the people's next menu skirt (`gear::ownAlloySkirt`; C16o, C18a). One pin (`kOwnHeads`): GWESTEEL's morion is the
   River's T4 signature, so it wears the snout (C16k). The gates: two kit alloys of one people at one tier are skipped
   (`gear::kitTwinPair`: C16k, C16o, C16m, C18a; same people, same kit band); a kingdom's kit pair across the T4/T5 boundary
   (its everyday and its masterwork: its signature a tier apart, the one body growing; `kitLadderPair`, kit bands 1 and 2) is
   parted by the head alone (C16o, C16m, C18b: head 0.10; decision 9, ruled 2026-11-07: reading 1). **(P5b2 fix r1)** The freed helm (family and variant), a moved skirt and an own body's motif come from the
   own-look table (`rpg/sim/gear_forge_own.inc`, solved by `rpg_test --forge-own-solve` on the real figures against C18a-b on
   the board set and worlds 1-6, C16o and C16m on seeds 1..8, C16k on the owner's 37 and the judge's board check: no same-tier
   pair with an OWN side a head or greyscale twin), as the shape table is by `--forge-solve`; cells it never measured keep the
   rule above. The rule's per-culture picks put two kingdoms' freed helms on one look (FROSTSTEEL's kettle on BARSTEEL's).
   **Kings:** the king's helm never takes a ranked fighter's kit jitter (his masterwork at T5 is the kit alloy: the T5 head);
   declared king variants: the Heartland king wears his masterwork alloy's own head (the Heartland row signs each helm with
   its own, the hero's road's gilt onion is the material's, D83), the Sylvan and Starspire kings the T5 kit's family and crest
   (their own hair and the king's band-4 crown helm move 20-23 % of the head's top 14 rows). C16m reads the torso with the shield aside (the people's
   round shield covered BARSTEEL's torso: 67 px of the box). *Why not the plan's first text* ("alloys part by the family's variant and crest; the
   secondary serves the warriors' alloy"): measured in fix r2, a variant or crest moves a head 0.00-0.05 OD (C16k wants 0.10),
   section 5's one body per tier gives body OD 0 between a kingdom's alloys (C16o wants 0.08), the Fjordfolk secondary is the
   rejected cone, and the slice's own T5 kits of seeds 1-3 (one masterwork each of three kingdoms, pixel-identical by the
   signature, the slice gate's 567/567) are C18a twins of the board set by construction: the plan's text and the P2c gates
   cannot both hold. Reading 2 and the measures are in section 10, decision 8.
6. **Every metal keeps its helmet** (D83). Gilded steel keeps the onion, starmetal the crystal, obsidian the mask,
   emberforged the furnace helm: on those the people shows by its crest or plume colour, cloak, lining and motif.
7. **Surfaces meet with intent** (28.2, 29.3): the layer (sash, plaid, tabard) sits over the armour with a dark seam, the
   cloak hangs behind the shoulders, a crest or feather rises from its own crown, a fringe hangs from a drawn hem.
8. **Shape language per people** (section 5's first line each): round, rectangular, flared, vertical, ragged, curved,
   sharp. Keep each people inside its own.
9. **Rig 1, maker-less pieces, culture-0 classic pieces and the Heartland hero's road** (D54, D55, D83) are unchanged.

## 3. The palette (dominant, accent) and where it lands

`fp::Row::dye` becomes the people's **dominant** and `dye2` its **accent** (P5's thread pair, retuned to the concepts). The
starting values below come from a search that keeps P5's F1 rules (primaries >= dE 25 apart, each >= 20 from the leather,
bronze, iron, steel, gilded and brass steps 1-3, the accent >= 20 from its dominant), measured on approximate material
ramps; MARKS re-checks them with `python tools/doll/p5_marks.py --table` and may tune within each hue family.

| people | dominant | accent | luma d / a | P5 had | from |
|---|---|---|---|---|---|
| Fjordfolk | woad `#4878C8` | bleached wool `#E8E2CE` | 115 / 226 | woad, wool | levy cloaks, quartered shields |
| Highland | tartan green `#185030` | saffron `#D89A28` | 60 / 160 | green, red | the plaid over the saffron leine |
| Heartland | heraldic red `#C02030` | white `#EAE6D8` | 82 / 230 | red, white | surcoats (the kingdom's tinctures where it has arms) |
| Imperial | crimson `#7E1430` | gold `#E2B840` | 55 / 183 | purple, gold | legion red and brass |
| Dune | teal `#18849E` | cream `#ECE2C8` | 103 / 226 | indigo, saffron | teal robes over cream, gold at the top |
| Steppe | rust `#9C4426` | sky blue `#3CACD6` | 91 / 143 | sky, red | rust riding coats, sky-blue silk |
| Marsh | moss `#707E28` | bone `#D6CCA8` | 112 / 203 | moss, indigo | moss hide and reed, bone trim |
| Jade | vermilion `#E8641C` | black lacquer `#1C1A1E` | 131 / 27 | same | akaito-odoshi lacing, lacquer |
| River | linen white `#E8E4D6` | lapis `#2A4C9C` | 228 / 75 | teal, white | the guild's white-and-blue livery |
| Sun Temple | turquoise `#1CA086` | feather yellow `#F0C030` | 118 / 190 | magenta, teal | turquoise cotton, yellow feathers |
| Sylvan | leaf green `#A0C850` | forest `#2E5A2A` | 174 / 71 | leaf, amber | green cloth, leaf-cut edges |
| Starspire | indigo `#5A3CB4` | star white `#DCE4F8` | 83 / 228 | violet, white | indigo hoods and capes, stars |

Closest dominants in the search: Heartland-Steppe dE 26.3, Imperial-Steppe 28.5, Heartland-Imperial 30.4. Steppe's rust
sits at the material floor (20 from leather): MARKS reddens it first if the real ramps disagree. Every pair keeps 40 or
more luma between dominant and accent, so the two-tone reads in greyscale.

**Where it lands, by tier** (the doll and the icon alike; ICONS mirrors):

| piece | T1-T2 | T3-T4 | T5-T6 |
|---|---|---|---|
| a cloth or soft head (hood, turban, bonnet, cap, headband) | its cloth: dominant; band or edge: accent; the item's own material on the brim, rim or brow band | (metal heads) the lining under the brow (P5's D), the crest or plume: dominant | as T3; gilt trim (the ladder's) |
| the layer (sash, plaid, scarf, tabard, half cape) | dominant, accent edge (Highland: the sett, section 5) | same | same, gilt edge |
| the body piece | leather and metal planes untouched; collar and hem 2 rows, laces and cuffs: the pair (P5's band, widened to the hem) | the lacing in full where the people laces (Jade, Steppe), the cloth under the hem and in the brigandine's cover: dominant | as T3 |
| cape and cloak | (none: the ladder) | the people's cape and the officer's cloak: dominant, accent border and lining | the long cloak: dominant, accent lining |
| shield | T1 plain wood, its binding: dominant; T2 the field in the pair, in the people's division | the people's field and division, the kingdom's device on it (decision 3) | gilt rim, gem boss (the ladder's) |

Kings keep D96's deep crimson cape. Heraldic layers (the Heartland tabard, a kingdom's device) keep the kingdom's
tinctures where the kingdom has arms; the people's pair is their default.

## 4. The heads: one per people per tier

Source: **E** an existing drawn part or view (by its part file, recoloured), **O** a new `when` view over an existing part
(a people overlay or replace, keyed by the helm's maker), **N** a new part, **OL** a new T5-T6 legend overlay. Every new
view is ADDED; no drawn view's cells change. The head is drawn in the helm item's material (a T1-T2 soft head's cloth in
the people's colours, its band or rim in the item's leather or bronze).

| tier | Fjordfolk | Highland | Heartland | Imperial | Dune | Steppe |
|---|---|---|---|---|---|---|
| T1 | round leather cap, fur brow (spangen T1 + O) | flat bonnet, toorie (O on `cloth/cap`) | the chapel: brimmed leather kettle (O: kettle T1) | bronze bowl, knob, cheek guards (O: nasal T1 replace) | turban (E `cloth/turban`) | fur-brimmed felt hat (E `cloth/furhat`) |
| T2 | spangen cap (E spangen T2) | plaid hood, up (E `cloth/hood` + the plaid tile) | soft leather cap (E softcap, D55) | Montefortino with a short tuft (E crested T2 + crest T2) | turban over a spiked cap (O) | peaked felt cap, ear lappets (O) |
| T3 | spangenhelm, nasal, mail aventail (E spangen T3 + O) | kettle, fur band, feather (E kettle `furtrim`) | nasal, knife comb (E, hero T3) | crested, transverse crimson crest (E crested T3 + `crested_crest` people-imperial) | kulah: needle spike, aventail, turban band (E veil T3 + O) | spike and horsetail (E plumed) |
| T4 | spangenhelm T4, aventail (E spangen T4 + O) | kettle T4, fur band, feather (E furtrim; O if the T4 growth needs it) | open crested, fore-aft comb (E, hero T4) | crested, taller transverse crest, cheek guards (E crested T4 + crest T4) | kulah T4, turban band (E veil T4 + O) | plumed T4 (E) |
| T5 | horned helm, gilt fittings (E horned_legend) | kettle legend, two eagle feathers (E + OL) | gilt onion (E, D83) | Attic crested legend, tall fore-aft crest (E crested_legend) | gilt kulah legend, turban, falcon plume (E veil_legend + OL) | plumed legend, fur lappets (E) |
| T6 | horned legend T6 (E) | three eagle feathers, the chief's (E kettle_legend + OL) | crystal (E, D83) | + laurel wreath (E crested_legend + crown laurel or O) | turban-wrapped onion, aventail (E onion_legend + OL) | plumed legend T6 (E) |
| secondary (alloys; fix r2: superseded by 2.5, each non-kit alloy its own helmet; the Fjordfolk never the cone) | cone, its crests | snout | great helm (census knights) | the crested family's crests | onion (T4, T6) | plumed's variants |

| tier | Marsh | Jade | River | Sun Temple | Sylvan | Starspire |
|---|---|---|---|---|---|---|
| T1 | fringed hide hood (O on `cloth/hood`) | jingasa, black lacquer, vermilion mon (E `cloth/conical`) | the chaperon: rolled roundel, hanging tail (N) | headband, one upright feather (O on `cloth/circlet`) | leaf circlet (E `cloth/circlet`) | deep hood with a point (O on `cloth/hood`) |
| T2 | rawhide cap, bone crest, two feathers (O on leathercap) | the great kasa (E warhat, its kasa variant) | the chapel (E kettle) | quilted cap, three-feather fan (O on `cloth/cap`) | leaf crown, taller points (O on circlet) | stiff tall pointed cowl, high collar (O on hood) |
| T3 | reed war hat, drooping brim, tassels (E kettle `tassels`) | kabuto: bowl and shikoro (E kabuto) | sallet with its tail (E sallet T3) | jaguar-mask helm, ears and jaws (E masked + O) | leaf-winged helm (E winged T3) | star-pointed helm (E crown) |
| T4 | wide reed war hat (O/N: warhat, Marsh reed variant) | kabuto, flare, small maedate (E kabuto T4) | morion (E morion T4) | jaguar mask, feather crest (E masked T4 + O) | winged T4 (E) | crown T4 (E) |
| T5 | reed hat legend, heron plume (E warhat_legend + OL) | kabuto legend, gilt kuwagata (E kabuto_legend crest horns) | morion legend, plume (E) | mask legend, feather fan (E masked_legend + OL) | winged legend, gilt veins (E) | crown legend, star spires (E) |
| T6 | + bone brow and antler tines (OL) | + crest crown (E) | morion legend T6 (E) | the full fan (E masked_legend + OL) | winged legend T6 (E) | crown legend T6, glowing slit (E) |
| secondary (alloys; fix r2: superseded by 2.5) | warhat's variants | kabuto's variant and crests | sallet | plumed's feather-fan variant | winged's variants; antlers at T4 | crown's variants |

**Pairs that share a family or a class at one tier** (each must pass the head measure, 9.3 S1, at production): T1
Fjordfolk round cap / Imperial knob cap, Marsh fringed hood / Starspire pointed hood, Sun Temple feather band / Sylvan
leaf circlet; T2 Highland plaid hood / Starspire stiff cowl, Heartland soft cap / Marsh rawhide cap / Sun Temple quilted
cap; T3 Highland kettle (fur, feather) / Marsh kettle (tassels); T4 Heartland crested (fore-aft) / Imperial crested
(transverse); T6 Heartland crystal / Starspire spires. The rest differ by family.

**How it is chosen** (DOLL implements; data in `rpg/core/fingerprint.h`): the helm slot's maker people and its tier pick
the head; the helm's parts see that maker's flags (per-slot people flags, so a Jade helm on a Fjordfolk shirt is Jade);
`E` heads resolve to the existing part and tier view; T1-T2 cloth heads draw through the guise path that already draws
headwear shapes as helms (`forge::guiseHeadwear`). The kit's T1 helm (non-Heartland peoples: a band-1 leather helm,
Imperial's bronze) is new, made last from its own Rng stream, so no other kit draw moves.

## 5. The twelve signatures

Each: the read at 32 px; shoulders (mass and shape, from T2: T1 has none); hem; body cut (arm, layer); colour; motif;
shield; the ladder; where it comes from. Body forms stay the generator's (T1-T2 soft, T3 `body[0]`, T4+ the alloy's
finer cut); the axes come from the people's STYLE_P2 28.5 menu, now applied from T1 (inside the envelope) instead of the
materials' hero shapes, which keep the Heartland hero's road.

**Fjordfolk** (round, heavy-shouldered). A round iron dome with a nasal over a mail byrnie, fur at the neck, the biggest
round shield there is; woad and wool on undyed brown and iron.
- Shoulders: MANTLE (fur), growing to the great fur at T5. Hem: HAUBERK (split mail skirt; at T1 the jerkin's split hem).
- Body: BARE forearms (bronze arm rings from T3); layer none at T1-T2, the fur-edged woad CAPE at T3, the long cloak at T5.
- Colour: woad on the cloak, hem and shield quarters; wool on edges and the other quarters; madder red only as one stroke
  on the shield. Motif: the knot (bronze interlace at T5-T6).
- Shield: ROUND, iron boss; T1 plain boards with a woad binding, T2 quartered woad and wool, T3+ riveted rim and the
  kingdom's device, T5 gilt boss.
- Ladder: a bondi in a leather cap and jerkin -> a hirdman in mail, spangenhelm and fur -> a jarl in gilt horns, a great
  fur mantle and a cloak to the heels.
- From: `fjordfolk_0920_spangen_helmet_from_5th_6th_c_replica.jpg`, `fjordfolk_0921_spangen_helmet_from_groningen_...jpg`,
  `fjordfolk_leidang_kriger_..._0730.jpg` and `..._01.jpg` (hauberk, nasal, round shield), `fjordfolk_2004_sutton_hoo_01.jpg`;
  FLUX `fjordfolk_full_{levy,veteran,elite}_*.png`, `fjordfolk_helmets_1.png`, `fjordfolk_shields.png`, `fjordfolk_pixel_2.png`
  (the horned pixel read; decision 2).

**Highland** (asymmetric, long quilted lines, a small round targe). A saffron-shirted kern under a dark plaid flung over
one shoulder; later a fur-banded kettle; a chief's eagle feathers.
- Shoulders: none to T3 (the plaid's drape over the left shoulder is the shoulder read), BROAD from T4. Hem: STRIPS★
  (kilted; the aketon's vertical quilting to the knee from T3).
- Body: BARE arms; layer SASH★ = the plaid over the left shoulder from T1, HALFCAPE (plaid) from T4.
- Colour: the plaid in tartan green with the sett's saffron and red threads (P5's plaid tile on the layer's cloth);
  saffron on the leine's sleeves and hem. Motif: the plaid (the gilt lattice at T5-T6).
- Shield: BUCKLER (the targe): round, small, studded, a boss; T2 green field, saffron studs; T3+ brass studs.
- Ladder: a kern in a bonnet and plaid -> a gallowglass in aketon, mail and a fur-banded kettle -> a chief in a gilt
  kettle with three eagle feathers and a plaid half cape.
- From: `highland_gallowglass_d_rer.jpg`, `highland_three_irish_kerns_albert_durer_1521.jpg`, `highland_charles_edward_stuart_s_targe.jpg`,
  `highland_davidson_tartan_modern_tiling.jpg`, `highland_42nd_black_watch_officer_by_edward_dayes_c_1791_93.jpg` (bonnet,
  plaid); FLUX `highland_full_*.png` (saffron and green), `highland_shields.png` (studded targes), `highland_pixel_2.png`.

**Heartland** (the hero's road; square, heraldic). Unchanged from T2 to T6 (D54, D55, D83: soft cap, nasal with knife
comb, open crested, gilt onion, crystal). P5b adds: a Heartland-made T1 helm draws the chapel (the brimmed kettle of the
levy); the people's pair on the TABARD★ from T2 where the kingdom has no arms; the great helm stays the census knights'.
- Shoulders: ROUND (cop) -> TALL at T5. Hem: TASSETS. Body: HEAVY or CLOSE; layer TABARD★. Motif: oak.
- Shield: KITE at T3, HEATER from T4, the kingdom's device.
- From: `heartland_chapel_de_fer_kettle_helmet_or_war_hat_snowshill_manor_nt_13...jpg`, `heartland_elbl_g_kapalin_helmet_xv_c.jpg`,
  `heartland_1350_topfhelm_des_hans_rieter_zu_kornburg_anagoria.jpg`, `heartland_bataille_de_courtrai_1302.jpg`; FLUX
  `heartland_full_levy_man.png` (the kettle hat), `heartland_shields.png`.

**Imperial** (rectangular, banded, the crest's arc). Crimson and gold: a bronze pot with a knob, then a crested helm with
a transverse crimson crest, banded shoulders, pteruges at the hips and a tall curved scutum; a gilt Attic helm at the top.
- Shoulders: BROAD (banded lames), great at T5. Hem: STRIPS★ (pteruges, crimson-edged), longer each tier.
- Body: CLOSE arms; layer SCARF (the focale, crimson) at T1-T2, HALFCAPE★ (the paludamentum, pinned on the right
  shoulder) at T3-T4, the long crimson cloak at T5.
- Colour: crimson on the tunic hem under the armour, the crest, the cape and the scutum's field; gold on bindings, the
  scutum's rim and device. Motif: laurel.
- Shield: TOWER (the scutum, curved); T1 plain boards, crimson binding; T2 crimson field; T3+ gold rim and device; T5 gilt.
- Ladder: a recruit in a bronze pot, leather jerkin and focale -> a legionary in scale or segments, crested, with the
  scutum -> a legate in a gilt Attic helm, muscled cuirass and crimson cloak.
- From: `imperial_bronze_legionary_helmet_1st_half_of_the_1st_century_ad_xante...jpg` (T1-T2), `imperial_legiii.jpg`,
  `imperial_legio_xxi_rapax_sechsel_uten_2011_lindenhof_2011_04_10_15_58.jpg` (crests, pteruges, scutum),
  `imperial_cricau_festival_2013_lorica_squamata_1.jpg`, `imperial_bronze_roman_cavalry_helmet_and_mask_found_in_luxembourg_70_...jpg`
  (the secondary); FLUX `imperial_full_*.png`, `imperial_helmets_1.png` and `_2.png` (Attic crests), `imperial_shields.png`,
  `imperial_pixel_2.png`.

**Dune** (vertical spike, flowing kaftan, a small round shield). Teal and cream: a turban, then a needle-spiked kulah
with mail aventail and a turban band, a long split kaftan, wide sleeves; gilt and a turban-wrapped onion at the top.
- Shoulders: ROUND (lobed). Hem: SPLIT★ (kaftan), long. Body: WIDE★ sleeves from T2 (T1 close: the envelope); layer
  SCARF★ (the headcloth's tail) and SASH from T1.
- Colour: teal on the kaftan, sash and turban band; cream on the turban and under-robe; gold from T5. Motif: arabesque.
- Shield: CRESCENT or ROUND (the dhal): small, four bosses; T2 teal and cream; T3+ brass bosses.
- Ladder: a levy in turban and robe -> a mamluk in mail and plates under a kulah, kaftan split for the saddle -> an amir
  in gilt, a turban-wrapped onion, teal and gold.
- From: `dune_18th_century_sikh_kulah_khud_helmet_inside_the_white_tower_a...jpg`, `dune_helmet_khula_khud_met_02_5_5_001mar2015.jpg`,
  `dune_13_mameluke_of_egypt.jpg`, `dune_armour_of_the_ottoman_empire_zirah_baktar_or_zirh_gomlek.jpg`; FLUX `dune_full_*.png`,
  `dune_helmets_1.png` and `_2.png` (spikes, turban wraps). Not `dune_shields.png` (heater-like): the game's Dune bears the dhal.
- As built (P5b production, area B; T1-T4, `tools/doll/p5b_sig_B.py`, `fp::kSigDune`): T1 the turban wound two rows lower
  (T1's 2 rows; the drawn turban stood 4), its two brow wraps teal, a brass brooch, the headcloth's TAIL to the shoulder; T2 the
  same with the spiked cap's steel crown over it (T2's 3 rows); T3-T4 the kulah with a teal turban band wound round the bowl
  (Plume, so B8's 48-colour budget never snaps it). Skirt none at T1 (the split stood 5 px outside T1's bare outline), split
  T2-T4; layer sash T1, scarf T2 and T4, half cape T3 (the T3 cape stood 4 px past the legs behind the split kaftan); lobed
  round shoulders and wide sleeves from T2; the small dhal with four brass bosses. T5-T6: these axes on the own heads.
- As built (P5b production 2, area B; the owner's ruling 3 of 2026-11-06, the elite T5-T6; `tools/doll/p5b_sig_B2.py` on the
  kulah's own part `veil`, `fp::SigTier::ownGrowth`, Sig::gilt): T5 the amir, the kulah grown: its needle spike and finial gilt
  over the steel bowl, a fat cream turban (Cloth 3: the dye slots snap a large cream to the skin, B8) wound round its base and
  crossed by the teal wrap (the plume), a gilt mirror plate with its gem at the front and the falcon plume (teal) rising from it,
  the mail aventail round the face to the shoulders; T6 the sultan, still the kulah's family (2.5, one family T3-T6): the bowl
  swells into the onion under a gilt crescent (the alem), the turban fuller and girt in gilt, the great jewel and its feather
  tuft, the aventail longer, its hem gilt (the T6 living finish lights the gilt). Body T5-T6: split kaftan, round lobed
  shoulders, the long teal cloak, wide sleeves. Not section 4's T6 "turban-wrapped onion" as a family change: the onion family
  draws the Heartland's T5 gilt onion (D83) and would break 2.5's one family.
- As built (P5b round 3, DUNE; judge P5b2 r4 items 1b and 2; `tools/doll/p5b_fix_dune3.py`, every view keyed on the T3 kit):
  the T3 kulah's turban TAIL (the 'adhaba) hangs from the band's knot down behind the aventail to the shoulders (plume, 2 px,
  lit fold and frayed end; layer hairBack, so a fist raised behind the head, near or far, stays before it: doll_paint draws an
  overlay on the layer its view names), the mamluk read T2's scarf tail grows into (lt2 -> lt3 facing R 542 -> 527 man, 526 ->
  508 woman); facing U the cloth wound up the dome reaches the spike's collar. The wide sleeves' bell ends at the kaftan's hem
  in the drawn guard (greatsword, staff, crossbow) instead of hanging over the thighs; at the bow's low hold and full draw the
  near sleeve rides the drawing forearm (FORGE-LIMBS had traced it down the bowstring) and both sleeves stop at the bow wrist;
  in the man's run the bell opens in the kaftan's split at the back of the hem.

**Steppe** (a tall point, horizontal lamellar rows, a fur roll at the neck). A rust riding coat with a cream fur collar,
a pointed helm with a horsetail, black-lacquered lamellar, sky-blue silk.
- Shoulders: DROP★ (lamellar flaps) with the fur collar (MANTLE). Hem: SPLIT (riding coat), long.
- Body: CLOSE to T3, WIDE from T4; layer SCARF (streaming sky blue) from T3.
- Colour: rust on the coat's hem and sleeves; sky blue on the scarf and sash; the lacquer is the lamellar's own; fur
  natural. Motif: running horses.
- Shield: ROUND (or BUCKLER): rust field, black studded rim.
- Ladder: a herder in a felt hat and coat -> a lancer in lamellar, pointed helm and fur -> a khan in gilt lamellar under
  horsetails, fur and blue silk.
- From: `steppe_closeup_of_mongolian_reenactors_at_khaan_quest_07.jpg`, `steppe_bronze_helmet_mongol_empire_19219793614.jpg`,
  `steppe_a_man_and_woman_in_traditional_mongolian_dress_look_on_as_se...jpg`, `steppe_pursuit_by_mongol_warriors_diez_albums.jpg`;
  FLUX `steppe_full_*.png`, `steppe_helmets_1.png` (lappets), `steppe_shields.png` (studded rims), `steppe_pixel_2.png`.
- As built (P5b production, area B; `fp::kSigSteppe`, laces: the coat's seams in full rust): T1 the furhat's felt crown two rows
  lower, its fur flaps grown to the jaw; T2 the crown to a peak under a rust knot (the peaked felt cap); T3-T4 the plumed
  helm's cheek and neck guards grown into lamellar flaps (the people's horizontal courses, in the helm's own metal). Skirt
  none T1-T2 (the herder's coat), strips T3 (lamellar), split T4; the fur mantle at T2, lamellar flaps (drop) from T3 (the
  scale shoulders stood 22 px at T3, its cap 20); the scarf from T3 in the dominant; the round shield, a black studded rim.
- As built (P5b production 2, area B; `p5b_sig_B2.py` on the pointed helm's own part `plumed`): T5 the khan, the pointed helm
  grown tall: a steel cone ribbed in gilt to a gilt cup and spire, the horsetail (the rust, the plume) bursting from the cup and
  falling all round the cone to the brow, so the head is one tall point; a fur roll at the brow and at the neck (STYLE 5: "a fur
  roll at the neck"), a gilt brow plate and its gem, lamellar cheek and neck flaps laced in sky silk; T6 the khan of khans: the
  taller spire, the great three-row fur crown and collar, three gilt ribs, five-plate flaps whose laces glow in the living finish.
  Body T5 drop flaps (flap.5), T6 the fur mantle (fur.6); the split coat, the rust cloak, wide sleeves. T3's shoulders fur.3
  (20 px: flap.3 measured 22 against T3's 20, scale.3's `shoulder-scale` view 22 in the figure).

**Marsh** (ragged, drooping, a long oval shield). Moss and bone: a fringed hide hood, a fringed skirt, bare arms, a long
oval wooden shield; tasselled reed war hats, a heron plume and bone at the top.
- Shoulders: DROP (hide or reed flaps). Hem: STRIPS, fringed, at every tier (the fringe is the Marsh).
- Body: BARE arms; layer HALFCAPE (reed cape) from T3.
- Colour: moss on the hood, skirt cloth and reed cape; bone on fringe tips, crest and gorget; bog-brown hide is the
  leather's own. Motif: reeds.
- Shield: OVAL (long, wooden, painted moss and bone stripes); LEAF where the culture rolls it.
- Ladder: a bog hunter in a fringed hood and grass skirt -> a reed warrior in a tasselled war hat and hide flaps -> a bog
  king under a heron plume, bone at his brow.
- From: `marsh_01973_tollundmanden.jpg` (the bog cap), `marsh_coconut_fiber_armor_from_the_republic_of_kiribati_micronesia.jpg`,
  `marsh_ainu_group_aborigines_of_japan_from_the_island_of_hokkaido_o...jpg`, `marsh_ancient_celt_with_carnyx_ancient_briton.jpg`;
  FLUX `marsh_full_*.png` (fringe, hide, long ovals), `marsh_helmets_1.png` (skulls and antlers: the T6 bone only),
  `marsh_shields.png`.
- As built (P5b production, area B; `fp::kSigMarsh`): T1 the BOG CAP (the Tollund cap of the From list: the hide kerchief,
  `cloth/headscarf`, in bone, fringed in moss over the brow and beside the face, its knot's tails a moss hank) in place of the
  fringed hood: the hood is a third of the figure and with the body's band measured 40.6 % people colour (S2's cap 30 %); T2 the
  rawhide kettle (form2 in the hide) under a bone crest and two moss feathers; T3 the tasselled reed hat (the kettle's tassels
  views, keyed to the Marsh helm); T4 the wide war hat drooping, a moss wrap and a bulrush tuft. Skirt none at T1 (the strips
  stood 12 px outside T1's bare outline), strips T2-T4; hide flaps from T2; the reed half cape T3-T4; the oval in moss with
  three bone pales.
- As built (P5b production 2, area B; `p5b_sig_B2.py` on the T3 reed hat's own part `kettle`, fam.kettle at T5-T6: the war hat's
  part draws a generic T5 bevor over the face): T5 the bog king, the reed hats grown: a steel skullcap on bog hide, out of it a
  crown of bulrushes (moss stalks, brown cattails, the outer two tipped in bone), the widest brim of any head drooping into ragged
  grass beside the face, bone knotted to it, and the reed war-dress's grass collar strung with bone teeth; T6 crowned in bone: a
  stag's antlers about the bulrushes, a bone brow (a jaw's teeth) under the brim, a skull at the collar. Body T5-T6 the hide
  flaps (flap.5-6), the fringed strips, bare arms, the reed half cape (T5) and cloak (T6).

**Jade** (flared, banded). Vermilion and black: a lacquered jingasa, then a kabuto with a flared neck guard, then gilt
kuwagata; wide sode, a bell of laced kusazuri, wide sleeves, lacing in vermilion bands.
- Shoulders: DROP★ (sode) from T2, great at T5 (26 px). Hem: BELL★ (kusazuri), short at T1.
- Body: WIDE★ sleeves from T2 (T1 close); layer SASH (the obi, vermilion) T1-T4, TABARD (jinbaori) from T5; the horo
  (balloon cape) as its T5 cloak.
- Colour: vermilion on every lacing row, the obi and the mon; black lacquer on the jingasa and the horo's ground; steel
  and the alloy's plates stay metal (the lacing is the colour). Motif: cloud scrolls.
- Shield: none (its glaive or bow); where the culture rolls the OVAL, black lacquer with a vermilion mon.
- Ladder: an ashigaru in a jingasa and a laced leather do -> a samurai in laced lamellar, sode and kabuto -> a daimyo in
  gilt kuwagata and menpo, vermilion lacing, the horo.
- From: `jade_3_jingasa.jpg`, `jade_ashigaru_jingasa.jpg`, `jade_akaito_odoshi_yoroi_replica.jpg` and `_kabuto.jpg`,
  `jade_helmet_hoshi_kabuto_met_dt306020.jpg`, `jade_antique_japanese_samurai_menpo.jpg`, `jade_antique_samurai_o_yoroi_1.jpg`;
  FLUX `jade_full_{levy,veteran,elite}_*.png`, `jade_helmets_1.png` and `_2.png` (kuwagata), `jade_pixel_1.png` (kuwagata and
  sode at 32 px). Not `jade_shields.png` (round oni shields): the game's Jade bears none or the oval.

**River** (decision 1: the game's guild river-city, `priors.cpp` river(): brick towns, coats and gowns, crossbows,
halberds, heaters, brigandine; crisp sallet and morion, puffed sleeves, the guild's stripes). White and lapis livery: a
chaperon, an iron chapel, a sallet with its tail, a morion with comb and plume; a brigandine in livery cloth with puffed,
slashed sleeves; a heater painted per pale.
- Shoulders: BROAD (slab). Hem: TASSETS. Body: HEAVY (puffed, slashed sleeves, lapis in the slashes); layer TABARD (the
  livery: white with a lapis stripe) from T2.
- Colour: linen white on the livery and puffs; lapis on stripes, slashes and bindings; brass rivets. Motif: waves.
- Shield: HEATER per pale white and lapis; at T5-T6 drawn tall as the guild's pavise.
- Ladder: a burgher militiaman in a chaperon with a crossbow -> a guild soldier in a sallet and livery brigandine -> a
  guild captain in an etched morion with a plume and slashed velvet.
- From: `progression_castrum_maris_medieval_festival_militia_1_2022.jpg`, `progression_krigshistorisk_festival_2019_freie_rostocker_f_hnlein_02.jpg`,
  `progression_renesansni_festival_koprivnica_streli_ari.jpg` (crossbowmen), `heartland_backplate_from_a_brigandine_met_29_150_95_003august2014.jpg`,
  `heartland_elbl_g_kapalin_helmet_xv_c.jpg`; FLUX `river_shields.png` (the tall painted shield) and `river_full_*.png` (the
  white, blue and gold only). The INDEX's River (`river_*.jpg`, FLUX `river_helmets_*.png`: nemes, khepresh) is the Nile
  alternative of decision 1.

**Sun Temple** (radiating fans, round chimalli, animal heads). Turquoise and feather yellow: a headband with one
feather, then a jaguar's head over the face, then a fan of feathers; a quilted cotton suit, bare arms, a feathered bell.
- Shoulders: TALL (feathers); MANTLE (the jaguar pelt, 2 px spots) as its T5 alternative. Hem: BELL★ (feathered).
- Body: BARE arms; layer CAPE★ (feathers) from T3.
- Colour: turquoise on the quilted suit and the shield's field; yellow on the fan and fringe; red-orange as the plume's
  second feather. Motif: rays.
- Shield: ROUND (the chimalli), turquoise and yellow, its lowest rim row painted as hanging feather tips.
- Ladder: a warrior in a quilted tunic and headband -> a jaguar warrior in the jaguar's head and a spotted suit -> a
  sun lord under the feather fan.
- From: `sun_temple_aztec_stone_figure_of_jaguar_warrior_9754751902.jpg`, `sun_temple_aztec_eagle_warrior_9781717682.jpg`,
  `sun_temple_guerreros_mixtecas_con_ichcahuipilli_codice_selden_12_13.jpg`, `sun_temple_chimalli_ahuizotl.jpg`; FLUX
  `sun_temple_full_*.png`, `sun_temple_helmets_1.png` and `_2.png` (fans), `sun_temple_shields.png` (fringed), `sun_temple_pixel_2.png`.

**Sylvan** (curved, swept back, slim: the lightest silhouette at every tier). Leaf green and forest: a leaf circlet,
then a leaf crown, then a helm whose wings are leaves swept back; slim leaf plates at shoulder and hip, a long split leaf
skirt, a long cloak, a tall leaf shield.
- Shoulders: DROP (leaf). Hem: SPLIT (long leaf skirt). Body: CLOSE; layer CAPE (the leaf cloak) from T3.
- Colour: leaf green on the cloak, tunic and shield; forest on the leaf-cut edges and lining; silver metal, gold veins at
  T5. Motif: leaf veins.
- Shield: LEAF (tall, narrow).
- Ladder: a scout in green with a leaf circlet -> a warden in leaf plates and a winged helm -> a lord of the wood in
  gilt-veined silver and a long cloak.
- From: `sylvan_corona_a_foglie_di_quercia_in_oro_da_vulci_350_ac_ca.jpg`, `sylvan_corona_a_foglie_d_alloro_in_oro_da_vulci_350_ac_ca.jpg`,
  `sylvan_alfons_mucha_1896_lorenzaccio.jpg` (the whiplash line), `sylvan_elf_archer_in_forest.jpg`; FLUX `sylvan_full_*.png`,
  `sylvan_helmets_1.png` (leaf crests), `sylvan_shields.png` (tall leaf shields), `sylvan_pixel_2.png`.

**Starspire** (sharp, vertical, symmetric). Indigo and star white: a deep pointed hood, then a stiff pointed cowl, then a
helm crowned with star points that grow into spires; fluted plate, tall pointed shoulders, a long indigo cape, a kite.
- Shoulders: TALL★ (crystal and shard points). Hem: SPLIT, then pointed TASSETS. Body: CLOSE, the high collar; layer
  CAPE (indigo) from T3.
- Colour: indigo on the cape, hood, cowl and shield; star white on stars and edges; the alloy's violet or cyan glow at
  T5-T6. Motif: stars.
- Shield: KITE, indigo field, a white star.
- Ladder: an acolyte in a deep hood -> a star knight in fluted plate under star points -> a celestial lord whose spires
  and visor slit glow.
- From: `starspire_german_fluted_maximillian_style_armor_about_1505_ce_1_870388...jpg`, `starspire_berlin_deutsches_historisches_museum_gothic_field_armour.jpg`,
  `starspire_arabic_astrolabe_1.jpg`, `starspire_armillary_sphere_9541169465.jpg`; FLUX `starspire_full_*.png` (the hood to
  the spires), `starspire_helmets_1.png`, `starspire_shields.png` (indigo kites with stars), `starspire_pixel_2.png`.

## 6. The budget (pool bytes, no compression)

**The budget is 5 MiB (5,242,880 B) since 2026-11-06** (the owner: "you can have 5 MB man"; D46 restated, still no
compression): `kPoolBudget` and pxc.mjs's `POOL_BUDGET`. P5b2 E measured 2,620,573 B (2.50 MiB) after its three short views.
The T3 atlas row takes half the step to T4 (RUN 72, DRAWN 288, DEATH 56 KB; doll_envelope.h).

**At f914d03:** `node tools/doll/pxc.mjs` at f914d03: 871 parts, 29,153 views, **2,601,938 B** of the **2,621,440 B** budget:
**19,502 B free**. (P5 spent 12,194 B of it; its plan had kept 19 KB for P6.)

**What things cost** (measured 2026-11-05: each part removed from a scratch copy of the maps and the pool recompiled):

| part | views | bytes | | part | views | bytes |
|---|---|---|---|---|---|---|
| `cloth/turban` | 5 | 1,078 | | `pauldron_layered4_{l,r}` (one tier pair) | 24 | 2,017 |
| `cloth/furhat` | 5 | 1,200 | | `pauldron_layered*` (T2-T6) | 132 | 11,708 |
| `helm/spangen` | 19 | 2,481 | | `skirt_bell*` (man and woman) | 98 | 7,583 |
| `helm/kabuto` | 19 | 3,403 | | `lamellar` + `_hang` (man) | 178 | 12,216 |
| `helm/kabuto_legend` | 49 | 4,787 | | `shield/round` | 317 | 20,438 |
| `helm/leathercap` | 20 | 3,870 | | `shield/tower` | 286 | 21,225 |
| `helm/kettle` | 35 | 7,079 | | `finish/mark` (P5) | 144 | 6,918 |
| `helm/winged` | 68 | 15,410 | | every `helm/*_legend` | 1,764 | 78,830 |

So a new soft head (5 views) is about 1,100 B, a `when` overlay on an existing head about 450-650 B, a T5-T6 legend
overlay pair about 1,000-1,500 B; a new shoulder family would be about 12 KB, a skirt about 7.5 KB, a body form about
12 KB a body, a shield form about 20 KB. **So P5b draws no new body, shoulder, skirt or shield form**: those all come
from the library by data; it spends bytes only on heads and region re-cuts.

| item | peoples | source | bytes (estimate) |
|---|---|---|---|
| the palette retune; the people's axes (arm, skirt, layer, motif, shoulder) from T1; the heads table; shields by form and paint | all | data, existing parts | 0 |
| region re-cuts for the colour landing (hem 2 rows, full lacing, the brigandine's cover, cloth under the hem; `D` cells on existing planes, man and woman) | all | regions only | 1,500-3,000 |
| T1 heads existing (turban, felt hat, jingasa, circlet) | Dune, Steppe, Jade, Sylvan | E | 0 |
| T1 overlays (fur brow, bonnet, chapel, knob cap, fringed hood, feather band, pointed hood) | 7 | O | 3,150-4,550 |
| T1 new part (the chaperon) | River | N | 1,100 |
| T2 overlays (spiked turban, felt cap, bone crest, feather cap, leaf crown, stiff cowl) | 6 | O | 2,700-3,900 |
| T3 overlays (aventail, turban band, jaguar) | 3 | O | 1,350-1,950 |
| T4 overlays (aventail, turban band, feather crest) and Marsh's reed hat | 4 | O, N | 2,150-3,050 |
| **core: T1-T4 for all twelve** | | | **11,950-17,550** |
| T5-T6 legend overlays (eagle feathers, turban and plume, heron and bone, feather fan; the laurel if the crown's is not drawn) | 4-5 | OL | 4,000-6,650 |
| **the full plan** | | | **15,950-24,200** |

**Plainly: the full plan does not fit with any margin.** At its middle (about 20 KB) it takes the whole 19,502 B left,
and at its high end it is about 4.7 KB over; either way P6 would start with nothing. The core (every people's head at
T1-T4, every colour, every body axis) fits, leaving 2-7.5 KB. Options for the owner (decision 5):
- **A (recommended): raise the budget by 64 KiB to 2,686,976 B** (2.5 %, no compression; D46 restated). The full plan
  fits and P6 starts with 60 KB or more.
- **B: keep 2,621,440 B and build the core only.** Highland, Dune, Marsh and Sun Temple wear their existing T5-T6 legend
  heads (still twelve different families at T5 and at T6, section 4), showing their people by colour, crest and cloak,
  without the eagle feathers, turban, heron and feather fan.
- **C: keep the budget and cut peoples** (for example the slice's three first): not recommended; the ruling is every
  culture.
The slice (section 9) fits under every option and calibrates the estimates: it reports its bytes per head, and if a
head costs more than 1.5 times its estimate the planner re-costs the rest before production.

## 7. Icons (16 px; no pool bytes)

The bag says what the doll says. `art_items.cpp` gains a helm icon per head class of section 4 (the hoods, turban, bonnet,
felt hat, jingasa, kasa, chaperon, feather band, circlet; the kabuto, sallet, morion, onion, crown, war hat; the people
crests on the existing forms), keyed by the piece's people and tier (`IconLook::people`, `tier`, P5); the body icon its
form with the colour landing (lacing, hem, layer); the shield icon its form and the people's field. Each icon reads in
greyscale and agrees with the worn piece (9.3 S5).

## 8. What P5 built: kept, and what changes

**Kept:** `HumanLook::makers` per slot and the census makers (CORE); `IconLook::people` and `tier`; the dye slots (`Dye`,
`Dye2`, `DyeT0..7`, `Dye2T0..7`) and the compositor's routing; the `D` region (the dye seam) and its re-cuts; the marks
(`finish/mark.pxm`), Highland's plaid filigree and the filigree per slot (all twelve motifs); `fp-landing`;
`rpg_test --fingerprint` (F1-F8), `fingerprint_gallery`, `fingerprint_audit.h`; `p5_marks.py`, `p5_pack.py`; the FIX_FP A/B.

**Changes:**
1. `fp::Row` gains the signature: the head per tier (part, view, variant, crest), the secondary family, the body axes per
   tier band (arm, skirt, layer, motif, shoulder mass and shape), the shield form, and the colour landing; `dye` and
   `dye2` become the dominant and accent of section 3.
2. **A new switch, `FIX_SIG = 8192`** (`FIX_ALL` 16383), active only with `FIX_FP`: `EMB_FIX_RULES=8191` draws P5's look,
   `4095` the pre-P5 look, from the same build. Every P5b change sits behind it, the kit's T1 helm included.
3. P5's "never move an outline" becomes: never change a drawn view; maker pieces may wear new ADDED views and the
   people's existing parts (the heads, the axes). Maker-less pieces, culture-0 pieces and rig 1 are untouched.
4. The dye's role widens from thread to cloth (section 3's table); P5's F4 shares are restated per area: a leather or
   metal piece keeps P5's caps (the material reads first), the people's cloth (heads, layer, cape, lining) is the
   people's own and counts only toward the 30 % figure cap.
5. Per-slot people flags for the head, body and shield parts (the figure-level `people-<name>` stays for D74's cowls).
6. P5's open F5 items (T3 face helms and T4 alloy bodies with no dye) close by identity: such a piece carries its people
   by head family, crest colour and lining; F5 accepts the people's head family or 2 px of its dominant.
7. The gates gain the silhouette line (9.3 S1) for all twelve peoples at every tier at production.

## 9. The style slice (wave 1; the owner signs it off before production)

### 9.1 Who and why

**Jade, Imperial, Fjordfolk** (the default kept). Between them they span the hardest contrasts: the head (a flare, a
crest's arc, a round dome), the body family (laced lamellar, scale or segments, mail), the shield (none or the oval, the
tall scutum, the big round boss), the palette (warm vermilion on black, crimson on gold, cool woad on wool), and the
hardest T1 pair of the plan (Imperial's knob cap against Fjordfolk's round cap, both bowls inside 2 rows).

### 9.2 What it contains

- **Kits:** each people at **T1, T3 and T5**, on the **man and the woman**, from the ladder kits (`kit=<people>:<tier>:<seed>`,
  sword, seeds 1-3; sheets from seed 1), with P5b on: Jade T1 jingasa, laced leather do, vermilion obi, the oval (if its
  culture rolls it); T3 kabuto, vermilion-laced lamellar, sode, bell; T5 kabuto legend with gilt kuwagata, great sode,
  the horo. Imperial T1 bronze knob cap, leather jerkin, focale, short pteruges, the plain scutum; T3 crested with the
  transverse crimson crest, banded shoulders, pteruges, half cape, the crimson scutum; T5 the Attic crested legend, great
  banded shoulders, long crimson cloak, gilt scutum. Fjordfolk T1 round leather cap with a fur brow, jerkin with woad
  laces, the plain round shield; T3 spangenhelm with nasal and aventail, mail, fur mantle, woad cape, the quartered round
  shield; T5 the horned legend, scale, great fur, long woad cloak, gilt boss.
- **Worn on the doll, facings D and R:** the composed idle (and the walk's passing frame) at 1x and 3x, in colour,
  greyscale and black silhouette fill (`fingerprint_gallery --slice`); and **in play at NEAR**, by a hidden Witness run
  (`tools/scripts/p5b_slice.txt` through `tools/gates/witness_pack.mjs`; never a visible window): the three peoples' T1,
  then T3, then T5 kits side by side, facing D and R.
- **Their icons:** every slice piece's icon (helm, body, shield, gloves, boots, cloak) at 1x and 4x, colour and greyscale.
- **Before | after:** the same sheets with `EMB_FIX_RULES=8191` (P5) beside P5b.
- Everything in `C:\Users\joshu\AppData\Local\Temp\claude\C--Users-joshu\1979ea42-238a-4969-bda8-d76ab6bf2de5\scratchpad\p5b-slice\`
  with `captions.txt` and `measure.txt`.

### 9.3 Done (each line checkable)

- **R3a and the hero's road (fix r2 ruling, item 4).** R3a (the all-twelve greyscale nearest-neighbour read, idle, walk and no
  shield, D and R, man and woman, seeds 1-3) does not take the **Heartland's T4 figures as subjects**: the Heartland's T4 is the
  frozen hero's road (D54, D55, D83; its rows null from T2), whose T4 helm changes by seed (seed 2 draws another head than
  seeds 1 and 3), so its own-people term compares two different heads and its misses (5 of 1728 at 9a41ab8, all Heartland T4
  seed 2 facing R, nearest the Imperial or the Highland at 0.81-0.90) measure the frozen road, not a signature. They stay in
  every other figure's field (as "other"), and S1, J1 and J2 keep them. Under the ruling R3a reads 0 of 1692.
- **The slice's signed T3 reads (fix r2, restated, never redrawn).** The Jade and Imperial T3 sode and banded lames (`pauldron
  layered.3`, 22 px across against T3's 20) and the Jade kabuto at T3 (its signed T2-T4 views, no T3 view of its own) are the
  owner-signed slice: `--doll-coverage --scope envelope` does not list them where a Jade or Imperial body or helm wears its
  signature. The production peoples' over-wide shoulders (pelt.4, feather.3-4, flap.3, leaf.3, slab.2-3) are drawn narrower,
  not restated.
- **C17's visor and the jaguar (fix r2).** A signed head drawn in the mask family's place (the Sun Temple's jaguar, "ears and
  jaws", section 4) shows the face between the jaws: an open head, so C17's visor band (closed helms) does not apply to it.
- **S1 silhouette, greyscale.** For every pair of the three peoples, at T1, T3 and T5, on the man and the woman, facing D
  and facing R (idle frame 0, aligned on the pivot, seeds 1-3): the figures' opaque masks differ by at least
  `max(12, smaller area / 25)` px; **without the shield** (the same kit, no shield) by at least `max(8, smaller area / 40)`;
  and the head alone (the helm and headwear parts' pixels) by at least `max(6, smaller head / 5)` px in one facing and 3
  in the other. No pair is a twin or colour-only under P5's audit measure (`tools/preview/fingerprint_audit.h`).
- **S2 material reads.** Each worn leather body and cap: mean hue 15-45 and saturation at least its pre-P5 value minus
  0.10 (P5's F4); no people-colour pixel on a metal or leather plane outside the cloth, lining, crest, lacing, `R`, `N`,
  `D` and mark cells (count 0); the dominant and accent together 5-30 % of each figure (idle D); `icons_gallery --check`
  green with D117's lines unchanged.
- **S3 pool.** `node tools/doll/pxc.mjs --check` clean and within 2,621,440 B; the slice's spend over f914d03's
  2,601,938 B printed per head and in total, and at most 4,000 B.
- **S4 the switch.** `EMB_FIX_RULES=4095` draws the pre-P5 look: P5's A/B proof reproduces p4f1's records (art hash,
  view packs, freeze manifests) unchanged; `EMB_FIX_RULES=8191` reproduces f914d03's look on `tools/doll/fp_looks.txt` and
  the kits (every P5b move is `FIX_SIG`'s).
- **S5 worn equals icon.** Each slice icon's head class is its worn head's (section 4), each carries 3 px or more of its
  people's dominant hue family, and each reads apart from the other two peoples' icon of that piece and tier in greyscale.
- **S6 the ladder.** The envelope gates on the slice kits: heads inside T1 2 rows (the jingasa's brim inside 29.3's flare
  rule), T3 4 rows, T5 8 rows; areas T3 >= 1.20 and T5 >= 1.55 times T1;
  consecutive heads differ in outline; the doll gates as recorded but the declared moves.
- **S7 the rules.** Rig 1 identical; the sim digest identical (no new draw; the kit's T1 helm from its own stream); no
  save change; `git diff rpg/art/doll/maps` adds views and region rows only (a `--verify` like `p5_marks.py`'s); new views
  pass `pxc --strict`.
- **S8 eyes and the owner.** An `eyes` read of the 1x greyscale sheet names each people at each tier; the owner signs the
  slice before the other nine peoples start.

### 9.4 Who builds it (disjoint files)

- **SIG-RULES (engineer-high):** `rpg/core/fingerprint.h` (the signature rows, the palette), `rpg/art/doll/doll_envelope.h`
  (`FIX_SIG`), `rpg/art/doll/doll_resolve.cpp` (heads, per-slot people flags, axes from T1, colour landing),
  `rpg/art/doll/doll_ramps.cpp`, `rpg/sim/gear_forge.cpp` (the people's axes over the hero shapes, behind `FIX_SIG`),
  `rpg/sim/gear_ladder.cpp` (the kit's T1 helm, made last).
- **SIG-ART (artist-xhigh):** the slice's new views in `rpg/art/doll/maps/headwear/**` and the region re-cuts in
  `rpg/art/doll/maps/armour/**`, written by a new `tools/doll/p5b_sig.py` (ADD only, `--verify`), then `node tools/doll/pxc.mjs`.
- **SIG-ICONS (artist):** `rpg/art/art_items.cpp` (the slice's helm icons and colour landing).
- **SIG-GATES (engineer):** `tools/tests/test_fingerprint.cpp` (`--signature`: S1-S6), `tools/preview/fingerprint_gallery.cpp`
  (`--slice`), `tools/doll/sig_looks.txt`, `tools/scripts/p5b_slice.txt`, `tools/doll/p5_pack.py` (the slice sheets),
  `tools/gates/baseline/moves_p5b/*.txt` (every moved record declared with its reason).
- Wave order: SIG-RULES's table and switch first (a contract commit), then ART, ICONS and GATES in parallel; one judge.

## 10. Decisions for the owner (the plan proceeds on the first reading of each)

1. **River is the game's guild river-city** (its towns, coats, crossbows, heaters and brigandine), its armour drawn as the
   guild's: chaperon, chapel, sallet, morion, livery in white and lapis. (Other reading: the reference's Nile people,
   nemes and khepresh, which would also need River's dress, towns and music re-themed: a larger batch.)
2. **Fjordfolk keep horns at T5-T6 only** (the existing horned helm, the saga tier; T1-T4 hornless as in the concepts).
   (Other reading: hornless throughout, a Sutton Hoo face-mask legend instead, about 2 KB more.)
3. **The people's dominant owns the big cloth:** heads, layer, cape and cloak, linings, crests and the shield's field; the
   kingdom keeps its device, a heraldic tabard and the king's crimson (D96). (Other reading: P5's decision 1, the
   kingdom's colours on cloaks and shield fields, the people's beside them.)
4. **The T1 kits wear their people's cap**; the Heartland hero's road stays bareheaded at T1 (D54) and a Heartland-made T1
   helm draws the chapel.
5. **The budget:** option A, +64 KiB (recommended); else option B, the core only (section 6). **Ruled 2026-11-06: 5 MiB
   (5,242,880 B), no compression** (section 6).
6. **The palette retune** of section 3 (Imperial crimson, not purple; Dune teal and cream; Steppe rust and sky; River
   white and lapis; Sun Temple turquoise and yellow; Sylvan leaf and forest; Highland green and saffron; Marsh moss and bone).
7. **One signature family a people from T3 to T6, growing** (section 2.5); 29.4's "every tier its own family" stays the
   Heartland hero's road's rule.
8. **(fix r2) The kingdoms' kit alloys.** **Ruled 2026-11-06: reading 1, built in P5b2 E (section 2.5, "As built").**
   **Restated by the owner 2026-11-07, built in P5b4:** "no thats what i ment i think t3-t4 is a better representaion of every
   day armor": the everyday is the kingdom's T3 AND T4 gear (the T3 steel and the first alloy at T4, the census's common
   soldiers at T4 included), the masterwork stays T5-T6, every other alloy keeps its own helmet in the people's colours; the
   Heartland hero's road keeps its gilded onion (T5) and starmetal crystal (T6) relic helms (D83). Section 2.5 says what it is
   in code (`gear::kitBand`, `kitAlloy`, `rankTiers(..., everyday)`) and, per people, what the census's T3 soldiers draw
   (P5b5: the people's T3 head in the kingdom's livery metal; D57's watch kit retired for the 11 signed peoples, kept for the
   Heartland until the owner rules: **open question for the owner**). The text below is the 2026-11-06 ruling as asked. Reading 1 (the planner's recommendation; built on the owner's yes): every kingdom's kit alloy of a people (the
   everyday at T4, the masterwork at T5-T6) wears the people's one signature (the look the owner approved: "i really like
   these"); every other alloy keeps its own helmet and cut (2026-10-24/25) in the people's colours; two kit alloys of one
   people at one tier are not parted by the P2c alloy gates. Reading 2: each kingdom's kit carries a MARK on the signature
   (its own crest and one body axis of the people's menu; the seed-1 kingdom plain), solved with the alloy shape table
   (`rpg_test --forge-solve` given the signature's constraints: S1, R3a's own-people read, C18a no twins, C16o in a
   kingdom), so every alloy differs; the slice's T5 kits of seeds 2-3 then change (the owner's seed-1 sheets keep). Measured
   in fix r2 on prototypes (S1 misses / R3a / C18a twins / C16o / C16k): every kit alloy signed, others own: 0 / 7 (all the
   Heartland's frozen T4) / 145 (same-people kit pairs) / 30 / 3; only the seed-1 kingdom signed: 14 / 241 / 0 / 2 / 1; marks
   from the alloy's own axes, unsolved: 2 / 8 / 20 / 8 / 6; own heads on the signature body: 22 / 23 / 107 / 41 / 8. Reading
   2 needs the solver round (engineer-high); reading 1 needs the owner's yes.

9. **(fix r1) A kingdom's kit pair a tier apart.** **NOT YET RULED (the owner's answer is needed; corrected in P5b2 fix r3).**
   Fix r2 wrote "ruled 2026-11-07, reading 1" quoting "Ah yes that is a clash. Let's do your recommendation"; that message
   (2026-11-06 15:28 UTC) answered DECISION 8 (the 08:09 UTC question "you have two rules that now clash"), and decision 9
   was only written that evening, so it carries no owner answer yet. Until the owner answers this decision itself, the plan
   proceeds on its first reading (this section's rule) and the code keeps `kitLadderPair`; when the owner answers, this
   line quotes that answer with its date. The question: the owner's ruling (decision 8) exempts only two kit
   alloys of one people at ONE tier; the gates also part a kingdom's own everyday (T4) and masterwork (T5) by the head alone
   (`kitLadderPair`, tools/tests/test_doll_shapes.cpp: C16o, C16m, C18b), the people's signature a tier apart on its one body.
   Gated like any other pair they fail C16o 51, C16m 3 and C18b 40, 40, 58, 48, 41 and 42 in sil_w1-6 (the board set's 40
   counted in each world; the judge's measure, P5b2 r2, with `kitLadderPair` returning false; fix r1's "174" was wrong): the
   signature's T4 and T5 bodies sit 0.041 (Sun Temple) to 0.079 (Steppe) body OD apart where C16o and C18b ask 0.08, the
   slice's approved T5 kits among them. Reading 1 (recommended): keep the exemption (two kit alloys of one
   kingdom at T4 and T5 parted by the head, 0.10). Reading 2: remove it and redraw every people's T5 body 0.08 off its T4 (the
   approved slice's T5 bodies change; slice_same falls). Until ruled the code keeps the exemption, exactly as above:
   a kingdom's kit alloys at T4 and T5, FIX_SIG and FIX_FP on, parted by the head (0.10) alone.

### 10.1 Residuals that touch the approved slice (RULED 2026-11-07 by the main session as planner: R1 and R2)

Each line below failed a gate only on looks of the approved slice kits (Fjordfolk, Imperial and Jade at T1/T3/T5, frozen as
the owner signed them: slice_same 567/567). Fix r3 named them; the main session ruled them in P5b round 3:

- **R1. The approved slice frames may change ONLY to fix real defects** (1 px holes, a grip outside the fist), with the
  smallest pixel change, each declared as a move; slice_same is re-recorded for exactly the frames listed here and for no
  other, and the look must not visibly change. Built in P5b3 GRIPS (doll_paint.cpp):
  - W4.doll.arms_matrix C3a (196 frames at fc4afc6: armour/shared/pauldron_pelt5_r 109 on the fjordfolk:5 looks,
    armour/woman/skirt_strips 48 on the Imperial T5 woman, armour/man/skirt_bell 32 and armour/shared/pauldron_layered5_r 8
    on the Jade T5 man): closeGearPockets now runs on the slice kits at T5 too (T1 and T3 stay exempt: they have no
    pinhole). Each pinhole takes its darkest drawn neighbour. C3a 0.
  - W4.doll.arms_matrix C3d "the grip outside the fist" (64 frames: the Imperial T5 kit's axe strike v2 and dagger strike
    and follow v0, facing U). The cause was the near layered sode, not the crest. Facing U it is drawn after the arm, at
    its own layer (not lifted), and covered the grip. The dagger showed a 1 px sliver of fist. The axe showed its fist only
    through the weapon's own hand pass, so the weaponless frame had none. The rule: on a signed look facing U, when the
    same side's pauldron is drawn over the grip pixel itself, the gripping hand is drawn again after it, as after the
    lifted pad facing D. Imperial C3d 0.
  - The slice frames that move (re-recorded; every other slice PNG stays identical to a3f5a4c): 9 of 567, 3 px each, the
    pelt pinhole beside the scale coat. In each, the black hole pixel takes the line colour, and the two pixels the hole
    had lit as an edge take their unlit shade:
    slice_fjordfolk_T5_w_s1_D, _s1_D_ns, _s1_D_walk, _s2_D, _s2_D_ns, _s2_D_walk, _s3_D, _s3_D_ns, _s3_D_walk (.png).
    No Imperial or Jade slice frame moves (their pinholes and the facing-U grips are not in the slice's idle and walk D/R).
  - Outside the slice, these frames move: 69 of 39,544 freeze frames per rig (66 ACTION facing U, where the fist now shows
    on the grip: the strike, follow, block and cast of the signed kits; 2 DEATH fall facing U, g_jd2w; 1 LOCO hurt L,
    a_jade3's pinhole). The manifests are re-signed, and the moves are declared in
    tools/gates/baseline/moves_p5b3/GRIPS.txt. FIX_SIG off still equals f914d03 (0 changed on both rigs).
- **R2. The 8 approved-slice ladder_finish lines that are threshold-only are ACCEPTED as approved looks**, declared as
  W4.doll.ladder_finish_L moves with this ruling as the reason. Approved art is not repainted for a threshold.
  The lines (C12b, `rpg_test --doll-ladder --finish --scope ladder --seeds 1..3`), each with the failures it names:
  - fjordfolk sword man s1, s2 and s3:
    - "lt5 D slot 1 (T6) trim 9.9 % (band 10-36 %; 20 of 202 gear px)";
    - "lt3 R slot 0 (T3) trim 2.4 % (band 3-30 %; 3 of 125 gear px)";
    - "lt5 R: no glow pixel at T5 (gems, runes)".
  - fjordfolk sword woman s1, s2 and s3: "lt5 D slot 1 (T6) trim 9.9 % (band 10-36 %; 20 of 202 gear px)".
  - jade sword man s1 and jade sword woman s1: "lt5 R: no glow pixel at T5 (gems, runes)".
  Every other ladder_finish_L line (C12 and C14) must reach rc 0.
  Every other ladder line is fixed (P5b3 FINISH, `tools/doll/p5b3_finish.py`: River lt1 D colours, Sun Temple s3 lt1 -> lt2 R,
  Marsh lt3 C14, the T2 Fjordfolk/Heartland twin parted by the cone cap; P5b3 DUNE, `tools/doll/p5b_fix_dune3.py`: the Dune
  lt2 -> lt3 R shrink, by the kulah's turban tail. FINISH's own dune-t3-wrap for the same line was dropped at integration,
  so one fix stands). Moves: `tools/gates/baseline/moves_p5b3/{GRIPS,DUNE,FINISH}.txt`.

## 11. Don'ts

- Never trace, sample or import a FLUX concept or a reference photo; draw our own pixels.
- Never change a drawn view's cells, outline, mask, build row or stretch row; new looks are new views keyed by the maker.
- Never put the people's colour on a lit metal or leather plane; never past 30 % of a figure; never a checker or dither.
- Never let T1 grow past its envelope; T1 stays plain, slim and used.
- Never give two peoples the same head at one tier unless the head measure (S1) parts them.
- Never touch rig 1, maker-less or culture-0 pieces, or the Heartland hero's road beyond its T1 chapel.
- Never compress the pool or delete drawn art to make room; never add a draw to the census or the loot.
