#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

namespace OrbisCloudsPlanetPresets
{
	const FVector SmallCenter(1000000., 1000000., 350000.);
	constexpr float SmallAtmosphereRadius = 500000.f;
	constexpr float SmallCloudInnerRadius = 490000.f;
	constexpr float SmallCloudOuterRadius = 500000.f;

	const FVector LargeCenter(700000000., 700000000., 350000.);
	constexpr float LargeAtmosphereRadius = 600000000.f;
	constexpr float LargeCloudInnerRadius = 590000000.f;
	constexpr float LargeCloudOuterRadius = 600000000.f;
}

struct FOrbisCloudsPlanetRenderData
{
	FVector PlanetCenter = FVector::ZeroVector;
	float AtmosphereRadius = 0.f;
	float CloudInnerRadius = 0.f;
	float CloudOuterRadius = 0.f;
	float CloudDensity = 0.f;
	float CloudCoverageNoiseScale = 4.f;
	uint32 NoiseSeed = 1337u;
	uint32 BaseNoiseType = 1u;
	float NoiseOutputMin = -1.f;
	float NoiseOutputMax = 1.f;
	int32 CloudsCoverageOctaves = 8;
	float CloudsCoverageLacunarity = 2.f;
	float CloudsCoverageGain = 0.5f;
	bool bCloudsCoverageUseWarp = false;
	float CloudsCoverageWarpStrength = 1.f;
	int32 CloudsCoverageWarpOctaves = 3;
	float CloudTypeNoiseScale = 2.f;
	uint32 CloudTypeNoiseSeed = 7331u;
	uint32 CloudTypeNoiseType = 1u;
	int32 CloudsTypeOctaves = 8;
	float CloudsTypeLacunarity = 2.f;
	float CloudsTypeGain = 0.5f;
	float BaseShapeWorldSpan = 17000000.f;
	// Resolved pixel value, not the UENUM — render-thread side doesn't need to know about
	// ECoverageMapResolution at all, matching how the other plain values here work.
	uint32 CoverageMapResolution = 2048u;

	// RHI refs, not raw UTexture pointers — safe to read on the render thread. Extracted from
	// UOrbisCloudsComponent's BaseShapeNoiseTexture/DetailNoiseTexture on the game thread in
	// BuildPlanetRenderData. Null if no texture is assigned.
	FTextureRHIRef BaseShapeNoiseTextureRHI;
	FTextureRHIRef DetailNoiseTextureRHI;
};
