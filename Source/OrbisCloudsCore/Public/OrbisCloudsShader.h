#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

// Bakes Coverage/Type (the same signal CalculateWeatherMap computes live, per-raymarch-sample, in
// OrbisClouds.usf) into a 6-face cube texture once, instead of recomputing the FBM+warp every raymarch
// step — that live recompute (8 octaves Coverage + 8 Type + up to 6 warp, at every primary-ray step and
// every light sample) is what made close-up views unusably slow. Dispatched with Z = 6 (one cube face per
// group in the Z dimension) so a single dispatch covers all 6 faces.
class ORBISCLOUDSCORE_API FCloudCoverageMapCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCloudCoverageMapCS);
	SHADER_USE_PARAMETER_STRUCT(FCloudCoverageMapCS, FGlobalShader);

	static constexpr uint32 ThreadGroupSize = 8;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(float, OuterRadius)
		SHADER_PARAMETER(float, CloudCoverageNoiseScale)
		SHADER_PARAMETER(uint32, BaseNoiseType)
		SHADER_PARAMETER(uint32, NoiseSeed)
		SHADER_PARAMETER(float, NoiseOutputMin)
		SHADER_PARAMETER(float, NoiseOutputMax)
		SHADER_PARAMETER(int32, CloudsCoverageOctaves)
		SHADER_PARAMETER(float, CloudsCoverageLacunarity)
		SHADER_PARAMETER(float, CloudsCoverageGain)
		SHADER_PARAMETER(uint32, bCloudsCoverageUseWarp)
		SHADER_PARAMETER(float, CloudsCoverageWarpStrength)
		SHADER_PARAMETER(int32, CloudsCoverageWarpOctaves)
		SHADER_PARAMETER(float, CloudTypeNoiseScale)
		SHADER_PARAMETER(uint32, CloudTypeNoiseSeed)
		SHADER_PARAMETER(uint32, CloudTypeNoiseType)
		SHADER_PARAMETER(int32, CloudsTypeOctaves)
		SHADER_PARAMETER(float, CloudsTypeLacunarity)
		SHADER_PARAMETER(float, CloudsTypeGain)
		// RDG-tracked UAV, not SHADER_PARAMETER_UAV — this pass is dispatched through FRDGBuilder, and
		// SHADER_PARAMETER_UAV is for the older immediate-mode RHI binding, which doesn't participate in
		// RDG's dependency tracking (the pixel shader pass that later reads this texture wouldn't be
		// guaranteed to run after this write completes).
		// Was: SHADER_PARAMETER_RDG_UAV — doesn't exist in UE 5.8 (confirmed against ShaderParameterMacros.h);
		// SHADER_PARAMETER_RDG_TEXTURE_UAV is the real macro for an RDG-tracked texture UAV (the ShaderType
		// text is only ever stringified for the HLSL declaration, never compiled as real C++, which is why
		// "RWTexture2DArray<float2>" is fine here despite float2 not being a real C++ type in this codebase).
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float2>, OutCoverageMap)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
	}
};

class ORBISCLOUDSCORE_API FOrbisCloudsPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOrbisCloudsPS);
	SHADER_USE_PARAMETER_STRUCT(FOrbisCloudsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, PlanetCenterRelative)
		SHADER_PARAMETER(FVector3f, PlanetCenterRelativeHi)
		SHADER_PARAMETER(FVector3f, PlanetCenterRelativeLo)
		SHADER_PARAMETER(float, InnerRadius)
		SHADER_PARAMETER(float, OuterRadius)
		SHADER_PARAMETER(float, CloudDensity)
		SHADER_PARAMETER(float, CloudCoverageNoiseScale)
		SHADER_PARAMETER(uint32, NoiseSeed)
		SHADER_PARAMETER(uint32, BaseNoiseType)
		SHADER_PARAMETER(float, NoiseOutputMin)
		SHADER_PARAMETER(float, NoiseOutputMax)
		SHADER_PARAMETER(int32, CloudsCoverageOctaves)
		SHADER_PARAMETER(float, CloudsCoverageLacunarity)
		SHADER_PARAMETER(float, CloudsCoverageGain)
		SHADER_PARAMETER(uint32, bCloudsCoverageUseWarp)
		SHADER_PARAMETER(float, CloudsCoverageWarpStrength)
		SHADER_PARAMETER(int32, CloudsCoverageWarpOctaves)
		SHADER_PARAMETER(float, CloudTypeNoiseScale)
		SHADER_PARAMETER(uint32, CloudTypeNoiseSeed)
		SHADER_PARAMETER(uint32, CloudTypeNoiseType)
		SHADER_PARAMETER(int32, CloudsTypeOctaves)
		SHADER_PARAMETER(float, CloudsTypeLacunarity)
		SHADER_PARAMETER(float, CloudsTypeGain)
		SHADER_PARAMETER(float, BaseShapeWorldSpan)
		SHADER_PARAMETER(uint32, CloudsViewMode)
		SHADER_PARAMETER(uint32, bDebugSolid)
		SHADER_PARAMETER(uint32, bDepthOcclusion)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, BaseShapeNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BaseShapeNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, DetailNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DetailNoiseSampler)
		SHADER_PARAMETER(uint32, bHasBaseShapeNoiseTexture)
		SHADER_PARAMETER(uint32, bHasDetailNoiseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, CoverageMapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CoverageMapSampler)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
