# Art study A3: take A's body, A2's rendering

The owner preferred A's mass ("the skinny legs look bad"). A3 starts from A's own grids (`orig_a/` is an untouched copy), and every outline pixel stays where A put it. Only the inside is repainted, and the missing animation is added on A's parts.

`python build.py` builds every output and prints the silhouette check.

## Silhouette check (A3 idle frames laid over A's)
- **Idles:** 0 px differ in all 8 idle frames. The hero's D/R/U/L idles cover 467/407/462/407 px and the woman's 418/371/402/370 px, all identical to A. The overlay is `work/silhouette_diff.png`.
- **Walks:** the only differences against A's own walks come from secondary motion.
  - Hero walking down: 0/1/2 px per frame (the cowlick lags).
  - Hero walking right: 8 px (the scarf tail flies).
  - Woman walking down: 2 px on the down frames (the braid tip swings).
  - Woman walking right: 3 px on the down frames (the braid tip swings).

## What changed inside A's outline
- **Faces:**
  - The eyes went from 2x2 (lash, then white + dark-brown iris) to 2x3: lash, white + iris, white + glint. The hero's iris is blue (`#3A78B0` / `#8FC6E8`) and the woman's green.
  - Brows are a hair shade instead of the outline colour.
  - A's 2 px mouth and shadowed chin row, which read as a beard, became a 1 px mouth and a clean skin chin. The woman got light lips and a blush on the cheeks.
  - The profile eyes gained a white.
- **Ramps:** A2's hue-shifted 5-step ramps (table below) replace A's. Every character code maps one to one.
- **Materials:**
  - The hero's tunic has a lit plane on the chest, a fold under the baldric, and clean vertical folds in the skirt where A's alternated 7/6 pixels.
  - The boots get a shine on the lit toe; steel and brass keep their own ramps.
  - A's dotted hem trims became a solid lit band, so there is no single-pixel noise.
- **Selective outlines:** a finishing pass recolours outline pixels that face the top-left light to the next lighter step of their material. It recolours existing pixels only, so the silhouette is untouched.
- **Read from behind:**
  - Both characters have a highlight ring across the crown.
  - The woman's hair is now shaded as a volume: lit locks on the left, a darker lower right, and strands that gather into the braid.
  - Her apron bow at the small of the back is cleaned into loops, a knot and two tails.
  - In the side views, the bow ties on A's outline read as strings in shadow with one lit loop, so they no longer look like a stick.
- **Left facings:** A had already drawn them separately and lit them from the top-left. They keep that, gain the same face and outline treatment, and their new walks use legs repainted for the left light rather than mirrored.
- **Orphans:** zero isolated pixels in all 64 frames (`python checks.py hero woman swing`).

## Animation (all on A's parts)
- **Walks:** 6 frames in each of the 4 facings, for both characters.
  - **Down and right:** A's own walks, plus the secondary motion: the cowlick stretches and squashes, the scarf tail flows, and the woman's braid tip swings.
  - **Up (new):** A's back views on A's leg frames. The hero's free arm swings, his hilt hand stays on the hilt, and the scabbard bobs. The woman's braid tip swings over her shoulder.
  - **Left (new):** for the hero, A's left-facing upper body, the scabbard in front of the legs (it lags on the down frames), and the far hand peeking past his hip on the back-swing. The woman's near arm is lifted off so it can swing; her shoes and hem are mirrored for position and reshaded so the light stays top-left.
- **Swing:** A's 44 x 44 canvas facing right, and a new 48 x 48 facing down.
  - The wind-up is a coiled back-swing: weight on the back foot, the hand behind the back shoulder, and the blade cocked up and back, clear of the jaw.
  - The arc is rebuilt from a 1 px wisp to 6 px at the blade, in three tones. The follow-through trails a tail.
  - The strike, follow-through and recover arms are A's.
  - Facing down, the hero sweeps horizontally and the arc passes in front of his feet.
- **Timing:** hero walk 70/50/70 ms (74 px/s on the 28 px cycle). Woman walk about 18 px/s. Swing 70/110/70/60 ms; `anim_swing_slow.gif` runs at quarter speed.

## Ramps (outline > shadow > base > light > highlight)

| | hero | woman |
|---|---|---|
| hair | `#3A1622 #742E2C #AA4A2C #D9733A #F5B060` | `#3E2418 #8C5A2A #C79346 #EBC26C #FFEBA8` |
| skin | `#5E2A36 #B8644F #E9A077 #F8C597 #FFE6C2` | `#5E2C36 #C4705A #EEAA86 #FCD2AA #FFEAD0` |
| cloth | tunic `#162540 #1F4A68 #28728C #3E9FA7 #8BD8C6` | dress `#2C1030 #5E1D48 #922F60 #C24F76 #EC8E9E` |
| more | scarf `#44122C #87232F #C83E38 #F47650`, trousers `#1C1732 #33304E #4F4B6B #777493` | chemise `#4B3C55 #ADA4C0 #DCD6E2 #FAF6EE`, apron `#4E3A30 #A99070 #D9C69E #F3E6C4` |
| leather | `#2C1621 #5B3129 #8D5536 #BF8249 #E7B672` | the same first four |
| metal | steel `#201C31 #525C7A #95A2BC #D5DEEA #FFFFFF`, brass `#704020 #CB9034 #F8DA80` | |
| arc FX | `#7FA8D8 #C9E6F4 #FFFDF4` | |

The ramps were tuned against the world capture: green roofs `#44655A`, olive grass `#8C8D50`, terracotta `#B76B49`, timber `#643C2C`, and the darkest darks `#453035`.

## Open issues
- In the hero's left walk, the scabbard crossing in front of his legs shares the boots' leather ramp. That is A's design, and it reads as a sword at 1x, but at 4x the back boot and the scabbard merge.
- The faces have eye whites, which STYLE_P2 section 4 bans; that rule needs to change if A3 is approved.
