/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Logging.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Object.h"

#include "SplatSettings.generated.h"

UENUM(BlueprintType)
enum class ECovarianceFormat : uint8
{
	Float10 = 0 UMETA(DisplayName = "64 Bits: Float10/11x6"),
	Float16 = 1 UMETA(DisplayName = "128 Bits: Float16x6 + Pad"),
	Float32 = 2 UMETA(DisplayName = "256 Bits: Float32x6 + Pad")
};

UENUM(BlueprintType)
enum class EDepthFormat : uint8
{
	InvertedUInt16 = 0 UMETA(DisplayName = "16 Bits: Inverted UInt16")
};

UENUM(BlueprintType)
enum class EPositionFormat : uint8
{
	UNorm10 = 0 UMETA(DisplayName = "32 Bits: UNorm10/11x3"),
	Float16 = 1 UMETA(DisplayName = "64 Bits: Float16x3 + Pad"),
	Float32 = 2 UMETA(DisplayName = "128 Bits: Float32x3 + Pad")
};

UENUM(BlueprintType)
enum class ESortingMethod : uint8
{
	CPUAsynchronous = 0 UMETA(DisplayName = "CPU Asynchronous"),
	GPUSynchronous = 1 UMETA(DisplayName = "GPU Synchronous"),
};

UENUM(BlueprintType)
enum class ESplatRadius : uint8
{
	TwoSqrt2 = 0 UMETA(DisplayName = "2 * Sqrt(2) σ (Standard)"),
	Three = 1 UMETA(DisplayName = "3 σ"),
};

/**
 * Global settings.
 *
 * @param Config - `= Engine`, saves settings in `Engine.ini`.
 * @param DefaultConfig - Saves settings to default `.ini`s, not local.
 */
UCLASS(Config = Engine, DefaultConfig)
class PICOSPLATRUNTIME_API USplatSettings final : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Helper to check config `.ini` for sorting method.
	 *
	 * @return Whether to use GPU sorting.
	 */
	static bool IsSortingOnGPU()
	{
		FString SortingMethod;
		if (GConfig->GetString(
				TEXT("/Script/PICOSplatRuntime.SplatSettings"),
				TEXT("SortingMethod"),
				SortingMethod,
				GEngineIni))
		{
			const FString NAME_GPU_SYNC(TEXT("GPUSynchronous"));
			const FString NAME_CPU_ASYNC(TEXT("CPUAsynchronous"));
			if (SortingMethod == NAME_GPU_SYNC)
			{
				return true;
			}
			else if (SortingMethod == NAME_CPU_ASYNC)
			{
				return false;
			}
			else
			{
				PICO_LOGE("Unknown sorting method: %s", *SortingMethod);
			}
		}

		return false;
	}

private:
	/**
	 * Specifiers:
	 * @param Category - `= NAME`, section header property grouped under.
	 * @param Config - Saves to `.ini`.
	 * @param EditAnywhere - Editable via UI.
	 *
	 * @see https://dev.epicgames.com/documentation/en-us/unreal-engine/property-specifiers?application_version=4.27
	 */

	/** Format used to store covariance (i.e. scaling and rotation) of splats. Larger formats increase asset size, memory usage and time spent reading data in shaders, in exchange for improved visual quality. */
	UPROPERTY(
		Category = Configuration,
		Config,
		EditAnywhere,
		meta =
			(ConfigRestartRequired = true,
	         DisplayName = "Covariance Format",
	         EditCondition = false))
	ECovarianceFormat CovarianceFormat = ECovarianceFormat::Float10;

	/** Format used for depth values when sorting splats. Higher bit counts may have slightly better results in certain scenes, at an increased performance cost. */
	UPROPERTY(
		Category = Configuration,
		Config,
		EditAnywhere,
		meta =
			(ConfigRestartRequired = true,
	         DisplayName = "Depth Format",
	         EditCondition = false))
	EDepthFormat DepthFormat = EDepthFormat::InvertedUInt16;

	/** Format used to store position of splats. Larger formats increase asset size, memory usage and time spent reading data in shaders, in exchange for improved visual quality. */
	UPROPERTY(
		Category = Configuration,
		Config,
		EditAnywhere,
		meta =
			(ConfigRestartRequired = true,
	         DisplayName = "Position Format",
	         EditCondition = false))
	EPositionFormat PositionFormat = EPositionFormat::UNorm10;

	/** How splat sorting is performed. Asynchronous methods will be faster in exchange for a slight (albeit likely no noticeable) decrease in visual fidelity. CPU sorting will generally net a much higher framerate, but use a significant amount of CPU time. */
	UPROPERTY(
		Category = Configuration,
		Config,
		EditAnywhere,
		meta = (ConfigRestartRequired = true, DisplayName = "Sorting Method"))
	ESortingMethod SortingMethod = ESortingMethod::CPUAsynchronous;

	/** The distance from the center of each splat, in standard deviations σ, in which to evaluate it. Larger values will improve visual fidelity with diminishing returns, while costing increasingly more time in fragment shading. */
	UPROPERTY(
		Category = Configuration,
		Config,
		EditAnywhere,
		meta =
			(ConfigRestartRequired = true,
	         DisplayName = "Splat Radius",
	         EditCondition = false))
	ESplatRadius SplatRadius = ESplatRadius::TwoSqrt2;
};