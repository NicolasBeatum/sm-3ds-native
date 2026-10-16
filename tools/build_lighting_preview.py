#!/usr/bin/env python3
"""Build the native, read-only slot-2 preview; no SDL or ROM embedded."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import argparse

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--audio',action='store_true',help='Render original SPC/DSP PCM with the split command queue on the host')
args = parser.parse_args()
out = root / ('build-audio-preview' if args.audio else 'build-lighting-preview')
out.mkdir(exist_ok=True)
sources = [p for p in (root / 'sm/src').glob('*.c')
           if p.name not in ('main.c', 'config.c', 'opengl.c', 'glsl_shader.c')]
sources += list((root / 'sm/src/snes').glob('*.c'))
sources += [root / ('tools/audio_preview.c' if args.audio else 'tools/lighting_preview.c'), root / 'source/ppu_gpu_model.c',
            root / 'source/scene_lighting.c']
headers = list((root / 'sm/src').rglob('*.h')) + list((root / 'source').glob('*.h'))
# Compiler flags in this script also invalidate previously built objects.
header_mtime = max(p.stat().st_mtime for p in [Path(__file__), root / 'tools/lighting_preview.c', *headers])
def compile_source(source):
    obj = out / (source.stem + '.o')
    if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, header_mtime):
        # Match the trial's optimized audio translation units; keep the rest of
        # the host frontend at O2, as in the independent audio comparisons.
        optimization = ['-O3', '-Ofast', '-flto'] if args.audio and source.stem in ('sm_rtl', 'spc_player', 'dsp') else ['-O2']
        subprocess.run(['cc', *optimization, '-fno-strict-aliasing', '-w', '-DFULL_NATIVE',
                        *(['-DSM3DS_SPLIT_APU_QUEUE'] if args.audio else []), '-I', str(root / 'sm'),
                        '-I', str(root / 'source'), '-c', str(source), '-o', str(obj)], check=True)
    return str(obj)
with ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_source, sources))
binary = root / ('output-dump-oct15-trial/audio-preview' if args.audio else
                 'output-lighting-trial/lighting-preview')
binary.parent.mkdir(exist_ok=True)
subprocess.run(['cc', *(['-O3', '-Ofast', '-flto'] if args.audio else []), *objects, '-Wl,--wrap=ppu_runLine',
                '-Wl,--wrap=LoadFromLoadStation', '-lm', '-o', str(binary)], check=True)
print(binary)
