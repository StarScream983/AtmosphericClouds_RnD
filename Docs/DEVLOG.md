# DEVLOG

## 2026-08-22 — FBM for Coverage/Type, Octaves/Lacunarity/Gain exposed

Coverage and Type were single-octave noise. Replaced with FBM (`FbmSimplexNoise3D`/`FbmValueNoise3D`/`FbmPerlinNoise3D` in `Noise.ush`), dispatched via `SampleFbmNoise3D`.

New per-section params (mirrored for Coverage and Type):

- `CloudsCoverageOctaves` / `CloudsTypeOctaves` — int, 1-8, default 8
- `CloudsCoverageLacunarity` / `CloudsTypeLacunarity` — frequency multiplier per octave, 1-4, default 2
- `CloudsCoverageGain` / `CloudsTypeGain` — amplitude multiplier per octave, 0.1-0.9, default 0.5

Why Lacunarity/Gain: with gain=0.5, octave _N_ contributes only `0.5^N` of the total — going 6→8 octaves was barely visible (~1.6% vs ~0.4% contribution). Exposing gain lets higher octaves actually matter.

Plumbed through the full chain: `OrbisCloudsComponent.h` (UPROPERTY sliders) → `BuildPlanetRenderData()` → `FOrbisCloudsPlanetRenderData` → `FOrbisCloudsPS::FParameters` → `OrbisCloudsViewExtension.cpp` → `OrbisClouds.usf` globals → `Noise.ush::CalculateWeatherMap`. ImGui controls added per-section in `OrbisCloudsImGui.cpp`.

Bug hit: `FbmSimplexNoise3D` was called from `CalculateWeatherMap` before it was defined further down the file — HLSL/USF requires functions defined above their call site, no forward declarations. Fixed by moving the FBM block above `CalculateWeatherMap`.

## 2026-08-22 — Domain warp for Coverage, with toggle

Coverage's FBM shape still looked too regular/smooth. Added a domain warp: displace the sample position through another noise field before evaluating the final FBM, following IQ's `f(p + h(p))` (https://iquilezles.org/articles/warp/), adapted to 3D and scaled down from his full double-nested example (5 fbm calls) to a single level (3 fbm calls building a float3 offset + 1 final call).

`WarpPosition3D(Position, NoiseType, Seed, Lacunarity, Gain, WarpOctaves, WarpStrength)` in `Noise.ush` — offset vector built from 3 decorrelated fbm samples (one per axis, seed/position offset per axis), each at a fixed low octave count. Applied only to Coverage; Type untouched.

New toggle: `bCloudsCoverageUseWarp` (bool, off by default). `CalculateWeatherMap` branches between the plain-FBM path and the warped path.

Same plumbing chain as above, extended with the bool (converted to `uint32` at the `OrbisCloudsViewExtension.cpp` pass-parameter assignment, matching the existing `bDebugSolid`/`bDepthOcclusion` pattern).

## 2026-08-22 — Warp Strength/Octaves exposed

First warp pass used hardcoded `ORBIS_WARP_STRENGTH = 4.0` and `ORBIS_WARP_OFFSET_OCTAVES = 3`. Result was too wiggly/marbled — the warped position is already frequency-scaled (~1 unit ≈ one noise cell), so displacing by up to 4.0 units scrambled the sample across several noise cells instead of gently distorting the macro shape.

Both became live params:

- `CloudsCoverageWarpStrength` — float, 0-8, default lowered to 1.0
- `CloudsCoverageWarpOctaves` — int, 1-8, default 3 (unchanged)

Added a "Coverage Warp" section title above the warp checkbox in ImGui, with both sliders below it.

---

## 2026-08-24 — Adaptive raymarch step count (zenith/horizon)

HZD/Nubis scale raymarch step count by view angle: 64 steps at zenith, up to 96-128 at the horizon (more shell distance to cover at grazing angles). Their version assumes the camera is always near the ground looking up — angle from world "up" is a reliable proxy for shell chord length.

We don't have that assumption (camera can be anywhere: in the shell, below it, in space above it), so "up" has to be computed per-camera, not assumed as a world axis:

```
UpVector = normalize(CameraPosition - PlanetCenter)   // outward radial at the camera
d = abs(dot(RayDirection, UpVector))                  // 1 = straight up or down, 0 = horizon
StepCount = ceil(lerp(128, 64, d))
```

`abs()` is required: straight up (dot=+1) and straight down (dot=-1) are both short chords through the shell and should both get 64 steps — only near-perpendicular rays (grazing the shell, dot≈0) need the full 128.

Range not settled:

- `64-128` (2015 HZD)
- `54-96` (2017 Nubis).

---

## TEXTURE AUTHORING COMPONENT

---

## GHOST PLANET BEHIND CAMERA

a ghost planet appeared asymetric to canera position opposite to actual camera.

### The problem:

IqRaySphereIntersection returns two ray-parameter values (near hit, far hit) by solving a quadratic. Whenever a root came out negative (the intersection point is behind the camera), the code clamped it to 0 with max(t, 0.). That clamp was fine when only one root was negative (camera inside the sphere: near root behind you, far root ahead — clamping the near one to 0 is correct). But it silently did the wrong thing when both roots were negative — which happens whenever a ray points away from the sphere entirely, even though the camera is clearly outside it. In that case both roots got clamped to (0, 0), and IntersectAtmosphereShell's hit test (OuterRadiusHit.x >= 0 && OuterRadiusHit.y >= 0) read (0, 0) as a valid hit at distance zero — a false positive.

That's the "ghost disc": wherever your camera's screen rays happened to point away from the planet, the shader rendered something anyway instead of correctly reporting "no intersection," with a real black gap on either side where rays that were even further off just failed the test outright.

### The fix:

the product of the two roots always equals C = |camera-to-center|² - radius², which depends only on the camera's distance from the sphere center — never on ray direction. That means "camera outside vs. inside the sphere" is one fixed fact for the whole frame; what varies per-pixel is only whether a same-signed pair of roots is positive (ray toward the sphere, real hit) or negative (ray away from it, no hit at all). I added an explicit check: if the far root is negative, both roots are behind the camera, so return a genuine miss (-1, -1) instead of clamping to (0, 0).

This was a bug shared by the original formula too (it did the same unconditional clamp) — it just took the earlier fixes (depth-occlusion gate, precision rewrite) clearing away the other issues before this one became visible on its own.

---

## ENGINE CRASHES WHEN WE CHANGE FROM INT TO FLOAT FOR LOOP

The problem, plainly:

The loop moves forward by doing t += StepSize each time, and it stops when t reaches tRaymarchExit. That works fine most of the time. But near the edge of the visible planet, some rays barely graze the cloud shell — they pass through a very thin sliver of it. That thin sliver means SegmentLength (and therefore StepSize, which is SegmentLength / StepCount) becomes extremely small for those specific pixels.

Here's the trap: floating-point numbers lose precision as they get bigger — the further t is from zero, the less precisely tiny additions to it can be represented. If t is already a large number and StepSize is small enough, t + StepSize can come out exactly equal to t — the addition just gets swallowed, like adding a fraction of a cent to a billion dollars and the total not changing. When that happens, t never advances. t < tRaymarchExit stays true forever. The loop never ends. The GPU can't run one shader invocation forever, so Windows eventually kills the driver — that's the crash.

The old int-based loop never had this problem because its stop condition (StepIndex < StepCount) was a plain integer counter, completely unrelated to what t's float value was doing — integers don't have this "swallowed by a big number" issue. When we switched the loop to be driven by t directly, we lost that built-in safety net.

The solution: add that safety net back explicitly — a second, independent stop condition that counts iterations with a plain integer and bails out after some maximum, regardless of what t is doing. That guarantees the loop always terminates, even in the broken case.

Concretely, one new line: track an iteration counter, and add && Iteration < MaxIterations to the loop condition alongside t < tRaymarchExit

---

## HEIGHT SIGNAL

```c
Remap(h, 0, 0.07, 0, 1) * Remap(h, type*0.2, type, 1, 0)
```

## 2026-08-30 — Beer-HG-Powder lighting plan (proposed, not yet applied)

Sun direction/color are already available for free — `ResolvedView.AtmosphereLightDirection[0]`/`AtmosphereLightIlluminanceOnGroundPostTransmittance[0]` come from the `View` uniform buffer we already bind, same as Epic's own `VolumetricCloud.usf`. The HG phase function also already exists engine-side (`ParticipatingMediaCommon.ush:91`, `HenyeyGreensteinPhase(G, CosTheta)`), no need to write our own.

Here's the suggested addition, in three pieces:

**1. Density-toward-light sampler** (add to `OrbisClouds.usf`, after `ComputeDensityAt`):

```hlsl
// Cheap approximation of HZD's 6-sample light cone: march a few samples toward the sun and sum density.
// Reuses the current step's HeightGradient for every light sample instead of recomputing each one's actual
// radius — the light-ray offsets here are small relative to InnerRadius, so this is a reasonable simplification.
float SampleDensityTowardLight(float3 SamplePosition, float3 SunDirection, float InnerRadius, float BaseHeightGradient)
{
	const int LightSampleCount = 4;
	const float LightStepSize = InnerRadius * 0.01; // ~1% of inner radius per step, tune to taste

	float DensitySum = 0.0;
	float3 LightSamplePosition = SamplePosition;
	for (int i = 0; i < LightSampleCount; i++)
	{
		LightSamplePosition += SunDirection * LightStepSize;
		const float2 LightWeatherMap = CalculateWeatherMap(
			LightSamplePosition, OuterRadius, CloudCoverageNoiseScale, BaseNoiseType, NoiseSeed,
			NoiseOutputMin, NoiseOutputMax, CloudsCoverageOctaves, CloudsCoverageLacunarity, CloudsCoverageGain,
			bCloudsCoverageUseWarp != 0u, CloudsCoverageWarpStrength, CloudsCoverageWarpOctaves,
			CloudTypeNoiseScale, CloudTypeNoiseType, CloudTypeNoiseSeed,
			CloudsTypeOctaves, CloudsTypeLacunarity, CloudsTypeGain);
		DensitySum += ComputeDensityAt(LightSamplePosition, InnerRadius, LightWeatherMap, 0.0, BaseHeightGradient) * LightStepSize;
	}
	return DensitySum;
}
```

**2. Beer-HG-Powder combine** (small standalone function, next to it):

```hlsl
// Schneider's "Beer's-Powder" approximation (HZD 2015 talk, slides 61-64): Beer's law for primary
// extinction, Henyey-Greenstein for forward-scatter silver lining, powder term for the dark edges facing
// the light (1 - exp(-2d) — saturates faster than plain Beer's, punches through on shallow/thin edges).
float ComputeLightEnergy(float DensityTowardLight, float CosTheta, float PhaseG)
{
	const float BeersLaw = exp(-DensityTowardLight);
	const float Powder = 1.0 - exp(-DensityTowardLight * 2.0);
	const float Phase = HenyeyGreensteinPhase(PhaseG, -CosTheta);
	return BeersLaw * Powder * Phase;
}
```

**3. Integration into the `case 2u` raymarch loop** — replace the `OpticalDepth`-only accumulation with front-to-back radiance/transmittance, so `Color` becomes lit and sun-tinted instead of a flat grey alpha ramp:

```hlsl
// Was: float OpticalDepth = 0.0; float WeightedHeight = 0.0;
float3 Radiance = 0.0;
float Transmittance = 1.0;

const float3 SunDirection = ResolvedView.AtmosphereLightDirection[0].xyz;
const float3 SunIlluminance = ResolvedView.AtmosphereLightIlluminanceOnGroundPostTransmittance[0].rgb;
const float CosTheta = dot(SunDirection, RayDirection);
const float PhaseG = 0.2; // forward-scatter eccentricity, tune to taste

...inside the loop, replacing the StepContribution/OpticalDepth block...

if (Density > 0.0)
{
	...
	const float DensityTowardLight = SampleDensityTowardLight(SamplePosition, SunDirection, InnerRadius, HeightGradient);
	const float LightEnergy = ComputeLightEnergy(DensityTowardLight, CosTheta, PhaseG);

	const float StepTransmittance = exp(-Density * StepSize);
	Radiance += Transmittance * (1.0 - StepTransmittance) * LightEnergy * SunIlluminance;
	Transmittance *= StepTransmittance;

	if (Transmittance < 0.01)
	{
		break;
	}
}
```

And at the end, instead of `Color = 1.0 - exp(-OpticalDepth);`:

```hlsl
Color = ... // Color is currently a scalar broadcast to OutColor.rgb — this needs OutColor = float4(Radiance, 1.0 - Transmittance) directly for case 2u instead, since Radiance is now colored, not grayscale.
```

That last part means restructuring the end of `MainPS` slightly since `Color`/`OutColor = float4(Color,Color,Color,1)` is shared across all view modes — cases 0/1 stay scalar, case 2 needs its own `OutColor` write with the colored `Radiance` and `(1.0 - Transmittance)` alpha.

Not yet applied — `Noise.ush`'s Coverage/CloudType formula is currently mid-flux (uncommitted `clamp` vs `lerp` swap), sort that out first.

---

96 steps are not enough for planet scale, Scrappy told me he's using 512 steps, which gives us plenty of head room, i tried 512 but the performance goes down dramatically because density sampling computes the weather map many times, so i decided to start optimizing by caching the weather map with a compute shader:

### GOALS:

- in IMGUI, make options select for RT cube map: 512, 1024, 2048, 4096 (maybe 8k)
- write the compute shader that computes the coverage/type map
- store it in buffer and bind it to the CustomViewSceneComponent
