/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

using System.IO;
using UnrealBuildTool;

public class PICOSplatEditor : ModuleRules
{
	public PICOSplatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("SPLAT_EXPORT_API=__declspec(dllimport)");
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetDefinition",
				"Core",
				"CoreUObject",
				"GeometryCore",
				"PICOSplatRuntime",
				"PICOSplatThirdParty",
				"UnrealEd",
			}
		);
	}
}