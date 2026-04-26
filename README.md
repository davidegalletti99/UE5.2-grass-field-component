# UE5 Grass Rendering Plugin

A UE5 plugin built for my university thesis. It renders grass fields in real time using compute shaders, with GPU frustum culling and indirect instancing to keep things fast even with a lot of blades on screen.

## What it does

Grass blades are generated procedurally on the GPU via a compute shader. Before drawing, a second compute shader culls anything outside the camera frustum, then the surviving instances are passed to an indirect draw call — so the CPU never touches the instance data per frame.

Core pieces:
- `GrassComponent` — drop it on any actor to get grass in the scene
- `GrassComputeShader` — generates blade positions and orientations on the GPU
- `FrustumCullingCS` — discards off-screen instances before the draw
- Indirect draw via `DrawIndexedPrimitiveIndirect`, driven by an `IndirectDrawBuffer` written by the culling pass

Everything goes through UE5's Render Dependency Graph (RDG).

## Setup

1. Copy the plugin into your project's `Plugins/` folder
2. Right-click your `.uproject` → **Generate Visual Studio project files**
3. Build from Visual Studio
4. Enable the plugin in UE5 under **Edit → Plugins**
5. Add `GrassComponent` to an actor and it should just work

Tested on UE5.1+, requires Shader Model 6.

## Tech

C++ for engine integration, HLSL for the shader logic.

## License

Academic project — feel free to read, reference, or build on it for non-commercial use.
