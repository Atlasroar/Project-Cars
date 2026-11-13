# Recovered original-source build foundation

This is an **inactive development build of original 0.8.19**, not a playable
release or a claimed reproduction of the canonical September 15, 2026 0.8.16
DLL. No game, mod loader, SDK DLL, or generated plugin is run by these scripts.
The existing root deployment and `remake` tree are unchanged.

## Source and attribution

`upstream` contains all 80 blobs from
[Teirdalin/Project-Cars](https://github.com/Teirdalin/Project-Cars/tree/740a9961c2ae47c75013fab5402189abca459354),
commit **740a9961c2ae47c75013fab5402189abca459354**. The version is stated by
`upstream\plugin\ProjectCars.cpp:36`. `source-manifest.json` records the immutable
Git tree, individual Git blob IDs, byte lengths and SHA256 hashes.

The archive was extracted with `core.autocrlf=false`; every extracted file was
checked against its original Git blob ID with `git hash-object --no-filters`.
`original\.gitattributes` prevents newline conversion of the snapshot. No
upstream source or build-script bytes were edited. Local build adaptation is
entirely outside `upstream`. There are no source submodules or LFS objects.
The GitHub commit and manifest identify the baseline, not a signed authenticity
attestation or bit-for-bit reproducible binary guarantee.

Upstream's README and complete `upstream\LICENSE` are preserved: **GPL-3.0-only**.
The adapted KenshiLib `NavMesh.h` retains its provenance notice in
`upstream\plugin\compat\README.md`; its documented local VC10 compatibility
change is part of the recovered snapshot, not a new modification to the SDK.
Keep these notices and the corresponding source with any future distribution.

External KenshiLib is GPLv3; the approved SDK headers retain Ogre's Torus Knot
Software MIT notice (`Include\ogre\OgrePrerequisites.h`) and MyGUI's Developers
MIT notice (`Include\mygui\COPYING.MIT`). Boost retains its Boost Software
License. Microsoft compiler/SDK inputs retain their separate terms. No game
binaries, compiler/SDK libraries, external engine libraries, or audio are
vendored here. Do not interpret the external example repository's availability
as clearance to redistribute all of its payloads. Audio rights remain a
separate unresolved release gate, including when synthesized fallback exists.

## Build

From the repository root, using the existing approved external dependency root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\original\build.ps1
```

An alternative external root must contain exactly the pinned inputs:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\original\build.ps1 -ToolchainRoot 'C:\path\to\kenshi-v100'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\original\build.ps1 -ValidateOnly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\original\tests.ps1
```

`ValidateOnly` checks inputs without compiling. Focused wrapper tests reject
changed/extra source files, changed verifier helpers, engine pin drift and missing dependencies;
they use an isolated disposable fixture below ignored `out\original`, not
changes to the preserved snapshot or external dependency root.

The wrapper fully inspected and follows `upstream\tools\build.ps1`'s ten
translation units and release flags. It adapts only the external layout,
isolated per-run output, explicit import-library selection, environment hygiene,
and static verification. It does **not** execute upstream setup/build scripts,
modify global environment settings, initialize the SDK, override versions,
install anything, or copy active mod settings/catalog/assets into the output.
`PATH`, `INCLUDE`, `LIB`, `CL`, `_CL_`, `LINK`, and `_LINK_` are scoped to the
build process and restored in `finally`.

Outputs are `out\original\<timestamp>-<id>\ProjectCars.dll`, its matching PDB,
objects/import library, and `artifact-report.json`. A fresh directory prevents
an old build being mistaken for a successful new attempt. The report is written
only after the compiler, linker and static checks succeed. Failed builds throw
and leave any partial developer output for diagnosis; never deploy it.

`remake\dependencies.json` supplies the already approved VC100
**16.00.30319.01 / _MSC_VER 1600 x64**, Windows SDK **7.1**, KenshiLib **0.5.0**
source **11cd65d557dea76c6540239acf467e0e06e09cf7**, official release
DLL/import/PDB/ordered Steam and GOG 1.0.65 RVA hashes, and complete compiler,
SDK, KenshiLib and Boost **1.60.0** header-tree pins. The setup/provenance
instructions remain in `remake\README.md`. This build needs no standalone test
CRT installation because it executes no native test binary.

`original\dependencies.json` pins that baseline manifest, the additional
Windows `winmm.lib`, and the actual MyGUI/Ogre import payloads from
KenshiLib_Examples_deps commit
**b566d74bf3d74629cc2fb632a97595b8202993f1**. Both engine hashes exactly match
that commit's Git LFS SHA256 OIDs, not ZIP pointer text:

| Input | SHA256 |
| --- | --- |
| MyGUIEngine_x64.lib (1,465,892 bytes) | `065e42b65440910fd5faf5bb7e52dfb1298ac977d6c18fe164cfd81634e2e83e` |
| OgreMain_x64.lib (4,360,656 bytes) | `97db9603acaa5ff19fa58f73bf7362e7298f99e96b36243563beba0334337928` |

These payloads already existed externally; no new dependency download or global
installation was required. The example checkout's **different**
`KenshiLib.lib` hashes to
`d407bf18c807cd3390643227ca4dc3ee4628fedc520870eee250201c04db311d` and is
deliberately **not used**. The wrapper selects the official v0.5.0 import
`fb0a5308d9073ab7538d176c9ec13bf04354cf7daf94e4254e168ea5f695513b`
and its matched full SDK header tree. There was no header workaround, upgrade
to 0.5.1, SDK override or ordered-RVA bypass.

To avoid changing `remake`, the wrapper imports only named, hash-pinned function
definitions from its existing scripts through PowerShell's AST. Neither
script's entry point, test runner, default output path, nor seven-symbol
remake-only import policy is executed. The original plugin legitimately imports
many SDK/Ogre/MyGUI symbols. The parent's pending verifier default-path fix is
not required. Changes to those helper files or baseline pins deliberately fail
closed until reviewed and repinned. Their recorded file hashes reflect the
existing Windows checkout bytes; retain those bytes when reproducing this
foundation, rather than silently accepting a newline-converted helper.

## Observed static build evidence

The command above completed with exit code **0** on November 13, 2026, emitting
`out\original\20261113-163047-bf89e3a5`. Compiler warnings include existing
SDK VC10 extension warnings; warnings were not hidden or converted into a
claim of clean runtime behavior.

| Property | Observed result |
| --- | --- |
| PE architecture / linker | AMD64 PE32+ DLL / VC100 linker 10.0 |
| Export | Exactly `?startPlugin@@YAXXZ` (global C++ `void startPlugin()`) |
| DLL SHA256 | `5295ff35fc7027712100040ddde136f9f516fe6f6d7b2efd18c21949b2c077a1` |
| PDB SHA256 | `cf76cd64a3891b49795c98057a00df73b4c7f40b4c98925c5b20892a6e4a169f` |
| DLL RSDS / PDB information stream | GUID `7edd6782-40f6-49eb-8053-687d323385f5`, age 1, `ProjectCars.pdb` |
| Imports (symbol counts) | KenshiLib 216, OgreMain_x64 192, MyGUIEngine_x64 71, MSVCR100 64, MSVCP100 138, KERNEL32 43, USER32 8, WINMM 6 |

The existing static PE/PDB readers validate AMD64, linker version, the
nonforwarded export, import names and matching RSDS/PDB identity directly from
file bytes. They never call `LoadLibrary`. Full decorated import names are in
the ignored per-run report. Timestamp macros, linker/PDB identities and build
paths mean future builds need not reproduce these particular binary hashes.

## Deployment reconciliation and runtime gates

A read-only scan found **96 distinct literal** `./mods/Project Cars/` paths in
the recovered C++/header/inl files: **79 existing files**, the existing
`shaders` directory, and **16 missing mesh files**. The light-frame
mesh/icon paths resolve in the canonical tree. Both renderer-selected
`shaders\vehicle_tracks.hlsl` and `shaders\vehicle_trails.hlsl` exist. This
establishes path availability, **not** content/rig/shader ABI acceptance or
complete coverage of dynamically assembled filenames.

All missing literals are under `meshes\xsmall_frame`:

| Family | Missing mesh basenames |
| --- | --- |
| Bike | XS_Bike_Frame, XS_Bike_Engine, XS_Bike_Fuel_Tank, XS_Bike_Seat_Driver, XS_Bike_Fork, XS_Bike_Wheel_FL, XS_Bike_Wheel_RL |
| Quad | XS_Quad_Frame, XS_Quad_Engine, XS_Quad_Fuel_Tank, XS_Quad_Seat_Driver, XS_Quad_Steering_Wheel, XS_Quad_Wheel_FL, XS_Quad_Wheel_FR, XS_Quad_Wheel_RL, XS_Quad_Wheel_RR |

Each basename requires `.mesh`. The upstream generated definitions associate
these with `201`-`209` and `211`-`217` `-Project Cars.mod` IDs. Their FCS record
types/contents and associations remain unresolved; matching a string or path
would not prove them correct.

The unchanged canonical `vehicles.catalog` has 11 parts, 3 frames and 32 slots,
with only `small_frame` bound to `10-Project Cars.mod`. It is not a reconstructed
0.8.19 motorcycle/xsmall catalog. HEAD itself supplies no catalog, FCS,
`RE_Kenshi.json`, shaders or audio. None were taken from the older upstream
commit or substituted into the canonical deployment.

Before any separately approved real-host work: reconcile the catalog, FCS IDs,
rigs/materials/assets and legacy saves; establish the supported executable,
matching game Ogre/MyGUI DLL identities, and RE_Kenshi/KenshiLib runtime/RVA pack;
resolve audio rights; and obtain
evidence for raw Steam/PhysX 2.8.4 vtable/layout assumptions and MFC100u ordinal
266. Referenced `simple-update.txt` and `docs\DRIVING.md` are absent from this
snapshot, so their claimed ABI evidence is unavailable here.

Unchanged startup code sets `g_started` before successful initialization
(`ProjectCars.cpp:34-35`), and `installNativeRuntime` continues after fitting
installation failure. Partial hook installation/rollback, retries, allocator
ownership, callback logging/threading/lifetime and native persistence remain
runtime acceptance gates, **not** permission for speculative redesign.
RE_Kenshi/KenshiLib safety and API contracts are authoritative; departures
require explicit approval after upstream maintainer/expert consultation.
**Build success does not establish runtime safety or legacy-save acceptance.**
