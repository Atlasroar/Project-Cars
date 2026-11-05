# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [string]$ToolchainRoot = (Join-Path $env:USERPROFILE '.copilot\toolchains\kenshi-v100')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([IntPtr]::Size -ne 8 -or $PSVersionTable.PSVersion.Major -lt 5) {
    throw 'Use x64 Windows PowerShell 5.1 or later.'
}

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing pinned dependency: $Path. See remake\README.md for setup."
    }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "Dependency hash mismatch: $Path; expected $Expected, got $actual."
    }
}

function Assert-HeaderTree([string]$Path, $Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Missing pinned header directory: $Path."
    }
    $rows = New-Object 'System.Collections.Generic.List[string]'
    foreach ($file in Get-ChildItem -LiteralPath $Path -File -Recurse) {
        $relative = $file.FullName.Substring($Path.Length + 1).ToLowerInvariant()
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $rows.Add($relative + "`t" + $hash)
    }
    $ordered = $rows.ToArray()
    [Array]::Sort($ordered, [StringComparer]::Ordinal)
    $bytes = [Text.Encoding]::UTF8.GetBytes(([string]::Join("`n", $ordered) + "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $actual = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
    if ($rows.Count -ne $Expected.fileCount -or $actual -ne $Expected.sha256) {
        throw "Header tree mismatch: $Path; got $($rows.Count) files, SHA256 $actual."
    }
}

function Invoke-Native([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$File failed with exit code $LASTEXITCODE."
    }
}

$repo = Split-Path -Parent $PSScriptRoot
$ToolchainRoot = [IO.Path]::GetFullPath($ToolchainRoot).TrimEnd('\')
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
if ($pins.schemaVersion -ne 1) {
    throw 'Unsupported dependency pin schema.'
}
Write-Host 'Checking pinned VC100, Windows SDK 7.1, KenshiLib 0.5.0 and x64 test CRT...'
foreach ($file in $pins.files) {
    Assert-Hash (Join-Path $ToolchainRoot $file.path) $file.sha256
}
foreach ($tree in $pins.headerTrees) {
    Assert-HeaderTree (Join-Path $ToolchainRoot $tree.path) $tree
}
foreach ($file in $pins.testRuntime.files) {
    Assert-Hash (Join-Path "$env:WINDIR\System32" $file.name) $file.sha256
}

$vc = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files(64)\Microsoft Visual Studio 10.0\VC'
$vcInclude = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files\Microsoft Visual Studio 10.0\VC\include'
$sdk = Join-Path $ToolchainRoot 'windows-sdk-7.1\Program Files\Microsoft SDKs\Windows\v7.1'
$cl = Join-Path $vc 'bin\amd64\cl.exe'
$link = Join-Path $vc 'bin\amd64\link.exe'
$out = Join-Path $repo 'out\remake'
$obj = Join-Path $out 'obj'
New-Item -ItemType Directory -Path $obj -Force | Out-Null
$reportPath = Join-Path $out 'artifact-report.json'
if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath
}

$saved = @{}
$names = @('PATH', 'INCLUDE', 'LIB', 'CL', '_CL_', 'LINK', '_LINK_')
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    $env:PATH = "$vc\bin\amd64;$sdk\Bin\x64;$env:PATH"
    $env:INCLUDE = "$vcInclude;$sdk\Include"
    $env:LIB = "$vc\lib\amd64;$sdk\Lib\x64"
    foreach ($name in @('CL', '_CL_', 'LINK', '_LINK_')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $flags = @('/nologo', '/EHsc', '/MD', '/O2', '/GL', '/Zi', '/W4', '/WX', '/Gd',
               '/Zc:wchar_t', '/Zc:forScope', '/DWIN32', '/D_WIN64', '/DNDEBUG',
               '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/D_WIN32_WINNT=0x0601',
               "/I$PSScriptRoot\src")
    Invoke-Native $cl ($flags + @('/c', "$PSScriptRoot\src\catalog.cpp",
                                  "/Fo$obj\catalog.obj", "/Fd$obj\catalog.compiler.pdb"))
    Invoke-Native $cl ($flags + @('/c', "$PSScriptRoot\tests\catalog_tests.cpp",
                                  "/Fo$obj\catalog_tests.obj", "/Fd$obj\tests.compiler.pdb"))
    $linkFlags = @('/nologo', '/MACHINE:X64', '/LTCG', '/DEBUG', '/INCREMENTAL:NO',
                   '/OPT:REF', '/OPT:ICF', '/PDBALTPATH:%_PDB%')
    Invoke-Native $link ($linkFlags + @("/OUT:$out\CatalogTests.exe",
        "/PDB:$out\CatalogTests.pdb", "$obj\catalog.obj", "$obj\catalog_tests.obj", 'kernel32.lib'))
    Invoke-Native "$out\CatalogTests.exe" @("$repo\vehicles.catalog")

    Invoke-Native $cl ($flags + @("/I$ToolchainRoot\KenshiLib\Include", '/c',
        "$PSScriptRoot\src\plugin.cpp", "/Fo$obj\plugin.obj", "/Fd$obj\plugin.compiler.pdb"))
    Invoke-Native $link ($linkFlags + @('/DLL', "/OUT:$out\ProjectCarsRemake.dll",
        "/PDB:$out\ProjectCarsRemake.pdb", "/IMPLIB:$out\ProjectCarsRemake.lib",
        "$obj\catalog.obj", "$obj\plugin.obj", "$ToolchainRoot\KenshiLib\Libraries\KenshiLib.lib",
        'kernel32.lib', 'user32.lib'))

    Copy-Item -LiteralPath "$repo\vehicles.catalog" -Destination "$out\vehicles.catalog" -Force
    Copy-Item -LiteralPath "$PSScriptRoot\development\RE_Kenshi.json.example" -Destination $out -Force
    Invoke-Native "$out\CatalogTests.exe" @("$out\vehicles.catalog")
    $report = & "$PSScriptRoot\verify-artifacts.ps1" -OutputDirectory $out
    $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "Foundation built and checked: $out"
    Write-Host 'Development only. Live Kenshi startup and gameplay are NOT verified.'
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}
