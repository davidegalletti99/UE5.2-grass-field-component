# UE5 Grass Rendering Plugin

This project is a **plugin for Unreal Engine 5**, developed as part of my university thesis. The plugin enables **real-time rendering of grass fields** using **Compute Shaders**, with a focus on performance and scalability through **GPU Frustum Culling** and **Indirect Instancing**.

## ¿? Objective

The goal of this project is to implement a performant and scalable real-time grass rendering system with the following core features:

- Procedural generation of individual grass blades using compute shaders.
- Indirect instancing for efficient rendering and draw call reduction.
- GPU-based frustum culling to discard non-visible instances before rendering.
- Full integration as a UE5 plugin usable directly in any C++ project.

---

## ¿ Technologies Used

- **Unreal Engine 5**
- **C++** (for engine-level integration)
- **HLSL** (for compute shader logic)
- **Shader Model 6+**
- **Render Dependency Graph (RDG)** for optimized rendering control
- **Indirect Draw Calls** using `DrawIndexedPrimitiveIndirect`

---

## ¿? Plugin Architecture

- **GrassComponent**: A custom component that can be attached to any actor in the scene.
- **GrassComputeShader**: Responsible for procedurally generating grass blade data on the GPU.
- **FrustumCullingCS**: Compute shader that performs GPU-based frustum culling.
- **IndirectDrawBuffer**: Stores instance draw parameters dynamically generated at runtime.

---

## ¿? How to Build and Use

### Prerequisites

- Unreal Engine 5 installed (recommended version: 5.1 or higher)
- A UE5 C++ project setup

### Steps

1. Clone or copy this repository into your project's `Plugins` folder.
2. Regenerate project files (`Right-click` on `.uproject` ¿ **Generate Visual Studio project files**).
3. Open the project in Visual Studio and build it.
4. Open Unreal Engine and enable the plugin from the **Plugins** menu.
5. Add the `GrassComponent` to any actor in your level to start rendering grass.

---

## ¿? Screenshots / Demo

*(Insert your screenshots or demo videos here)*

---

## ¿? References

- [Unreal Engine Documentation ¿ Custom Shaders](https://docs.unrealengine.com/)
- Research articles on real-time rendering, GPU culling, and indirect instancing
- Developer blogs and GPU programming tutorials

---

## ¿? License

This project is for academic and research use. You are welcome to explore, reference, or extend it for non-commercial purposes.

