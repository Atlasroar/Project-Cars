# Project Cars FCS setup

There are 14 workbench crafts: the frame kit and 13 reusable component items. Vehicle Mechanics I unlocks all of them. The 43 positional armor ITEM records are removed, including their crafting and research references. The user-saved deletions are preserved.

Standard Wheel fits any of the four wheel mounts. Seat is a generic item; its installed slot assigns driver or passenger duty. Headlights, engines and other modules are component designs that may fit multiple vehicle frames. Frame-specific visual adapters supply mesh choice and mount position.

Armor has just three inventory items: Light Armor Plate, Medium Armor Plate and Heavy Armor Plate. Each can supply any compatible armor mount; individual installed panels may still use separate meshes for damage. Initial tuning in bindings.json is 100 HP / 2 kg, 175 HP / 4 kg and 275 HP / 7 kg respectively. FCS inventory weights are set; installed durability and vehicle mass effects still require runtime integration. All three currently share a representative inventory mesh/icon.

Your original frame-kit, gasoline-engine, workbench and research recipes are preserved. New recipes, weights and inventory sizes are provisional. Fuel refining has not been added.

The frame remains 10-Project Cars.mod and consumes exactly one kit. It still starts bare. The Mechanic start is unchanged.

Vehicle_Atlas (11-Project Cars.mod) is the MATERIAL_SPEC record for building parts. Inventory items use Vehicle_Atlas_Items (140-Project Cars.mod), a MATERIAL_SPECS_CLOTHING record required by FCS. Both reuse the same atlas diffuse and normal maps. PC_Light_Atlas_ItemMask.dds packs metalness into blue and disables character recoloring with zero red/green channels. The Blender vehicle still has one shared atlas material.

The 67 BUILDING_PART records are visual definitions, not inventory items or craft entries. Group meshes remain alternatives to the individual visual pieces. Their placements and exact record IDs are in bindings.json.

bindings.json is an integration manifest; the current runtime DLL does not consume it yet. These records do not implement installation, driving, combat damage, paint swapping or working beams. In-game crafting, dropped items and collision need testing.

## Component item IDs and recipes

- **Light Armor Plate** — `143-Project Cars.mod`; 1 plating, 0.25 iron.
- **Cargo Crate** — `131-Project Cars.mod`; 2 iron, 1 building.
- **Control Assembly** — `132-Project Cars.mod`; 2 iron, 1 steel.
- **Gasoline Engine** — `21-Project Cars.mod`; 4 iron, 4 steel, 2 electrical.
- **Front Rack** — `133-Project Cars.mod`; 2 iron, 1 steel.
- **Spiked Front Bumper** — `134-Project Cars.mod`; 3 iron, 3 steel.
- **Fuel Tank** — `135-Project Cars.mod`; 2 iron, 1 steel.
- **Seat** — `136-Project Cars.mod`; 1 iron, 2 fabric, 1 leather.
- **Steering Wheel** — `137-Project Cars.mod`; 1 iron, 1 steel.
- **Standard Wheel** — `138-Project Cars.mod`; 2 iron, 1 steel, 1 fabric.
- **Headlights** — `139-Project Cars.mod`; 1 iron, 2 electrical, 1 copper.
- **Medium Armor Plate** — `141-Project Cars.mod`; 2 plating, 0.5 steel.
- **Heavy Armor Plate** — `142-Project Cars.mod`; 3 plating, 1 steel.

## First game checks

1. Research Vehicle Mechanics I, or use the existing Mechanic start.
2. Build the Vehicle Workbench and confirm its 14 crafts include Standard Wheel, Seat, Headlights and only three armor types.
3. Craft a component and check its ingredients, icon, inventory footprint and dropped model.
4. Craft a frame kit and construct the frame from the build menu. It should still be bare.
5. Save and reload to confirm the crafted items and frame persist.
