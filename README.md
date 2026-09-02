# UE5 Grass Rendering Plugin

A GPU-driven grass rendering system for Unreal Engine 5, built as the practical component of my university thesis. It combines procedural noise-based terrain generation with a fully GPU-resident grass pipeline: per-blade frustum culling, distance-based LOD, and indirect instanced drawing, all wired through the Render Dependency Graph (RDG) so the CPU never touches per-instance data on the hot path.

The project ships as a self-contained UE5 plugin (`Plugins/Grass`) plus a minimal host project used to develop and demo it.

## Highlights

- **Procedural terrain** — a compute shader samples fractal Brownian motion (fBm) noise over a grid to generate the terrain's vertex positions, normals and tangents, and triangulates it directly on the GPU. The result is read back once and fed into a `UProceduralMeshComponent`.
- **Authored grass placement** — grass blades are distributed with Poisson-disc sampling over the terrain's bounds, projected onto the surface with line traces, and packed into per-cell buffers (`FPackedGrassData`) at edit time — no runtime placement cost.
- **GPU frustum + distance culling** — every blade is tested against the view frustum and a configurable cutoff distance in a compute pass, entirely on the GPU.
- **Indirect instanced drawing** — surviving instances are written straight into an indirect-args buffer consumed by the draw call, so the number of instances actually drawn is decided by the GPU, never read back to the CPU.
- **Distance-based LOD** — blade geometry (a Bézier-curved quad strip) is pre-built at several tessellation levels; each terrain cell picks its LOD per view based on distance, interpolated with a cubic Bézier curve for a smooth falloff.
- **Spatial partitioning** — the grass field is split into a grid of sections (`Divisions × Divisions`), each with its own bounds, so whole sections can be trivially rejected by CPU-side frustum/distance checks before any GPU work is dispatched.
- **Buffer pooling** — GPU buffers for culling/instancing are recycled across frames per section/view and only released after a few frames of disuse, avoiding per-frame allocation churn.

## How it works

### 1. Terrain generation

`ATerrain` dispatches `FTerrainShader`, a compute shader (`TerrainShader.usf`) that evaluates fBm noise (`Noise.ush`) over a `Size.x × Size.y` grid at a given `Spacing`/`Scale`, computing world-space position, normal and tangent per vertex and emitting a triangle list — all in one pass, using screen-space-style finite differences for the normals. The buffers are read back and pushed into the actor's `UProceduralMeshComponent`.

### 2. Grass authoring (edit-time)

`UGrassFieldComponent` sits alongside the terrain and does its placement work in the editor, not at runtime:

- `InitSections()` splits the component's bounds into a `Divisions × Divisions` grid of `UGrassMeshSection`s, each owning a sub-box.
- `SampleGrassData()` generates candidate points with Poisson-disc sampling (spacing derived from `Density`), line-traces each one down onto the terrain mesh to find its position and surface normal, and computes a randomized blade (`GrassUtils::ComputeData`): a facing direction perpendicular to the surface normal, height/width interpolated between `Min`/`MaxHeight` and `Min`/`MaxWidth`, and a random stiffness value. Each blade is packed (`FPackedGrassData`) and bucketed into whichever section's box contains it.

The result is baked, per-section grass data that ships with the level — sampling is a `CallInEditor` workflow, not a runtime cost.

### 3. Rendering (per frame, per view)

`UGrassFieldComponent::CreateSceneProxy()` builds an `FGrassInstancingSceneProxy` holding one `FGrassInstancingSectionProxy` per grid section. On `GetDynamicMeshElements`:

1. **CPU coarse cull** *(optional)* — each section's bounds are tested against the view frustum and `CutoffDistance` before anything else happens.
2. **LOD selection** — the surviving section picks a blade mesh LOD based on distance from the camera, via a cubic Bézier remap between the configured `LodStepsRange`.
3. **GPU fine cull + instancing** — a custom renderer extension (`FGrassInstancingRendererExtension`, modeled after Epic's Virtual Heightfield Mesh plugin, hooked into the engine's pre/post-render delegates) runs three RDG compute passes per section/view:
   - `InitIndirectArgsCS` — resets the indirect draw-args buffer.
   - `CullInstancesCS` — tests every blade in the section against the 5 view-frustum planes (derived from the view-projection matrix) and the cutoff distance, appending survivors into a culled buffer and atomically incrementing the indirect instance count.
   - `ComputeInstanceGrassDataCS` — converts each surviving blade into a compact rotation/scale matrix + origin (`FGrassInstance`) ready for the vertex shader.
4. **Indirect draw** — one mesh batch per visible section is submitted through a custom `FGrassInstancingVertexFactory`, which reads the per-vertex blade shape plus the GPU-culled `FGrassInstance` buffer, and draws with the indirect-args buffer produced above — the instance count for the draw call is never known on the CPU.

## Project layout

```
Plugins/Grass/
├── Grass.uplugin              Plugin descriptor (2 runtime modules, requires ProceduralMeshComponent)
├── Shaders/Private/           HLSL: grass compute passes, vertex factory glue, terrain noise/shader
├── Source/Grass/              Gameplay-facing module: UGrassFieldComponent, ATerrain, sampling utils
└── Source/ComputeShaders/     Low-level RHI/RDG module: compute shaders, scene proxy, vertex factory

Source/GrassField/             Host project module (thin game-module boilerplate)
Content/                       Demo level, grass/terrain materials, camera-path blueprints for capture
```

Module split follows UE5 convention: `Grass` is the `Default`-phase module exposing components and actors to gameplay/Blueprint code; `ComputeShaders` loads at `PostConfigInit` so its global shaders are registered before rendering starts.

## Requirements

- Unreal Engine 5.2 (developed and tested against this version)
- A Shader Model 6 capable RHI — the compute shaders declare `ERHIFeatureLevel::SM6` as a hard requirement
- The `ProceduralMeshComponent` plugin (bundled with the engine, enabled automatically via the `.uplugin` dependency)

## Getting started

1. Copy `Plugins/Grass` into your project's `Plugins/` folder (or open this repo's `.uproject` directly to explore the demo level).
2. Regenerate project files and build (`Generate Visual Studio project files` on the `.uproject`, then build from your IDE).
3. Enable the **Grass** plugin under **Edit → Plugins** if it isn't already.
4. Place an `ATerrain` actor (or add a `UGrassFieldComponent` to your own actor) and assign a terrain surface for it to sample against.
5. In the details panel, use the `CallInEditor` buttons in order: **Init Sections** → **Sample Grass Data**. This bakes the grass placement for the current bounds.
6. Assign a grass `Material` and tune the parameters below to taste.

### Key component parameters (`UGrassFieldComponent`)

| Property | Purpose |
|---|---|
| `Terrain` | Actor providing the surface (and its collision) that grass is sampled onto |
| `Divisions` | Grid resolution the field is split into for spatial culling (`Divisions × Divisions` sections) |
| `Density` | Controls Poisson-disc sampling spacing — higher density, more blades |
| `MinHeight` / `MaxHeight`, `MinWidth` / `MaxWidth` | Range for randomized per-blade dimensions |
| `LodStepsRange` | Min/max blade tessellation (LOD) used across the cutoff distance |
| `CutoffDistance` | Distance beyond which sections/blades are culled |
| `bIsCPUCullingEnabled` | Toggles the coarse per-section frustum/distance cull |
| `bIsGPUCullingEnabled` | Toggles the fine per-blade GPU frustum/distance cull |

## Known limitations

This is thesis/research-grade code, not a shipped product — a few gaps are called out directly in the source:

- On UE5.0 specifically, shadow-pass rendering can be skipped for grass because the required two-phase `InitViews`/`InitViewsAfterPrepass` renderer hooks only landed in UE5.1.
- Occlusion culling and world-position-offset (wind sway) hooks are present in the proxy as scaffolding but are not wired up.
- Grass placement is edit-time only; there's no runtime re-sampling (e.g. for streaming or destructible terrain).

## Tech stack

C++ for engine/RHI/RDG integration, HLSL for the compute and vertex-factory shaders.

## License

Academic project — feel free to read, reference, or build on it for non-commercial use.
