/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

namespace PICO::Splat
{

/**
 * Provides 3DGS rendering support, to both games and editor.
 */
class FPICOSplatRuntimeModule final : public IModuleInterface
{
	virtual void StartupModule() override
	{
		// Register shader adapters.
		AddShaderSourceDirectoryMapping(
			TEXT("/Plugin/PICOSplat"),
			FPaths::Combine(
				IPluginManager::Get()
					.FindPlugin(TEXT("PICOSplat"))
					->GetBaseDir(),
				TEXT("Shaders")));

		// Register open-source shaders.
		AddShaderSourceDirectoryMapping(
			TEXT("/Plugin/PICOSplat/ThirdParty"),
			FPaths::Combine(
				IPluginManager::Get()
					.FindPlugin(TEXT("PICOSplat"))
					->GetBaseDir(),
				TEXT("Source/ThirdParty/Shaders")));
	}
};

} // namespace PICO::Splat

IMPLEMENT_MODULE(PICO::Splat::FPICOSplatRuntimeModule, PICOSplatRuntime);