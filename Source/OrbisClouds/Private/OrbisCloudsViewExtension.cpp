#include "OrbisCloudsViewExtension.h"

#include "OrbisCloudsCVars.h"
#include "OrbisCloudsShader.h"
#include "OrbisCloudsSubsystem.h"
#include "FXRenderingUtils.h"
#include "Math/DoubleFloat.h"
#include "PostProcess/PostProcessInputs.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "SystemTextures.h"

static TAutoConsoleVariable<int32> CVarOrbisCloudsDebugSolid(
	TEXT("r.OrbisClouds.DebugSolid"),
	0,
	TEXT("1 = fullscreen magenta debug (verify pass runs). 0 = shell march."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarOrbisCloudsDepthOcclusion(
	TEXT("r.OrbisClouds.DepthOcclusion"),
	1,
	TEXT("1 = clip shell by scene depth (terrain occludes). 0 = shell ignores depth."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarOrbisCloudsViewMode(
	TEXT("r.OrbisClouds.ViewMode"),
	0,
	TEXT("0 = Cloud Coverage. 1 = Cloud Type. 2 = Clouds. 3 = DEBUG: near/far root used."),
	ECVF_RenderThreadSafe);

FOrbisCloudsViewExtension::FOrbisCloudsViewExtension(const FAutoRegister &AutoRegister, UWorld *InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
{
}

void FOrbisCloudsViewExtension::BeginRenderViewFamily(FSceneViewFamily &InViewFamily)
{
	if (InViewFamily.FrameNumber != CachedFrameNumber)
	{
		bHasCachedPlanet = false;
		CachedFrameNumber = InViewFamily.FrameNumber;
	}

	if (const FSceneInterface *Scene = InViewFamily.Scene)
	{
		if (UWorld *ViewWorld = Scene->GetWorld())
		{
			bHasCachedPlanet = UOrbisCloudsSubsystem::FindPlanetRenderData(ViewWorld, CachedPlanet);
		}
	}
}

bool FOrbisCloudsViewExtension::CoverageMapParamsMatch(const FOrbisCloudsPlanetRenderData& A, const FOrbisCloudsPlanetRenderData& B)
{
	// Only fields CloudCoverageMap.usf's bake actually reads — OuterRadius feeds PlanetDiameter there, so it
	// belongs here despite also being a general planet property.
	return A.CoverageMapResolution == B.CoverageMapResolution
		&& A.CloudOuterRadius == B.CloudOuterRadius
		&& A.CloudCoverageNoiseScale == B.CloudCoverageNoiseScale
		&& A.BaseNoiseType == B.BaseNoiseType
		&& A.NoiseSeed == B.NoiseSeed
		&& A.NoiseOutputMin == B.NoiseOutputMin
		&& A.NoiseOutputMax == B.NoiseOutputMax
		&& A.CloudsCoverageOctaves == B.CloudsCoverageOctaves
		&& A.CloudsCoverageLacunarity == B.CloudsCoverageLacunarity
		&& A.CloudsCoverageGain == B.CloudsCoverageGain
		&& A.bCloudsCoverageUseWarp == B.bCloudsCoverageUseWarp
		&& A.CloudsCoverageWarpStrength == B.CloudsCoverageWarpStrength
		&& A.CloudsCoverageWarpOctaves == B.CloudsCoverageWarpOctaves
		&& A.CloudTypeNoiseScale == B.CloudTypeNoiseScale
		&& A.CloudTypeNoiseSeed == B.CloudTypeNoiseSeed
		&& A.CloudTypeNoiseType == B.CloudTypeNoiseType
		&& A.CloudsTypeOctaves == B.CloudsTypeOctaves
		&& A.CloudsTypeLacunarity == B.CloudsTypeLacunarity
		&& A.CloudsTypeGain == B.CloudsTypeGain;
}

void FOrbisCloudsViewExtension::UpdateCoverageMap(FRDGBuilder& GraphBuilder, const FOrbisCloudsPlanetRenderData& PlanetForPass, ERHIFeatureLevel::Type FeatureLevel)
{
	const bool bNeedsBake = !bHasBakedCoverageMap || !CoverageMapRHI.IsValid() || !CoverageMapParamsMatch(PlanetForPass, LastBakedCoverageMapParams);
	if (!bNeedsBake)
	{
		return;
	}

	// Always (re)create fresh rather than trying to detect "just needs a rewrite vs. needs resizing" —
	// RHICreateTexture isn't expensive enough here to justify that extra complexity, and this only runs
	// when parameters actually changed, not every frame.
	const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::CreateCube(TEXT("OrbisClouds.CoverageMap"), PlanetForPass.CoverageMapResolution, PF_G16R16F)
											.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
	CoverageMapRHI = RHICreateTexture(Desc);

	FRDGTextureRef CoverageMapTexture = RegisterExternalTexture(GraphBuilder, CoverageMapRHI, TEXT("OrbisClouds.CoverageMap"));

	FCloudCoverageMapCS::FParameters* ComputeParams = GraphBuilder.AllocParameters<FCloudCoverageMapCS::FParameters>();
	ComputeParams->Resolution = PlanetForPass.CoverageMapResolution;
	ComputeParams->OuterRadius = PlanetForPass.CloudOuterRadius;
	ComputeParams->CloudCoverageNoiseScale = PlanetForPass.CloudCoverageNoiseScale;
	ComputeParams->BaseNoiseType = PlanetForPass.BaseNoiseType;
	ComputeParams->NoiseSeed = PlanetForPass.NoiseSeed;
	ComputeParams->NoiseOutputMin = PlanetForPass.NoiseOutputMin;
	ComputeParams->NoiseOutputMax = PlanetForPass.NoiseOutputMax;
	ComputeParams->CloudsCoverageOctaves = PlanetForPass.CloudsCoverageOctaves;
	ComputeParams->CloudsCoverageLacunarity = PlanetForPass.CloudsCoverageLacunarity;
	ComputeParams->CloudsCoverageGain = PlanetForPass.CloudsCoverageGain;
	ComputeParams->bCloudsCoverageUseWarp = PlanetForPass.bCloudsCoverageUseWarp ? 1u : 0u;
	ComputeParams->CloudsCoverageWarpStrength = PlanetForPass.CloudsCoverageWarpStrength;
	ComputeParams->CloudsCoverageWarpOctaves = PlanetForPass.CloudsCoverageWarpOctaves;
	ComputeParams->CloudTypeNoiseScale = PlanetForPass.CloudTypeNoiseScale;
	ComputeParams->CloudTypeNoiseSeed = PlanetForPass.CloudTypeNoiseSeed;
	ComputeParams->CloudTypeNoiseType = PlanetForPass.CloudTypeNoiseType;
	ComputeParams->CloudsTypeOctaves = PlanetForPass.CloudsTypeOctaves;
	ComputeParams->CloudsTypeLacunarity = PlanetForPass.CloudsTypeLacunarity;
	ComputeParams->CloudsTypeGain = PlanetForPass.CloudsTypeGain;
	ComputeParams->OutCoverageMap = GraphBuilder.CreateUAV(CoverageMapTexture);

	TShaderMapRef<FCloudCoverageMapCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
	const uint32 GroupCount = FMath::DivideAndRoundUp(PlanetForPass.CoverageMapResolution, FCloudCoverageMapCS::ThreadGroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("OrbisClouds.CoverageMapBake"),
		ComputeShader,
		ComputeParams,
		FIntVector(GroupCount, GroupCount, 6));

	LastBakedCoverageMapParams = PlanetForPass;
	bHasBakedCoverageMap = true;
}

bool FOrbisCloudsViewExtension::ResolvePlanetForPass(FOrbisCloudsPlanetRenderData& OutPlanet) const
{
	OutPlanet = CachedPlanet;
	bool bShouldDraw = bHasCachedPlanet;

	if (!bShouldDraw && CVarOrbisCloudsDebugSolid.GetValueOnRenderThread() != 0)
	{
		OutPlanet.PlanetCenter = FVector::ZeroVector;
		OutPlanet.CloudInnerRadius = 590000000.f;
		OutPlanet.CloudOuterRadius = 600000000.f;
		bShouldDraw = true;
	}

	return bShouldDraw;
}

void FOrbisCloudsViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FOrbisCloudsPlanetRenderData PlanetForPass;
	if (!ResolvePlanetForPass(PlanetForPass))
	{
		return;
	}

	UpdateCoverageMap(GraphBuilder, PlanetForPass, InViewFamily.GetFeatureLevel());
}

void FOrbisCloudsViewExtension::PrePostProcessPass_RenderThread(
	FRDGBuilder &GraphBuilder,
	const FSceneView &View,
	const FPostProcessingInputs &Inputs)
{
	const bool bDebugSolid = CVarOrbisCloudsDebugSolid.GetValueOnRenderThread() != 0;
	const bool bDepthOcclusion = CVarOrbisCloudsDepthOcclusion.GetValueOnRenderThread() != 0;

	FOrbisCloudsPlanetRenderData PlanetForPass;
	if (!ResolvePlanetForPass(PlanetForPass) || !View.Family)
	{
		return;
	}

	Inputs.Validate();
	if (!Inputs.SceneTextures)
	{
		return;
	}

	const FIntRect PrimaryViewRect = UE::FXRenderingUtils::GetRawViewRectUnsafe(View);
	FScreenPassTexture SceneColor((*Inputs.SceneTextures)->SceneColorTexture, PrimaryViewRect);
	if (!SceneColor.IsValid())
	{
		return;
	}

	FGlobalShaderMap *GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FOrbisCloudsPS> PixelShader(GlobalShaderMap);
	if (!PixelShader.IsValid())
	{
		return;
	}

	FScreenPassRenderTarget Output(SceneColor, ERenderTargetLoadAction::ELoad);
	const FScreenPassTextureViewport OutputViewport(Output);

	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
	const FVector PlanetCenterRelative = PlanetForPass.PlanetCenter - ViewOrigin;

	// Double-float (High/Low) split of the same value, built directly from the double-precision FVector so
	// no precision is lost before it reaches the shader. Feeds ComputePreciseOuterSurfaceDirection in
	// OrbisClouds.ush — see the comment there for why the plain PlanetCenterRelative above isn't enough for
	// that specific computation.
	const FDFVector3 PlanetCenterRelativeDF(PlanetCenterRelative);

	FOrbisCloudsPS::FParameters *PassParameters = GraphBuilder.AllocParameters<FOrbisCloudsPS::FParameters>();
	PassParameters->PlanetCenterRelative = FVector3f(PlanetCenterRelative);
	PassParameters->PlanetCenterRelativeHi = PlanetCenterRelativeDF.High;
	PassParameters->PlanetCenterRelativeLo = PlanetCenterRelativeDF.Low;
	PassParameters->InnerRadius = PlanetForPass.CloudInnerRadius;
	PassParameters->OuterRadius = PlanetForPass.CloudOuterRadius;
	PassParameters->CloudDensity = PlanetForPass.CloudDensity;
	PassParameters->CloudCoverageNoiseScale = PlanetForPass.CloudCoverageNoiseScale;
	PassParameters->NoiseSeed = PlanetForPass.NoiseSeed;
	PassParameters->BaseNoiseType = PlanetForPass.BaseNoiseType;
	PassParameters->NoiseOutputMin = PlanetForPass.NoiseOutputMin;
	PassParameters->NoiseOutputMax = PlanetForPass.NoiseOutputMax;
	PassParameters->CloudsCoverageOctaves = PlanetForPass.CloudsCoverageOctaves;
	PassParameters->CloudsCoverageLacunarity = PlanetForPass.CloudsCoverageLacunarity;
	PassParameters->CloudsCoverageGain = PlanetForPass.CloudsCoverageGain;
	PassParameters->bCloudsCoverageUseWarp = PlanetForPass.bCloudsCoverageUseWarp ? 1u : 0u;
	PassParameters->CloudsCoverageWarpStrength = PlanetForPass.CloudsCoverageWarpStrength;
	PassParameters->CloudsCoverageWarpOctaves = PlanetForPass.CloudsCoverageWarpOctaves;
	PassParameters->CloudTypeNoiseScale = PlanetForPass.CloudTypeNoiseScale;
	PassParameters->CloudTypeNoiseSeed = PlanetForPass.CloudTypeNoiseSeed;
	PassParameters->CloudTypeNoiseType = PlanetForPass.CloudTypeNoiseType;
	PassParameters->CloudsTypeOctaves = PlanetForPass.CloudsTypeOctaves;
	PassParameters->CloudsTypeLacunarity = PlanetForPass.CloudsTypeLacunarity;
	PassParameters->CloudsTypeGain = PlanetForPass.CloudsTypeGain;
	PassParameters->BaseShapeWorldSpan = PlanetForPass.BaseShapeWorldSpan;
	PassParameters->CloudsViewMode = static_cast<uint32>(FMath::Clamp(
		CVarOrbisCloudsViewMode.GetValueOnRenderThread(),
		0,
		5));
	PassParameters->bDebugSolid = bDebugSolid ? 1u : 0u;
	PassParameters->bDepthOcclusion = bDepthOcclusion ? 1u : 0u;
	PassParameters->SceneDepthTexture = (*Inputs.SceneTextures)->SceneDepthTexture;
	PassParameters->SceneDepthSampler = TStaticSamplerState<SF_Point>::GetRHI();

	// SHADER_PARAMETER_RDG_TEXTURE always needs something bound, so fall back to a 1x1 black volume when
	// the component doesn't have a texture assigned — bHasBaseShapeNoiseTexture/bHasDetailNoiseTexture tell
	// the shader whether what's bound is real or just the dummy.
	PassParameters->bHasBaseShapeNoiseTexture = PlanetForPass.BaseShapeNoiseTextureRHI.IsValid() ? 1u : 0u;
	PassParameters->BaseShapeNoiseTexture = PlanetForPass.BaseShapeNoiseTextureRHI.IsValid()
												? RegisterExternalTexture(GraphBuilder, PlanetForPass.BaseShapeNoiseTextureRHI, TEXT("OrbisClouds.BaseShapeNoise"))
												: GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
	PassParameters->BaseShapeNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	PassParameters->bHasDetailNoiseTexture = PlanetForPass.DetailNoiseTextureRHI.IsValid() ? 1u : 0u;
	PassParameters->DetailNoiseTexture = PlanetForPass.DetailNoiseTextureRHI.IsValid()
											 ? RegisterExternalTexture(GraphBuilder, PlanetForPass.DetailNoiseTextureRHI, TEXT("OrbisClouds.DetailNoise"))
											 : GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
	PassParameters->DetailNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	// CoverageMapRHI is a plain RHI resource that outlives this GraphBuilder, so it needs re-registering into
	// this frame's graph even on frames UpdateCoverageMap didn't rebake it (RDG resources don't persist across
	// FRDGBuilder instances the way the RHI resource itself does).
	PassParameters->CoverageMapTexture = RegisterExternalTexture(GraphBuilder, CoverageMapRHI, TEXT("OrbisClouds.CoverageMap"));
	PassParameters->CoverageMapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	FRHIBlendState *AlphaBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	FRHIDepthStencilState *DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("OrbisClouds"),
		View,
		OutputViewport,
		OutputViewport,
		VertexShader,
		PixelShader,
		AlphaBlendState,
		DepthStencilState,
		PassParameters);
}
