#pragma once

#include "CoreMinimal.h"
#include "OrbisCloudsRenderTypes.h"
#include "Components/SceneComponent.h"
#include "Engine/VolumeTexture.h"
#include "OrbisCloudsComponent.generated.h"

UENUM(BlueprintType)
enum class EOrbisCloudsBaseNoise : uint8
{
	Perlin UMETA(DisplayName = "Perlin"),
	Simplex UMETA(DisplayName = "Simplex"),
	Value UMETA(DisplayName = "Value"),
};

// Per-face resolution for the baked Coverage/Type cube texture (CloudCoverageMap.usf). Values, not a free
// int, so the ImGui/editor selector can only ever request something the compute shader was actually sized
// to test against — see Docs/NOTES.md for the Nyquist reasoning behind picking a resolution.
UENUM(BlueprintType)
enum class ECoverageMapResolution : uint8
{
	Res512 UMETA(DisplayName = "512"),
	Res1024 UMETA(DisplayName = "1024"),
	Res2048 UMETA(DisplayName = "2048"),
	Res4096 UMETA(DisplayName = "4096"),
	Res8192 UMETA(DisplayName = "8192"),
};

UCLASS(ClassGroup = (OrbisClouds), meta = (BlueprintSpawnableComponent, DisplayName = "Orbis Clouds Component"))
class ORBISCLOUDS_API UOrbisCloudsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOrbisCloudsComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Planet", meta = (ClampMin = "1.0"))
	float AtmosphereRadius = 500000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Planet", meta = (ClampMin = "1.0"))
	float CloudInnerRadius = 480000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Planet", meta = (ClampMin = "1.0"))
	float CloudOuterRadius = 500000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Planet", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudDensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "0.1", ClampMax = "2048.0"))
	float CloudCoverageNoiseScale = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "0"))
	int32 NoiseSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage")
	EOrbisCloudsBaseNoise BaseNoise = EOrbisCloudsBaseNoise::Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float NoiseOutputMin = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float NoiseOutputMax = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "1", ClampMax = "8"))
	int32 CloudsCoverageOctaves = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float CloudsCoverageLacunarity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float CloudsCoverageGain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage")
	bool bCloudsCoverageUseWarp = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float CloudsCoverageWarpStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage", meta = (ClampMin = "1", ClampMax = "8"))
	int32 CloudsCoverageWarpOctaves = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type", meta = (ClampMin = "0.1", ClampMax = "4096.0"))
	float CloudTypeNoiseScale = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type", meta = (ClampMin = "0"))
	int32 CloudTypeNoiseSeed = 7331;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type")
	EOrbisCloudsBaseNoise CloudTypeNoise = EOrbisCloudsBaseNoise::Simplex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type", meta = (ClampMin = "1", ClampMax = "8"))
	int32 CloudsTypeOctaves = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float CloudsTypeLacunarity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Type", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float CloudsTypeGain = 0.5f;

	// Swappable authored noise volumes (from TextureAuthoringComponent) for testing against the procedural
	// noise. Either can be left unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Authored Textures")
	TObjectPtr<UVolumeTexture> BaseShapeNoiseTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Authored Textures")
	TObjectPtr<UVolumeTexture> DetailNoiseTexture = nullptr;

	// UU that one full tile of BaseShapeNoiseTexture covers — controls how large a single cloud puff reads as
	// in world space. Exposed for live testing; was a hardcoded shader constant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Authored Textures", meta = (ClampMin = "1.0"))
	float BaseShapeWorldSpan = 17000000.f;

	// Per-face resolution of the baked Coverage/Type cube texture (CloudCoverageMap.usf) — replaces the
	// live per-raymarch-sample CalculateWeatherMap call that made close-up views unusably slow. Exposed for
	// live A/B testing against the actual noise settings' Nyquist requirement (Docs/NOTES.md), not a fixed
	// "final" value yet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrbisClouds|Cloud Coverage")
	ECoverageMapResolution CoverageMapResolution = ECoverageMapResolution::Res2048;

	FOrbisCloudsPlanetRenderData BuildPlanetRenderData() const;
	void NotifyChanged();

	UFUNCTION(CallInEditor, Category = "OrbisClouds|Planet")
	void ApplySmallPlanetPreset();

	UFUNCTION(CallInEditor, Category = "OrbisClouds|Planet")
	void ApplyLargePlanetPreset();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
	void UpdateSubsystemRegistration();
};
