# Quake Source Code Reference

This directory contains the official id Software Quake source code repositories, cloned as reference material for your PaintStrike BSP and rendering pipeline implementation.

## Repository Structure

### Quake 3 Arena (`quake3_src/`)
**Primary Reference** - Most relevant for your BSP needs
- **License**: GPL v2
- **Focus**: Complete BSP compilation pipeline, PVS generation, binary file formats

#### Key Directories for BSP Implementation:

##### `q3map/` - The BSP Compiler Tool
- `vis.c/vis.h` - **PVS (Potentially Visible Set) generation algorithms**
- `portals.c` - Portal generation between BSP leaves
- `bsp.c` - Main BSP tree construction
- `tree.c` - BSP tree building algorithms
- `writebsp.c` - Binary BSP file output
- `brush.c` - Brush/face processing

##### `code/bspc/` - Alternative BSP Compiler
- `brushbsp.c` - BSP building from brushes
- `portals.c` - Portal generation
- `writebsp.c` - BSP file writing
- `map_q3.c` - Quake 3 map file parsing
- `l_bsp_q3.c` - BSP file I/O for Q3 format

##### `common/` - File Format Definitions
- `qfiles.h` - **Binary BSP file format structures**
- `bspfile.h` - BSP file loading/writing utilities
- `surfaceflags.h` - Surface/content flags

#### BSP File Format Reference (from `qfiles.h`):

```cpp
// BSP Header Structure
typedef struct {
    int ident;              // "IBSP" (big-endian)
    int version;            // 46 for Quake 3
    lump_t lumps[HEADER_LUMPS]; // Data chunk offsets/sizes
} dheader_t;

// Lump Types (HEADER_LUMPS = 17)
enum {
    LUMP_ENTITIES,      // Map entities
    LUMP_SHADERS,       // Shader definitions
    LUMP_PLANES,        // BSP splitting planes
    LUMP_NODES,         // BSP tree nodes
    LUMP_LEAFS,         // BSP tree leaves
    LUMP_LEAFSURFACES,  // Surfaces in each leaf
    LUMP_LEAFBRUSHES,   // Brushes in each leaf
    LUMP_MODELS,        // Brush models
    LUMP_BRUSHES,       // Brushes
    LUMP_BRUSHSIDES,    // Brush sides
    LUMP_DRAWVERTS,     // Drawing vertices
    LUMP_DRAWINDEXES,   // Drawing indices
    LUMP_SURFACES,      // Drawing surfaces
    LUMP_LIGHTMAPS,     // Lightmap data
    LUMP_LIGHTGRID,     // Light grid
    LUMP_VISIBILITY,    // PVS data (compressed)
    LUMP_LIGHTARRAY     // Light array
};
```

### Quake 1 (`quake1_src/`)
**Secondary Reference** - Simpler BSP system
- **License**: GPL v2
- **Focus**: Basic BSP concepts, simpler implementation

## How to Use This Reference

### 1. **Study BSP Construction**
```cpp
// Look at q3map/tree.c for BSP tree building algorithms
// Key functions: BuildTree, SelectSplitPlane, SplitNode
```

### 2. **Understand PVS Generation**
```cpp
// Study q3map/vis.c for visibility algorithms
// Key concepts: portals, flood filling, visibility testing
void CalcVis (void);           // Main PVS generation
void PortalFlow (portal_t *p); // Flood fill visibility
```

### 3. **Binary File Format**
```cpp
// Reference common/qfiles.h for file structures
// Study common/bspfile.c for loading/saving
void LoadBSPFile(const char *filename);
void WriteBSPFile(const char *filename);
```

### 4. **Map Parsing**
```cpp
// Look at code/bspc/map_q3.c for .map file parsing
// Understand brush/face conversion to BSP geometry
```

## Mapping to Your Implementation

| Your Component | Quake 3 Reference |
|----------------|-------------------|
| `BSPTree::BuildFromFaces()` | `q3map/tree.c::BuildTree()` |
| PVS Generation | `q3map/vis.c::CalcVis()` |
| Binary BSP Format | `common/qfiles.h::dheader_t` |
| BSP Compiler Tool | `q3map/` directory |
| Map Loading | `code/bspc/map_q3.c` |

## Building the Quake 3 Tools (Optional)

If you want to experiment with the actual Quake 3 BSP compiler:

```bash
cd quake3_src/q3map
make  # Linux
# or for Windows: nmake /f makefile.nt
```

This will build `q3map` - the command-line BSP compiler that can process Quake 3 .map files.

## License Notes

- **GPL v2 License**: You can study, modify, and use this code as reference
- **Not for Direct Copying**: Create your own implementation based on understanding
- **Attribution**: While not required for understanding, consider acknowledging the influence

## Next Steps

1. **Study `q3map/vis.c`** - Understand how Quake 3 generates PVS data
2. **Examine `common/qfiles.h`** - Learn the binary BSP file format
3. **Review `q3map/tree.c`** - See how BSP trees are constructed
4. **Compare with your current implementation** in `src/world/BSPTree.cpp`

This reference material provides the complete blueprint for transforming your runtime BSP system into a production-ready pipeline following industry standards.
