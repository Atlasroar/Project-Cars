# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'out\remake')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Range([byte[]]$Data, [long]$Offset, [long]$Size) {
    if ($Offset -lt 0 -or $Size -lt 0 -or $Offset + $Size -gt $Data.Length) {
        throw "Invalid artifact offset $Offset, size $Size."
    }
}
function U16([byte[]]$Data, [long]$Offset) {
    Range $Data $Offset 2
    return [BitConverter]::ToUInt16($Data, [int]$Offset)
}
function U32([byte[]]$Data, [long]$Offset) {
    Range $Data $Offset 4
    return [BitConverter]::ToUInt32($Data, [int]$Offset)
}
function U64([byte[]]$Data, [long]$Offset) {
    Range $Data $Offset 8
    return [BitConverter]::ToUInt64($Data, [int]$Offset)
}
function CString([byte[]]$Data, [long]$Offset) {
    Range $Data $Offset 1
    $end = $Offset
    while ($end -lt $Data.Length -and $Data[$end] -ne 0) { ++$end }
    if ($end -eq $Data.Length) { throw 'Unterminated artifact string.' }
    return [Text.Encoding]::ASCII.GetString($Data, [int]$Offset, [int]($end - $Offset))
}
function GuidAt([byte[]]$Data, [long]$Offset) {
    Range $Data $Offset 16
    $bytes = New-Object byte[] 16
    [Array]::Copy($Data, $Offset, $bytes, 0, 16)
    return (New-Object System.Guid -ArgumentList (,$bytes)).ToString()
}
function RvaOffset([uint32]$Rva, $Sections, [uint32]$HeaderSize) {
    if ($Rva -lt $HeaderSize) { return [long]$Rva }
    foreach ($section in $Sections) {
        $extent = [Math]::Max([long]$section.VirtualSize, [long]$section.RawSize)
        if ($Rva -ge $section.Address -and [long]$Rva -lt [long]$section.Address + $extent) {
            $delta = [long]$Rva - $section.Address
            if ($delta -ge $section.RawSize) { throw 'RVA has no file-backed data.' }
            return [long]$section.RawOffset + $delta
        }
    }
    throw "Unmapped RVA $Rva."
}

function Read-PE([string]$Path) {
    $data = [IO.File]::ReadAllBytes($Path)
    if ((U16 $data 0) -ne 0x5a4d) { throw "$Path is not a PE image." }
    $pe = U32 $data 0x3c
    if ((U32 $data $pe) -ne 0x4550 -or (U16 $data ($pe + 4)) -ne 0x8664) {
        throw "$Path is not an AMD64 PE image."
    }
    $optional = [long]$pe + 24
    $optionalSize = U16 $data ($pe + 20)
    Range $data $optional $optionalSize
    if ($optionalSize -lt 168 -or (U16 $data $optional) -ne 0x20b -or
        (U32 $data ($optional + 108)) -lt 7) { throw "$Path is not native PE32+." }
    if ($data[$optional + 2] -ne 10 -or $data[$optional + 3] -ne 0) {
        throw "$Path was not linked by the pinned VC100 linker."
    }
    $headerSize = U32 $data ($optional + 60)
    $sections = @()
    $sectionCount = U16 $data ($pe + 6)
    for ($i = 0; $i -lt $sectionCount; ++$i) {
        $offset = $optional + $optionalSize + 40 * $i
        Range $data $offset 40
        $sections += [pscustomobject]@{
            VirtualSize = U32 $data ($offset + 8)
            Address = U32 $data ($offset + 12)
            RawSize = U32 $data ($offset + 16)
            RawOffset = U32 $data ($offset + 20)
        }
    }
    $exports = @()
    $exportRva = U32 $data ($optional + 112)
    $functionCount = 0
    if ($exportRva -ne 0) {
        $directory = RvaOffset $exportRva $sections $headerSize
        Range $data $directory 40
        $functionCount = U32 $data ($directory + 20)
        $nameCount = U32 $data ($directory + 24)
        $names = RvaOffset (U32 $data ($directory + 32)) $sections $headerSize
        $ordinals = RvaOffset (U32 $data ($directory + 36)) $sections $headerSize
        $functions = RvaOffset (U32 $data ($directory + 28)) $sections $headerSize
        for ($i = 0; $i -lt $nameCount; ++$i) {
            $ordinal = U16 $data ($ordinals + 2 * $i)
            if ($ordinal -ge $functionCount) { throw 'Invalid export ordinal.' }
            $functionRva = U32 $data ($functions + 4 * $ordinal)
            $exportSize = U32 $data ($optional + 116)
            if ($functionRva -eq 0 -or ($functionRva -ge $exportRva -and
                [long]$functionRva -lt [long]$exportRva + $exportSize)) {
                throw 'Missing or forwarded plugin export.'
            }
            $exports += CString $data (RvaOffset (U32 $data ($names + 4 * $i)) $sections $headerSize)
        }
    }
    $imports = @{}
    $importRva = U32 $data ($optional + 120)
    if ($importRva -eq 0) { throw 'Missing native import table.' }
    $descriptor = RvaOffset $importRva $sections $headerSize
    for (;;) {
        Range $data $descriptor 20
        $nameRva = U32 $data ($descriptor + 12)
        $originalThunk = U32 $data $descriptor
        $firstThunk = U32 $data ($descriptor + 16)
        if ($nameRva -eq 0) {
            if ($originalThunk -ne 0 -or $firstThunk -ne 0) { throw 'Malformed import descriptor.' }
            break
        }
        $name = CString $data (RvaOffset $nameRva $sections $headerSize)
        if ($imports.ContainsKey($name)) { throw "Duplicate import descriptor: $name." }
        if ($originalThunk -eq 0) { $originalThunk = $firstThunk }
        $thunkOffset = RvaOffset $originalThunk $sections $headerSize
        $symbols = @()
        for (;;) {
            $thunk = U64 $data $thunkOffset
            if ($thunk -eq 0) { break }
            if (($thunk -shr 63) -ne 0) {
                $symbols += '#' + ($thunk -band 0xffff)
            } else {
                if ($thunk -gt [uint32]::MaxValue) { throw 'Invalid import name RVA.' }
                $symbols += CString $data ((RvaOffset ([uint32]$thunk) $sections $headerSize) + 2)
            }
            $thunkOffset += 8
        }
        $imports[$name] = $symbols
        $descriptor += 20
    }
    $debugRva = U32 $data ($optional + 160)
    $debugSize = U32 $data ($optional + 164)
    if ($debugRva -eq 0 -or $debugSize % 28 -ne 0) { throw 'Missing or malformed debug directory.' }
    $debug = RvaOffset $debugRva $sections $headerSize
    $identity = $null
    for ($i = 0; $i -lt $debugSize / 28; ++$i) {
        $entry = $debug + 28 * $i
        if ((U32 $data ($entry + 12)) -ne 2) { continue }
        $size = U32 $data ($entry + 16)
        $raw = U32 $data ($entry + 24)
        Range $data $raw $size
        if ($size -lt 25 -or (U32 $data $raw) -ne 0x53445352) { throw 'Expected RSDS CodeView data.' }
        if ($null -ne $identity) { throw 'Multiple CodeView identities.' }
        $identity = [pscustomobject]@{
            Guid = GuidAt $data ($raw + 4)
            Age = U32 $data ($raw + 20)
            PdbPath = CString $data ($raw + 24)
        }
    }
    if ($null -eq $identity) { throw 'Missing RSDS identity.' }
    return [pscustomobject]@{
        IsDll = ((U16 $data ($pe + 22)) -band 0x2000) -ne 0
        Exports = $exports
        ExportFunctionCount = $functionCount
        Imports = $imports
        CodeView = $identity
    }
}

function Read-PdbIdentity([string]$Path) {
    $data = [IO.File]::ReadAllBytes($Path)
    Range $data 0 56
    $signature = "Microsoft C/C++ MSF 7.00`r`n" + [char]26 + "DS" + [char]0 + [char]0 + [char]0
    if ([Text.Encoding]::ASCII.GetString($data, 0, 32) -ne $signature) {
        throw "$Path is not an MSF 7.0 PDB."
    }
    $blockSize = U32 $data 32
    if ($blockSize -lt 512 -or $blockSize -gt 65536 -or
        ($blockSize -band ($blockSize - 1)) -ne 0) { throw 'Invalid PDB block size.' }
    if ([long](U32 $data 40) * $blockSize -ne $data.Length) { throw 'Invalid PDB block count.' }
    $directorySize = U32 $data 44
    if ($directorySize -lt 4 -or $directorySize -gt $data.Length) { throw 'Invalid PDB directory size.' }
    $directory = New-Object byte[] $directorySize
    $map = [long](U32 $data 52) * $blockSize
    $blockCount = [int][Math]::Ceiling([double]$directorySize / $blockSize)
    for ($i = 0; $i -lt $blockCount; ++$i) {
        $raw = [long](U32 $data ($map + 4 * $i)) * $blockSize
        $size = [Math]::Min([long]$blockSize, [long]$directorySize - [long]$i * $blockSize)
        Range $data $raw $size
        [Array]::Copy($data, $raw, $directory, [long]$i * $blockSize, $size)
    }
    $streamCount = U32 $directory 0
    if ($streamCount -lt 2) { throw 'Missing PDB information stream.' }
    Range $directory 4 ([long]$streamCount * 4)
    $blockList = 4 + [long]$streamCount * 4
    $info = $null
    for ($stream = 0; $stream -lt 2; ++$stream) {
        $size = U32 $directory (4 + 4 * $stream)
        if ($size -eq [uint32]::MaxValue) {
            if ($stream -eq 1) { throw 'Missing PDB information stream.' }
            continue
        }
        if ($size -gt $data.Length) { throw 'Invalid PDB stream size.' }
        $count = [int][Math]::Ceiling([double]$size / $blockSize)
        Range $directory $blockList ([long]$count * 4)
        if ($stream -eq 1) {
            if ($size -lt 28) { throw 'Truncated PDB information stream.' }
            $info = New-Object byte[] $size
            for ($i = 0; $i -lt $count; ++$i) {
                $raw = [long](U32 $directory ($blockList + 4 * $i)) * $blockSize
                $copySize = [Math]::Min([long]$blockSize, [long]$size - [long]$i * $blockSize)
                Range $data $raw $copySize
                [Array]::Copy($data, $raw, $info, [long]$i * $blockSize, $copySize)
            }
        }
        $blockList += [long]$count * 4
    }
    return [pscustomobject]@{ Guid = GuidAt $info 12; Age = U32 $info 8 }
}

function Verify-Pair([string]$ImageName, [bool]$Dll) {
    $imagePath = Join-Path $OutputDirectory $ImageName
    $pdbName = [IO.Path]::GetFileNameWithoutExtension($ImageName) + '.pdb'
    $pdbPath = Join-Path $OutputDirectory $pdbName
    $pe = Read-PE $imagePath
    $pdb = Read-PdbIdentity $pdbPath
    if ($pe.IsDll -ne $Dll) { throw "Incorrect DLL characteristic: $ImageName." }
    if ($pe.CodeView.Guid -ne $pdb.Guid -or $pe.CodeView.Age -ne $pdb.Age -or
        (Split-Path -Leaf $pe.CodeView.PdbPath) -ne $pdbName) {
        throw "DLL/EXE and PDB identity mismatch: $ImageName."
    }
    $allowed = @('KERNEL32.dll', 'MSVCR100.dll', 'MSVCP100.dll')
    $sdkLoggingImports = @()
    if ($Dll) {
        $allowed += @('KenshiLib.dll', 'USER32.dll')
        if ($pe.ExportFunctionCount -ne 1 -or $pe.Exports.Count -ne 1 -or
            $pe.Exports[0] -cne '?startPlugin@@YAXXZ') {
            throw 'Expected exactly the global C++ export ?startPlugin@@YAXXZ.'
        }
        if (-not $pe.Imports.ContainsKey('KenshiLib.dll')) { throw 'Missing actual KenshiLib import.' }
        $sdkSymbols = @($pe.Imports['KenshiLib.dll'] | Sort-Object)
        $sdkLoggingImports = @($pe.Imports['KenshiLib.dll'])
        if ($sdkSymbols.Count -ne 2 -or $sdkSymbols[0] -cne '?DebugLog@@YAXPEBD@Z' -or
            $sdkSymbols[1] -cne '?ErrorLog@@YAXPEBD@Z') {
            throw 'Expected only the pinned SDK public char-pointer logging imports; no Init/hooks.'
        }
    } elseif ($pe.ExportFunctionCount -ne 0) {
        throw 'Unexpected test executable export.'
    }
    $actualImports = @($pe.Imports.Keys | Sort-Object)
    if ($actualImports.Count -ne $allowed.Count) { throw "Unexpected import count: $ImageName." }
    foreach ($name in $actualImports) {
        if ($name -notin $allowed) { throw "Unexpected import $name in $ImageName." }
    }
    $result = [pscustomobject]@{
        image = $ImageName
        architecture = 'PE32+ AMD64 (0x8664)'
        linker = '10.0 (VC100)'
        exports = @($pe.Exports)
        imports = $actualImports
        sdkLoggingImports = $sdkLoggingImports
        pdb = $pdbName
        pdbGuid = $pdb.Guid
        pdbAge = $pdb.Age
        imageSha256 = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant()
        pdbSha256 = (Get-FileHash -LiteralPath $pdbPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Write-Host "$ImageName : x64; matched PDB $($pdb.Guid), age $($pdb.Age)"
    return $result
}

$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$plugin = Verify-Pair 'ProjectCarsRemake.dll' $true
$tests = Verify-Pair 'CatalogTests.exe' $false
$repo = Split-Path -Parent $PSScriptRoot
$catalogHash = (Get-FileHash -LiteralPath "$repo\vehicles.catalog" -Algorithm SHA256).Hash.ToLowerInvariant()
$stagedHash = (Get-FileHash -LiteralPath "$OutputDirectory\vehicles.catalog" -Algorithm SHA256).Hash.ToLowerInvariant()
if ($catalogHash -ne $stagedHash) { throw 'Staged catalog does not match the canonical catalog.' }
return [pscustomobject]@{
    schemaVersion = 1
    verifiedUtc = [DateTime]::UtcNow.ToString('o')
    compiler = 'Microsoft VC100 16.00.30319.01; _MSC_VER 1600; /MD /O2 /GL /Zi /W4 /WX'
    kenshiLibVersion = '0.5.0'
    kenshiLibSourceCommit = '11cd65d557dea76c6540239acf467e0e06e09cf7'
    plugin = $plugin
    tests = $tests
    catalogSha256 = $catalogHash
    liveHostVerified = $false
    gameplayImplemented = $false
}
