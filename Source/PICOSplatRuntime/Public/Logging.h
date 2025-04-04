/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Logging/LogMacros.h"

PICOSPLATRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogPICOSplat, Log, All);

/**
 * PICO_LOG prefix to avoid collisions, as we are in the global namespace.
 * @see https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine
 */

#define PICO_LOGF(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Fatal, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGE(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Error, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGW(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Warning, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGD(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Display, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGL(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Log, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGV(Format, ...)                                                 \
	UE_LOG(LogPICOSplat, Verbose, TEXT(Format), ##__VA_ARGS__)
#define PICO_LOGVV(Format, ...)                                                \
	UE_LOG(LogPICOSplat, VeryVerbose, TEXT(Format), ##__VA_ARGS__)