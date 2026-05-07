# Vulkan/DX12 RHI & Renderer

A backend-agnostic rendering hardware interface with Vulkan and DirectX 12 implementations, built around a GPU-driven indirect rendering pipeline. Currently focused on parsing and rendering SMB archives from **Oddworld: Stranger's Wrath HD** (PC) and the original Xbox release.

---

## Features

### Rendering Hardware Interface
- Dual backend support: Vulkan and DirectX 12, selectable at compile time
- GPU-driven indirect rendering pipeline with compute-based culling
- Double-buffered command update system for low-latency CPU/GPU synchronization
- Custom linear allocator stack with per-frame, static, and per-draw allocation tiers
- Backend-agnostic resource descriptions translated to native API barriers in the hot loop
- Enhanced Barriers (DX12) and explicit pipeline barriers (Vulkan) for correct synchronization
- Typed handle pool with runtime validation for all driver/COM objects

### GPU Culling & Scene Management
- Compute shader frustum culling using AABB plane testing (Arvo transform method)
- Sphere-frustum culling for secondary rejection
- Atomic compaction of visible/invisible draw commands into indirect draw buffers
- Uniform grid spatial partitioning for per-mesh light assignment
- Per-mesh point light, spot light (cone-sphere intersection), and directional light culling in a single compute pass

### Vertex Pipeline
- Programmable vertex pulling via raw byte buffer with no fixed vertex layout
- Runtime vertex format decoding supporting 125+ vertex format variations without PSO explosion
- Oddworld proprietary vertex compression formats fully reversed and implemented:
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
- GR2 (Granny) skeleton data extraction with joint local transforms, parent hierarchy, bind pose
- Generic vertex description system with compression metadata
- AABB and bounding sphere per mesh for culling

---

## Architecture

### Backend Abstraction
Resource descriptions are expressed in a backend-agnostic layer. Barrier semantics, pipeline states, and shader resource layouts are described once and translated to native API calls at submission time. This keeps barrier management correct across both backends without duplicating logic.

### Render Graph
Attachment graphs express render pass dependencies and resource lifetimes. Pipeline state objects and shader resource layouts are declared separately from draw submission, allowing the renderer to manage resource transitions automatically.

### Memory Model
Three allocation tiers:
- **PERFRAME** data valid for one frame, automatically recycled
- **STATIC** load-time allocations that persist for resource lifetime  
- **PERDRAW** per-draw scratch allocations within a frame

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
| GR2 skeleton parsing | In progress |
| GPU skinning | Planned |
| Backend unification | In progress |

---

## Technical Notes

**Why programmable vertex pulling over fixed layouts?**
The Oddworld data contains 125+ distinct vertex format variants. PSO permutation per format is not feasible. Runtime branching on a uniform `vertexComponents` flag per draw call avoids warp divergence within a draw while handling the full format space.

**Why atlas shadow maps?**
Packing multiple shadow maps into a single atlas reduces descriptor overhead and allows shadow map assignment to be resolved during the GPU culling pass rather than requiring a separate CPU-side pass.

**Vertex compression formats** were reversed from the SMB binary data using Ghidra and cross-referenced against known quantization constants in the Oddworld modding community's prior work. All decompressors were implemented from first principles.

---

## Building

*Build instructions to be added.*
