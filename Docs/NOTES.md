**Optional, not required.**

Plain 3D FBM on `shellPos` is enough to start. **Domain warp** helps if coverage/type look too regular or grid-like once shape 3D noise is in the mix.

**Minimal iq-style (one warp, cheap):**

```c
shellPos = inner shell point
q = fbm3d(shellPos * warpScale + planetSeed)

coverage = saturate(bias + fbm3d(shellPos * covScale + q * warpStrength))
type     = saturate(bias + fbm3d(shellPos * typeScale + q * warpStrength + 100))
```

One warp vector `q` (or `fbm3d` → 3 offsets) shared by both — breaks symmetry without a second full weather system.

**When to add it:** after base density works; if blobs look too uniform. **When to skip:** first wiring pass.

**Don’t** stack multiple warp layers on coverage/type — save that budget for shape/detail 3D textures.

---

## are all base noises the same as the files i gave you?

They are the same **algorithms**, but not bit-for-bit identical outputs.

- **Perlin:** same cubic lattice, 12 fixed gradients, quintic fade and trilinear interpolation.
- **Simplex:** same skewed tetrahedral lattice, four corner contributions and `32.0` normalization.
- **Value:** same hashed lattice values, quintic fade and trilinear interpolation.

Differences:

- Your source uses a signed `int32` hash; ours uses `uint`, so right-shift behavior and resulting patterns differ.
- Your source accepts a `Seed`; ours currently hardcodes `1337u`.
- Your source maps 31 hash bits to `[-1,1]`; ours uses the lower 24 bits for reliable float conversion.
- Our interpolation is algebraically expanded in places instead of chained `lerp`, but the math is equivalent.

So the visual character should match, but values at a given coordinate will not match the CPU preview exactly. For exact CPU/GPU parity, we must port its signed hash, hash-to-float conversion, and seed parameters exactly.

---

## What is `OrbisCloudsCVars.h`

```c++
#pragma once

#include "HAL/IConsoleManager.h"

extern TAutoConsoleVariable<int32> CVarOrbisCloudsWeatherMapChannel;
```

It **declares** (doesn't define) one CVar so other `.cpp` files can use it without pulling in the view extension.

## Where it's used

| File                               | Role                                                                         |
| ---------------------------------- | ---------------------------------------------------------------------------- |
| **`OrbisCloudsViewExtension.cpp`** | **Defines** the CVar and passes it to the shader as `WeatherMapChannel`      |
| **`OrbisCloudsImGui.cpp`**         | **Reads/writes** it for the "Displayed Weather Map" combo (Coverage vs Type) |

The CVar itself:

```cpp
TAutoConsoleVariable<int32> CVarOrbisCloudsWeatherMapChannel(
	TEXT("r.OrbisClouds.WeatherMapChannel"),
	0,
	TEXT("0 = Cloud Coverage. 1 = Cloud Type."),
	ECVF_RenderThreadSafe);
```

`DebugSolid` and `DepthOcclusion` stay `static` in that file — only `WeatherMapChannel` was extracted because ImGui needs it too.

## Why it exists

Standard UE pattern:

- **One `.cpp`** owns the CVar definition (linker needs exactly one)
- **Header with `extern`** lets ImGui toggle `r.OrbisClouds.WeatherMapChannel` without duplicating the variable or including view-extension internals

So the ImGui panel and the render pass stay in sync when you switch between coverage (R) and type (G) in the weather map debug view.

---

## Adaptive Raymarch Steps and LOD smapling:

### Adaptive Steps:

- large cheap steps: 64 at zenith, 128 at horizon, samples iso volume from coverage and 128^3 volume textures.
- small steps: switch to smaller steps when large steps hit non-zero density, aka hits iso volume, sample from 32^3 volume textures.

### LOD:

- low LOD: 128^3 volume textures.
- high LOD: 32^3 volume textures:

---

**Vol 9–11 + the SD volume-noise article** is the right core. Vol 1–4 are mostly “how I got here”; you already have the shell, depth clip, and march skeleton in `CloudScapes.ush`.

## Is it “just textures,” or does Yvan’s code matter?

**Both.** Textures alone won’t look like his screenshots if you only multiply one 3D sample by coverage. His **density function** is the main reason shapes read as clouds.

Right now you have:

```205:228:D:\PROJECTs\RND\CLoudsSurrogate\Plugins\AtmosphericClouds\Shaders\Private\CloudScapes.ush
// Density at ray distance t: 0.8 at inner (bottom), 0.1 at outer (top). Zero outside the annulus.
float CloudScapesDensityAt(...)
{
	...
	return lerp(0.8, 0.1, HeightFromBottom) * saturate(CloudDensity);
}
```

That’s a vertical gradient — no lumps, no erosion, no lighting. Yvan’s look comes from stacking **several deliberate stages**.

### What textures give you (~40%)

- **Base 3D** (R = Perlin–Worley, G/B/A = Worley LODs) — billowy macro shape
- **Detail 3D** — edge erosion
- **Weather map** (SD, seamless) — where clouds exist, peak height, density
- Vol 10 adds **HeightLUT** + **CloudShapeNoise** (2D displace, not curl)

Bad or generic FBM will still look mushy even with his code.

### What code gives you (~60% for shape + almost all of “realistic lighting”)

**Vol 9 `GetDensity`** — the important pattern:

1. Weather → coverage field (`WMc`, SA, DA)
2. Sample base 3D → build shape (`SN`) with Worley FBM from G/B/A
3. Sample detail 3D → **erode only at edges** (`remap(SN, DNmod, 1, 0, 1)`)
4. Height gradient sculpts bottom-heavy, wispy tops

That “shape then erode” split is HZD’s idea; it’s not one `texture3D * coverage`.

**Vol 10** is his best **shape** refinement over Vol 9:

| Tier     | Source                                         | Effect                                 |
| -------- | ---------------------------------------------- | -------------------------------------- |
| **Low**  | HeightLUT + weather `smoothstep`               | Regional cloud mass, vertical envelope |
| **Mid**  | 3D noise vs height-dependent thresholds        | Lumps inside the envelope              |
| **High** | **Displaced** 3D sample (shape noise warps UV) | Breaks tiling, sharpens edges          |

The displaced high tier (`FBMUVDisplace` → second 3D read) is a real code trick — not something the texture file alone provides.

**Vol 11** — less about shape, more about **not looking like grey fog**:

- Energy-conserving `integScatt` (scatter + transmittance)
- Adaptive steps inside cloud
- Proper composite (rgb = scattered light, a = transmittance)

**Presentation stack** (also code, not textures):

- Half-res march + **4-frame offset merge** (Vol 10) — hides banding
- Step **jitter**
- Dual HG (“silver lining”)
- **Powder / depth probability** (Vol 9 `OutScatterAbient`) — brighter bases
- Dual Beer (`max(prim, scnd)`) — softer self-shadow
- Depth occlusion

## Does Yvan improve on HZD?

**Not really at the algorithm level.** He’s a solid **UE4 classroom implementation** of HZD-ish ideas, with a few swaps:

| Yvan                             | HZD / Nubis                          |
| -------------------------------- | ------------------------------------ |
| Weather G = high-freq coverage   | G = **cloud type**                   |
| HeightLUT + Low/Mid/High tiers   | Stratus/cumulus/cumulonimbus presets |
| 2D shape noise **displace**      | **Curl** 2D                          |
| 2 sun steps, 256–800 march steps | Cone samples, reprojection, LOD      |
| 4-frame half-res merge           | Full temporal reprojection           |

His clouds look good because he runs the **full stack** (density stages + lighting + half-res/temporal), with **tuned remap constants** and **hand-authored SD assets** — not because he invented a better noise.

## Practical priority for CloudScapes

1. **SD** → base + detail 3D + weather (and HeightLUT/shape 2D if you follow Vol 10)
2. **Vol 9 `GetDensity`** — biggest shape jump
3. **Vol 10** displacement + HeightLUT — extra realism on silhouettes and variety
4. **Vol 11** march/light/composite — grey debug → actual clouds
5. Half-res + jitter when performance allows

**Bottom line:** His realism is **correct multi-stage density + erosion + lighting + presentation**, with **good SD textures** feeding it. Textures alone ≈ lumpy fog; code alone with procedural noise ≈ mush. You need both; Vol 9–11 is where the code side lives.

If you want to implement next, Vol 9 `GetDensity` ported into `CloudScapes.ush` with texture slots on the component is the highest-value step.

**No. Yvan’s code is not an improvement over HZD.**

It’s a **simplified UE4 recreation** of ideas from the HZD / Nubis papers — closer to “how to get a similar look in a classroom project” than “better than Guerrilla.”

**HZD is the reference.** Yvan copies the broad pipeline (spherical shell, weather map, Perlin–Worley + Worley LODs, detail erosion, Beer + HG + powder-ish terms) but **cuts or replaces** a lot of what makes Nubis production-grade:

- No real **temporal reprojection** (he uses half-res + 4-frame merge instead)
- Fewer **sun samples** (often 2 vs HZD’s cone sampling)
- **Weather G** = high-freq coverage, not HZD’s **cloud-type** blend
- **Shape noise displace** instead of HZD’s **curl** distortion
- **HeightLUT / Low/Mid/High** instead of HZD’s stratus/cumulus/cumulonimbus presets
- Weaker **LOD / horizon / optimization** story (Nubis 2017 stuff largely absent)
- No HZD split of **high clouds as 2D scrollers** above the volumetric layer

**Why his clouds still look good:** solid **density staging** (shape → edge erosion), reasonable lighting stack, tuned remaps, and **good SD-authored textures** — not because his shaders beat HZD.

**For your project:** treat Yvan as a **practical UE porting guide** (especially Vol 9–11). Treat **HZD/Nubis papers** as the spec when you want correctness and the features Yvan left out.

---

YVAN code has a good SD authored textures, density sampling, tuned remaps, do you think we can do HZD oriented code then have an option swappable alternative as YVAN LEE method?

**Yes — that’s a sensible split**, and you don’t need two full renderers.

Treat **HZD as the default spec** (shell march, type/coverage, height presets, curl, lighting, reprojection when you add it). Treat **Yvan as an alternate density model** (and its asset set), not a second cloud system.

**What should be shared (one pipeline):**

- Spherical shell intersection + height fraction
- Ray march loop, jitter, depth clip
- Sun march + Beer / HG / transmittance composite
- View extension / pass structure

**What should be swappable (one fork):**

- **Density sampling** — HZD `GetDensity` vs Yvan Vol 9/10 `GetDensity`
- **Bound textures** — HZD: weather (R coverage, G type), base 3D, detail 3D, curl. Yvan: weather (dual coverage + peak/density), base 3D, detail 3D, HeightLUT, optional shape noise for displace
- **Tuning constants** — remap weights, erosion strength, global coverage/density

Base/detail 3D noises can often be **the same assets**; weather and the extra 2D maps differ.

**What’s awkward to hot-swap:**

- **Weather map semantics** — same PNG can’t mean “type” and “high-freq coverage” at once; use separate weather assets per mode or separate channels with clear authoring rules
- **Temporal** — HZD reprojection vs Yvan 4-frame half-res merge are different post-march paths; fine as quality presets later, not as a runtime “density mode” toggle

**Practical shape:**

- Enum / preset: `DensityModel = HZD | YvanLee`
- One `SampleCloudDensity(...)` that dispatches internally
- Component holds texture refs for both sets; only the active mode’s textures need to be set

**Verdict:** HZD-oriented core + Yvan as an optional density/authoring preset is a good design. You’re not cloning two engines — you’re offering **two ways to build density on the same march/light/composite stack**, which matches how the articles actually differ.

**YVAN targets the same look**, but it’s built differently from HZD, and he admits the default can look **too flat** until you tune it.

**HZD:** one explicit idea in the density stack — **lower density at the cloud base** so bottoms read thin and wispy, plus powder in lighting.

**Yvan (Vol 9):** soft bottoms come from **several knobs**, not one “wispy base” switch:

1. **Shape field `SA`** — `SRb` ramps up near the bottom of the layer (`remap(ph, 0, 0.07, …)`), `SRt` tightens the top → **big below, small above**.
2. **Density field `DA`** — `DRb` / `DRt` → **more mass low, less high** in the shell.
3. **Detail erosion** — `DNmod` blends inverted Worley with height (`lerp(DNfbm, 1-DNfbm, ph*1.8)`) → **breakup and wisp at edges/bases**.
4. **Lighting** — powder / depth + vertical probability (`OutScatterAbient`) → **brighter, softer bases** even when density isn’t zero.

He literally says if the **bottom looks too flat**, change parameters and it becomes “much more natural” (Vol 9).

**Vol 10** adds **HeightLUT** + Low/Mid/High tiers — another way to sculpt vertical envelope and edge breakup.

**Verdict:** Same **visual goal** as HZD soft bottoms; not the same formula. Yvan gets there with **vertical remaps + erosion + powder**, and it’s **tuning-dependent**. If you implement HZD mode, you’ll want the explicit bottom density falloff; in Yvan mode, you lean on `SA`/`DA`/detail inversion and powder instead.

---

## Plan: stepping LODs — coarse/fine raymarch with low-detail texture LOD

**Context.** The Clouds raymarch (`case 2u` in `Shaders/Private/OrbisClouds.usf`) currently marches every step at full resolution — same fixed `StepSize` for the whole ray, same full-detail `Texture3DSample` (implicit mip) on every single `ComputeDensityAt` call, whether or not there's actually any cloud there yet. The shape/lighting was just brought in line with a verified working reference (plain Beer's law light march, Density-weighted opacity accumulation) and now looks close to the HZD 2015 talk's reference screenshots. Next: make the raymarch itself match the talk's actual technique — a cheap "low resolution cloud" pass to quickly find where the ray enters cloud material, then a full-detail pass once inside — both for performance and to match the paper's described modeling process ("Sample 3dTexture1 ... to build low resolution cloud" → refine with erosion/coverage).

There's a second, related motive. Earlier, a coarse/fine adaptive-stepping mechanism (Marshmallow-style: march coarse until density is first hit, then switch to a finer step size) was implemented, then fully commented out (not deleted) after being blamed for a contour/terracing visual artifact. That artifact was later root-caused precisely: an _unjittered_ fixed step lattice (regular, identical phase every ray) beats against the noise texture's own spatial frequency, regardless of step count or coarse/fine switching — confirmed by adding a per-ray start-phase jitter (`InterleavedGradientNoise` on `SvPosition`), which fixed it, and separately confirmed by removing it again (terracing came back) later. So reviving the coarse/fine mechanism now is safe _if_ the jitter comes back with it — reviving one without the other risks reproducing either the flicker or the terracing.

**Outcome:** the primary optical-depth raymarch does a cheap, blurry-texture coarse march to find cloud entry, then switches to a full-detail fine march for real accumulation. Scope is the **primary ray only** — the just-fixed 6-sample light march stays untouched (always full detail), since touching it now would undo the just-verified match to the working reference.

### Approach

**1. `ComputeDensityAt` gets an explicit LOD parameter** (`Shaders/Private/OrbisClouds.usf`). Add a `float DetailLOD` parameter (6th param, after `HeightGradient`). Switch the base-shape texture fetch from implicit-derivative `Texture3DSample` to explicit `Texture3DSampleLevel(BaseShapeNoiseTexture, BaseShapeNoiseSampler, SamplePosition * BaseShapeFrequency, DetailLOD)`. Mips are confirmed to exist on this asset (standard UE mip chain, `MipGenSettings = TMGS_FromTextureGroup`, not disabled) — no texture-authoring changes needed. A single mip fetch returns all 4 channels (R=base shape, GBA=erosion octaves) at once, so a coarser mip cheaply blurs both the base shape _and_ erosion detail together — no separate "skip erosion" logic needed.

Update all 3 existing call sites: primary ray fine phase → `DetailLOD = 0.0`; primary ray new coarse phase → `DetailLOD = CoarseDetailLOD` (new constant, start at `3.0`, tune visually); light march → unchanged, `DetailLOD = 0.0`; debug/test view → `DetailLOD = 0.0`.

**2. Restore coarse/fine adaptive stepping in the primary loop** (`case 2u`). Uncomment the existing `bZeroDensityHit`/`ZeroDensityHitCount` state and both branches (first-hit coarse→fine switch, 10-miss fine→coarse revert) — these are still sitting in the file as comments, so this is a straightforward uncomment, not a rewrite. Wire `DetailLOD` through: `CoarseDetailLOD` while `bZeroDensityHit` is true, `0.0` once switched to fine.

**3. Re-add per-ray start jitter.** Re-add the `InterleavedGradientNoise` helper function and the `SvPosition : SV_Position` parameter to `MainPS`. Jitter the loop's starting `t` by `InterleavedGradientNoise(SvPosition.xy) * StepSize`, same as the version that fixed the terracing before. Flagging explicitly: jitter was removed by direct instruction earlier tonight, and this reverses that — specifically because coarse/fine stepping (a fixed-lattice hazard) is coming back with it.

### Files touched

`Shaders/Private/OrbisClouds.usf` only — `ComputeDensityAt` signature + call sites, `MainPS` signature (`SvPosition`), the `case 2u` loop (jitter, uncommented coarse/fine branches, `DetailLOD` wiring), and the small `InterleavedGradientNoise` helper restored near the top with the other math helpers. No C++ changes, no new UPROPERTY/CVar — `CoarseDetailLOD` starts as a hardcoded shader constant, same as `BaseShapeWorldSpan` did before being promoted to an ImGui param. Shader-only edit — hot-reloads, no rebuild needed.

### Verification

1. Reload the shader, confirm `Clouds` view mode still renders the same overall shape/lighting as the just-fixed YVAN-reference-matched version (no regression).
2. Confirm the terracing/contour artifact does not return — check both a wide/orbital view and a close-up ground-level view.
3. Confirm no new flicker under a fully static camera.
4. Visually compare against the HZD 2015 "Modeling" slide reference image for overall shape character.

### REALIZATION:

large scale planets create banding and blurry renders, i will attempt optimized progressive large steps, shouldn't take more performance, as steps will grow as raymarch through large distances.
steps that increase in length according to distance from camera, we'll have to figure out the distance thresholds, far orbit close orbit, close clouds shell, mid range clouds shell, and far range clouds shell.
all this is still cheap large steps, when we hit density, we go back one large step and start small steps for high detail texture sample, the exit when desity hits saturation or zero density for 10 small steps.

#### COLLIN

- Blue noise temporal dithering
- Use blue noise dithering, static, not temporal
- So `TAA`/`TSR` isn't usable, so you get a lot of flickering because no temporal accumulation

Collin's explaining exactly the same mechanism we ran into ourselves earlier tonight. Breaking it down:

**Blue noise** is a noise pattern engineered to avoid low-frequency clumping (unlike plain random/white noise) — when used to dither something, the error gets spread out evenly instead of forming visible clusters, so it reads as fine, even grain rather than blotches.

**Temporal dithering** means using a different blue-noise pattern each frame (e.g. offset by frame number), specifically so that over many frames, `TAA`/`TSR`'s temporal accumulation (blending each frame with an exponential history of previous frames) averages all those slightly-different dither patterns together into something smooth — the banding gets hidden by diffusing it across time, not within a single frame.

**Why it flickers for him:** `TAA`/`TSR` is what actually does that averaging. If you're writing a custom shader pass that outputs directly to `SceneColor` without going through the engine's normal `TAA`/`TSR` history buffer — which is exactly our own situation, confirmed earlier tonight — then every frame's differently-dithered result gets shown raw, with nothing blending consecutive frames together. There's no accumulation to smooth it out, so you just see the per-frame noise pattern changing, which reads as flicker instead of clean dithering.

**Collin's suggested fix (static blue noise):** use the same blue-noise pattern every frame instead of varying it. It can't flicker anymore since nothing changes frame to frame, but you lose the "resolves into a perfectly smooth gradient over time" benefit — you'd see a fixed, static grain pattern overlaid on the banding permanently, in every frame, rather than banding that disappears. It's a real fix for flicker, at the cost of trading banding for visible (but stable) noise texture.

This is exactly how custom effects handle it when they can't ride the engine's built-in `TAA`/`TSR`. You build a minimal version yourself:

1. **A history buffer** — a persistent render target that survives across frames, storing last frame's output.
2. **Reprojection** — since the camera moves, you can't just blend history and current pixel-for-pixel; you need to re-project last frame's buffer into this frame's screen space using the previous frame's view/projection matrices, so you're blending the same world position, not misaligned pixels (otherwise you get smearing/ghosting).
3. **Per-frame jitter** — offset the sample position slightly each frame (sub-pixel), same idea as engine `TAA`, so consecutive frames sample slightly different points and accumulation actually gains resolution instead of just repeating the same value.
4. **Exponential blend** — `Result = lerp(History, Current, ~0.05–0.1)`, so many frames' worth of samples contribute, which is what actually smooths banding/noise over time.

This is real and buildable, but it's a genuinely bigger feature than anything we've done tonight — it needs a new persistent texture resource (C++ side, allocated and carried across frames, not just a shader tweak) plus the reprojection math, not something that fits in a quick `.usf` edit.

Reprojection is the core mechanism inside both. Here's how they use it: every frame, `TAA`/`TSR` jitters the camera projection by a sub-pixel offset, renders the frame, then takes last frame's accumulated history buffer and reprojects it into the current frame's screen space using per-pixel motion vectors (so a point on a moving object, or the camera turning, still lines up with where that same surface point is now). Then it blends the reprojected history with the new frame, with some rejection/clamping logic for places where the reprojection breaks down (disocclusion, fast motion, newly revealed geometry). `TSR` adds more sophisticated upsampling on top but the reprojection step is the same fundamental idea.

So what you'd already planned to build is literally the same core technique the engine's own `TAA`/`TSR` uses internally — you just wouldn't get their rejection/clamping heuristics or motion-vector infrastructure for free, since our custom pass sits outside that pipeline.

#### GOALS:

find the high threshold: from which 54 to 96 steps look good from orbit
find the low threshold: that will have the lowest cheap steps (before switching to high detailed steps), this would be YVAN's stepping scale
define stepping sizes with a concept called Nyquist-style bound

---
