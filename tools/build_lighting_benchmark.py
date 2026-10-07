#!/usr/bin/env python3
"""Build independent baseline/current native-capture CPU benchmarks.

Each executable uses that revision's own scene/atlas headers, avoiding ABI
mismatches as lighting structs change. Run with CAPTURE LIGHTS CAMERA WIDTH.
"""
from pathlib import Path
import argparse
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reference', default='d8e57b8')
args = parser.parse_args()
out = root / 'build-lighting-benchmark'
out.mkdir(exist_ok=True)
flags = ['cc', '-O3', '-std=c11', '-iquote', str(root / 'sm')]
for label, revision in [('previous', args.reference), ('current', None)]:
    if revision:
        src = out / label
        src.mkdir(exist_ok=True)
        for name in ['scene_lighting.c', 'scene_lighting.h', 'ppu_gpu_model.c',
                     'ppu_gpu_model.h', 'ppu_gpu_vertices.h', 'wide_config.h']:
            (src / name).write_bytes(subprocess.check_output(
                ['git', 'show', f'{revision}:source/{name}'], cwd=root))
    else:
        src = root / 'source'
    binary = out / f'{label}-lighting-benchmark'
    if revision:
        preview = src / 'lighting_raster_preview.c'
        preview.write_bytes(subprocess.check_output(
            ['git', 'show', f'{revision}:tools/lighting_raster_preview.c'], cwd=root))
        benchmark = src / 'lighting_benchmark.c'
        benchmark.write_bytes((root / 'tools/lighting_benchmark.c').read_bytes())
    else:
        benchmark = root / 'tools/lighting_benchmark.c'
    subprocess.run([*flags, '-I', str(src), str(benchmark),
                    str(src / 'ppu_gpu_model.c'), str(src / 'scene_lighting.c'),
                    '-o', str(binary)], check=True)
    print(binary)
