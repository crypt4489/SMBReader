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
- Add error strings and error return codes (done)
- Remove all stl / unecessary bloat and bad design decisions / turn over any render interface data stored in class back to render instance (done)
- Implement TLSF allocator to be used by VkAllocationCallbacks (done)
- Support programmable requests for specific features via the agnostic render interface (done)
- Finish multi threaded queue support and return queue management to the render interface (done)
- Update vulkan to use descriptor heap extension, timeline semaphores and use coherent, explicit system to manage dependencies in command execution (timeline and explicit sync system done)
- Programmable GPU request and supprott multi gpu setup (done)
- Pack structs and remove all dead code within different modules (done) 

Finish render instance based on minimum specs for both Vulkan and DX12
- Create configuration struct for initialization of flat arrays and remove hardcoded bounds where appropriate (done). 
- Finish pipeline data updating and any skeleton functions (done)
- Finish resource transition and layout management and create an image tracking and initialization (allow user to specify image layout, usage by function and specify depth and layers explicitly) (almost done)
- Create mechanism for attachments layouts to be tracked and integrate with general image tracking pool (done)
- Create feature request system and make it so user can query features request (while also maintaining minimum viable feature request for this layer) 
- Make robust all bounds checking, add failure designation to all possible failure (including integration with new vulkan failure designation and error handling)
- Make shader graph have hardcoded bounds and cleanup initialization function for shader graph (no std::string, no separate allocators for shader graph and shader details, check shader bounds during compilation) (done)
- Separate window/swapchain creation from instance creation and make it explciitly controllable by app layer (done)
- Separate descriptor pool/heap creation and make it explicitly controllable by app layer and allow different heap/pool managers (done).
- Separate all vulkan specific functions or things that need to know it's using vulkan to separate file with compile type linking.
- Accurately handle fallback and failure designations at all levels.
- Correct staging buffer uploads and make it use optimal alignment and make it so that big uploads via memory tag can be chunked and do batching control there. 
- Fix all transfer/update command generation structures to be quicker and remove O(n) search (where applicable).
- Add destructor functions for various different handle creation and make use of the pool allocators.
- Create separate GPU command streams and have them be selectable for submission.
- Finish logging and error tracking in shader resource binding.
- Remove texture from vulkan layer and make the image views attachable to the render texture resource.

Finish app level architecture
- Finish OS systems in windows and remove bad design decisions
- Create OS linux layer with what is already done in agnostic interface
- Make robust the shader translation layer and have compute shader execute in uniform way (minimize divergence among wave invocations)
- Try to finish any unimplemented paths in the shaders (uncompressed data loading, light assignment and culling, etc.)
- Finish build system and manage the Vulkan dependency properly(done)

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
