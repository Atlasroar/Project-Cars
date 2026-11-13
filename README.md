# Project Cars

Project Cars is a native-plugin vehicle mod for **Kenshi x64**. The uploaded
package contains the FCS mod, runtime DLL, debug symbols, modular vehicle assets,
workbench and oil assets, audio, configuration, and rendering shaders.

Development now starts from the [recovered original 0.8.19 source](original/README.md),
preserved at an exact upstream revision with an isolated VC100 build. This is
not a proven reproduction of the packaged 0.8.16 DLL or an approved live-game
release. The existing deployment files remain unchanged.

The separate [independent remake core](remake/README.md) adds a strict catalog
parser, a standalone vehicle model with atomic part-count exchanges, typed
fuel accounting and remake-only serialization, plus a VC100 x64
development-only `ProjectCarsRemake.dll` catalog bootstrap and an explicitly
opt-in observational lifecycle probe, **disabled by default**.
**No drivable gameplay is implemented in the remake**, and the legacy runtime
and active plugin registration are unchanged.

## Documentation

The [Project Cars wiki](https://github.com/Atlasroar/Project-Cars/wiki) documents
the package's features and code evidence:

- [Feature status](https://github.com/Atlasroar/Project-Cars/wiki/Feature-status)
- [Installation and controls](https://github.com/Atlasroar/Project-Cars/wiki/Installation-and-controls)
- [Vehicle workshop](https://github.com/Atlasroar/Project-Cars/wiki/Vehicle-workshop)
- [Catalog and FCS records](https://github.com/Atlasroar/Project-Cars/wiki/Catalog-and-FCS)
- [Fuel and oil production](https://github.com/Atlasroar/Project-Cars/wiki/Fuel-and-oil)
- [Assets and rendering](https://github.com/Atlasroar/Project-Cars/wiki/Assets-and-rendering)
- [Native code map](https://github.com/Atlasroar/Project-Cars/wiki/Native-code-map)
- [Shader code](https://github.com/Atlasroar/Project-Cars/wiki/Shaders)
- [Persistence and debugging](https://github.com/Atlasroar/Project-Cars/wiki/Persistence-and-debugging)
- [Development and validation](https://github.com/Atlasroar/Project-Cars/wiki/Development-and-validation)
- [Licensing](https://github.com/Atlasroar/Project-Cars/wiki/Licensing)

## Reporting issues

Use the [issue forms](https://github.com/Atlasroar/Project-Cars/issues/new/choose)
for problems, compatibility/build help, feature requests, documentation fixes,
or asset/licensing clarifications. Both forms accept logs, screenshots, and
videos in their attachment fields; GitHub controls file types and size limits.
Redact private information before uploading. Blank issues remain available
when neither form fits.

Kenshi has its own built-in logging in the game installation folder, including
`kenshi.log` and `kenshi_info.log` (for example,
`D:\SteamLibrary\steamapps\common\Kenshi`). Preserve relevant logs from the
affected run before relaunching, because logs may be overwritten. Include them
alongside Project Cars / RE_Kenshi logs when reporting game problems. Remake
lifecycle callback diagnostics are separate debugger output, not entries
guaranteed to appear in Kenshi's log files.

## Package layout

| Path | Purpose |
| --- | --- |
| `Project Cars.mod` | Canonical Forgotten Construction Set mod database |
| `ProjectCars.dll` | Native x64 runtime plugin |
| `ProjectCars.pdb` | Debug symbols from a different build; not a match for the current DLL |
| `RE_Kenshi.json` | Plugin registration for `ProjectCars.dll` |
| `ProjectCarsControls.ini` | Vehicle configuration; currently includes first-person FOV |
| `vehicles.catalog` | Text definitions for frame types, slots, parts, and fuel |
| `remake` | New original GPL-3.0-only source, native tests, pinned build procedure and inactive development registration example |
| `original` | Recovered upstream 0.8.19 source, attribution/hash manifest and isolated pinned build; no active deployment |
| `assets` | Integration manifests, asset notes, and recorded offline verification |
| `meshes`, `materials`, `Textures`, `icons` | Vehicle, workbench, and oil visual/collision assets |
| `shaders` | Editable HLSL track and tread-mark code |
| `audio\v8` | Prepared engine/vehicle samples and their provenance manifest |

The original **764-file package import** remains archived in Git history. The
current tree removes **382 byte-identical, unreferenced `(2)` copies** and retains
the canonical package filenames, backups, caches, and historical logs.
Deliberately distinct filenames are retained even when their contents match.
Documentation uses canonical filenames unless discussing the archived import.

## Important limits

The DLL's embedded banner identifies it as **0.8.16**, built September 15, 2026.
Several manifests and notes describe earlier exports; their counts and pending
features are not authoritative for this later binary. No live Kenshi validation
was performed while preparing the wiki.

**Recovered upstream native source is now preserved under `original`.**
Its 0.8.19 revision and build inputs are documented, but correspondence to the
packaged 0.8.16 DLL remains unproven. Asset-authoring tools and referenced ABI
evidence are still missing. The wiki's earlier binary/PDB analysis remains
historical evidence, not a matching-source claim. The `remake` source is an
independently written partial foundation, separate from the recovered source.
Its DLL/PDB can be built and checked offline. Separately approved remake
controls have captured startup and disposable-world tick/reset observations;
the complete live-host lifecycle gate and playable gameplay remain unverified.

## License

Project Cars original contributions are declared under **GNU GPL version 3
only** (`SPDX-License-Identifier: GPL-3.0-only`); see [LICENSE](LICENSE).

Read [LICENSING.md](LICENSING.md) before redistribution: the packaged native
binary's exact Corresponding Source remains unestablished, V8 audio rights are
undocumented, and this license does not grant rights to Kenshi or other
third-party material. Adding the
license is not a claim that the existing binary package satisfies GPLv3 source
distribution requirements.
