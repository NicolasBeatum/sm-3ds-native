# sm-3ds-native

![Old 3DS: game completed](docs/assets/playthrough.svg) ![Atmospheric lighting: experimental](docs/assets/lighting.svg)

[Español](README.es.md) · [Downloads](https://github.com/NicolasBeatum/sm-3ds-native/releases/latest) · [Changelog](CHANGELOG.md)

> [!IMPORTANT]
> **Unofficial port developed with extensive AI assistance.** NicolasBeatum directs and tests this project; OpenAI Codex has been used for analysis, implementation, debugging, optimization and documentation. Choosing not to play this port because it uses AI is completely understandable and respected. This is not an official release from Nintendo or any upstream project.

A Nintendo 3DS port of Super Metroid, based on [CharlesAverill/sm-3ds](https://github.com/CharlesAverill/sm-3ds) and the [snesrev PC port/decompilation](https://github.com/snesrev/sm), with a MetroidArch-inspired companion screen.

## Current status

**Playable, with a start-to-finish playthrough completed on Old 3DS using the development CIA.** This confirms a route to the ending, not 100% item completion or every route. Supports Old and New 3DS; performance varies by room and settings.

**0.2.0** adds atmospheric lighting, map improvements, new HUD modes and an English/Spanish interface.

## Features

- Native gameplay, original stereo audio and optional widescreen.
- Optional atmospheric lighting: ambient color, doors, lava, Samus and selected luminous enemies, with short shadows. **SETUP → VIDEO → LIGHTING**.
- Five upper HUD modes: full, energy only, energy and minimap, hidden with expanded scenery, or the classic black band. Reduced HUD modes have transparent backgrounds.
- Area and world maps with zoom, touch dragging and Samus tracking. QoL for pending/collected items (**OFF, ZERO/FUSION or SUPER METROID**), colored doors, hazardous rooms and secrets.
- Lower-map station letters: **E** (energy), **A** (ammo) and **M** (map). **SETUP → MAP → MAP IMPROVEMENTS**.
- English/Spanish lower-screen UI, configurable HUD, equipment and settings saved on the microSD. **SETUP → HUD → LANGUAGE**.
- ROM selector, per-ROM saves and diagnostic dumps with **L + R + A**.

See the [changelog](CHANGELOG.md) for all changes and fixes.

## In pictures

Actual **Azahar** captures show appearance, not real-console performance.

**Updated map:** **E** = energy refill · **A** = ammo refill · **M** = map station. These letters appear on the lower map; **S** still marks save stations.

![Lower map with energy, ammo and map station letters](screenshots/qol/stations.png)

**Full HUD and transparent reduced HUD.**

![HUD comparison](screenshots/qol/hud-transparent.gif)

**Lighting: OFF on the left, ON on the right.**

![Crateria — Landing Site](screenshots/lighting/ship-animated.gif)

![Norfair — lava](screenshots/lighting/lava-animated.gif)

<details>
<summary>View GIFs from the other areas</summary>

![Exterior](screenshots/lighting/exterior-animated.gif)

![Green Brinstar](screenshots/lighting/green-brinstar-animated.gif)

![Red Brinstar](screenshots/lighting/red-brinstar-animated.gif)

![Pink Brinstar](screenshots/lighting/pink-brinstar-animated.gif)

![Blue Brinstar](screenshots/lighting/blue-brinstar-animated.gif)

![Wrecked Ship](screenshots/lighting/wrecked-ship-animated.gif)

![Maridia](screenshots/lighting/maridia-animated.gif)

![Tourian](screenshots/lighting/tourian-animated.gif)

</details>

## Download and installation

Download **[version 0.2.0](https://github.com/NicolasBeatum/sm-3ds-native/releases/tag/v0.2.0)**: **[CIA](https://github.com/NicolasBeatum/sm-3ds-native/releases/download/v0.2.0/SuperMetroid3DSPort.cia)** for FBI, or **[3DSX](https://github.com/NicolasBeatum/sm-3ds-native/releases/download/v0.2.0/SuperMetroid3DSPort.3dsx) + [SMDH](https://github.com/NicolasBeatum/sm-3ds-native/releases/download/v0.2.0/SuperMetroid3DSPort.smdh)** for Homebrew Launcher.

Install the CIA using **FBI → Remote Install → Scan QR Code**:

![FBI QR code for the v0.2.0 CIA](docs/assets/fbi-v0.2.0.png)

Supply your own compatible `.smc` or `.sfc` ROM in `sdmc:/3ds/sm3dsnative/` and select it at startup. Packages contain no ROM or patches. Updates preserve games in `saves/` and preferences in `settings.cfg`.

[Building and diagnostics](docs/BUILDING.md) · [Porting notes](docs/PORTING_NOTES.md)

<details>
<summary>Credits, licenses and AI assistance</summary>

This repository is a custom integration of several independent projects; it is
not presented as wholly original work:

| Project | Role in this repository |
| ------- | ----------------------- |
| [CharlesAverill/sm-3ds](https://github.com/CharlesAverill/sm-3ds) | Nintendo 3DS port base. |
| [CharlesAverill/sm-3ds-lib](https://github.com/CharlesAverill/sm-3ds-lib) | Native game/decompilation submodule used by the port. |
| [snesrev/sm](https://github.com/snesrev/sm) | Original Super Metroid decompilation/PC port ancestry. |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | Platform, input and audio layer. |
| [Raekwon1603/RetroArch `metroidarch-dual-screen`](https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen) | Lower-screen design reference and source of derived UI code, ROM decoding behavior and Redux suit data. |
| [EstebanPdN/zelda-alttp-3ds](https://github.com/EstebanPdN/zelda-alttp-3ds) | Reference for dump organization and session performance diagnostics. |

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

If you prefer not to play a port developed with AI assistance, that choice is completely understandable and respected. The disclosure is here so you can make an informed decision.

</details>
