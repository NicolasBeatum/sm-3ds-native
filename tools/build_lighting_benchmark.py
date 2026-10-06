#!/usr/bin/env python3
"""Build comparable warm-cache CPU models from the two local baseline commits."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build-lighting-benchmark'
out.mkdir(exist_ok=True)
flags = ['cc', '-O3', '-std=c11', '-iquote', str(root / 'sm'), '-I', str(root / 'source')]
objects = []
for revision, prefix in [('b5d38d0', 'Before'), ('59424b8', 'Initial')]:
    source = out / f'{prefix}-model.c'
    source.write_bytes(subprocess.check_output(['git', 'show', f'{revision}:source/ppu_gpu_model.c'], cwd=root))
    renames = [f'-DPica{name}={prefix}{name}' for name in
               ['AtlasInit', 'AtlasBegin', 'BuildFrame', 'CaptureLine', 'HudLineCount', 'ScrollOffset']]
    if prefix == 'Initial':
        renames += ['-DSceneLighting_Begin=InitialSceneBegin', '-DSceneLighting_AddLight=InitialSceneAdd',
                    '-DSceneLighting_Tile=InitialSceneTile', '-DSceneLighting_EmitShadows=InitialSceneShadows']
    obj = source.with_suffix('.o')
    subprocess.run([*flags, *renames, '-c', str(source), '-o', str(obj)], check=True)
    objects.append(str(obj))
    if prefix == 'Initial':
        scene = out / 'Initial-scene.c'
        scene.write_bytes(subprocess.check_output(['git', 'show', f'{revision}:source/scene_lighting.c'], cwd=root))
        obj = scene.with_suffix('.o')
        subprocess.run([*flags, *renames, '-c', str(scene), '-o', str(obj)], check=True)
        objects.append(str(obj))
binary = out / 'lighting-benchmark'
subprocess.run([*flags, str(root/'tools/lighting_benchmark.c'), str(root/'source/ppu_gpu_model.c'),
                str(root/'source/scene_lighting.c'), *objects, '-o', str(binary)], check=True)
print(binary)
