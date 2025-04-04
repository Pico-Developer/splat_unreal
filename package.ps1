#
# Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
#
# This script copies the minimal set of files needed for submission to FAB.
#

param (
    [Parameter(Mandatory=$true)]
    [string]$Path
)

if (Test-Path -Path "$Path") {
    Write-Host "Directory $Path already exists. Exiting."
    exit 1
}

New-Item -Path "$Path" -ItemType Directory | Out-Null

$pluginDir = $PSScriptRoot

#
# Copy Unreal-specific components.
#

Copy-Item -Path "$pluginDir\Config\" -Destination "$Path" -Recurse
Copy-Item -Path "$pluginDir\Resources\" -Destination "$Path" -Recurse
Copy-Item -Path "$pluginDir\PICOSplat.uplugin" -Destination "$Path"

# Copy source code, excluding open-source parts.
New-Item -Path "$Path\Source\" -ItemType Directory | Out-Null
Copy-Item -Path "$pluginDir\Source\*" -Destination "$Path\Source\" -Exclude "ThirdParty" -Recurse

#
# Copy open-source shaders.
#

# Copy shader adapters.
New-Item -Path "$Path\Shaders\Private\" -ItemType Directory | Out-Null
Copy-Item -Path "$pluginDir\Shaders\Private\*.usf" -Destination "$Path\Shaders\Private\" -Recurse

# Copy shaders, changing extension from `.hlsl` to `.ush`.
$shaderDir = "$Path\Source\ThirdParty\Shaders\Public\"
New-Item -Path $shaderDir -ItemType Directory | Out-Null
Get-ChildItem -Path "$pluginDir\Source\ThirdParty\splat\shaders\" -File | ForEach-Object {
    $newName = $_.Name -replace "\.hlsl$", ".ush"
    $output = Join-Path -Path $shaderDir -ChildPath $newName
    Copy-Item -Path $_.FullName -Destination $output
}

# Copy over `splat` license.
Copy-Item -Path "$pluginDir\Source\ThirdParty\splat\LICENSE.md" -Destination $shaderDir

#
# Copy open-source core.
#

Copy-Item -Path "$pluginDir\Source\ThirdParty\*" -Destination "$Path\Source\ThirdParty\" -Exclude splat, Shaders -Recurse
New-Item -Path "$Path\Source\ThirdParty\splat\" -ItemType Directory | Out-Null
Copy-Item -Path "$pluginDir\Source\ThirdParty\splat\*" -Destination "$Path\Source\ThirdParty\splat\" -Exclude .git, README.md, shaders -Recurse