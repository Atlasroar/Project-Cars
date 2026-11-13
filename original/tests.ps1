# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [string]$ToolchainRoot = (Join-Path $env:USERPROFILE '.copilot\toolchains\kenshi-v100')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $repo ('out\original\validation-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$fixture\original", "$fixture\remake" -Force | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\upstream" -Destination "$fixture\original" -Recurse
foreach ($name in @('build.ps1', 'dependencies.json', 'source-manifest.json')) {
    Copy-Item -LiteralPath "$PSScriptRoot\$name" -Destination "$fixture\original"
}
foreach ($name in @('build.ps1', 'dependencies.json', 'verify-artifacts.ps1')) {
    Copy-Item -LiteralPath "$repo\remake\$name" -Destination "$fixture\remake"
}

function Expect-Rejection([string]$Root, [string]$Message) {
    $ErrorActionPreference = 'Continue'
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$fixture\original\build.ps1" `
        -ToolchainRoot $Root -ValidateOnly 2>&1
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -eq 0 -or ($output | Out-String) -notmatch [regex]::Escape($Message)) {
        throw "Expected validation rejection: $Message; got $output."
    }
}

try {
    & "$PSScriptRoot\build.ps1" -ToolchainRoot $ToolchainRoot -ValidateOnly
    [IO.File]::AppendAllText("$fixture\original\upstream\plugin\ProjectCars.cpp", "`n")
    Expect-Rejection $ToolchainRoot 'Dependency hash mismatch:'
    Copy-Item -LiteralPath "$PSScriptRoot\upstream\plugin\ProjectCars.cpp" `
        -Destination "$fixture\original\upstream\plugin\ProjectCars.cpp" -Force

    [IO.File]::WriteAllText("$fixture\original\upstream\unexpected.cpp", '')
    Expect-Rejection $ToolchainRoot 'Original source file count changed.'
    Remove-Item -LiteralPath "$fixture\original\upstream\unexpected.cpp"

    [IO.File]::AppendAllText("$fixture\remake\verify-artifacts.ps1", "`n")
    Expect-Rejection $ToolchainRoot 'Foundation helper changed:'
    Copy-Item -LiteralPath "$repo\remake\verify-artifacts.ps1" `
        -Destination "$fixture\remake\verify-artifacts.ps1" -Force

    $pins = Get-Content -LiteralPath "$fixture\original\dependencies.json" -Raw | ConvertFrom-Json
    $pins.files[0].sha256 = '0' * 64
    $pins | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath "$fixture\original\dependencies.json" -Encoding UTF8
    Expect-Rejection $ToolchainRoot 'Dependency hash mismatch:'
    Copy-Item -LiteralPath "$PSScriptRoot\dependencies.json" `
        -Destination "$fixture\original\dependencies.json" -Force

    Expect-Rejection "$fixture\absent-toolchain" 'Missing pinned dependency:'
    if (Test-Path -LiteralPath "$fixture\out") {
        throw 'Rejected input validation must not create build output.'
    }
    Write-Host 'ORIGINAL_VALIDATION_TESTS_OK (source drift, extra source, helper drift, engine pin drift, missing dependency)'
} finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
