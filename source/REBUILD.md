# Rebuilding Towerbound Static Recomp Playtest 17

The player needs only the packaged x64 executable, its launcher art and a Gauntlet Legends USA revision 0 .z64. The following instructions are for developers building the executable.

## Dependencies

Use x86-64 Linux with Python 3.11 or newer, GCC, G++, CMake, Git, GNU ld with the i386pep target, and libLLVM.so.19.1. Python's standard library handles cartridge extraction. Build 24's assembler bridge uses LLVM to produce Windows COFF objects. The game uses Win32, XInput and waveOut. The optional Vulkan output path loads the system Vulkan driver at runtime.

## Generate and compile

Run the following in this source directory. Substitute the path to your own cartridge in the generation step. Build in a private working directory because the generator writes ROM-derived code there.

```sh
git clone --recursive https://github.com/N64Recomp/N64Recomp.git N64Recomp
git -C N64Recomp checkout ffb39cdad1da5de07eaaa48bd1db4a89a7986771
git -C N64Recomp submodule update --init --recursive
git -C N64Recomp apply ../recomp/N64Recomp.patch
cmake -S N64Recomp -B N64Recomp/build -DCMAKE_BUILD_TYPE=Release
cmake --build N64Recomp/build --target N64RecompCLI RSPRecomp -j4
python3 recomp/generate.py '/path/to/Gauntlet Legends (USA).z64' N64Recomp/build/N64Recomp
python3 build_win.py
```

Place TowerboundRecomp.exe beside the four .bgr files supplied in the playtest. Extract it to a different folder from Build 24 to give it its own settings and Controller Pak.

## Execution

The generator verifies the cartridge SHA-256 and the synthetic decompressed code image. The function map covers game, Transitions, Tower, Boss and the game tail. The boot ranges are instruction-aligned intervals observed on the USA loader path, relative to ROM offset 0x1000. The process runs the pinned, patched N64Recomp and adapts its simple scalar assignments to the Towerbound register file. For bus, floating point, branches and remaining operations it specializes Towerbound's MIPS semantics with build-time constants. GCC emits the native x64 operation bodies. Identical instruction encodings share code. The program counter, delay slot, hardware clock and interrupts are preserved at instruction boundaries.

The fast lookup is indexed by guest instruction address and validates the complete instruction encoding using a bijective 32-bit signature. Code copied into exception vectors or executed from an alias can resolve to the same native body through the second index. An unknown instruction stops the playtest and records its address. The executable has no CPU interpreter fallback or runtime machine-code generator. The original idle loop optimization advances pairs of guest instructions up to the next scheduled event.

Gauntlet-log.txt reports native dispatches, copied-address dispatches (included in native), missing translations and dispatches in the Tower address range. Counters are hexadecimal 32-bit values.

## Provenance

Towerbound Build 24 supplies the Windows host, scheduler, display, audio and input implementation. The Gauntlet symbol map, extraction utility, N64Recomp configuration and patch come from timboat/gauntlet-legends-recomp at 0674514c1db676ee11f30d9063d0a40c915a3133. The included MIT notices are in recomp/. The MusyX implementation retains its license in musyx/.

## Repeat the headless comparison

The tests exercise the actual packaged PE with simulated OS audio and controller devices on x86-64 Linux. The baseline uses Build 24's game input API. The candidate uses its XInput polling and controller mapping. Install the Python pefile and Pillow packages for these tests. From the distribution root run the following commands.

```sh
gcc -fPIC -shared -O2 -pthread tests/host_bridge.c -o tests/bridge.so
python3 tests/pe_smoke.py TowerboundRecomp.exe '/path/to/Game.z64' candidate_frames
python3 tests/pe_smoke.py '/path/to/Build24/GauntletLauncher.exe' '/path/to/Game.z64' reference_frames
```

The harness checks the ROM validator against the supplied cartridge and a one-byte mutation, records screen hashes, and computes a hash of all submitted audio samples. Add --neutral to hold the stick centred while retaining button presses. Windows window management, GPU drivers, speakers and physical controllers require an on-device test.


## Level entry and sound in Playtest 02

The USA symbol map now includes the 113-instruction routine at 0xE00977D0 through 0xE0097990. The previous range ended immediately before this routine. RSPRecomp now translates the resident USA audio microcode from the same privately generated code image. Its native x64 vector code reads and writes the cartridge runtime's big-endian RDRAM. The vector-unit headers come from N64ModernRuntime revision e4342ae030a9d94aa97a81aafd3368ed7daa74df, with their original notices and COPYING included under rsp/. The legacy MusyX path remains available for other task descriptors. Native and other audio task counters are included in the session log.

To repeat the level transition, use tests/mountain_valley.py with the same arguments as pe_smoke.py. For the reported instruction specifically, tests/level_entry.py re-enters the routine in a booted guest and checks its return and buffer writes. Passing --expect-stop with Playtest 01 verifies the logged failure. Build 24 is the CPU reference. To run the queue stall test from the release root, run gcc -O2 tests/audio_queue.c -o audio_queue followed by ./audio_queue.

## Playtest 03 Vulkan graphics

The world-resolution pass uses the original visibility buffer and projected triangles to shade opaque geometry at the selected output height. It evaluates coverage and perspective-correct texture coordinates per output pixel. The original framebuffer supplies untracked surfaces, transparent effects and overlays. Anisotropic filtering samples along the projected texture footprint with a selectable tap limit. The second compute pass applies optional FXAA. GPU work uses a fence so a busy device can repeat the previous completed frame.

Rebuild the embedded shader with python3 compile_shader.py (libshaderc.so.1) before running build_win.py if you change gpu.comp. Source and shader storage structures must agree. The Original setting uses the previous frame-scaling path. Graphics configuration version 5 accepts versions 1 through 4 and writes the new settings beside the executable.

## Playtest 04

The 2160p buffers support up to 5760 by 2160 at the renderer's maximum aspect ratio. The Vulkan storage block stays below 128 MiB. Set window resolution independently of world resolution. F1 uses an owned, modeless Win32 window and the main game message loop. The graphics controls dispatch to the same settings functions as startup. The F3 graph samples QPC when a new presentation buffer reaches WM_PAINT.

The USA MusyX snd_master_volume wrapper dispatches the music master at E008E058 and the effects master at E008E078. The runtime scales each target on its verified call instruction before the game's existing fade logic. The normal 100% gain leaves both targets intact.

The launcher requires the standard Windows common controls library for its controller trackbar. New preferences are saved in GFX v6. The previous GFX v5 settings migrate with a 15% analogue movement deadzone, full mixer levels and overlay off.

To run Pak, deadzone, graphics migration and overlay checks from the distribution root on Linux, compile tests/pak_archive.c with GCC -O2 -fshort-wchar -fno-builtin -ffunction-sections -fdata-sections -Wl,--gc-sections. Run the resulting test in an empty temporary directory.

## Playtest 05

The HD compute pipeline checks native texel visibility before the high-resolution colour pass. A separate optional bilinear pass writes a window-sized image for readback when the window is smaller than the requested world resolution. The host retains distinct internal and bitmap dimensions. A later timer tick can consume a completed Vulkan submission independently of the guest scan counter. Redraws are scheduled on changes to picture_version.

High-resolution mode sets a minimum audio refill target of 160 ms. When the queue is still below 56 ms after the refill slice, the timer skips video work to give audio the next slice. Run tests/hd_audio_queue.c both without arguments and with the argument hd to compare the normal and high-resolution targets in the simulated callback stall test.

## Playtest 06

The Windows message pump services gameplay on a 3 ms high-resolution-clock deadline. The existing audio clock still paces guest execution. An ordinary window timer services the game during nested Win32 modal loops. The renderer retries a deferred submission until a newer guest scan is available or the current scan is submitted. The high-resolution renderer bounds guest texture memory transfers to the address ranges reachable through visible triangle tile addressing and uploads contiguous 16 KiB pages. Each submitted frame uses fresh copies of these pages.

The three-second perf lines use wall time. swaps/s counts changes of the VI colour-buffer origin on accepted scans, scans/s counts captured VI scans, gpu_submits/s counts queue submissions, and paints/s counts distinct presented buffer versions seen by WM_PAINT. deferred_polls counts submission attempts that found an unsignalled fence. game_cpu_ms/s includes guest execution, original rasterization and audio production. gfx_host_ms/s and paint_ms/s measure wall time inside host presentation and painting. These are host measurements; they do not directly measure GPU execution time. texture_KiB covers guest texture bytes in the most recent HD submission. held_scans and underruns report audio pressure.

## Playtest 07

The CPU rasterizer evaluates four adjacent pixels at once and handles wrapped CI4 texture tiles in batches. The instruction dispatcher caches precompiled operations by address and instruction word. It checks both on each hit so copied or overwritten guest code is revalidated.

Run tests/intro_performance.py with the same arguments and host bridge as tests/pe_smoke.py. It lets the opening cinematic play after confirming the startup prompts, enables widescreen HD geometry capture, and prints a checksum of the colour and triangle-owner buffers every 60 scans. The final line includes the hash of submitted audio. The timing column measures host process CPU seconds and excludes GPU presentation.

## Playtest 08

A persistent CPU worker takes alternating rows from batches of up to 32 triangles sharing draw state. The main thread keeps the other rows. Both preserve original draw order on their own rows and finish before the next state change, overlapping guest read, or display-list yield. Batches below an estimated 8,192 pixels run on the main thread. Overlapping colour, depth and texture storage use the serial path. The worker sleeps between bursts. The native rasterizer also shades four eligible opaque CI4 pixels together and masks writes to transparent or uncovered pixels.

The runtime coalesces taken iterations of the verified USA cache-writeback loop on coherent guest memory. It checks the instruction words and loop registers, advances the guest Count register and stops before pending hardware events. tests/writeback_timing.py compares the resulting register, timer and interrupt checksums with the previous executable.

TowerboundRecomp.exe enables the second rendering thread. Play - Single Thread.cmd sets TOWERBOUND_RASTER_THREADS to 1 for one launch. If worker creation fails, the executable continues on one thread. The startup log records the selected mode, and split_batches and split_tris count work handed to the worker per reporting interval.

Run tests/intro_performance.py with --two-threads to exercise the Windows worker through the pthread adapters. tests/raster_lifecycle.py checks creation failure and repeated shutdown. tests/raster_compare.py uses the same executable, ROM, output directory and --two-threads flag to fork four trials from an identical guest state. Run that last test without the thread flag on Playtest 07 for a reference. Each interval advances exactly one guest scan from scan 1320 through 1500, excluding GPU presentation and physical audio playback.

Playtest 09 adds the tabbed Win32 launcher, keyboard bindings, HD frame handoff and revised audio rebuffering. See tests/verified-run.txt for the release checks.
