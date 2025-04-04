# PICOSplat: 3DGS Rendering for Unreal Engine

This repository contains the source code for the [PICOSplat Unreal Engine Plugin](https://www.fab.com/listings/a7e35c41-592d-493d-bbf6-2d048f398c1e).
For the open-source portions of the plugin, see [splat](https://github.com/Pico-Developer/splat).

As the open-source portions of the plugin live within another project, special handling is required in order to adapt those components to Unreal Engine.
These adaptations are atypical of standard Unreal Engine source code, and thus require some extra explanation:

- `Source/ThirdParty` Module

  In our testing, Unreal Engine's build system did not correctly pick up on changes to files accessed via symlink.
  To get around this, while still following the FAB requirement that third-party packages live in `Source/ThirdParty`, we've opted to make `Source/ThirdParty` an Unreal module.
  See `PICOSplatEditor.Build.cs` and `PICOSplatThirdParty.Build.cs` for DLL import/export configuration.

- Shader Symlinks and Wrappers

  Unreal Engine's shader compiler requires all shaders to end with a `.ush` or `.usf` extension.
  As we want to keep our open-source shaders with `.hlsl` extensions for consistency across other rendering pipelines, we instead rely on symlinks to give the shader compiler a path ending in `.ush` (as all are included into wrapper `.usf` files).
  The symlinks live under `Source/ThirdParty/Shaders/Public`, as all open-source code must be under `Source/ThirdParty`, while their wrappers are in `Shaders\Private`.
  These wrappers contain all needed includes and definitions.

  We've tested that, unlike the Unreal Engine build system, the shader compiler *does* work correctly with symlinks; changes to the target file are picked up even if the symlink has not been modified.

  Note that when building the plugin via `RunUAT.bat`, these symlinks do not work.
  Instead, all shaders are copied over with extensions changed to `.ush` via `package.ps1`.

- Logging

  On startup, the Editor module (`PICOSplatEditorModule.cpp`) will register a log receiver with the open-source parser.
  This receives logged strings from the parser and redirects them to `UE_LOG`.