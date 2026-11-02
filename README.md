# tinyxell

Launches a libxenon homebrew ELF from a `.xex` on an RGH/JTAG Xbox 360, without the full XeLL menu, fuse dump, or
network stack. It does one thing: load the homebrew and run it.

Based on [XellLaunch2](https://github.com/ExposureMG/XellLaunch2) (the hypervisor calls and HVPP expansion) and
[xell-reloaded](https://github.com/Free60Project/xell-reloaded) (the CPU catch and jump, used from the submodule).
Built with [xsdk](https://github.com/xenosworks/xsdk) instead of the official Xbox 360 SDK.

## How it works

1. **The launcher** (`launcher/`) is an xsdk `.xex` title that runs on the dashboard like any homebrew.
2. It checks for the FreeBoot syscall 0 backdoor (RGH/JTAG/RGLoader) or installs the HVPP expansion as a fallback.
3. It reads a homebrew `.elf` or `.elf32` from beside the `.xex` (e.g. `snes9x.elf` next to `tinyxell.xex`).
4. It parses the ELF, flattens its loadable segments into physical memory below 0x1C000000, and fills in the handoff
   struct inside the stage1 blob.
5. It has the hypervisor copy stage1 to 0x1C000000 and jump there.
6. **Stage1** (`stage1/`) catches all six CPU threads away from the running kernel, copies the homebrew image down to
   its load address (physical 0 for standard libxenon titles), clears its BSS, and jumps to its entry point.

If anything fails, it shows a message box and returns to the dashboard.

## Building

### Prerequisites

- **libxenon toolchain** (`xenon-gcc` in `$PATH`) — for stage1, which uses xell-reloaded's lv1 sources directly.
- **xsdk** built (`make` in the xsdk checkout), with its toolchain at `~/xsdk-gcc` (or set `$XSDK_TOOLCHAIN`).
- CMake ≥ 3.25 and Ninja.

### Steps

```
git submodule update --init
make XSDK_ROOT=../xsdk
```

That builds:
- `stage1/build/stage1.bin` — the raw stage1 blob (≤ 16 KB), and `stage1.h` for the launcher to embed.
- `build/tinyxell.xex` — the launcher title.

### Using it

Copy `tinyxell.xex` and the homebrew ELF to the same folder on the console (USB, hard disk, DVD, or flash). The
homebrew file must be named after the `.xex` with a `.elf` or `.elf32` extension:

```
tinyxell.xex
tinyxell.elf        (or tinyxell.elf32)
```

Or rename the `.xex` to match the homebrew:

```
snes9x.xex          (a renamed copy of tinyxell.xex)
snes9x.elf32
```

Launch the `.xex` from the dashboard. The log (`tinyxell.log` beside the `.xex`) says what happened.