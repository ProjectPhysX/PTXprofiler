# PTXprofiler
A simple single-source profiler to count Nvidia [PTX assembly](https://docs.nvidia.com/cuda/parallel-thread-execution/) instructions of [OpenCL](https://github.com/ProjectPhysX/OpenCL-Wrapper)/CUDA kernels for roofline model analysis.

## Compile
- on Windows: compile with Visual Studio Community
- on Linux: run `chmod +x make.sh` and `./make.sh path/to/kernel.ptx`

## Usage
1. Generate a `.ptx` file from your application; this works only with an Nvidia GPU. With the [OpenCL-Wrapper](https://github.com/ProjectPhysX/OpenCL-Wrapper), you can simply uncomment `#define PTX` in [`src/opencl.hpp`](https://github.com/ProjectPhysX/OpenCL-Wrapper/blob/master/src/opencl.hpp#L4) and compile and run. A file `kernel.ptx` is created, containing the PTX assembly code.
2. Run `bin/PTXprofiler.exe path/to/kernel.ptx`. For [FluidX3D](https://github.com/ProjectPhysX/FluidX3D) for example, this table is generated:
```
kernel name                     |flops  (float int    bit  )|copy  |branch|cache  (load  store)|memory (load  cached store)
--------------------------------|---------------------------|------|------|--------------------|---------------------------
initialize                      |   283    129     61     93|    33|     6|     0      0      0|   135     35      0    100
stream_collide                  |   363    261     35     67|    23|     2|     0      0      0|   153     77      0     76
update_fields                   |   160     56     37     67|    21|     2|     0      0      0|    93     77      0     16
voxelize_mesh                   |   170     91     34     45|    40|    11|    84     48     36|    37     36      0      1
transfer_extract_fi             |   460      0    221    239|   122|    63|     0      0      0|   180     80     20     80
transfer__insert_fi             |   483      0    247    236|   115|    47|     0      0      0|   180     80     20     80
transfer_extract_rho_u_flags    |    47      0     39      8|    23|     1|     0      0      0|    68     34      0     34
transfer__insert_rho_u_flags    |    47      0     39      8|    23|     1|     0      0      0|    68     34      0     34
```
3. For each [OpenCL](https://github.com/ProjectPhysX/OpenCL-Wrapper)/CUDA kernel, instructions are counted and listed:
   - GPUs compute floating-point, integer and bit manipulation operations on the same ALUs, so they are counted combined as `flops`, but also listed separately as `float`, `int` and `bit`.
   - Data movement operations are listed under `copy`.
   - Branches are listed under `branch`.
   - Total shared/local memory (L1 cache) accesses in Byte are listed under `cache`, with separate counters for `load` and `store`.
   - Total global memory (VRAM) accesses in Byte are listed under `memory`, with separate counters for `load`, `cached` (load from VRAM or L2 cache) and `store`.
4. You can use the counted `flops` and `memory` accesses, together with the measured execution time of the kernel, to place it in a [roofline model](https://en.wikipedia.org/wiki/Roofline_model) diagram.

## Limitations
- Matrix/tensor operations are not yet supported.
- Non-unrolled loops are only counted for one iteration, but may be executed multiple times, duplicating the number of actually executed instructions inside the loop.