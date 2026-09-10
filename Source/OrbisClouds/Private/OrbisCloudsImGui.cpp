#include "OrbisCloudsImGui.h"

#if WITH_EDITOR
#include "OrbisCloudsComponent.h"
#include "OrbisCloudsCVars.h"

#include <imgui.h>
#endif

void OrbisCloudsImGui::Register()
{
}

void OrbisCloudsImGui::Unregister()
{
}

void OrbisCloudsImGui::Draw(UOrbisCloudsComponent *Component)
{
#if WITH_EDITOR
	if (!ImGui::Begin("OrbisClouds"))
	{
		ImGui::End();
		return;
	}

	int32 CloudsViewModeIndex = FMath::Clamp(
		CVarOrbisCloudsViewMode.GetValueOnGameThread(),
		0,
		2);
	const char *CloudsViewModeNames[] = {"Cloud Coverage", "Cloud Type", "Clouds"};
	if (ImGui::Combo(
			"Clouds View Mode",
			&CloudsViewModeIndex,
			CloudsViewModeNames,
			UE_ARRAY_COUNT(CloudsViewModeNames)))
	{
		CVarOrbisCloudsViewMode->Set(CloudsViewModeIndex);
	}

	if (!Component)
	{
		ImGui::TextUnformatted("No OrbisClouds component.");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Small Radius"))
	{
		Component->ApplySmallPlanetPreset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Large Radius"))
	{
		Component->ApplyLargePlanetPreset();
	}

	ImGui::TextUnformatted("Cloud Coverage");

	if (ImGui::InputInt("Seed##CloudCoverage", &Component->NoiseSeed))
	{
		Component->NoiseSeed = FMath::Max(Component->NoiseSeed, 0);
		Component->NotifyChanged();
	}

	int32 BaseNoiseIndex = static_cast<int32>(Component->BaseNoise);
	const char *BaseNoiseNames[] = {"Perlin", "Simplex", "Value"};
	if (ImGui::Combo("Base Noise##CloudCoverage", &BaseNoiseIndex, BaseNoiseNames, UE_ARRAY_COUNT(BaseNoiseNames)))
	{
		Component->BaseNoise = static_cast<EOrbisCloudsBaseNoise>(BaseNoiseIndex);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Noise Scale##CloudCoverage", &Component->CloudCoverageNoiseScale, 0.1f, 0.1f, 2048.f, "%.2f"))
	{
		Component->CloudCoverageNoiseScale = FMath::Clamp(Component->CloudCoverageNoiseScale, 0.1f, 2048.f);
		Component->NotifyChanged();
	}

	if (ImGui::SliderInt("Octaves##CloudCoverage", &Component->CloudsCoverageOctaves, 1, 8))
	{
		Component->CloudsCoverageOctaves = FMath::Clamp(Component->CloudsCoverageOctaves, 1, 8);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Lacunarity##CloudCoverage", &Component->CloudsCoverageLacunarity, 0.01f, 1.f, 4.f, "%.2f"))
	{
		Component->CloudsCoverageLacunarity = FMath::Clamp(Component->CloudsCoverageLacunarity, 1.f, 4.f);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Gain##CloudCoverage", &Component->CloudsCoverageGain, 0.01f, 0.1f, 0.9f, "%.2f"))
	{
		Component->CloudsCoverageGain = FMath::Clamp(Component->CloudsCoverageGain, 0.1f, 0.9f);
		Component->NotifyChanged();
	}

	ImGui::TextUnformatted("Coverage Warp");

	if (ImGui::Checkbox("Use Warp##CloudCoverage", &Component->bCloudsCoverageUseWarp))
	{
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Strength##CloudCoverageWarp", &Component->CloudsCoverageWarpStrength, 0.01f, 0.f, 8.f, "%.2f"))
	{
		Component->CloudsCoverageWarpStrength = FMath::Clamp(Component->CloudsCoverageWarpStrength, 0.f, 8.f);
		Component->NotifyChanged();
	}

	if (ImGui::SliderInt("Warp Octaves##CloudCoverageWarp", &Component->CloudsCoverageWarpOctaves, 1, 8))
	{
		Component->CloudsCoverageWarpOctaves = FMath::Clamp(Component->CloudsCoverageWarpOctaves, 1, 8);
		Component->NotifyChanged();
	}

	ImGui::TextUnformatted("Cloud Type");

	if (ImGui::InputInt("Seed##CloudType", &Component->CloudTypeNoiseSeed))
	{
		Component->CloudTypeNoiseSeed = FMath::Max(Component->CloudTypeNoiseSeed, 0);
		Component->NotifyChanged();
	}

	int32 CloudTypeNoiseIndex = static_cast<int32>(Component->CloudTypeNoise);
	if (ImGui::Combo("Base Noise##CloudType", &CloudTypeNoiseIndex, BaseNoiseNames, UE_ARRAY_COUNT(BaseNoiseNames)))
	{
		Component->CloudTypeNoise = static_cast<EOrbisCloudsBaseNoise>(CloudTypeNoiseIndex);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Noise Scale##CloudType", &Component->CloudTypeNoiseScale, 0.1f, 0.1f, 4096.f, "%.2f"))
	{
		Component->CloudTypeNoiseScale = FMath::Clamp(Component->CloudTypeNoiseScale, 0.1f, 4096.f);
		Component->NotifyChanged();
	}

	if (ImGui::SliderInt("Octaves##CloudType", &Component->CloudsTypeOctaves, 1, 8))
	{
		Component->CloudsTypeOctaves = FMath::Clamp(Component->CloudsTypeOctaves, 1, 8);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Lacunarity##CloudType", &Component->CloudsTypeLacunarity, 0.01f, 1.f, 4.f, "%.2f"))
	{
		Component->CloudsTypeLacunarity = FMath::Clamp(Component->CloudsTypeLacunarity, 1.f, 4.f);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Gain##CloudType", &Component->CloudsTypeGain, 0.01f, 0.1f, 0.9f, "%.2f"))
	{
		Component->CloudsTypeGain = FMath::Clamp(Component->CloudsTypeGain, 0.1f, 0.9f);
		Component->NotifyChanged();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Coverage Map");

	int32 CoverageMapResolutionIndex = static_cast<int32>(Component->CoverageMapResolution);
	const char *CoverageMapResolutionNames[] = {"512", "1024", "2048", "4096", "8192"};
	if (ImGui::Combo("Resolution##CoverageMap", &CoverageMapResolutionIndex, CoverageMapResolutionNames, UE_ARRAY_COUNT(CoverageMapResolutionNames)))
	{
		Component->CoverageMapResolution = static_cast<ECoverageMapResolution>(CoverageMapResolutionIndex);
		Component->NotifyChanged();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Authored Textures");

	if (ImGui::DragFloat("Base Shape World Span", &Component->BaseShapeWorldSpan, 10.f, 1.f, 1000000.f, "%.1f"))
	{
		Component->BaseShapeWorldSpan = FMath::Max(Component->BaseShapeWorldSpan, 1.f);
		Component->NotifyChanged();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Noise Output");

	if (ImGui::DragFloat("Noise Output Min", &Component->NoiseOutputMin, 0.01f, -1.f, 1.f, "%.2f"))
	{
		Component->NoiseOutputMin = FMath::Clamp(Component->NoiseOutputMin, -1.f, Component->NoiseOutputMax);
		Component->NotifyChanged();
	}

	if (ImGui::DragFloat("Noise Output Max", &Component->NoiseOutputMax, 0.01f, -1.f, 1.f, "%.2f"))
	{
		Component->NoiseOutputMax = FMath::Clamp(Component->NoiseOutputMax, Component->NoiseOutputMin, 1.f);
		Component->NotifyChanged();
	}

	ImGui::End();
#endif
}
