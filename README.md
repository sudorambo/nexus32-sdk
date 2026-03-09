# nexus32-sdk

Game SDK for the NEXUS-32 fantasy console: C compiler (nxcc), assembler (nxasm), linker (nxld), standard library (libnx), asset tools, and build orchestrator (nxbuild). Conforms to [NEXUS-32 specification](../nexus32-spec/NEXUS32_Specification_v1.0.md) §10 and §13 and the [implementation checklist](../nexus32-spec/docs/implementation-checklist.md).

## Build

Requirements: C17 compiler (GCC/Clang). CMake 3.14+ optional.

```bash
# Build host tools (from repo root)
gcc -std=c17 -Wall -o build/nxasm compiler/nxasm/main.c
gcc -std=c17 -Wall -o build/nxld linker/nxld/main.c
gcc -std=c17 -Wall -o build/nxcc compiler/nxcc/main.c
gcc -std=c17 -Wall -o build/nxbuild tools/nxbuild/main.c
gcc -std=c17 -Wall -o build/shaderc tools/shaderc/main.c
gcc -std=c17 -Wall -o build/img2tex tools/img2tex/main.c
gcc -std=c17 -Wall -o build/obj2mesh tools/obj2mesh/main.c
gcc -std=c17 -Wall -o build/wav2smp tools/wav2smp/main.c
gcc -std=c17 -Wall -o build/map2lvl tools/map2lvl/main.c
# Build libnx.a (assemble lib stubs, then archive)
for f in lib/src/*.asm; do ./build/nxasm -o "${f%.asm}.nxo" "$f"; done
ar rcs lib/libnx.a lib/src/*.nxo
```

Or use CMake: `mkdir build && cd build && cmake .. && make`.

**Tools:** nxasm, nxld, nxcc, nxbuild, shaderc, img2tex, obj2mesh, wav2smp, map2lvl.

**Lib modules (lib/include + lib/src):** sys, gfx, input, audio, math, mem, asset, save, debug; nx_vec.h (vector intrinsics per spec §2.4).

## Assembler (nxasm)

AT&T-style assembler; outputs NEXUS-32 object files (`.nxo`).

```bash
nxasm -o foo.nxo foo.asm
```

Supports: `.section .text` / `.data` / `.rodata` / `.bss`, `.global name`, labels, and integer mnemonics per spec §2 and [encoding-tables/integer-instructions.csv](../nexus32-spec/encoding-tables/integer-instructions.csv).

## Linker (nxld)

Links `.nxo` files and `libnx.a` into a `.nxbin` (or code/data blobs for rompack).

## Compiler (nxcc)

Cross-compiles C to NEXUS-32. Use with nxld and rompack to produce `.nxrom`.

## Minimal C workflow

```bash
# Compile C to .nxo
nxcc -c main.c -o main.nxo
# Link with libnx (resolve jal to sys_*, gfx_*, etc.)
nxld -o game.nxbin main.nxo -L lib -lnx
# Pack to ROM (use rompack from nexus32-romtools)
rompack -o game.nxrom game.nxbin  # or pack.toml flow
```

Link order: put your .nxo first so entry is your code; use `-L lib -lnx` to pull in sys, gfx, input. Lib provides sys_init, sys_vsync, sys_set_irq_handler, sys_frame_count, sys_cycles_used, sys_halt, gfx_init, gfx_clear, gfx_present, input_buttons.

## nxbuild

From a project with `build.toml`:

```bash
nxbuild --config build.toml
```

Minimal build.toml: `name`, `entry`, `sources` (space-separated .c/.asm), `screen_width`, `screen_height`, `cycle_budget`. Runs nxcc/nxasm, nxld with -lnx, then rompack if available.

## Asset tools

- **shaderc** — Mini shader compiler: `shaderc -o out.shd input.shader` (one opcode per line: MOV, ADD, … NOP).
- **img2tex** — BMP to .tex: `img2tex -o out.tex in.bmp`.
- **obj2mesh** — OBJ to .mesh: `obj2mesh -o out.mesh in.obj`.
- **wav2smp** — WAV to .smp: `wav2smp -o out.smp in.wav`.
- **map2lvl** — Text map to .lvl: first line `W H`, then W×H tile IDs: `map2lvl -o out.lvl in.txt`.

## Conformance

ROM output targets format_version 0x0100; instruction encodings follow spec §2 and the encoding tables; shader tooling follows spec §5.6 and shader-opcodes.csv.
