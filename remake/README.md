# Original source core

**Development only: no drivable gameplay yet.** This is new, original
GPL-3.0-only C++ source, not recovery of the unavailable source for the legacy
`ProjectCars.dll` 0.8.16. It implements a catalog model/parser, a standalone
vehicle domain core and an RE_Kenshi bootstrap that loads and validates a
DLL-local `vehicles.catalog`. The domain core has mechanical readiness, atomic
part-count exchanges, typed litres fuel accounting and remake-only vehicle
serialization. The bootstrap still only validates the catalog: unused domain
functions are not wired into startup or linked into the development DLL. It
does not replace the legacy DLL, repair its missing Corresponding Source, or
constitute a deployment-ready mod.

There are no gameplay hooks, `KenshiLib::Init` calls, FCS lookups, vehicle world
objects, live inventory/save access, fitting UI, driving, save migration or
playable features here. Scout and hauler are catalog declarations, not verified
native buildings or finished deployment assets. The parser preserves the
existing `10-Project Cars.mod` binding as an exact string; it does not establish
that a live FCS object exists.

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
and `link.exe` directly, runs the standalone catalog and vehicle tests against
both the canonical and staged catalog, and inspects the DLL/EXE and their PDBs.
It uses `/EHsc /MD /O2 /GL /Zi /W4 /WX`, then `/LTCG /DEBUG /INCREMENTAL:NO /OPT:REF
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
| `CatalogTests.exe`, `CatalogTests.pdb` | Combined catalog/vehicle native tests; no SDK linkage or host loading |
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
returns a complete model only after all checks succeed. The native suite also exercises malformed fields, duplicates, unresolved
forward references, numeric boundaries, locale changes, BOM/quotes/comments
and I/O errors. Shared text helpers retain these diagnostics for both formats.
`ValidateCatalog` applies the same definition/reference rules to programmatic
catalogs, since the public catalog value can be edited after parsing.

## Standalone vehicle domain

`src\vehicle.h` exposes value-state APIs, not native Kenshi objects:

| API | Contract |
| --- | --- |
| `Vehicle(catalog, frameId)` | Validates and owns a deep catalog snapshot; starts as a bare frame with no installed parts or fuel |
| `Installed`, `FrameId`, `FuelState`, `TankCapacityLitres` | Read-only IDs and values; capacity comes only from the installed `tank` role |
| `Readiness` | Mechanical/configuration completeness with explicit missing and unsupported role-slot lists |
| `PartStock(catalog, capacity, counts)` | Explicit standalone stock, with unsigned 32-bit per-part counts and total part-count capacity |
| `Fit`, `Remove`, `Replace` | All-or-nothing exchanges of one part per slot; failures throw a reason-coded `VehicleError` |
| `Refuel`, `Drain`, `Consume` | Explicit typed litres accounting; return actual added/removed/consumed quantities |

No pointers into a caller's catalog are retained. Mutating or destroying that
catalog cannot invalidate an existing vehicle. Copies are independent, and
assignment stages a complete copy before swapping. Invalid programmatic
catalogs are rejected, rather than relying on callers to have parsed them.

**Bounded readiness profile.** All three canonical frames declare
`seat_driver` (seat), `engine` (engine), `tank` (tank), and `wheel_fl`,
`wheel_fr`, `wheel_rl`, `wheel_rr` (wheels). These seven exact slots must be
occupied, and every catalog-compatible part for each role must have that
role's kind. Missing or mixed-kind role declarations report unsupported
slots, never readiness. Optional passenger-seat, armor, headlights and cargo
slots do not gate this core's readiness. Other catalog-declared fittings can
be stored, but no extra role, seat layout, track/wheel conflict, attachment
mapping or legacy 0.8.16 rule is inferred.

A ready configuration can have an empty tank and no native building binding.
An installed driver seat is not a boarded driver. Readiness says nothing
about live FCS records, actors, physics, movement, collision or playability.

**Atomic stock exchanges.** Compatibility is exactly the selected frame's
catalog slot allowlist. `Fit` requires an empty slot, `Remove`/`Replace` require
an occupied slot, and replacing with the same part is an explicit no-op error.
Fitting consumes one count; removal returns one; replacement consumes one
new part and returns one old part. Zero-count entries are normalized away.
Unknown references, insufficient stock, final capacity failure and per-part
or total count overflow leave both vehicle and stock unchanged. A replacement
is validated as a complete exchange: a full stock can accept the old part
when consuming the new part frees the required unit of capacity.

This is a single-threaded **count/capacity test model**, not Kenshi inventory
grid, stack, allocation, reservation or ownership semantics. Stock is neither
serialized with vehicles nor transferred into the game. Future native adapters
must perform authoritative live transfers; these tests are not proof those
transfers work.

**Fuel rules.** Empty fuel is exactly `none 0`; a nonempty tank carries either
gasoline or diesel. Tanks themselves retain catalog fuel `none`, meaning no
fixed gasoline/diesel restriction, not a third usable fuel. The first nonzero
refuel explicitly selects the stored type. Refueling requires the installed
`tank` role, must match any installed `engine` and existing nonempty fuel, and
cannot exceed tank capacity. A tank may be refueled while its engine is absent.
Engine removal preserves tank contents; fitting/replacing an incompatible
engine, removing a fueled tank, or downsizing below stored volume is rejected
before either stock or vehicle changes. `Drain(litres)` is the only explicit
non-consumption fuel removal; it reports the removed type/quantity and cannot
remove more than exists. Draining/exhausting the tank resets it to `none 0`.
There is no implicit mixing, conversion, clamping or excess-fuel discard.

`Consume(simulationSeconds)` requires an installed engine and uses its
`burnLitresPerSimSecond`, not wall-clock time or an invented can-item mechanic.
It reports actual `consumedLitres`, bounded `usableSimSeconds` and `exhausted`.
Zero time consumes nothing; an empty tank yields zero usable time; exhaustion
uses only the remaining fuel and its usable time. Consumption does not require
mechanical readiness or enable motion; a future driving adapter must apply its
own host/driver/readiness gates before requesting a step.

Every quantity/time input must be finite and nonnegative. Volumes use
double-precision litres, with reported quantities reflecting actual stored
volume changes. Positive operations too small to change the stored value,
underflowed burn amounts, or unrepresentable positive exhaustion time fail
explicitly without mutation. Large finite steps can exhaust normally even
when the unconstrained time-times-burn product would overflow.

## Remake-only vehicle serialization

`src\vehicle_io.h` provides `WriteVehicle(output, vehicle, destination)` and
`ParseVehicle(input, source, catalog)` on **caller-owned streams**. No game
save path, filesystem installation or native save-format detection is chosen.
The unmistakable namespace and independent version are:

```text
projectcars-remake-vehicle 1
frame small_frame
installed engine small_gas_engine
installed tank tank_40
fuel gasoline binary64:4029000000000000
end
```

The writer emits the header, exact frame ID, installed slot/part IDs sorted by
ASCII slot ID, one fuel record and an end marker. Fuel volume is an exact
IEEE-754 binary64 bit pattern: `binary64:` plus 16 lowercase hexadecimal
digits, most significant digit first. The example holds **12.5 litres**; empty
fuel is `none binary64:0000000000000000`. This is a scalar volume encoding,
not a pointer or native identity. The real VC100 CRT loses a bit on some
17-digit decimal round trips, so saves deliberately do not use its decimal
conversion. All finite nonnegative doubles, including subnormals, survive
exactly; NaN, infinity, negative values and negative zero are rejected.

Output uses no BOM and LF text, independent of the destination stream's
locale/formatting. Use binary-mode file streams if exact LF bytes are needed
on Windows. Capacity, burn rate, stock, native pointers/handles and FCS objects
are not persisted; definitions are validated against the supplied catalog on load.

The first directive must be the exact version-1 header. Frame and fuel are
mandatory and unique, installed slot IDs cannot repeat, and `end` is mandatory.
Records after the header may be reordered before `end`; only blank/comment
lines may follow it. Catalog-style ASCII whitespace, CRLF, a first-line UTF-8
BOM and `#` comments are accepted, but quoted fields are not. Shared helpers
enforce identifier/token rules and escaped diagnostics without changing IDs.
The catalog's separate decimal numeric syntax is unchanged.
Unknown directives, versions, IDs, duplicate fields, incompatible parts,
noncanonical fuel types/empty state and invalid tank/engine fuel state fail
with source, physical line and original context. Load constructs and validates
the complete state before returning it, so failed assignment preserves an
existing vehicle and never changes inventory.

Read, write and flush failures are explicit `VehicleSaveError`s. The writer
stages the serialization before touching the stream, but a failing output
stream may already contain partial bytes; any atomic file replacement belongs
to the caller's future persistence adapter. This format has **no legacy native
save compatibility**, migration or live Kenshi save integration.

The existing `CatalogTests.exe` runner covers every canonical catalog field,
bare/ready/incomplete/unsupported roles, ownership/copies, stock quantities,
full-capacity replacement, rollback and count overflow, fuel preservation,
fuel/time/precision boundaries, deterministic output, exact numeric round
trips, invalid saved-state rollback and read/write/flush failures. Artificial
catalogs stress validation/numeric limits only; they are not additions to the
canonical catalog or assertions about legacy gameplay.

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

## Remaining native work

This stops at source-remake milestone 2, the standalone vehicle domain.
Subsequent separate units are native lifecycle/typed handles/reset,
workshop/live inventory/attachments, driving/fuel integration, then gated
playable deployment. Passengers, tracks, oil and combat remain unimplemented.
No live-game startup or gameplay gate has passed.
