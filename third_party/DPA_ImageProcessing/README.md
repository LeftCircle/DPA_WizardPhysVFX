# Image Editor

A C++ image processing library with an interactive OpenGL viewer and Python bindings via SWIG. Built on top of [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) for broad format support (EXR, PNG, JPEG, etc.).

---

## Features

### Image I/O
- Read and write images in any format supported by OpenImageIO (EXR, PNG, JPEG, TIFF, etc.)
- Per-pixel and bulk access to floating-point image data

### Image Processing
- **Gamma correction**
- **Convolution** — bounded and wrapping modes with configurable stencils
- **Histogram equalization**
- **Greyscale conversion**
- **Downscaling** with alpha-aware filtering
- **Palette matching** and **quantization**
- **Ensemble averaging** (box and Gaussian kernels)
- **Image blending**
- **Bilinear interpolation** per channel
- **Contrast unit conversion**

### Fractal Generation
- **Julia sets** with configurable center, range, iteration count, and color LUT
- **Fractal flames** using an extensible IFS (Iterated Function System) framework with many built-in variations (Sinusoidal, Spherical, and more)

### Optical Flow
- Compute dense optical flow between image sequences
- **Sequence retiming** — extend video duration by generating interpolated in-between frames
- **Stylized video** — artistic applications by manipulating flow direction and iteration count
- Per-frame and multi-frame iterative velocity field refinement

### Interactive Viewer
- OpenGL/GLUT-based viewer with keyboard controls
- MVC architecture (Model / View / Controller singletons)
- Real-time stencil application, Julia set exploration, and image export from the viewer

### Python Bindings (SWIG)
- Access `ImageData`, `ImageDataModifier`, `Stencil`, `LUT`, `IFSFunction`, and statistics utilities from Python

---

## Dependencies

| Dependency | Purpose |
|---|---|
| **OpenImageIO** | Image I/O (read/write EXR, PNG, JPEG, etc.) |
| **OpenGL / GLU / GLUT** | Interactive image viewer |
| **OpenMP** | Parallel pixel processing |
| **SWIG** | Python binding generation (optional) |
| **Python 3.12+** | Python bindings (optional) |

---

## Building

### C++ Viewer

```bash
make            # builds the 'imgviewer' executable
```

### Python Bindings

```bash
make python     # generates _image_editor.so and image_editor.py
```

> **Note:** Update `OPEN_IMG_IO_AND_EXR_INCLUDE_PATH` and `OPEN_IMG_IO_AND_EXR_LIB_PATH` in the Makefile to point to your local OpenImageIO / OpenEXR installations.

### Tests

```bash
make test       # builds and runs the test suite
```

### Clean

```bash
make clean          # remove C++ build artifacts
make clean-python   # remove SWIG-generated files
make clean-all      # both
```

---

## Usage

### C++ Viewer

```bash
./imgviewer -image <path_to_image>
```

### Python

```python
from image_editor import ImageData, ImageDataModifier

# Load an image
img = ImageData("test_images/walk_colored.exr")
print(f"{img.get_width()} x {img.get_height()}, {img.get_channels()} channels")

# Apply gamma correction
ImageDataModifier.gamma_filter(img, 2.2)

# Convert to greyscale
grey = ImageDataModifier.greyscale(img)

# Scale all pixel values
img.scale_values(0.5)
```

---

## Project Structure

```
image_editor/
├── main.cpp                  # Entry point for the OpenGL viewer
├── Makefile                  # Build system (C++, tests, SWIG bindings)
├── image_editor.i            # SWIG interface definition
├── include/
│   ├── image_data.h          # Core image container (read/write, pixel access, math ops)
│   ├── image_data_modifier.h # Static image processing functions
│   ├── image_editing.h       # High-level editor (optical flow, video retiming)
│   ├── optical_flow.h        # Dense optical flow computation
│   ├── stencil.h             # Convolution kernel
│   ├── lut.h                 # Generic look-up table with lerp
│   ├── IFSFunction.h         # IFS / fractal flame function hierarchy
│   ├── stats.h               # Ensemble statistics (average, Gaussian)
│   ├── color.h               # Color type for LUT
│   ├── point.h               # 2D point type
│   ├── file_utils.h          # Directory listing and filename parsing
│   ├── string_funcs.h        # String utilities
│   ├── model.h               # MVC Model singleton
│   ├── view.h                # MVC View singleton (OpenGL/GLUT)
│   ├── controller.h          # MVC Controller singleton (keyboard input)
│   ├── command_line_parser.h # CLI flag parser
│   └── tests.h               # Test declarations
├── src/                      # Corresponding .cpp implementations
├── test_images/              # Sample images for testing
└── example_usage.py          # Python binding demo
```

---

## Optical Flow — Background

Optical flow estimates per-pixel motion between consecutive frames. This implementation:

1. Computes spatial gradients ($\frac{\partial I}{\partial x}$, $\frac{\partial I}{\partial y}$) of the current frame
2. Computes temporal differences between consecutive frames
3. Builds a correlation matrix from ensemble-averaged gradient products
4. Solves for a per-pixel, per-channel velocity field
5. Warps the target image using bilinear interpolation of the velocity field

An **iterative refinement** loop re-estimates the velocity after each warp, converging to smoother results. Applications include:

- **Retiming** — extending a video's duration by synthesizing intermediate frames
- **Stylization** — reversing or exaggerating flow to produce artistic motion effects

