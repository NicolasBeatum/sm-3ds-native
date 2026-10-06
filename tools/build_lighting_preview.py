#!/usr/bin/env python3
"""Build the native, read-only slot-2 preview; no SDL or ROM embedded."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build-lighting-preview'
out.mkdir(exist_ok=True)
sources = [p for p in (root / 'sm/src').glob('*.c')
           if p.name not in ('main.c', 'config.c', 'opengl.c', 'glsl_shader.c')]
sources += list((root / 'sm/src/snes').glob('*.c'))
sources += [root / 'tools/lighting_preview.c', root / 'source/ppu_gpu_model.c',
            root / 'source/scene_lighting.c']
headers = list((root / 'sm/src').rglob('*.h')) + list((root / 'source').glob('*.h'))
header_mtime = max(p.stat().st_mtime for p in headers)
def compile_source(source):
    obj = out / (source.stem + '.o')
    if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, header_mtime):
        subprocess.run(['cc', '-O2', '-w', '-DFULL_NATIVE', '-I', str(root / 'sm'),
                        '-I', str(root / 'source'), '-c', str(source), '-o', str(obj)], check=True)
    return str(obj)
with ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_source, sources))
subprocess.run(['cc', *objects, '-Wl,--wrap=ppu_runLine',
                '-Wl,--wrap=LoadFromLoadStation', '-lm', '-o',
                str(root / 'output-lighting-trial/lighting-preview')], check=True)
