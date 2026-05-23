# Vulkan/DX12 RHI & Renderer

A backend-agnostic rendering hardware interface with Vulkan and DirectX 12 implementations, built around a GPU-driven indirect rendering pipeline. Currently focused on parsing and rendering SMB archives from **Oddworld: Stranger's Wrath HD** (PC) and the original Xbox release.

---

## Features

### Rendering Hardware Interface (WIP)
- Dual backend support: Vulkan and DirectX 12, selectable at compile time
- GPU-driven indirect rendering pipeline with compute-based culling
- Double-buffered command update system for low-latency CPU/GPU synchronization
- Custom linear allocator stack with per-frame, static, and per-draw allocation tiers
- Backend-agnostic resource descriptions translated to native API barriers in the hot loop
- Enhanced Barriers (DX12) and explicit pipeline barriers (Vulkan) for correct synchronization
- Typed handle pool with runtime validation for all driver/COM objects

### GPU Culling & Scene Management
- Compute shader frustum culling 
- Atomic compaction of draw commands into indirect draw buffers
- Uniform grid spatial partitioning for per-mesh light assignment
- Per-mesh point light, spot light, and directional light culling in a single compute pass

### Vertex Pipeline
- Programmable vertex pulling via raw byte buffer with no fixed vertex layout
- Runtime vertex format decoding supporting multiple vertex format variations without PSO explosion
- Use vertex compression formats fully reversed and implemented from OddWorld:
  - 6-byte quantized positions (AABB-relative fixed point, signed 16-bit per axis)
  - 10-10-10-2 packed normals
  - 10-10-10-1 packed tangents with handedness bit
  - Fixed point UV coordinates (signed 16-bit with quantization scale)
  - 8-bit per channel vertex colors
- Separate compressed and uncompressed decode paths with shared decompressor functions

### Shadow Mapping
- Atlas-based shadow mapping with viewport transform baked into vertex shader
- Per-mesh shadow map assignment via renderable index indirection
- NDC-to-atlas tile transform for multi-light shadow atlases

### Asset Pipeline
- SMB archive parsing for Oddworld: Stranger's Wrath HD and original Xbox
- GR2 (Granny) skeleton data extraction 
- Generic vertex description system with compression metadata
- AABB and bounding sphere per mesh for culling

---

## Architecture

### Backend Abstraction
Resource descriptions are expressed in a backend-agnostic layer. Barrier semantics, pipeline states, and shader resource layouts are described once and translated to native API calls at submission time. This keeps barrier management correct across both backends without duplicating logic.

### Render Graph
Attachment graphs express render pass dependencies and resource lifetimes. Pipeline state objects and shader resource layouts are declared separately from draw submission, allowing the renderer to manage resource transitions automatically.

---

## Current Status

| System | Status |
|---|---|
| Vulkan backend | Working |
| DX12 backend | In progress |
| GPU frustum culling | Working |
| Atlas shadow mapping | Working |
| Vertex decompression | Working |
| SMB archive parsing | Working |
| GR2 skeleton parsing | Working |
| GPU skinning | Planned |
| Backend unification | In progress |

---

## Technical Notes

**Why programmable vertex pulling over fixed layouts?**
The Oddworld data contains 125+ distinct vertex format variants. PSO permutation per format is not feasible. Runtime branching on a uniform `vertexComponents` flag per draw call avoids warp divergence within a draw while handling the full format space.

**Why atlas shadow maps?**
Packing multiple shadow maps into a single atlas reduces descriptor overhead and allows shadow map assignment to be resolved during the GPU culling pass rather than requiring a separate CPU-side pass.

**Vertex compression formats** were reversed from the SMB binary data using Ghidra and cross-referenced against released PDBs.

---

## To-do list

Clean-up Vulkan and add explicit error handling mechanism plus fallback when things fail.
- Add error strings and error return codes
- Remove all stl / unecessary bloat and bad design decisions / turn over any render interface data stored in class back to render instance
- Implement TSLF allocator to be used by VkAllocationCallbacks
- Support programmable requests for specific features via the agnostic render interface
- Finish multi threaded queue support and return queue management to the render interface
- Update vulkan to use descriptor heap extension, timeline semaphores and use coherent, explicit system to manage dependencies in command execution

Finish render instance based on minimum specs for both Vulkan and DX12
- Finish pipeline data updating
- Finish resource transition and layout management
- Create feature request system
- Make robust all hardcoded bounds checking, add failure designation to all possible failure
- Fix messed up memory allocations and allow user to instantiate with explicit bounds and sizes
- Update all systems to conform with changes within Vulkan class

Finish app level architecture
- Finish OS systems in windows and remove bad design decisions
- Create OS linux layer with what is already done in agnostic interface
- Make robust the shader translation layer and have compute shader execute in uniform way (minimize divergence among wave invocations)
- Try to finish any unimplemented paths in the shaders (uncompressed data loading, light assignment and culling, etc.)
- Finish build system and manage the Vulkan dependency properly

## Longer term to-do list

- DX12 unification and finish sample code for what I have currently (https://github.com/crypt4489/DirectX12-Stuff)
- Add more math functionality (especially what is available in the PS2 repo (https://github.com/crypt4489/PS2-Development)
- Create deferred deletion queues and pool allocators for all gpu buffers to allow O(1) deletion and insertions
- Move over to industry standard SPIR-V reflection as DX12 migrates to SPIR-V compilation
- Move shadow map assignment to GPU with priorities, adaptive tile resizing and allocation
- GPU skinning and (at least initially) have GPU skinning be done with granny data (and eventually migrate to own implementation and conversion)
- Finish materials and add PBR system and other blending and texture mapping techniques for effects
- Advanced post processing (blur per object, TAA/FXAA, etc.)
- Ray traced shadows instead of shadow mapping and attempt tiled drawing in the compute shader
- Create minimialistic (at least presentation layer, maybe no interactivity) UI system with compute draw list generation
- Write parser for GLTF/GLB to load in other scenes, possibly finish geometric SMB specs for Oddworld


---

## Building

*Build instructions to be added.*
