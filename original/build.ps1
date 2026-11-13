# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [string]$ToolchainRoot = (Join-Path $env:USERPROFILE '.copilot\toolchains\kenshi-v100'),
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([IntPtr]::Size -ne 8 -or $PSVersionTable.PSVersion.Major -lt 5) {
    throw 'Use x64 Windows PowerShell 5.1 or later.'
}
$repo = Split-Path -Parent $PSScriptRoot
$ToolchainRoot = [IO.Path]::GetFullPath($ToolchainRoot).TrimEnd('\')
$pins = Get-Content -LiteralPath "$PSScriptRoot\dependencies.json" -Raw | ConvertFrom-Json
$origin = Get-Content -LiteralPath "$PSScriptRoot\source-manifest.json" -Raw | ConvertFrom-Json
if ($pins.schemaVersion -ne 1 -or $origin.schemaVersion -ne 1) {
    throw 'Unsupported original foundation manifest schema.'
}

# Reuse only pinned function definitions, never either remake script's entry point.
foreach ($helper in $pins.helpers) {
    $path = Join-Path $repo $helper.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $helper.sha256) {
        throw "Foundation helper changed: $path. Review and repin deliberately."
    }
    $tokens = $null
    $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
    if ($errors.Count) { throw "Cannot parse foundation helper: $path." }
    foreach ($name in $helper.functions) {
        $definitions = @($ast.FindAll({
            param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $false))
        if ($definitions.Count -ne 1) { throw "Expected one helper function: $name." }
        . ([scriptblock]::Create($definitions[0].Extent.Text))
    }
}

Write-Host 'Checking immutable 0.8.19 source and pinned VC100/SDK 7.1/KenshiLib 0.5.0 inputs...'
$source = Join-Path $PSScriptRoot 'upstream'
if (@(Get-ChildItem -LiteralPath $source -File -Recurse -Force).Count -ne $origin.files.Count) {
    throw 'Original source file count changed.'
}
foreach ($file in $origin.files) {
    Assert-Hash (Join-Path $source $file.path) $file.sha256
}
$baselinePath = Join-Path $repo 'remake\dependencies.json'
Assert-Hash $baselinePath $pins.baselineSha256
$baseline = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json
foreach ($file in @($baseline.files) + @($pins.files)) {
    Assert-Hash (Join-Path $ToolchainRoot $file.path) $file.sha256
}
foreach ($tree in $baseline.headerTrees) {
    Assert-HeaderTree (Join-Path $ToolchainRoot $tree.path) $tree
}
if ($ValidateOnly) {
    Write-Host 'ORIGINAL_INPUTS_OK (no compilation or runtime execution)'
    return
}

$vc = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files(64)\Microsoft Visual Studio 10.0\VC'
$vcInclude = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files\Microsoft Visual Studio 10.0\VC\include'
$sdk = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files\Microsoft SDKs\Windows\v7.1'
$include = Join-Path $ToolchainRoot 'KenshiLib\Include'
$libraries = Join-Path $ToolchainRoot 'KenshiLib_Examples_deps\KenshiLib\Libraries'
$outRoot = Join-Path $repo 'out\original'
if (Test-Path -LiteralPath "$outRoot\RE_Kenshi.json") {
    throw 'Active mod configuration is forbidden in original developer output.'
}
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$out = Join-Path $outRoot $runId
New-Item -ItemType Directory -Path $out -Force | Out-Null
$saved = @{}
$names = @('PATH', 'INCLUDE', 'LIB', 'CL', '_CL_', 'LINK', '_LINK_')
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    $env:PATH = "$vc\bin\amd64;$env:PATH"
    $env:INCLUDE = "$include;$include\ogre;$include\mygui;$ToolchainRoot\KenshiLib_Examples_deps\boost_1_60_0;$vcInclude;$sdk\Include"
    $env:LIB = "$vc\lib\amd64;$sdk\Lib\x64"
    foreach ($name in @('CL', '_CL_', 'LINK', '_LINK_')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    Push-Location $out
    try {
        $flags = @('/nologo', '/c', '/O2', '/GL', '/Gy', '/Oi', '/MD', '/EHsc', '/W4', '/Zi',
            '/DNDEBUG', '/D_WINDOWS', '/DWIN32', '/D_WIN64', '/DUNICODE', '/D_UNICODE',
            '/D_CRT_SECURE_NO_WARNINGS', '/DBOOST_ALL_NO_LIB', '/DBOOST_ERROR_CODE_HEADER_ONLY',
            '/DBOOST_SYSTEM_NO_DEPRECATED', '/Fdcompile.pdb')
        $sources = @('src\Catalog.cpp', 'src\Vehicle.cpp', 'src\Persistence.cpp', 'src\WorkshopModel.cpp',
            'plugin\ProjectCars.cpp', 'plugin\NativeRuntime.cpp', 'plugin\VehicleWorkshop.cpp',
            'plugin\VehicleDebug.cpp', 'plugin\FittingRuntime.cpp', 'plugin\OilProduction.cpp')
        $objects = @()
        foreach ($relative in $sources) {
            Invoke-Native "$vc\bin\amd64\cl.exe" ($flags + @(Join-Path $source $relative))
            $objects += [IO.Path]::GetFileNameWithoutExtension($relative) + '.obj'
        }
        Invoke-Native "$vc\bin\amd64\link.exe" (@('/nologo', '/DLL', '/LTCG', '/MACHINE:X64',
            '/SUBSYSTEM:WINDOWS', '/DEBUG', '/INCREMENTAL:NO', '/OUT:ProjectCars.dll',
            '/IMPLIB:ProjectCars.lib', '/PDB:ProjectCars.pdb', '/PDBALTPATH:%_PDB%') + $objects +
            @("$ToolchainRoot\KenshiLib\Libraries\KenshiLib.lib", "$libraries\MyGUIEngine_x64.lib",
              "$libraries\OgreMain_x64.lib", 'kernel32.lib', 'user32.lib', 'winmm.lib'))
        $pe = Read-PE "$out\ProjectCars.dll"
        $pdb = Read-PdbIdentity "$out\ProjectCars.pdb"
        if (-not $pe.IsDll -or $pe.ExportFunctionCount -ne 1 -or
            @($pe.Exports).Count -ne 1 -or $pe.Exports[0] -cne '?startPlugin@@YAXXZ') {
            throw 'Expected AMD64 VC100 DLL with exactly the void C++ startPlugin export.'
        }
        if ($pe.CodeView.Guid -ne $pdb.Guid -or $pe.CodeView.Age -ne $pdb.Age -or
            $pe.CodeView.PdbPath -cne 'ProjectCars.pdb') {
            throw 'Original DLL/PDB identity mismatch.'
        }
        $allowed = @('KERNEL32.dll', 'USER32.dll', 'WINMM.dll', 'MSVCR100.dll', 'MSVCP100.dll',
            'KenshiLib.dll', 'OgreMain_x64.dll', 'MyGUIEngine_x64.dll')
        foreach ($name in $pe.Imports.Keys) {
            if ($name -notin $allowed) { throw "Unexpected original DLL import: $name." }
        }
        foreach ($name in @('KenshiLib.dll', 'MSVCR100.dll', 'MSVCP100.dll')) {
            if (-not $pe.Imports.ContainsKey($name)) { throw "Missing original import: $name." }
        }
        [ordered]@{
            sourceCommit = $origin.commit
            compilerVersion = $baseline.compiler.version
            sdkVersion = $baseline.kenshiLib.version
            machine = 'AMD64'
            linkerVersion = '10.0'
            dllSha256 = (Get-FileHash -LiteralPath "$out\ProjectCars.dll").Hash.ToLowerInvariant()
            pdbSha256 = (Get-FileHash -LiteralPath "$out\ProjectCars.pdb").Hash.ToLowerInvariant()
            codeView = $pe.CodeView
            exports = $pe.Exports
            imports = $pe.Imports
            runtimeVerified = $false
        } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath "$out\artifact-report.json" -Encoding UTF8
        Write-Host "ORIGINAL_BUILD_OK $out (inactive; no DLL loaded or installed)"
    } finally {
        Pop-Location
    }
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}
