# Project Cars

Project Cars is a native-plugin vehicle mod for **Kenshi x64**. The uploaded
package contains the FCS mod, runtime DLL, debug symbols, modular vehicle assets,
workbench and oil assets, audio, configuration, and rendering shaders.

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
| `assets` | Integration manifests, asset notes, and recorded offline verification |
| `meshes`, `materials`, `Textures`, `icons` | Vehicle, workbench, and oil visual/collision assets |
| `shaders` | Editable HLSL track and tread-mark code |
| `audio\v8` | Prepared engine/vehicle samples and their provenance manifest |

The initial import preserves **all 764 package files**, including `(2)` copies,
backups, caches, and historical logs. Documentation uses canonical filenames
without `(2)` unless discussing the preserved archive. Do not enable both copies
of the mod or load both copies of the DLL.

## Important limits

The DLL's embedded banner identifies it as **0.8.16**, built September 15, 2026.
Several manifests and notes describe earlier exports; their counts and pending
features are not authoritative for this later binary. No live Kenshi validation
was performed while preparing the wiki.

**The native C++ source, headers, build project, SDK setup, and asset-authoring
tools are not included.** The wiki documents actual shader source, binary-visible
behavior, manifests, and a clearly labeled non-matching-PDB code map. It does not
claim to recover a rebuildable C++ project.

## License

Project Cars original contributions are declared under **GNU GPL version 3
only** (`SPDX-License-Identifier: GPL-3.0-only`); see [LICENSE](LICENSE).

Read [LICENSING.md](LICENSING.md) before redistribution: the native binaries'
corresponding source is missing, V8 audio rights are undocumented, and this
license does not grant rights to Kenshi or other third-party material. Adding the
license is not a claim that the existing binary package satisfies GPLv3 source
distribution requirements.
