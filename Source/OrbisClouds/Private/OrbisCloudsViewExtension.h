#pragma once

#include "OrbisCloudsRenderTypes.h"
#include "SceneViewExtension.h"

class FOrbisCloudsViewExtension : public FWorldSceneViewExtension
{
public:
	FOrbisCloudsViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	// Once per view family (i.e. once per frame, not once per view) — the coverage-map bake doesn't depend
	// on any particular view, only on the planet's noise parameters, so it belongs here rather than in
	// PrePostProcessPass_RenderThread below (which runs per-view and exists for inserting into that view's
	// post-process chain). Runs before PrePostProcessPass_RenderThread, so the baked texture is ready by the
	// time the raymarch pass binds it.
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PrePostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessingInputs& Inputs) override;

private:
	uint32 CachedFrameNumber = 0;
	bool bHasCachedPlanet = false;
	FOrbisCloudsPlanetRenderData CachedPlanet;

	// Cached bake of Coverage/Type (CloudCoverageMap.usf) — a persistent RHI resource we create ourselves
	// (not a Blueprint-assigned asset), regenerated only when CoverageMapParamsMatch says the relevant
	// parameters below actually changed since the last bake, not every frame.
	FTextureRHIRef CoverageMapRHI;
	bool bHasBakedCoverageMap = false;
	FOrbisCloudsPlanetRenderData LastBakedCoverageMapParams;

	// Ensures CoverageMapRHI exists and is up to date for this frame, (re)baking it via FCloudCoverageMapCS
	// through GraphBuilder if the relevant parameters changed or it doesn't exist yet. Called before the
	// main raymarch pass parameters are built, so that pass can register CoverageMapRHI as an input.
	void UpdateCoverageMap(FRDGBuilder& GraphBuilder, const FOrbisCloudsPlanetRenderData& PlanetForPass, ERHIFeatureLevel::Type FeatureLevel);

	// Only the fields that actually feed CloudCoverageMap.usf's bake — deliberately excludes things like
	// PlanetCenter/CloudDensity/BaseShapeWorldSpan that have no bearing on the Coverage/Type signal, so
	// changing those doesn't trigger an unnecessary rebake.
	static bool CoverageMapParamsMatch(const FOrbisCloudsPlanetRenderData& A, const FOrbisCloudsPlanetRenderData& B);

	// Shared by PreRenderViewFamily_RenderThread and PrePostProcessPass_RenderThread so both hooks resolve
	// the same planet data and debug-solid fallback the same way, without duplicating the logic.
	bool ResolvePlanetForPass(FOrbisCloudsPlanetRenderData& OutPlanet) const;
};
