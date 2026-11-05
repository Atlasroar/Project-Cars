# Project Cars

Project Cars is a native-plugin vehicle mod for **Kenshi x64**. The uploaded
package contains the FCS mod, runtime DLL, debug symbols, modular vehicle assets,
workbench and oil assets, audio, configuration, and rendering shaders.

The separate [original source foundation](remake/README.md) adds a strict catalog
parser and a VC100 x64 development-only `ProjectCarsRemake.dll` bootstrap.
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

**The original runtime's native C++ source, headers, build project and
asset-authoring tools are not included.** The wiki documents actual shader
source, binary-visible behavior, manifests, and a clearly labeled
non-matching-PDB code map. The new `remake` source is an independently written,
partial foundation, not recovered legacy source or a rebuild of `ProjectCars.dll`.
Its DLL/PDB can be built and checked offline; live Kenshi startup and gameplay
remain unverified.

## License

Project Cars original contributions are declared under **GNU GPL version 3
only** (`SPDX-License-Identifier: GPL-3.0-only`); see [LICENSE](LICENSE).

Read [LICENSING.md](LICENSING.md) before redistribution: the native binaries'
corresponding source is missing, V8 audio rights are undocumented, and this
license does not grant rights to Kenshi or other third-party material. Adding the
license is not a claim that the existing binary package satisfies GPLv3 source
distribution requirements.
