# Light Frame native asset package

The atlas edition is
`C:/Users/Administrator/Documents/3D Projects/3D Models/Project Cars/models/Car_frame_light_atlas.blend`.
It contains packed atlas images, remapped modules, headlights and both rear seats.
The artist's `Car_frame_light.blend` is preserved separately. The atlas edition
was reconstructed from the verified modular exports to preserve their geometry
and pivots. Do not regenerate either file with the old `build_car.py`.
The old model README and exchange manifests describe the initial reconstruction,
not this export or its current scale and pivots.

## Package

Native assets are staged under `mod/Project Cars/` and copied to the installed
`E:/SteamLibrary/steamapps/common/Kenshi/mods/Project Cars/` asset directories.
Existing FCS records, DLLs, catalog and the original `meshes/small frame/` files
are not replaced by this asset export.

- `meshes/light_frame/modules/`: 16 modules, including the frame, four wheels,
  engine, three cockpit components, fuel tank, cargo crate, two front variants
  a pair of headlights as one removable assembly and two separate rear seats.
- `meshes/light_frame/armor/parts/`: all 43 individual armor pieces.
- `meshes/light_frame/armor/groups/`: ten convenience group meshes: left side,
  right side, front nose, rear, engine cover, suspension, and four wheel sets.
- Every mesh has a matching cooked PhysX `.xml` collision file beside it.
- `materials/PC_Light.material`: one material, `PC_Light_Atlas`, using Kenshi's
  `StaticObject` shader. All 69 meshes reference it, including the armor groups.
- `textures/light_frame/PC_Light_Atlas_D.dds`: diffuse RGB and gloss in alpha.
- `textures/light_frame/PC_Light_Atlas_N.dds`: normal map.
- `textures/light_frame/PC_Light_Atlas_M.dds`: metalness map.
- These three 4096 x 2048 BC3 DDS atlases include mipmaps. Older texture files
  remain in the mod for preservation; the new modular meshes do not use them.
- `assets/light_frame/manifest.json`: placements, part-to-submesh mappings,
  group membership, collision hulls and source fingerprint.
- `assets/light_frame/verification.json` and `checksums.json`: export evidence.

## Placement and interchangeable components

Mesh geometry is relative to the pivot recorded for that asset. Both its visual
mesh and collision use the same pivot. Place the asset at `pivot_kenshi` beneath
the vehicle root. Placing every individual mesh at zero will not assemble the car.

The established authoring scale is preserved without another unit conversion.
The binary Ogre coordinates are `(Blender X, Blender Z, -Blender Y)`: native +X
is forward, +Y is up, and -Z is left. These measured export axes supersede the
earlier proposed model handoff axes; the driving/render adapter must account for
them. Wheel roll is around native Z (the recorded axis is negative Z).

`Front_Rack` is the original front bumper bar and collars, extracted from `Frame`.
`Front_Spikes` uses that bar plus seven forward spikes. They share the same mount
and belong to the exclusive `front_attachment` slot: install one at a time.
The source Blender file shows `Front_Rack` by default and hides `Front_Spikes`.
Front nose armor remains independently removable.

`Headlights` uses an independent `headlights` slot and fits either front variant.
It has two round lamps, supporting brackets, bezels and protective lens bars.
Root-relative beam anchors and forward direction are recorded in the manifest;
the atlas Blender file also contains `Mount_Headlight_L/R` empties. The export
provides the lamp model, not working illumination. Runtime beam lights and
switching still need integration. The glass shares the non-emissive atlas material.

The fuel tank sits below the rear rack. The rear bay supports either `Cargo_Crate`
or one or both `Seat_PassengerL` / `Seat_PassengerR` modules. The seats face backward,
exactly as placed in the artist's September 12 blend. Each uses the existing Seat
item (`136-Project Cars.mod`), in `seat_passenger_l` or `seat_passenger_r`; no extra
FCS items or recipes are needed. Both mounts appear under Cockpit.

The asset manifest declares symmetric exclusions: cargo excludes either rear seat,
and each seat excludes cargo. The generated workshop bindings enforce these rules
when editing a work queue, starting work and transferring the completed part.
Queue removal of cargo before fitting seats; remove both seats before fitting
cargo. Unqueuing a prerequisite removal is rejected while its dependent installs
remain queued. The existing empty-trunk check still applies to cargo removal.

The atlas blend shows the two-seat layout and hides cargo for inspection. Toggle
the cargo/seats visibility to inspect the alternative; runtime visuals follow
installed parts. Rear passenger boarding/occupancy is not implemented by this
asset import; the native boarding controller still operates the driver seat only.

Use either an armor group mesh or its individual member meshes. A group retains
named submeshes for each plate, so a renderer can hide every submesh
listed for a damaged plate. Wheel armor uses its owning wheel's pivot and must
follow that wheel. Do not render a group and its separate members together.

## Collision and integration

The collision package uses 129 cooked convex hulls across 69 files, including
the group alternatives. The bare frame uses a 16-shape compound approximation;
wheels use 12-sided convex cylinders, armor uses small oriented boxes, and the
spikes have individual tapered hulls. The headlights add two simple hulls, and each
rear seat has its own rotated seat/body and base hulls.
It does not use the detailed visual tubes
as triangle-mesh collision. Fine collision fit and handling still need game tests.

Ogre's native mesh adapter changes the coordinate axes automatically. The PhysX
cooker does not. Cooked hull vertices remain in Blender Z-up; Kenshi's collision
loader converts them to Y-up exactly once. Native mesh and decoded hull readback
verify their orientation. XML actor
and shape transforms are identity, with placement supplied by the manifest.

Collision actors are serialized without dynamic bodies. The future runtime must
attach installed shapes to the vehicle and configure contact/query filtering.
Do not create an independently simulated body for every installed plate. Removed
debris can use the individual part collision when that behavior is implemented.
XML files contain actual cooked hull data. No game-generated `.bin` caches are
invented or copied from unrelated geometry.

Workshop slots, component items and installed mesh visibility are bound by the
native plugin; filenames alone do not register those systems. Individual dynamic
debris, passenger occupancy and LOD meshes remain separate runtime/asset work.
The new passenger modules have native readback and offline fitting tests, with
live in-game validation still required.

## Reproduce and inspect

The original seven-material export is retained under
`build/light-frame-assets-20260911-exports`. `tools/atlas-light-frame.py` creates
the atlas edition from that verified package; `tools/add-light-frame-headlights.py`
adds the optional lamps and fills the eighth atlas tile. The old exporter is for
the original seven-material source and must not be used on the atlas edition.
`tools/import-light-frame-passengers.py` verifies that the new source seats are
rigid copies of the existing seat before reusing its atlas UVs. It preserves their
rear-facing rotation, recooks matching collisions, keeps a backup of the previous
atlas blend and leaves the artist source and existing binary assets unchanged.
The current verified package is `build/light-frame-passengers-20260912`.
Run `tools/verify-light-frame-assets.py` in a fresh Blender process with the
atlas build directory after `--`. Use `--python-exit-code 1` so Python errors fail
the command. Publish only after verification with `tools/publish-light-frame-assets.ps1`.

The atlas requires the accompanying remapped meshes: assigning it to an old mesh
without changing that mesh's UVs will display incorrect textures. Existing FCS
references to `meshes/small frame/Frame_Light.mesh` still use the legacy frame;
the atlas frame is `meshes/light_frame/modules/PC_Light_Frame.mesh`.

The repeating rust and armor UVs are clipped at tile boundaries before remapping,
with 16 pixels of padding around each atlas tile. This preserves texture pattern
density, geometry bounds and pivots, at the cost of additional triangles:
155,620 original component triangles become 191,552, plus 5,504 for headlights
and 3,904 for the two rear seats.
Armor group alternatives are not counted twice here. One shared material does
not imply one draw call: independent components retain their submeshes.

The verifier loads every binary mesh anew, checks names, materials, triangle
counts, UVs and bounds, decodes every cooked collision, verifies DDS color
preservation and checks file hashes. It also creates an inspection `.blend` and
renders under the build output's `authoring/light_frame/` directory, including rear
views of both cargo and passenger layouts. The review
scene uses calculated display normals because Blender 5.1 can crash when setting
custom corner normals on these imported meshes; the native normals remain intact
in the exported `.mesh` files and are checked for finite values.

Exporter reference: https://github.com/Lucius64/kenshi_io_blender
