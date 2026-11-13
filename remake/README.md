# Original source core

**Historical independent remake:** development now starts from the separately
preserved [recovered original 0.8.19 source](../original/README.md).
This tree and its tests remain available; its incomplete host acceptance gate
has not been waived or represented as acceptance of the recovered runtime.

**Development only: no drivable gameplay yet.** This is new, original
GPL-3.0-only C++ source, not recovery of the unavailable source for the legacy
`ProjectCars.dll` 0.8.16. It implements a catalog model/parser, a standalone
vehicle domain core and an RE_Kenshi bootstrap that loads and validates a
DLL-local `vehicles.catalog`. The domain core has mechanical readiness, atomic
part-count exchanges, typed litres fuel accounting and remake-only vehicle
serialization. By default the bootstrap still only validates the catalog.
Phase **3A** adds an explicitly opt-in, observational world-tick/reset/clear
boundary, disabled by default. Unused vehicle domain functions are not wired
into startup or linked into the development DLL. It
does not replace the legacy DLL, repair its missing Corresponding Source, or
constitute a deployment-ready mod.

There are no gameplay hooks, `KenshiLib::Init` calls, FCS lookups, vehicle world
objects, live inventory/save access, fitting UI, driving, save migration or
playable features here. The optional lifecycle hooks observe callback
ownership and remake-owned epochs/observation tickets, not game-object fields.
Scout and hauler are catalog declarations, not verified
native buildings or finished deployment assets. The parser preserves the
existing `10-Project Cars.mod` binding as an exact string; it does not establish
that a live FCS object exists.

**Upstream safety authority:** RE_Kenshi and KenshiLib's documented safety
and API contracts for the supported versions govern this plugin. A proposed
departure requires explicit user approval after upstream maintainer/expert
consultation. Passing tests does not authorize bypassing those contracts;
unknown contracts must be surfaced rather than guessed.

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
and `link.exe` directly, runs the standalone catalog, vehicle and lifecycle tests against
both the canonical and staged catalog, and inspects the DLL/EXE and their PDBs.
It uses `/EHsc /MD /O2 /GL /Zi /W4 /WX`, then `/LTCG /DEBUG /INCREMENTAL:NO /OPT:REF
/OPT:ICF`. `UNICODE` / `_UNICODE` match the pinned public headers' Win32
wide-string contracts; those headers are not edited. Source compilation
rejects a compiler other than `_MSC_VER == 1600`,
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
| `CatalogTests.exe`, `CatalogTests.pdb` | Combined catalog/vehicle/lifecycle native tests; no SDK linkage or host loading |
| `vehicles.catalog` | Byte-identical copy of the canonical catalog beside the DLL |
| `RE_Kenshi.json.example` | Inactive opt-in development registration example |
| `ProjectCarsRemake.lifecycle.cfg.example` | Inactive configuration example; `observe disabled` |
| `artifact-report.json` | Actual PE architecture, exports/imports, paired GUID+age and SHA256 identities |
| `obj`, `.lib`, `.exp` | Compiler/linker intermediates |

To inspect existing output again without executing the plugin:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\remake\verify-artifacts.ps1
```

The default output directory is resolved relative to the verifier script,
independent of the current directory. Use `-OutputDirectory` to inspect another
inactive build directory; an explicitly empty argument is rejected.

The verifier reads the actual PE export/import/RSDS records and MSF 7.0 PDB
information stream, checking **both GUID and age**. It requires exactly
`?startPlugin@@YAXXZ`, the VC100 CRT imports and **exactly seven** SDK symbols:
public `DebugLog(const char*)` / `ErrorLog(const char*)`, `GetRealAddress`,
`AddHook`, and the three nonvirtual world stubs documented below. It rejects
any other SDK symbols, initialization/queued-hook/version-override APIs,
engine libraries, a different architecture or unmatched symbols. Both
inactive examples must match their source bytes. An active `RE_Kenshi.json`
or `ProjectCarsRemake.lifecycle.cfg` in developer output fails the build/check;
neither is created or silently removed. The verifier does not load
`KenshiLib.dll` or `ProjectCarsRemake.dll`.

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
`$root\KenshiLib\Libraries\KenshiLib.lib`.

Phase 3A also compiles the SDK's unmodified public `core\Functions.h` and
`kenshi\GameWorld.h`. Their transitive declarations need the SDK's bundled
Ogre headers and Boost **1.60.0** at
`$root\KenshiLib_Examples_deps\boost_1_60_0`. The already approved example
dependencies checkout is pinned at
`b566d74bf3d74629cc2fb632a97595b8202993f1` from
`https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps`. Its setup
script changes global environment variables: **do not run it for this build**.
The build supplies include paths only to its own process.

The complete 857-file SDK include unit and 11,013-file Boost header tree are
hash-pinned using the same manifest format as the compiler headers. The
specific SDK hook/logging implementations, GameWorld assembly and ordered symbol-table
sources are also pinned as evidence. These wider header pins do not allow
wider imports: **no Ogre, MyGUI, Boost or third-party hook library is linked**.
Only the existing KenshiLib, Windows kernel/user and VC100 CRT link inputs
are used. No engine binaries or replacement SDK are staged.

The source pin is **KenshiLib 0.5.0**
`11cd65d557dea76c6540239acf467e0e06e09cf7`, matching the public RE_Kenshi
contract at `c8dfbc9621b9263863722852f819c4c0f440475a`. Do not mix v0.5.1's
larger ordered RVA table with the 0.5.0 runtime/RVA pack or bypass SDK size
checks. The standalone core tests use fake typed originals/installers and
Win32 thread fixtures only. They do not initialize, link or load the real SDK.

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

Lifecycle coverage adds strict present/missing/unreadable configuration,
inactive staged example parsing, immediate callbacks during installation,
teardown still in flight when installation finishes, preparation and partial
AddHook failures (including exceptions and missing originals), duplicate
installation refusal, exact typed pass-through arguments and chaining,
bounded queues/rejections, concurrent enqueue, owner changes, nested and
overlapping teardown, in-flight ticks crossing reset, original exceptions,
diagnostic failures, bounded reset logging and epoch exhaustion. A child
test process verifies a missing trampoline terminates rather than returning
fake success. None of these fixtures loads a game or the real SDK; they are
unit evidence, **not Kenshi ABI, scheduling or coexistence acceptance**.

## Host boundary and development registration

The sole exported entry is the **global C++** `void startPlugin()`, decorated
`?startPlugin@@YAXXZ`: not `extern "C"`, not namespaced and not `dllStartPlugin`.
The host owns SDK initialization before normal active-mod plugin discovery.
Bootstrap uses the pinned SDK's global public logging functions, loads the
catalog beside its own DLL through a Unicode Windows module path, and logs
foundation-only counts after complete validation.
The bootstrap is claimed once, including failed starts: repeated
`startPlugin()` calls report a bounded debugger diagnostic and cannot retry
a partially started adapter or hot-replace its context.

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

## Phase 3A: opt-in observational lifecycle

The optional file is **`ProjectCarsRemake.lifecycle.cfg` beside the new DLL**,
not the active root `RE_Kenshi.json` or the legacy controls file. Missing only
the optional file (`ERROR_FILE_NOT_FOUND`) explicitly reports **DISABLED**.
Other open/read/close errors, an inaccessible directory, sharing violations,
oversized files and malformed present configuration explicitly fail startup;
none silently selects disabled/enabled or a default configuration. Reading is
bounded to **4096 bytes**, with read-only `FILE_SHARE_READ` access. A path
observed as present cannot fall back to disabled if the subsequent open fails.

The strict text format requires exactly one header and one observe directive:

```text
projectcars-remake-lifecycle 1
observe disabled
```

ASCII whitespace, blank lines, `#` comments and an initial UTF-8 BOM are
supported. Quoted tokens, unknown/duplicate records and other versions or
values are errors. Only an explicitly approved later test should copy the
inactive `.example` into a separate test package, remove its suffix and change
`disabled` to `enabled`. Merely rebuilding never activates hooks. Configuration
is read once; changes require complete host exit/restart, not hot reload.

### Verified public targets and ABI

Evidence is from KenshiLib **0.5.0** source commit
`11cd65d557dea76c6540239acf467e0e06e09cf7`, not guessed addresses or the legacy
ProjectCars DLL/PDB:

| Public SDK declaration | Stored Win64 trampoline / detour type | Pinned declaration and assembly evidence |
| --- | --- | --- |
| `void GameWorld::_NV_mainLoop_GPUSensitiveStuff(float time)` | `void (__cdecl *)(GameWorld*, float)` | `Include\kenshi\GameWorld.h:195-196`; `Source\kenshi\functions\GameWorld.inc:338-344`, alias of slot `GameWorld + 66` |
| `void GameWorld::resetGame()` | `void (__cdecl *)(GameWorld*)` | `Include\kenshi\GameWorld.h:72`; `Source\kenshi\functions\GameWorld.inc:33-36`, slot `GameWorld + 5` |
| `void GameWorld::_clearAndDestroyGameWorldStuff()` | `void (__cdecl *)(GameWorld*)` | `Include\kenshi\GameWorld.h:73`; `Source\kenshi\functions\GameWorld.inc:38-41`, slot `GameWorld + 6` |

The declaration/stub sources are available at the pinned
[GameWorld header](https://github.com/BFrizzleFoShizzle/KenshiLib/blob/11cd65d557dea76c6540239acf467e0e06e09cf7/Include/kenshi/GameWorld.h)
and [assembly](https://github.com/BFrizzleFoShizzle/KenshiLib/blob/11cd65d557dea76c6540239acf467e0e06e09cf7/Source/kenshi/functions/GameWorld.inc).
`Source\kenshi\symbols.asm:177,903-918` establishes the ordered GameWorld
base/table and stub-size invariants. Slot numbers are evidence only; source
contains **no hardcoded game addresses or RVA indexing**.

Only the public nonvirtual member stubs are passed to the SDK's own
`GetRealAddress` convenience template (`Include\core\Functions.h:8-17`).
The virtual `mainLoop_GPUSensitiveStuff` member pointer is never converted.
The `_NV_` alias shares the public assembly entry's address/slot without
virtual dispatch. No headers, access specifiers or inheritance declarations
are rewritten. Win64 supplies the original `this` in RCX and the second
argument's float in XMM1; all three targets return **void**. The original
world and float are forwarded unchanged, including signed zero. No meaning
or time unit is inferred from the tick parameter or method name.

Offline PE-byte inspection also confirmed the pinned SDK binary's `_NV_`
alias and relative ordered stub relationships, and that the built DLL loads
the three imported SDK stub addresses from its IAT before `GetRealAddress`
(not addresses of local import thunks). This is static evidence only;
neither DLL was loaded or called.

Resolved targets must be nonzero, distinct, executable committed memory in
the current host EXE. That check detects absent initialization/bad resolutions;
it is **not** executable-version, world-liveness or runtime-ABI acceptance.

### Installation, lifetime and failure rules

The installation state is explicit: `NOT_STARTED`, `DISABLED`, `INSTALLING`,
`ACTIVE` or sticky `FAILED`, with a reason/target and separate attempted,
SDK-success and original-ready masks. All targets are resolved/checked before
the first hook attempt. The DLL is process-pinned and its heap-owned callback
context published before any `AddHook` can enable a detour. It is deliberately
never destroyed or replaced during the process.

Disabled startup also emits one bounded installation diagnostic:
`state=1` (`DISABLED`) and attempted/success/original masks `0/0/0`.
`Install` returns before SDK target preparation or hook calls in this state.
The `LIFECYCLE` prefix alone does not mean hooks are active; distinguish this
startup summary from actual callback/installation state and masks.

The pinned public `AddHook(void*, void*, void**)` is immediate
(`Include\core\Functions.h:26-34`). Its implementation
(`Source\core\Functions.cpp:84-100`) calls `MH_CreateHookEx` with the original
slot, then `MH_EnableHookEx`, checking both results; either failure returns
`FAIL`, and error logging can throw after a partial start.
The SDK documents its [multihook dependency](https://github.com/BFrizzleFoShizzle/KenshiLib/blob/11cd65d557dea76c6540239acf467e0e06e09cf7/README.md).
Read-only reference inspection of
[multihook source](https://github.com/m417z/minhook/blob/4f18d1809a81e6c700bcc4a21aeb43ade72bbf8b/src/hook.c#L767-L829)
shows the trampoline output is published while the new hook is disabled,
before enable. This reference revision is **not proven to be the exact fork
revision compiled into the shipped SDK**; the pinned packaged runtime and a
controlled real-host test remain the contract/acceptance boundary.

Every result and typed original pointer is checked separately, including
original slots published before an exception. Callbacks may begin before
`AddHook` returns: essential ownership/teardown bookkeeping still runs during
installation, but tickets cannot enqueue/retire until all three successful
hooks and originals are ready. An in-flight teardown stays counted when
installation becomes active.

A failed/indeterminate attempt is never retried. Any already-installed
callbacks keep calling their stored typed trampolines, exactly once, with
probe behavior disabled. There is no recursive call through the hooked
method, queued-hook API, SDK reinitialization, global unhook/uninitialize,
version override, third-party hook replacement or removal of another
plugin's hooks. Compatible multihook chains remain owned by the SDK.

If a callback nevertheless has no original/context, it reports a fatal
contract failure through `OutputDebugStringA` and terminates its own process
with `0xe050434c`; it cannot fabricate a successful void call or recover by
reentering the hook. This is a last-resort broken-contract response, not a
normal installation failure path. **Do not hot-unload, replace or overwrite
the DLL while Kenshi is running**, even after a partial failure.

No plugin lock surrounds an original, installer/SDK call, logging or other
host operation. Original C++ exceptions disable/invalidate observation and are
re-thrown unchanged; diagnostics cannot convert a game failure into success.
Initialization or diagnostic failures also disable observation explicitly,
without dismantling any installed hook chain.

### Epoch, queue and observed ownership

Epoch/queue/ownership state is protected by one Win32 SRW lock; operations
under it use only fixed-size value state. Original slots and context
publication use atomic Win32 pointer reads. While installing/active, each
reset/clear entry advances the 64-bit epoch,
clears the opaque world identity and invalidates queued tickets **before**
calling the original teardown. Nested and concurrent reset/clear entries
each invalidate, with a shared depth retained until their originals finish.
No enqueue or retirement is allowed during teardown. Returning from reset
does not rebind a world; only a subsequent eligible tick can observe an
identity again. An older in-flight tick cannot retire tickets in a new epoch.
Epoch exhaustion permanently fails observation instead of wrapping.

The fixed **64-ticket** queue contains only `(epoch, nonzero requestId)`
observation values: no game pointers, function callbacks, mutations or
vehicle work. Each enqueue returns `ACCEPTED`, `DISABLED`, `UNOBSERVED`,
`RESETTING`, `STALE`, `FULL` or `INVALID`; it is never silently dropped or
unbounded. An accepted ticket is retired only after a successful original
tick in its unchanged epoch, or explicitly invalidated by an epoch change.
Snapshot counters distinguish accepted/rejected/retired/invalidated/queued,
and the last retired ID is available. IDs are correlation labels, not an
exactly-once gameplay delivery API; callers must treat changed epochs as
cancellation. Diagnostic counters saturate; epochs do not wrap. **There is
no production ticket producer or gameplay consumer in this unit.**

First callback thread IDs are recorded separately for tick/reset/clear;
different first owners between surfaces are allowed. A later owner change
on the same surface fails observation and invalidates its queue. These are
**observed thread IDs only**, not proof of UI-thread ownership, thread
liveness or arbitrary mutation safety. An unannounced world-identity change
also fails observation. Identity is an opaque integer compared only; no
GameWorld/game-object field is dereferenced or retained as a safe reference.
This is not exhaustive world-creation/destruction/liveness coverage.

The pinned SDK logger uses unsynchronized global streams
(`Source\Debug.cpp:13-43,67-79`). Callbacks therefore use bounded
`OutputDebugStringA` diagnostics, **never SDK logging, MessageBox or MyGUI**:
first owners once per target, the first eight reset/clear entries plus one
suppression notice, and sticky failure/duplicate notices. There is no
per-frame log. SDK startup logging retains the established export-boundary
exception reporting/fallback. Capture debugger output during any approved
manual probe test; lack of captured callback output is not acceptance.

### Game, loader and probe logs

Kenshi has its own built-in logging. Its game installation folder contains
`kenshi.log` and `kenshi_info.log`, independently of the RE_Kenshi/plugin logs.
For the observed installation these are under
`D:\SteamLibrary\steamapps\common\Kenshi`, even though the supported executable
is launched from its `RE_Kenshi` subdirectory.

Preserve the affected run's game logs alongside the loader log and debugger
capture before relaunching; logs may be overwritten. The game logs provide
resource, mod-loading and save/load context. The loader log establishes SDK
initialization, plugin discovery and bootstrap reporting. Bounded lifecycle
owner/epoch/failure diagnostics use `OutputDebugStringA` and require debugger
capture; they are not guaranteed to appear in either game log. A game-log
`Reset game` entry alone does not prove that the typed probe callback ran.

Keep raw captures private and redact identifying paths or save information
before uploading issue attachments. Existing game logging is not evidence that
its logging API, or the SDK logger, is safe to call from every callback thread;
this probe's callback logging policy remains unchanged.

### Real-host gate: partial world observations captured, acceptance incomplete

The host must initialize the exact matching KenshiLib **0.5.0** DLL and
ordered Steam/GOG 1.0.65 RVA pack before normal active-mod `Plugins` discovery.
The matching compiler headers/import library, runtime DLL/PDB and RVAs must
stay together. No `PreloadPlugins` switch or initialization/version override
is used here. Compatibility fingerprints are the two MD5s above; a different
root executable cannot be treated as supported merely because a compatible
EXE exists elsewhere.

Previously recorded **read-only** installation evidence identifies a
compatible target at
`D:\SteamLibrary\steamapps\common\Kenshi\RE_Kenshi\kenshi_x64.exe`:
Steam MD5 `df4a5a7ef8a29deb24b70e7b7f4a222a`, SHA256
`504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`.
The main game-root EXE's MD5
`8a03c256f0da1555d9cceb939b41530a` is not one of those supported fingerprints.
Installed `KenshiLib.dll` SHA256
`5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b`
and both installed RVAs match the dependency pins. Installed `RE_Kenshi.dll`
SHA256 `58900f1678e4eef407fb19d12527fd8c1d3dbc5bc09903c088dc52a9134a04ee`
has **unproven exact source provenance**. That earlier read-only evidence
alone did not establish the selected process or loader initialization order.

On **2026-11-05**, a separately approved, user-launched main-menu control
verified the actual supported Steam 1.0.65 process, matching loaded SDK 0.5.0
DLL and developer DLL. The fresh log records SDK initialization and RVAs at
0.002 seconds, normal post-load developer discovery at 5.328 seconds, the
11-part/3-frame/32-slot/one-binding catalog at 5.330 seconds, and explicit
`Lifecycle DISABLED` with no SDK resolution or probe hooks. Only the developer
`.mod` was active and the legacy `ProjectCars.dll` was not loaded, but
RE_Kenshi also preloaded `KenshiExtensionPlugin` / `kep-core 1.0.0`.
The loader reported `Could not initialize UI` once before logging the
supported main menu again; this is recorded, not an error-free-startup claim.
The control establishes bootstrap/discovery behavior, not enabled hook ABI,
callback scheduling, world lifecycle acceptance or shared-hook coexistence.

After separate approval, matching artifacts and independent save/configuration
backups, the enabled main-menu run also reached the supported menu.
Process-scoped CDB output records `LIFECYCLE_ACTIVE`, no installation failure,
and attempted/success/original masks `7/7/7`. All three typed trampolines were
ready, but no world callback owner had yet been observed at installation.
The first enabled capture was closed by the user. In the second approved run,
the user created a disposable new game, saved it under a unique probe name,
explicitly reloaded that save and reported that the world returned.
New-game reset advanced epoch `1 -> 2` before the original at depth 1;
the subsequent first observed tick ran at epoch 2/depth 0. Tick and reset
owners were both thread `225736`. Reload reset advanced epoch `2 -> 3`
before the original, with the same recorded owners. Kenshi's own
`kenshi_info.log` independently records the new game, manual save and explicit
reload. These are partial live tick/reset observations, not proof of arbitrary
UI/mutation permission or complete lifecycle coverage. The bounded probe emits
no per-epoch tick message, so the reload diagnostic alone does not prove
post-reload identity reobservation.

After the user reported game-menu quit, the original process was still paused
in CDB on a first-chance `c0000005` access violation. The captured stack was
in `combase`, through `CoUninitialize` and game audio termination. Host/system
symbols were export-only and `.ecxr` failed; the absence of a remake frame on
that stack establishes neither causality nor exoneration. After forwarding
the exception with `gn` (not handled), the original process disappeared and CDB returned
code 1. The actual host exit code is unknown; clean shutdown is not established.
Raw game/loader logs, debugger captures and save/configuration audits remain
private in the isolated test package. After verifying no executing Kenshi
process, the isolated opt-in was returned to `observe disabled`.

The earlier capture used `-g` with `-cf`: the command file was processed only
when the first debugger prompt appeared at the access violation, rather than
at the initial breakpoint. Subsequent controls omit initial/final breakpoint
skip options, configure filters at the initial prompt and break on process
exit to record the actual host exit code. An SDK-free Windows-process fixture
verified that ordering and a target exit code of zero. At a host exception,
capture evidence before explicitly forwarding it with `gn`; do not
automatically consume or suppress a game failure. The corrected capture
differs from the earlier run, so a single control cannot establish causality.

No clear callback owner, nested/in-flight teardown, live queued ticket or
shared-hook-chain acceptance was captured. Queue/invalidation counts remained
zero. Loaded `kep-core` establishes plugin presence only, not shared-target
coexistence. All 2,847 baseline non-autosave files remained byte-identical and
the complete private backup reverified; the game replaced `autosave2` and
updated the settings' Continue/autosave index. These game-managed changes were
not blindly restored.

Before phase 3B, the user must explicitly approve and conduct a controlled
real-host observation test:

1. With Kenshi fully closed, verify the actual intended executable and
   matching SDK/RVA unit, loader route and normal initialization/discovery
   order. Preserve existing mods/configuration and independent save backups.
   This source milestone does not change the game root or launch route.
2. Prepare an isolated developer test package. If normal mod discovery needs
   an empty `.mod`, the user must create it through FCS or another verified
   method; no fake FCS binary writer or renamed empty file is supplied.
   Only the new DLL/matching PDB, canonical catalog and configuration/
   registration examples belong in the developer package, not proprietary
   game binaries, the source-less legacy DLL or unidentified V8 audio.
3. First verify disabled bootstrap behavior in that isolated package, keeping
   the optional lifecycle file absent or `observe disabled`. After complete
   process exit, explicitly opt into `observe enabled` only for the approved
   observation run. Do not edit the repository's active root registration or
   overwrite the legacy DLL. Any failure/change requires process restart.
4. Capture startup and bounded debugger diagnostics through startup, new
   game, load, reset and clear, including nested/in-flight transitions.
   Check actual owners/epochs, preserved original behavior and failure
   reporting. These observations must not be inferred from the fake fixtures
   or method names, and no live inventory/UI/save mutation is authorized.
5. Check controlled coexistence with the intended plugins and SDK multihook
   chains, without removing hooks or hot-unloading code. Record the actual
   process/runtime identities, initialization order, scenarios and evidence.
   A successful offline build or startup banner alone does not pass this gate.

The tests/build do not launch the game, load the real SDK, install the plugin,
or change installed files, active mod lists or saves. Separately, the user
prepared an isolated developer package and manually activated it for the
disabled main-menu control, with the original mod list backed up and the
legacy DLL/PDB/FCS/registration preserved. The separately approved enabled
installation/menu control also passed, and disposable new/save/reload scenarios
captured actual tick/reset owners and epoch transitions. **Clear, complete
teardown/reobservation, clean shutdown and shared-hook coexistence remain
unverified.** The full real-host gate has not passed.

## Remaining native work

This stops at source-remake **phase 3A**, the opt-in observational lifecycle
boundary. **Phase 3B native frame/building adoption is deliberately gated on
later explicit approval and the controlled real-host test above**. Typed
native handles/FCS adoption, workshop/live inventory/attachments,
driving/fuel integration and playable deployment are subsequent units, not
part of this one. Passengers, tracks, oil and combat remain unimplemented.
The disabled bootstrap and enabled installation/menu controls passed, with
partial disposable-world tick/reset observations captured. No complete world
lifecycle or gameplay gate has passed.
