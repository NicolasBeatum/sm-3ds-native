# sm-3ds

> [!IMPORTANT]
> **Private custom multi-project fork developed with AI assistance.** This is
> not an official Nintendo, RetroArch, SDL, snesrev or CharlesAverill release.
> Nicolás Andrés Hernández Vargas directed and tested the work; OpenAI Codex
> was used extensively for code analysis, implementation, debugging,
> optimization and documentation. See [Provenance and AI
> disclosure](#provenance-and-ai-disclosure).

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
- MetroidArch-compatible lower screen with ROM-decoded room/world maps and
  ammo icons, Redux suit art, equipment percentage, touch ammo/zoom controls,
  map markers and functional per-tab setup options. SETUP opens on BUILD INFO,
  showing the compiled `BUILD_FLAGS`, port-specific defines, `FULL_NATIVE` and
  `LTO`; its PORT UI page contains the companion-screen controls.
- Correct HDMA rain, fog, windows and colour math without scanline filtering.
- Save files at `saves/sm.srm`, with a backup created before replacement.
- Fixed room BTS loading on ARM, including the Landing Site terrain and blue
  door collision in Crateria.

The current architecture, build flags, diagnostics and verification history
are documented in [docs/PORTING_NOTES.md](docs/PORTING_NOTES.md). Changes made
in this fork are listed in [CHANGELOG.md](CHANGELOG.md).

## Provenance and AI disclosure

This repository is a custom integration of several independent projects; it is
not presented as wholly original work:

| Project | Role in this repository |
| ------- | ----------------------- |
| [CharlesAverill/sm-3ds](https://github.com/CharlesAverill/sm-3ds) | Nintendo 3DS port base. |
| [CharlesAverill/sm-3ds-lib](https://github.com/CharlesAverill/sm-3ds-lib) | Native game/decompilation submodule used by the port. |
| [snesrev/sm](https://github.com/snesrev/sm) | Original Super Metroid decompilation/PC port ancestry. |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | Platform, input and audio layer. |
| [Raekwon1603/RetroArch `metroidarch-dual-screen`](https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen) | Dual-screen visual and interaction reference. |

The lower-screen implementation and bundled Redux suit data are derived from
the GPL-3.0 MetroidArch branch. See `THIRD_PARTY_NOTICES.md` for exact file and
asset provenance; the original MIT license continues to identify the license
of the CharlesAverill base rather than relicensing third-party-derived code.

The buildable custom submodules are kept in the private repositories
[`NicolasBeatum/sm-3ds-lib-native`](https://github.com/NicolasBeatum/sm-3ds-lib-native)
and [`NicolasBeatum/SDL-3DS-native`](https://github.com/NicolasBeatum/SDL-3DS-native).
Their `origin` remotes still identify the public upstream repositories, and
their custom branches contain only this port's additions.

OpenAI Codex has been used extensively throughout this custom fork. AI-assisted
work includes implementation drafts, source comparison, performance analysis,
the PICA200 and lower-screen integration, bug diagnosis, test automation and
documentation. Human direction, acceptance testing and the decision to publish
remain with the repository owner. AI assistance does not change the licenses,
copyright or required attribution of any upstream project.

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
