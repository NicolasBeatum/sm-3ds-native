# Building and diagnostics

For ready-to-install packages, use the [GitHub releases](https://github.com/NicolasBeatum/sm-3ds-native/releases/latest). This page is for compiling from source and investigating problems.

## Toolchain and compilation

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
make -j FULL_NATIVE=1 LTO=1 BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG" 3dsx
make FULL_NATIVE=1 LTO=1 BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG" cia
```

Copy a compatible Super Metroid ROM to `sdmc:/3ds/sm3dsnative/` on the 3DS
microSD card, then choose it from the menu at launch. The folder is created
automatically if absent. Different ROM filenames use different save files
(`saves/<ROM filename>.srm`). Builds deliberately include no game ROM, even
when a local `romfs/sm.smc` exists in the source checkout.
The native game engine is selected by the release build. The `SM3DS_OLD3DS`
optimization is also fixed at build time and cannot be changed in the ROM selector.
The same packages run on Old and New 3DS. At startup the port requests the
New 3DS faster CPU clock and L2 cache through libctru; Old 3DS keeps its
normal clock. `SM3DS_OLD3DS` selects economical rendering code, not an Old
3DS CPU limit. Dumps identify the detected model in `hardware`. New 3DS
should therefore improve CPU-bound scenes, but its actual frame rate still
depends on the scene and needs to be measured on hardware.
For `sm.smc`, an existing `saves/sm.srm` is copied to the new SD save location
the first time, leaving the original file intact.
The public repository does not include game ROM data. The selector currently
loads existing ROM files; applying translation patches in the menu is planned
for a later version.

## Spanish translation 1.0 hardware test

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

## Hardware diagnostics

Stay in a slow scene for about ten seconds, then use **SETUP → i → SAVE
DUMP** or hold **L + R + A**. The newest folder under
`sdmc:/3ds/sm3dsnative/dump/` contains `info.txt`, `frame-times.csv`, WRAM,
SRAM and (when the display capture succeeds) `top.bmp`, `bottom.bmp` and their
raw framebuffers. `measured_fps` covers only the last 120 rendered frames.
Timing schema v4 also has `session_measured_fps` since launch and separate
`gameplay_standard_measured_fps` / `gameplay_widescreen_measured_fps` totals.
Paused, suspended and dump-writing gaps are excluded from frame intervals.
Session summaries use fixed memory and perform no SD writes during gameplay;
the CSV remains a recent sample rather than a complete session trace.
The phase timings identify game, upper-screen, lower-screen and presentation
work. Sector and scroll-block change counters, their work times, and the
recent CSV's location columns help investigate hitches while traversing a
room. Compare equivalent routes from a fresh launch in each build. Older v2
dumps contain no FPS data; v3 contains only recent timing data.
Diagnostic builds with `SM3DS_PHASE_DIAG` also split the game phase into native
logic and PPU preparation (`avg_logic_us` and `avg_ppu_us`). This adds only a few
clock reads per frame and helps locate slow scenes on the original 3DS.
Recent diagnostic builds further separate main/sub background and object work,
color composition, and vertex/atlas upload. These timings are recorded only
for frames that actually use the PICA renderer.

Optional build switches:

| Variable / define | Purpose |
| ----------------- | ------- |
| `FULL_NATIVE=1` | Run the decompiled native game logic. |
| `BUILD_FLAGS="-DSM3DS_OLD3DS"` | Enable the optimized Old 3DS timing and rendering path. |
| `APP_MAX_CPU=0xD0` | Allow an installed CIA to request up to 80% of the system CPU core (the default). `0x9E` limits it to 30%. This packaging setting does not affect 3DSX. |
| `LTO=1` | Enable link-time optimization for compilation and linking. |
| `-DSM3DS_PROFILE` | Write performance counters to `sdmc:/sm3ds-profile.log`. |
| `-DSM3DS_DOOR_TRACE` | Write targeted Crateria door/BTS diagnostics. |
| `-DSM3DS_EMULATED_CPU` | Diagnostic fallback to the original emulated CPU path. |


## Make targets

| Target | Output / action |
| --- | --- |
| `make 3dsx` | Homebrew Launcher `.3dsx` and `.smdh`. |
| `make cia` | Installable CIA with HOME Menu banner and sound. |
| `make elf` | ELF for development/debugging. |
| `make 3ds` | CCI image for development. |
| `make azahar` | Build and launch a 3DSX in Azahar. |
| `make fbi` | Build a CIA and send it to the configured console IP. |
| `make hblauncher` | Send a 3DSX to Homebrew Launcher. |
| `make release` | Package the supported build targets. |

The checked-in `resources/banner.cgfx` is ready for CIA packaging. To regenerate the animated scene or its audio, see [the banner resource notes](../resources/banner-animated/README.md).
