# Animated CIA HOME Menu banner

Source GIF: [Alpha Coders 12420](https://gifs.alphacoders.com/gifs/view/12420),
shared there by robokoboto, featuring Samus and the Super Metroid gunship.
`source.gif` retains all eight source frames and their 130 ms timing.
The source page labels the image for private, personal use. These assets are
prepared for this local build; this change does not publish a release.

`resources/banner.cgfx` has eight ordinary rigid mesh nodes named `Frame0` to
`Frame7`, with one visible at a time via STEP translation animation. It uses
the HOME Menu's `COMMON` model/animation naming, a 1.04-second loop, nearest
texture sampling and unlit RGB8 textures. There are no skin weights or soft
skinning primitive sets. The scene is below 512 KiB. The SMDH explicitly
enables `extendedbanner`; the existing small icon and publisher are retained.

Generator: `tools/build_animated_banner.py`, using
[skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx) revision
`1f78850086f3a77c41e07162e842f97a5bf3c18a`, Pillow and gltflib. Rigid mesh
node binding follows the documented hardware behavior in
[ClouDS Music FA's banner converter](https://github.com/Epic0522/ClouDS-Music-FA/blob/clouds-music-fa/tools/banner/convert_banner_cgfx.py).

To rebuild the scene, install gltflib/Pillow in a separate Python environment
and run from the project root:

```sh
PYCGFX_DIR=/absolute/path/to/pycgfx BANNER_PYTHON=/absolute/path/to/python \
  resources/build-banner.sh
```

Banner audio: the first three seconds of "Opening / Destruction of the Space
Colony" from Super Metroid, rendered from a user-supplied ROM by the project's
native SPC/DSP player (music bank index 3, cue 5). Original game/music assets
belong to Nintendo; the opening composition is credited to Minako Hamano in
the original soundtrack. No ROM or IPS is packaged. `resources/audio/audio.wav`
is stereo PCM16 at 44.1 kHz, with a short final fade to avoid a cut-off click.

To regenerate it, supply your own ROM:

```sh
cc -O2 -ffunction-sections -fdata-sections -iquote sm/src \
  tools/render_banner_opening.c sm/src/spc_player.c sm/src/snes/dsp.c \
  -Wl,--gc-sections -lm -o /tmp/render-banner-opening
/tmp/render-banner-opening /path/to/user-rom.sfc /tmp/opening.pcm
ffmpeg -f s16le -ar 32000 -ac 2 -i /tmp/opening.pcm -t 3 \
  -af 'volume=4,afade=t=out:st=2.85:d=0.15' -ar 44100 \
  -c:a pcm_s16le resources/audio/audio.wav
```

The original cue starts quietly; the banner excerpt has a fixed 12 dB gain
and does not clip. These resources run only in HOME Menu and do not affect
gameplay rendering or frame scheduling. Hardware HOME Menu animation/audio
validation is still pending.
