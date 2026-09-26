# Super Metroid 3DS native-port notes

## Scope

This branch keeps the `FULL_NATIVE` decompiled Super Metroid game logic and
targets a stable, full-quality experience on Nintendo 3DS. The implementation
does not lower the internal SNES resolution, remove HDMA effects, add scanline
filtering, or reduce audio to mono. The top screen preserves the original
image, centered at the SNES display aspect, while the bottom screen is a live
companion interface.

## Repository layout

- `source/main.c` owns 3DS startup, input, audio, frame pacing and presentation.
- `source/gpu_presenter.*` uploads and presents top/bottom textures through
  Citro3D, with the legacy framebuffer path retained as a fallback.
- `source/ppu_gpu.*`, `source/ppu_gpu_model.*` and `source/sm_pica.v.pica`
  implement the conservative PICA200 rendering path.
- `source/bottom_screen.*` implements the native 320x240 companion UI.
- `sm/` is the modified decompiled game and SNES hardware library.
- `SDL/` contains the 3DS audio-thread scheduling adjustment.

Both `sm/` and `SDL/` are git submodules. Their commits must be published and
the parent repository must reference those exact commits for a reproducible
checkout.

## Frame execution

The native game logic has already executed the CPU-side frame, so the Old 3DS
path does not advance the unused emulated master clock in two-cycle increments.
It schedules only the externally visible events the game depends on:

1. PPU line-zero/frame state.
2. Visible-line rendering.
3. Per-line HDMA at the correct phase.
4. Vertical IRQ handling.
5. VBlank, NMI and automatic joypad state.

The original master-clock path remains available outside `SM3DS_OLD3DS` for
comparison and diagnosis.

## PICA200 renderer and exact fallback

The GPU path currently accepts supported Mode 1 frames with normal brightness,
non-overscan output, supported tile sizes and no HDMA writes that mutate
critical tile/PPU state mid-frame. It captures per-scanline BG, window, color
math and scroll state, decodes SNES tiles into an atlas, and emits Citro3D
quads for main/sub layers and composition.

If a frame uses unsupported state or changes memory in a way that invalidates
the captured model, the DMA state is restored and the complete frame is
rendered by the exact CPU PPU. This is a correctness boundary, not a quality
setting: effects such as rain, fog, windows and unusual color math must remain
correct even when that costs more CPU for an occasional frame.

## CPU PPU optimization

The fallback renderer caches decoded 4 bpp tile rows, palette conversions,
fixed/backdrop color-math tables and per-scanline sprite membership. Fast paths
cover common Mode 1 spans while generic composition remains available for all
other cases. Profiling is compile-time gated and is absent from normal release
builds.

## Audio

Audio is stereo signed 16-bit at 32 kHz. The DSP resampler uses 16.16 fixed
point instead of software floating point, and echo inputs are accumulated
during the normal channel pass to avoid a second traversal. SDL places its
audio thread on the system core when available but preserves a larger CPU-time
budget already requested by the application.

Azahar sometimes reports a missing `sdmc:/3ds/dspfirm.cdc`. For emulator-only
development the launcher may create an empty marker and retry. If opening the
device still fails, the marker is removed. No DSP firmware is bundled.

## Lower-screen companion UI

The lower screen is updated at 15 Hz and presented every display frame. Its
data comes directly from the live native variables, so no memory bridge or
patched libretro core is required. It currently provides:

- live energy tanks, residual energy and ammo counts;
- direct touch selection/cancellation for missiles, supers and power bombs;
- explored-map cells centered on Samus;
- collected/equipped suits, movement upgrades and beams;
- play time, persistent MAP/ITEMS/SETUP tabs and an idle METROID screen.

The visual reference is the MetroidArch dual-screen project:

- <https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen>

MetroidArch's screenshots, palette, pixel-font treatment, layout and ROM asset
decoding are the compatibility target. The current native screen is the first
stage; the remaining fidelity work is to port its ROM-decoded ammo icons,
actual pause-map tiles/doors, world map, full-color Samus equipment wireframe,
map controls/markers and functional setup toggles at 320x240.

## Room collision bug and fix

Decompressed room data is laid out as a size word followed by level data, BTS
and custom-background data. On large rooms the fixed BTS/custom-background
destinations overlap those source ranges. `memcpy` has undefined behavior for
overlap and, on ARM, overwrote later BTS bytes while copying. In Landing Site
the vertical extension cells next to the left door therefore had BTS `00`
instead of `FF`, `FE` and `FD`. The same corruption produced false terrain
bumps.

Both room-load paths now use `memmove`. A targeted diagnostic build confirmed
the expected extension chain and blue-door PLM, and a clean release build was
then verified through this sequence:

1. Walk left across Landing Site with no false bumps.
2. Press against the closed door; Samus remains blocked.
3. Shoot the door; the blue-door PLM opens it.
4. Walk left; the room transition begins normally.

## Builds

Release build:

```sh
make sdl
make -j FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" 3dsx
make FULL_NATIVE=1 BUILD_FLAGS="-DSM3DS_OLD3DS" cia
```

Optional compile-time diagnostics:

- `SM3DS_PROFILE`: writes `sdmc:/sm3ds-profile.log`.
- `SM3DS_DOOR_TRACE`: writes `sdmc:/sm3ds-door.log`.
- `SM3DS_DISABLE_PICA`: forces exact CPU rendering.
- `SM3DS_EMULATED_CPU`: executes the original ROM CPU path for comparison.

Do not enable diagnostic defines in deliverable builds.

## Validation status

- The expected USA/Japan ROM SHA-1 used for local validation is
  `da957f0d63d14cb441d215462904c4fa8519c613`.
- Native audio, dual-screen presentation, Crateria rain/fog, touch ammo
  selection and the Landing Site door sequence were exercised in Azahar.
- Heavy exterior scenes measured approximately 59-63 FPS in the Old 3DS
  emulator profile used during development.
- A full-campaign regression run and verification on physical Old 3DS hardware
  are still recommended before declaring every rare PPU effect covered.

## Credits and licensing

- Original game decompilation/PC port: snesrev/sm.
- Nintendo 3DS port base: CharlesAverill/sm-3ds and sm-3ds-lib.
- SDL: libsdl-org/SDL, under its own license in the submodule.
- Dual-screen design reference: Raekwon1603/RetroArch,
  `metroidarch-dual-screen`.

This custom integration was developed with extensive OpenAI Codex assistance,
including source analysis, implementation, profiling, debugging, automated
emulator testing and documentation. Nicolás Andrés Hernández Vargas provided
the project direction and acceptance testing. This disclosure is informational
and does not replace or alter any upstream license or attribution.

No commercial ROM is included in source control. A legally obtained compatible
ROM must be supplied locally in `romfs/sm.smc`; `.gitignore` excludes it.
