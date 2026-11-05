# Original source foundation

**Development only: no drivable gameplay yet.** This is new, original
GPL-3.0-only C++ source, not recovery of the unavailable source for the legacy
`ProjectCars.dll` 0.8.16. It implements a catalog model/parser and an RE_Kenshi
bootstrap that loads and validates a DLL-local `vehicles.catalog`. It does not
replace the legacy DLL, repair its missing Corresponding Source, or constitute
a deployment-ready mod.

There are no gameplay hooks, `KenshiLib::Init` calls, FCS lookups, vehicle world
objects, fitting UI, driving, save migration or playable features here. Scout
and hauler are catalog declarations, not verified native buildings or finished
deployment assets. The parser preserves the existing `10-Project Cars.mod`
binding as an exact string; it does not establish that a live FCS object exists.

## Build and check

From the repository root, in **x64 Windows PowerShell 5.1**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\remake\build.ps1
```

The default external dependency root is
`$env:USERPROFILE\.copilot\toolchains\kenshi-v100`. To use another root with the
same extracted layout:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\remake\build.ps1 -ToolchainRoot 'C:\toolchains\kenshi-v100'
```

The script verifies dependency hashes, invokes the actual VC100 x64 `cl.exe`
and `link.exe` directly, runs the standalone catalog tests against both the
canonical and staged catalog, and inspects the DLL/EXE and their PDBs. It uses
`/EHsc /MD /O2 /GL /Zi /W4 /WX`, then `/LTCG /DEBUG /INCREMENTAL:NO /OPT:REF
/OPT:ICF`. Source compilation rejects a compiler other than `_MSC_VER == 1600`,
a non-x64 target, and a non-Release/non-DLL CRT. Errors fail the build rather
than selecting a different compiler, SDK, catalog or legacy binary.

Only process-local `PATH`, `INCLUDE`, `LIB` and compiler/linker option variables
are changed, and they are restored. No registered Visual Studio/MSBuild v100
installation, modern Visual Studio, CMake or Ninja is assumed. A modern
compiler is **not** a substitute for the pinned VC100 ABI.

All generated files stay under the ignored `out\remake` directory:

| Output | Purpose |
| --- | --- |
| `ProjectCarsRemake.dll`, `ProjectCarsRemake.pdb` | Development-only x64 plugin and its matching debug symbols |
| `CatalogTests.exe`, `CatalogTests.pdb` | Standalone native core tests; no SDK linkage or host loading |
| `vehicles.catalog` | Byte-identical copy of the canonical catalog beside the DLL |
| `RE_Kenshi.json.example` | Inactive opt-in development registration example |
| `artifact-report.json` | Actual PE architecture, exports/imports, paired GUID+age and SHA256 identities |
| `obj`, `.lib`, `.exp` | Compiler/linker intermediates |

To inspect existing output again without executing the plugin:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\remake\verify-artifacts.ps1
```

The verifier reads the actual PE export/import/RSDS records and MSF 7.0 PDB
information stream, checking **both GUID and age**. It requires exactly
`?startPlugin@@YAXXZ`, the VC100 CRT imports and only the SDK's public
`DebugLog(const char*)` / `ErrorLog(const char*)` imports. It rejects extra SDK
initialization/hooks, engine libraries, a different architecture or unmatched
symbols. It does not load `KenshiLib.dll` or `ProjectCarsRemake.dll`.

This is a repeatable source build, not a claim of byte-for-byte deterministic
DLL/PDB output: linker timestamps and PDB identity/age can change on rebuild.
Keep each newly generated DLL and its PDB together.

## External dependency setup

`dependencies.json` pins actual build inputs and the native-test CRT baseline.
Keep these dependencies **outside Git** and retain their original licenses.
They are already available at the default root on the development machine.
The build never installs software or changes global compiler settings.

**Microsoft compiler and Windows SDK.** Obtain the x64 Windows SDK 7.1 ISO from
the Microsoft URL in the pin file. Compare its SHA1 with Microsoft's official
[download/hash page](https://www.microsoft.com/en-us/download/details.aspx?id=8442):

```text
GRMSDKX_EN_DVD.iso
SHA1 9203529f5f70d556a60c37f118a95214e6d10b5a
VC100 compiler 16.00.30319.01, native x64
```

Mount the verified ISO using Windows. With `$media` set to its actual drive
letter, administratively extract these four MSI components into the same
user-scoped directory. `/a` extracts administrative images; do not run a normal
global SDK/Visual Studio installation or change environment/registry settings.
Windows Installer may require permission to perform the extraction.

```powershell
$root = Join-Path $env:USERPROFILE '.copilot\toolchains\kenshi-v100'
$media = 'E:'
$target = Join-Path $root 'windows-sdk-7.1'
foreach ($component in @('vc_stdx86', 'vc_stdamd64', 'WinSDKBuild_amd64', 'WinSDKTools_amd64')) {
    $msi = "$media\Setup\$component\$component.msi"
    $process = Start-Process -FilePath "$env:WINDIR\System32\msiexec.exe" -ArgumentList @(
        '/a', "`"$msi`"", '/qn', "TARGETDIR=`"$target`""
    ) -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Extraction failed: $component ($($process.ExitCode))" }
}
```

Unmount the ISO afterward. The required resulting paths, relative to `$root`,
are:

```text
windows-sdk-7.1\Program Files(64)\Microsoft Visual Studio 10.0\VC\bin\amd64
windows-sdk-7.1\Program Files\Microsoft Visual Studio 10.0\VC\include
windows-sdk-7.1\Program Files(64)\Microsoft Visual Studio 10.0\VC\lib\amd64
windows-sdk-7.1\Program Files\Microsoft SDKs\Windows\v7.1\Include
windows-sdk-7.1\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64
windows-sdk-7.1\Program Files\Microsoft SDKs\Windows\v7.1\Bin\x64
```

The native-test execution baseline uses Microsoft's x64 VC100 CRT
`msvcr100.dll` and `msvcp100.dll`, version **10.00.40219.325**, already installed
in Windows `System32`; their exact hashes are pinned. If absent, obtain the
official [Visual C++ 2010 SP1 redistributable MFC security update](https://www.microsoft.com/en-us/download/details.aspx?id=26999)
with appropriate installation approval. The build does not install a CRT,
copy system DLLs into the mod, or silently accept a different baseline.
Review and deliberately update the pins if adopting a serviced CRT.

**KenshiLib.** Clone the public SDK outside the repository with CRLF checkout
for the two pinned public headers, then detach at the exact commit:

```powershell
git -c core.autocrlf=true clone https://github.com/BFrizzleFoShizzle/KenshiLib.git "$root\KenshiLib"
git -C "$root\KenshiLib" -c core.autocrlf=true checkout --detach 11cd65d557dea76c6540239acf467e0e06e09cf7
```

Obtain
`https://github.com/BFrizzleFoShizzle/KenshiLib/releases/download/v0.5.0/KenshiLib_v0.5.0.zip`
and verify its SHA256:

```text
ad4534e92a3a244cf25d724d6a695d42ef8770e4b7a1f91ed53affcac171d0c4
```

Extract it into `$root\runtime-0.5.0`, retaining its notices and the complete
DLL/import-library/PDB/RVA unit. Copy its `KenshiLib.lib` into
`$root\KenshiLib\Libraries\KenshiLib.lib`. This foundation needs only
`Include\Debug.h`, `Include\Defines.h` and that import library; **no Ogre, MyGUI
or Boost libraries/headers are linked or required**.

The source pin is **KenshiLib 0.5.0**
`11cd65d557dea76c6540239acf467e0e06e09cf7`, matching the public RE_Kenshi
contract at `c8dfbc9621b9263863722852f819c4c0f440475a`. Do not mix v0.5.1's
larger ordered RVA table with the 0.5.0 runtime/RVA pack or bypass SDK size
checks. The standalone core tests deliberately do not simulate SDK/game
initialization.

Individual file pins are raw-byte SHA256 values. Header-tree digests are the
SHA256 of UTF-8, no-BOM manifest rows:
`lowercase relative Windows path<TAB>lowercase file SHA256<LF>`, sorted with
ordinal comparison and including a final LF. File counts are checked too.
Microsoft media identity and the complete compiler/SDK header trees are pinned
alongside the actual tools, link inputs, public SDK headers and runtime unit.

## Catalog contract

The unchanged root catalog contains **11 parts, 3 frames, 32 slots and one
building mapping**. Tests assert every declaration, numeric field, compatible
part order and the exact `10-Project Cars.mod` string, not just the counts.

```text
catalog 1
part ID KIND FUEL CAPACITY_LITRES BURN_LITRES_PER_SIM_SECOND
frame ID BUILD_WORK DISASSEMBLY_WORK SEATS MAX_MPS ACCEL_MPS2 TURN_RAD_S
slot FRAME_ID SLOT_ID comma-separated-PART_IDS
building FRAME_ID "exact FCS binding"
```

Directive/enum spelling and identifiers are case-sensitive. IDs contain one or
more ASCII letters, digits, underscores or hyphens. Blank lines, ASCII
whitespace, CRLF, an optional first-line UTF-8 BOM and `#` comments outside
quotes are supported. Only building bindings are quoted; their content is
preserved without trimming or escape interpretation, including spaces and
`#`. Empty bindings and control characters are rejected.

The first non-comment directive must be `catalog 1`. Duplicate headers, parts,
frames, per-frame slot IDs, compatible IDs within a slot, per-frame building
mappings and repeated exact building bindings are rejected. Forward references
are allowed but must resolve after parsing; no unknown frames/parts are
discarded. Part and frame IDs are separate namespaces.

Numbers use locale-independent decimal/scientific syntax. NaN, infinity,
overflow, underflow to zero, trailing junk and hexadecimal numbers are errors.
Capacity, burn and work are nonnegative. Engines require gasoline/diesel,
zero capacity and positive burn; tanks require `none`, positive capacity and
zero burn; wheels/upgrades/seats require `none` and zero capacity/burn. Seats
are unsigned decimal integers in `0..4294967295`; speed, acceleration and turn
rate must be positive. Zero work/seats is permitted; no gameplay tuning caps
or seat-mount/fitting rules are inferred. A header-only catalog is valid syntax,
but bootstrap configuration requires at least one part and frame.

Failures identify the source, physical line, field/reference and original line
context. Control bytes are escaped in human-readable diagnostics. Parsing
returns a complete model only after all checks succeed. The native suite also
exercises malformed fields, duplicates, unresolved forward references,
numeric boundaries, locale changes, BOM/quotes/comments and I/O errors.

## Host boundary and development registration

The sole exported entry is the **global C++** `void startPlugin()`, decorated
`?startPlugin@@YAXXZ`: not `extern "C"`, not namespaced and not `dllStartPlugin`.
The host owns SDK initialization before normal active-mod plugin discovery.
Bootstrap uses the pinned SDK's global public logging functions, loads the
catalog beside its own DLL through a Unicode Windows module path, and logs
foundation-only counts after complete validation.

Startup/configuration failures use `ErrorLog`; C++ exceptions are contained at
the exported host boundary. If the SDK logger itself throws, a debugger error
and Windows error dialog report the failure without escaping that boundary.
No default catalog, legacy-DLL fallback or gameplay-success message is emitted.

`development\RE_Kenshi.json.example` is opt-in reference data only. The build
leaves it with the `.example` suffix and **does not change root
`RE_Kenshi.json`, activate the new DLL, bundle the SDK, or copy anything into
Kenshi**. A later, separately approved host test must use an isolated
development package, keep the DLL/catalog together, and select the exact SDK
runtime/RVA unit for a supported executable.

The 0.5.0 SDK identifies Steam 1.0.65 by MD5
`df4a5a7ef8a29deb24b70e7b7f4a222a` and GOG 1.0.65 by
`213349bf76a0a758067f9ed0aef2ab01`; these are compatibility fingerprints, not
security hashes. RE_Kenshi has a compatible-executable route for 1.0.68, not
arbitrary-build support. **Live game startup and gameplay remain unverified.**
Legacy native saves are undocumented, and the V8 audio rights remain
unresolved; this foundation promises neither save compatibility nor asset
redistribution clearance. See [LICENSING.md](../LICENSING.md).
