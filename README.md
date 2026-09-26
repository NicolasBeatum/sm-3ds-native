# sm-3ds

<!-- ![banner](resources/ghpreview.png) -->
![ceres station on Azahar](screenshots/sm-3ds.gif)

This is a 3DS port of Super Metroid, based on [the PC port by snesrev](https://github.com/snesrev/sm).

This fork targets full-speed native gameplay on original 3DS hardware without
reducing the SNES image or audio quality. It keeps the decompiled game logic,
stereo audio and scanline effects, adds a conservative PICA200 renderer with an
exact CPU fallback, and presents a live MetroidArch-inspired companion UI on
the lower screen.

Highlights:

- Native game logic with stereo 32 kHz audio.
- PICA200 rendering for supported Mode 1 frames; automatic exact fallback for
  unsupported PPU state or mid-frame changes.
- Live lower-screen map, equipment and setup tabs with touch-selectable ammo.
- Correct HDMA rain, fog, windows and colour math without scanline filtering.
- Save files at `saves/sm.srm`, with a backup created before replacement.
- Fixed room BTS loading on ARM, including the Landing Site terrain and blue
  door collision in Crateria.

The current architecture, build flags, diagnostics and verification history
are documented in [docs/PORTING_NOTES.md](docs/PORTING_NOTES.md). Changes made
in this fork are listed in [CHANGELOG.md](CHANGELOG.md).

![title screen on Azahar](screenshots/titlescreen.png)

## Building

### Setup

```bash
# Install devkitARM - https://devkitpro.org/wiki/Getting_Started

# Clone
git clone --recurse-submodules https://github.com/CharlesAverill/sm-3ds.git

# Install bannertool
git clone https://github.com/carstene1ns/3ds-bannertool.git --depth=1 && cd 3ds-bannertool
cmake -B build && cmake --build build && sudo cmake --install build
cd ..

# Install makerom
git clone https://github.com/3DSGuy/Project_CTR.git --depth=1
make -C Project_CTR/makerom deps -j
make -C Project_CTR/makerom program -j
sudo cp Project_CTR/makerom/bin/makerom /usr/bin

cd sm-3ds

# Place your copy of Super Metroid in romfs
cp ~/Games/sm.smc romfs

# Build SDL2
make sdl

# Build the native Old 3DS release
make -j FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" 3dsx
make FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" cia
```

Optional build switches:

| Variable / define | Purpose |
| ----------------- | ------- |
| `FULL_NATIVE=1` | Run the decompiled native game logic. |
| `BUILD_FLAGS="-DSM3DS_OLD3DS"` | Enable the optimized Old 3DS timing and rendering path. |
| `LTO=1` | Enable link-time optimization for compilation and linking. |
| `-DSM3DS_PROFILE` | Write performance counters to `sdmc:/sm3ds-profile.log`. |
| `-DSM3DS_DOOR_TRACE` | Write targeted Crateria door/BTS diagnostics. |
| `-DSM3DS_EMULATED_CPU` | Diagnostic fallback to the original emulated CPU path. |

| Make Commands    | Action                                                                                    |
| -----------------| ----------------------------------------------------------------------------------------- |
| make             | 
| make 3ds         | The 3ds target will build a `<project name>.3ds` file.
| make 3dsx        | The 3dsx target will build both a `<project name>.3dsx` and a `<project name>.smdh` files.
| make cia         | The cia target will build a `<project name>.cia` file.
| make azahar      | The azahar target will build a `<project name>.3dsx` file and automatically run azahar.
| make elf         | The elf target will build a `<project name>.elf` file.
| make fbi         | The fbi target will build a `<project name>.cia` file and send it to your 3ds via [FBI].
| make hblauncher  | The hblauncher target will build a `<project name>.3dsx` file and send your 3ds via homebrew launcher.<sup>2</sup>
| make release     | The release target will build `.elf`, `.3dsx`, `.cia`, `.3ds` files and a zip file (.3dsx and .smdh only).<sup>3</sup>
