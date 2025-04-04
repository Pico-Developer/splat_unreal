/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

using System.IO;
using UnrealBuildTool;

public class PICOSplatThirdParty : ModuleRules
{
	public PICOSplatThirdParty(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("SPLAT_EXPORT_API=__declspec(dllexport)");
        PrivateDependencyModuleNames.Add("Core");

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "splat"));
	}
}