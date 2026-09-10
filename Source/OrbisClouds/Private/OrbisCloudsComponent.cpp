#include "OrbisCloudsComponent.h"

#include "OrbisCloudsSubsystem.h"

UOrbisCloudsComponent::UOrbisCloudsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ApplySmallPlanetPreset();
}

FOrbisCloudsPlanetRenderData UOrbisCloudsComponent::BuildPlanetRenderData() const
{
	FOrbisCloudsPlanetRenderData Data;
	Data.PlanetCenter = GetComponentLocation();
	Data.AtmosphereRadius = AtmosphereRadius;
	Data.CloudInnerRadius = CloudInnerRadius;
	Data.CloudOuterRadius = FMath::Max(CloudOuterRadius, CloudInnerRadius + 1.f);
	Data.CloudDensity = FMath::Clamp(CloudDensity, 0.f, 1.f);
	Data.CloudCoverageNoiseScale = FMath::Clamp(CloudCoverageNoiseScale, 0.1f, 2048.f);
	Data.NoiseSeed = static_cast<uint32>(FMath::Max(NoiseSeed, 0));
	Data.BaseNoiseType = static_cast<uint32>(BaseNoise);
	const float ClampedNoiseMin = FMath::Clamp(NoiseOutputMin, -1.f, 1.f);
	const float ClampedNoiseMax = FMath::Clamp(NoiseOutputMax, -1.f, 1.f);
	Data.NoiseOutputMin = FMath::Min(ClampedNoiseMin, ClampedNoiseMax);
	Data.NoiseOutputMax = FMath::Max(ClampedNoiseMin, ClampedNoiseMax);
	Data.CloudsCoverageOctaves = FMath::Clamp(CloudsCoverageOctaves, 1, 8);
	Data.CloudsCoverageLacunarity = FMath::Clamp(CloudsCoverageLacunarity, 1.f, 4.f);
	Data.CloudsCoverageGain = FMath::Clamp(CloudsCoverageGain, 0.1f, 0.9f);
	Data.bCloudsCoverageUseWarp = bCloudsCoverageUseWarp;
	Data.CloudsCoverageWarpStrength = FMath::Clamp(CloudsCoverageWarpStrength, 0.f, 8.f);
	Data.CloudsCoverageWarpOctaves = FMath::Clamp(CloudsCoverageWarpOctaves, 1, 8);
	Data.CloudTypeNoiseScale = FMath::Clamp(CloudTypeNoiseScale, 0.1f, 4096.f);
	Data.CloudTypeNoiseSeed = static_cast<uint32>(FMath::Max(CloudTypeNoiseSeed, 0));
	Data.CloudTypeNoiseType = static_cast<uint32>(CloudTypeNoise);
	Data.CloudsTypeOctaves = FMath::Clamp(CloudsTypeOctaves, 1, 8);
	Data.CloudsTypeLacunarity = FMath::Clamp(CloudsTypeLacunarity, 1.f, 4.f);
	Data.CloudsTypeGain = FMath::Clamp(CloudsTypeGain, 0.1f, 0.9f);
	Data.BaseShapeWorldSpan = FMath::Max(BaseShapeWorldSpan, 1.f);
	switch (CoverageMapResolution)
	{
	case ECoverageMapResolution::Res512:
		Data.CoverageMapResolution = 512u;
		break;
	case ECoverageMapResolution::Res1024:
		Data.CoverageMapResolution = 1024u;
		break;
	case ECoverageMapResolution::Res2048:
		Data.CoverageMapResolution = 2048u;
		break;
	case ECoverageMapResolution::Res4096:
		Data.CoverageMapResolution = 4096u;
		break;
	case ECoverageMapResolution::Res8192:
		Data.CoverageMapResolution = 8192u;
		break;
	}
	if (BaseShapeNoiseTexture && BaseShapeNoiseTexture->GetResource())
	{
		Data.BaseShapeNoiseTextureRHI = BaseShapeNoiseTexture->GetResource()->TextureRHI;
	}
	if (DetailNoiseTexture && DetailNoiseTexture->GetResource())
	{
		Data.DetailNoiseTextureRHI = DetailNoiseTexture->GetResource()->TextureRHI;
	}
	return Data;
}

void UOrbisCloudsComponent::OnRegister()
{
	Super::OnRegister();
	UpdateSubsystemRegistration();
}

void UOrbisCloudsComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UOrbisCloudsSubsystem* Subsystem = World->GetSubsystem<UOrbisCloudsSubsystem>())
		{
			Subsystem->UnregisterPlanet(this);
		}
	}

	Super::OnUnregister();
}

void UOrbisCloudsComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateSubsystemRegistration();
}

void UOrbisCloudsComponent::NotifyChanged()
{
	UpdateSubsystemRegistration();
}

void UOrbisCloudsComponent::UpdateSubsystemRegistration()
{
	if (UWorld* World = GetWorld())
	{
		if (UOrbisCloudsSubsystem* Subsystem = World->GetSubsystem<UOrbisCloudsSubsystem>())
		{
			Subsystem->RegisterPlanet(this);
		}
	}
}

void UOrbisCloudsComponent::ApplySmallPlanetPreset()
{
	SetRelativeLocation(OrbisCloudsPlanetPresets::SmallCenter);
	AtmosphereRadius = OrbisCloudsPlanetPresets::SmallAtmosphereRadius;
	CloudInnerRadius = OrbisCloudsPlanetPresets::SmallCloudInnerRadius;
	CloudOuterRadius = OrbisCloudsPlanetPresets::SmallCloudOuterRadius;

#if WITH_EDITOR
	if (GIsEditor)
	{
		Modify();
	}
#endif

	UpdateSubsystemRegistration();
}

void UOrbisCloudsComponent::ApplyLargePlanetPreset()
{
	SetRelativeLocation(OrbisCloudsPlanetPresets::LargeCenter);
	AtmosphereRadius = OrbisCloudsPlanetPresets::LargeAtmosphereRadius;
	CloudInnerRadius = OrbisCloudsPlanetPresets::LargeCloudInnerRadius;
	CloudOuterRadius = OrbisCloudsPlanetPresets::LargeCloudOuterRadius;

#if WITH_EDITOR
	if (GIsEditor)
	{
		Modify();
	}
#endif

	UpdateSubsystemRegistration();
}

#if WITH_EDITOR
#include "UObject/UnrealType.h"

void UOrbisCloudsComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateSubsystemRegistration();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
