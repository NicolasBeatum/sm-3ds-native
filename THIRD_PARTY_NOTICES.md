# Third-party notices

This fork combines work from several upstream projects. `LICENSE` contains the
GNU GPL version 3 terms for the combined port because it includes code derived
from MetroidArch. `LICENSE.MIT` preserves the CharlesAverill/sm-3ds license;
the submodules keep their own licenses and notices.

## MetroidArch dual-screen UI

- Source: <https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen>
- Upstream license: GNU General Public License version 3 (`COPYING` in that
  repository).
- Derived implementation: `source/bottom_screen.c`,
  `source/bottom_screen.h`, and the related presentation/input integration.
- Elements carried over: layout and palette conventions, ROM-address tables,
  SNES 2 bpp/4 bpp decoding behavior, pause-map/world-map composition,
  vanilla item-percentage calculation and lower-screen interactions.

## Redux suit graphic

`source/redux_suit_data.h` is a mechanical C transcription of MetroidArch's
`ReduxSuitData.java`. Its upstream comments state that the tile, tilemap and
palette bytes were extracted from a pre-built Super Metroid Redux ROM and the
Menu Colored Samus hack. The data is kept separate, named explicitly and used
only for the lower-screen equipment illustration.

No Super Metroid ROM is committed. The locally supplied `romfs/sm.smc` remains
ignored, and users are responsible for providing a legally obtained compatible
ROM. Nintendo, Super Metroid and related assets are property of their
respective owners. This project is unofficial and non-commercial.

## Other upstreams

- CharlesAverill/sm-3ds: MIT; see `LICENSE.MIT`.
- CharlesAverill/sm-3ds-lib and snesrev/sm: preserved through the `sm`
  submodule history and its notices.
- libsdl-org/SDL: zlib license; see `SDL/LICENSE.txt` in the initialized
  submodule.

The custom integration and documentation were produced with extensive OpenAI
Codex assistance under human direction and testing. This disclosure does not
change any upstream license.

## CIA HOME Menu animation and music

- Animation source: [Alpha Coders GIF 12420](https://gifs.alphacoders.com/gifs/view/12420), shared by robokoboto. The page labels it for private, personal use. The source GIF is retained in `resources/banner-animated/source.gif`; conversion preserves its eight-frame timing and pixel colors in RGB8 textures.
- Game graphics and music: Super Metroid, Nintendo. The three-second banner excerpt is from “Opening / Destruction of the Space Colony”, credited to Minako Hamano, rendered using the native SPC/DSP player from a user-supplied ROM.
- Conversion tool: [skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx), used as a build-time dependency; not bundled into the application. Hardware compatibility adjustments reference [ClouDS Music FA's converter](https://github.com/Epic0522/ClouDS-Music-FA/blob/clouds-music-fa/tools/banner/convert_banner_cgfx.py).
- See `resources/banner-animated/README.md` for conversion details and attribution. The game itself is not bundled as a ROM.
