param([string]$KenshiProjects)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $KenshiProjects) { $KenshiProjects = Split-Path -Parent $projectRoot }
$deps = Join-Path $kenshiProjects 'KenshiLib_Examples_deps'
$tc = Join-Path $kenshiProjects 'toolchain'
$vc64 = Join-Path $tc 'vc100-extract\Program Files(64)\Microsoft Visual Studio 10.0\VC'
$vcHeaders = Join-Path $tc 'vc100-x86-extract\Program Files\Microsoft Visual Studio 10.0\VC'
$sdk = Join-Path $tc 'sdk71-build-extract\Program Files\Microsoft SDKs\Windows\v7.1'
$cl = Join-Path $vc64 'bin\amd64\cl.exe'
$linker = Join-Path $vc64 'bin\amd64\link.exe'
$dumpbin = Join-Path $vc64 'bin\amd64\dumpbin.exe'
foreach ($required in @($cl, $linker, $dumpbin)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required VC10 tool missing: $required" }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$out = Join-Path $projectRoot "build\$runId"
New-Item -ItemType Directory -Path $out | Out-Null
$oldPath = $env:PATH
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
    $env:PATH = (Join-Path $vc64 'bin\amd64') + ';' + $env:PATH
    $env:INCLUDE = @(
        (Join-Path $deps 'KenshiLib\Include'),
        (Join-Path $deps 'KenshiLib\Include\ogre'),
        (Join-Path $deps 'KenshiLib\Include\mygui'),
        (Join-Path $deps 'boost_1_60_0'),
        (Join-Path $vcHeaders 'include'), (Join-Path $sdk 'Include')
    ) -join ';'
    $env:LIB = @(
        (Join-Path $deps 'KenshiLib\Libraries'),
        (Join-Path $vc64 'lib\amd64'), (Join-Path $sdk 'Lib\x64')
    ) -join ';'
    Push-Location $out
    try {
        $flags = @('/nologo','/c','/O2','/GL','/Gy','/Oi','/MD','/EHsc','/W4','/Zi',
            '/DNDEBUG','/D_WINDOWS','/DWIN32','/D_WIN64','/DUNICODE','/D_UNICODE','/D_CRT_SECURE_NO_WARNINGS',
            '/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/DBOOST_SYSTEM_NO_DEPRECATED', '/Fdcompile.pdb')

        $coreSources = @('Catalog.cpp','Vehicle.cpp','Persistence.cpp','WorkshopModel.cpp') |
            ForEach-Object { Join-Path (Join-Path $projectRoot 'src') $_ }
        & $cl @flags @coreSources
        if ($LASTEXITCODE -ne 0) { throw 'Core compile failed' }
        & $cl @flags (Join-Path $projectRoot 'plugin\ProjectCars.cpp') (Join-Path $projectRoot 'plugin\NativeRuntime.cpp') (Join-Path $projectRoot 'plugin\VehicleWorkshop.cpp') (Join-Path $projectRoot 'plugin\VehicleDebug.cpp') (Join-Path $projectRoot 'plugin\FittingRuntime.cpp') (Join-Path $projectRoot 'plugin\OilProduction.cpp')
        if ($LASTEXITCODE -ne 0) { throw 'Plugin compile failed' }
        & $linker /nologo /DLL /LTCG /MACHINE:X64 /SUBSYSTEM:WINDOWS /DEBUG /INCREMENTAL:NO /OUT:ProjectCars.dll /IMPLIB:ProjectCars.lib /PDB:ProjectCars.pdb ProjectCars.obj NativeRuntime.obj VehicleWorkshop.obj VehicleDebug.obj FittingRuntime.obj OilProduction.obj WorkshopModel.obj Catalog.obj Vehicle.obj Persistence.obj kenshilib.lib MyGUIEngine_x64.lib OgreMain_x64.lib kernel32.lib user32.lib winmm.lib
        if ($LASTEXITCODE -ne 0) { throw 'Plugin link failed' }
        $exports = & $dumpbin /exports ProjectCars.dll
        if ($LASTEXITCODE -ne 0 -or -not ($exports -match '\?startPlugin@@YAXXZ')) { throw 'Required C++ plugin export missing' }
        $exports | Set-Content -LiteralPath (Join-Path $out 'exports.txt')
        $headers = & $dumpbin /headers ProjectCars.dll
        if ($LASTEXITCODE -ne 0 -or -not ($headers -match '8664 machine')) { throw 'Plugin must be x64' }
        $headers | Set-Content -LiteralPath (Join-Path $out 'headers.txt')
        & $dumpbin /dependents ProjectCars.dll | Set-Content -LiteralPath (Join-Path $out 'dependents.txt')
        if ($LASTEXITCODE -ne 0) { throw 'Dependency inspection failed' }

        Write-Output "BUILD_OK $out"
    } finally { Pop-Location }
} finally {
    $env:PATH = $oldPath
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}
