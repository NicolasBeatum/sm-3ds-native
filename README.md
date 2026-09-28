# sm-3ds-native

> [!IMPORTANT]
> **Unofficial multi-project fork developed with AI assistance.** This is
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
- Lower screen with ROM-decoded room/world maps and ammo icons, Redux suit art,
  equipment percentage, touch ammo/zoom controls and map markers. The area map
  can zoom out to show the entire area, be dragged, and return to following
  Samus with its `S` button. Drag the world map or tap a point to center it;
  its `S` button centers Samus and `N` toggles area names. SETUP can hide the
  floating map buttons. World zoom has seven levels and keeps the current map
  center in place. SETUP also has separate status-bar switches for MAP, ITEMS
  and SETUP, widescreen and main-HUD controls. Its small `i` tab shows build
  information (`BUILD_FLAGS`, port-specific defines, `FULL_NATIVE` and `LTO`).
- Correct HDMA rain, fog, windows and colour math without scanline filtering.
- ROM selector on startup. Put `.smc` or `.sfc` files in
  `sdmc:/3ds/sm3dsnative/`. Saves are kept per ROM in `saves/`, and lower-screen
  SETUP and map choices persist in `settings.cfg` in the same SD folder. The
  upper selector screen displays the game title, version and project credits,
  with NicolasBeatum prominently credited for project direction and testing.
- Hold L + R + A to write a debug report and WRAM/SRAM snapshots into
  `sdmc:/3ds/sm3dsnative/dump/`. Each new dump also includes up to 120 recent
  frame timings (measured FPS, game/render/presentation work and widescreen/GPU
  state) and attempts BMP and raw captures of both LCDs; `info.txt` records
  whether each capture succeeded. The diagnostic layout was
  inspired by [zelda-alttp-3ds](https://github.com/EstebanPdN/zelda-alttp-3ds).
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
| [Raekwon1603/RetroArch `metroidarch-dual-screen`](https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen) | Lower-screen design reference and source of derived UI code, ROM decoding behavior and Redux suit data. |

The lower-screen implementation and bundled Redux suit data are derived from
the GPL-3.0 MetroidArch branch. The combined port is distributed under GPL-3.0;
the CharlesAverill MIT notice remains in `LICENSE.MIT`, and the original
licenses of the submodules remain in their own repositories. See
`THIRD_PARTY_NOTICES.md` for file and asset provenance. The Redux suit bytes
were extracted from a modified game ROM; the GPL notice on the MetroidArch
code does not establish ownership of those graphics.

The port uses public submodule forks
[`NicolasBeatum/sm-3ds-lib-native-fork`](https://github.com/NicolasBeatum/sm-3ds-lib-native-fork)
and [`NicolasBeatum/SDL-3DS-native-fork`](https://github.com/NicolasBeatum/SDL-3DS-native-fork).
Their fork relationships identify the original projects; the branches pinned
in `.gitmodules` contain the 3DS-specific changes.

OpenAI Codex has been used extensively throughout this custom fork. AI-assisted
work includes implementation drafts, source comparison, performance analysis,
the PICA200 and lower-screen integration, bug diagnosis, test automation and
documentation. Human direction, acceptance testing and the decision to publish
remain with the repository owner. AI assistance does not change the licenses,
copyright or required attribution of any upstream project.

![title screen on Azahar](screenshots/titlescreen.png)

## Building

### Release 0.1.0

Download the ROM-free `.3dsx` for the Homebrew Launcher or the `.cia` for FBI
from the [v0.1.0 release](https://github.com/NicolasBeatum/sm-3ds-native/releases/tag/v0.1.0).
The release also includes a QR code for FBI's **Remote Install → Scan QR Code**.
After installation, supply your own compatible `.smc` or `.sfc` ROM in
`sdmc:/3ds/sm3dsnative/` and select it at startup. Neither release build
contains a game ROM or translation patch.

![FBI QR code for the v0.1.0 CIA](docs/assets/fbi-v0.1.0.png)

### Setup

```bash
# Install devkitARM - https://devkitpro.org/wiki/Getting_Started

# Clone
git clone --recurse-submodules https://github.com/NicolasBeatum/sm-3ds-native.git

# Install bannertool
git clone https://github.com/carstene1ns/3ds-bannertool.git --depth=1 && cd 3ds-bannertool
cmake -B build && cmake --build build && sudo cmake --install build
cd ..

# Install makerom
git clone https://github.com/3DSGuy/Project_CTR.git --depth=1
make -C Project_CTR/makerom deps -j
make -C Project_CTR/makerom program -j
sudo cp Project_CTR/makerom/bin/makerom /usr/bin

cd sm-3ds-native

# Build SDL2
make sdl

# Build the native Old 3DS release
make -j FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" 3dsx
make FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" cia
```

Copy a compatible Super Metroid ROM to `sdmc:/3ds/sm3dsnative/` on the 3DS
microSD card, then choose it from the menu at launch. The folder is created
automatically if absent. Different ROM filenames use different save files
(`saves/<ROM filename>.srm`). Builds deliberately include no game ROM, even
when a local `romfs/sm.smc` exists in the source checkout.
The native game engine is selected by the release build. The `SM3DS_OLD3DS`
optimization is also fixed at build time and cannot be changed in the ROM selector.
For `sm.smc`, an existing `saves/sm.srm` is copied to the new SD save location
the first time, leaving the original file intact.
The public repository does not include game ROM data. The selector currently
loads existing ROM files; applying translation patches in the menu is planned
for a later version.

### Spanish translation 1.0 hardware test

The Klint/Pacochan Spanish 1.0 IPS uses offsets for a ROM with a 512-byte
copier header. Start with the unpatched JU ROM (3,145,728 bytes; CRC32
`D63ED5F8`), prepend a 512-byte copier header, then apply the IPS. The port
accepts that headered patched ROM directly. You can also remove the header
after applying the IPS; the resulting 3,145,728-byte ROM has CRC32 `A1BF5696`.
Copy the patched `.smc` or `.sfc` to `sdmc:/3ds/sm3dsnative/` and select it at
launch. A ROM patched without the header (CRC32 `CB6EF725`) corrupts the intro
and is rejected with an explanatory message.

The build contains neither the ROM nor the IPS. This change does not apply IPS
files in the launcher yet; use the already patched `.sfc` for this test. The
translation itself covers approximately 80% of the original game, and the
port's own lower-screen interface remains in English.

### Hardware diagnostics

Stay in a slow scene for at least three seconds, then use **SETUP → i → SAVE
DUMP** or hold **L + R + A**. The newest folder under
`sdmc:/3ds/sm3dsnative/dump/` contains `info.txt`, `frame-times.csv`, WRAM,
SRAM and (when the display capture succeeds) `top.bmp`, `bottom.bmp` and their
raw framebuffers. `measured_fps` is calculated from recent real frame
intervals; the phase timings identify game, upper-screen, lower-screen and
presentation work. Capturing the same scene once with widescreen on and once
off makes the difference measurable. Older v2 dumps contain no FPS data.

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
