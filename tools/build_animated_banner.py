#!/usr/bin/env python3
"""Convert the chosen GIF to rigid animated CGFX planes for HOME Menu.

Requires Pillow, gltflib and skyfloogle/pycgfx revision
1f78850086f3a77c41e07162e842f97a5bf3c18a. No Blender or SDK is needed.
"""
import argparse
import importlib.util
import json
import struct
import sys
from pathlib import Path

from PIL import Image
import gltflib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pycgfx", type=Path, required=True)
    parser.add_argument("--gif", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.pycgfx.resolve()))
    spec = importlib.util.spec_from_file_location("banner_pycgfx", args.pycgfx / "main.py")
    converter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(converter)
    from cgfx.sobj import BillboardMode
    from cgfx.mtob import MTOBFlag
    from cgfx.txob import TextureFormat
    from cgfx.primitives import VertexAttributeUsage
    from cgfx.canm import CANMBoneTransform, PrimitiveType
    from cgfx import swizzler

    args.work.mkdir(parents=True, exist_ok=True)
    image = Image.open(args.gif)
    frames, durations = [], []
    for i in range(image.n_frames):
        image.seek(i)
        # Preserve the GIF's palette and pixel edges: nearest resampling,
        # original aspect ratio, padding only to a power-of-two texture.
        h = round(128 * image.height / image.width)
        frame = Image.new("RGB", (128, 128), (0, 0, 0))
        frame.paste(image.convert("RGB").resize((128, h), Image.Resampling.NEAREST),
                    (0, (128 - h) // 2))
        frame.save(args.work / f"frame{i}.png")
        frames.append(frame)
        durations.append(image.info.get("duration", 100) / 1000)
    count = len(frames)
    if not 1 < count <= 16:
        raise ValueError("This banner supports 2–16 frames")
    blob = bytearray()
    views, accessors = [], []

    def accessor(values, components, kind, component_type=5126):
        while len(blob) % 4:
            blob.append(0)
        raw = struct.pack("<" + ("f" if component_type == 5126 else "H") * len(values), *values)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(raw)})
        blob.extend(raw)
        accessors.append({"bufferView": len(views) - 1, "byteOffset": 0,
                          "componentType": component_type, "count": len(values) // components,
                          "type": kind})
        return len(accessors) - 1

    # HOME Menu camera: (0,1,44.786), 30-degree vertical FOV. The image is
    # a flat, screen-facing card centered at camera Y, with safe margins.
    pos = accessor([-6, -5, 0, 6, -5, 0, -6, 7, 0, 6, 7, 0], 3, "VEC3")
    uv = accessor([0, 1, 1, 1, 0, 0, 1, 0], 2, "VEC2")
    normal = accessor([0, 0, 1] * 4, 3, "VEC3")
    index = accessor([0, 1, 2, 2, 1, 3], 1, "SCALAR", 5123)
    times = [0.0]
    for duration in durations:
        times.append(times[-1] + duration)
    time_accessor = accessor(times, 1, "SCALAR")
    nodes, meshes, materials, samplers, channels = [], [], [], [], []
    for i in range(count):
        name = f"Frame{i}"
        nodes.append({"name": name, "mesh": i, "translation": [0, 0 if i == 0 else 1000, 0]})
        meshes.append({"name": name, "primitives": [{"attributes": {
            "POSITION": pos, "NORMAL": normal, "TEXCOORD_0": uv},
            "indices": index, "material": i}]})
        materials.append({"name": name, "doubleSided": False,
                          "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1],
                              "baseColorTexture": {"index": i}, "metallicFactor": 0,
                              "roughnessFactor": 1}})
        positions = [value for j in range(count + 1)
                     for value in (0, 0 if j % count == i else 1000, 0)]
        samplers.append({"input": time_accessor,
                         "output": accessor(positions, 3, "VEC3"), "interpolation": "STEP"})
        channels.append({"sampler": i, "target": {"node": i, "path": "translation"}})
    model = {"asset": {"version": "2.0"}, "scene": 0,
             "scenes": [{"nodes": list(range(count))}], "nodes": nodes,
             "meshes": meshes, "materials": materials,
             "images": [{"name": f"Frame{i}", "uri": f"frame{i}.png"} for i in range(count)],
             "textures": [{"source": i, "sampler": 0} for i in range(count)],
             "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
             "animations": [{"name": "COMMON", "samplers": samplers, "channels": channels}],
             "buffers": [{"uri": "scene.bin", "byteLength": len(blob)}],
             "bufferViews": views, "accessors": accessors}
    (args.work / "scene.bin").write_bytes(blob)
    gltf_path = args.work / "scene.gltf"
    gltf_path.write_text(json.dumps(model))
    cgfx = converter.convert_gltf(gltflib.GLTF.load(str(gltf_path), load_file_resources=True))
    cmdl = cgfx.data.models["COMMON"]
    for i in range(count):
        cmdl.skeleton.bones[f"Frame{i}"].billboard_mode = BillboardMode.Screen
    for mesh in cmdl.meshes.data.contents:
        mesh.mesh_node_name = mesh.name.split("_")[0]
    for shape in cmdl.shapes.data.contents:
        assert all(p.skinning_mode == 0 for p in shape.primitive_sets.data.contents)
        assert all(a.usage not in (VertexAttributeUsage.BoneIndex, VertexAttributeUsage.BoneWeight)
                   for a in shape.vertex_attributes.data.contents)
    for name in cmdl.materials:
        material = cmdl.materials[name]
        material.flags = MTOBFlag(0)
        # Unlit texture replace: HOME lighting must not brighten the GIF.
        for j, stage in enumerate(material.fragment_shader.texture_combiners):
            stage.src_rgb = stage.src_alpha = 0x333 if j == 0 else 0xFFF
            stage.combine_rgb = stage.combine_alpha = 0
    # Use RGB8 rather than pycgfx's default RGBA4 to retain all GIF colors.
    for i, frame in enumerate(frames):
        txob = cgfx.data.textures[f"Frame{i}"]
        encoded = swizzler.to_txob(frame.convert("RGBA").transpose(Image.Transpose.FLIP_TOP_BOTTOM),
                                   TextureFormat.RGB8)
        txob.hw_format = encoded.hw_format
        txob.pixel_based_image = encoded.pixel_based_image
    animation = cgfx.data.skeletal_animations["COMMON"]
    assert animation.looping and sum(1 for _ in animation.member_animations_data) == count
    for name in animation.member_animations_data:
        member = animation.member_animations_data[name]
        assert isinstance(member, CANMBoneTransform) and member.primitive_type == PrimitiveType.Transform
    data = converter.write(cgfx)
    if len(data) >= 512 * 1024:
        raise ValueError(f"CGFX exceeds HOME Menu's 512 KiB limit: {len(data)}")
    args.output.write_bytes(data)
    frames[0].save(args.work / "preview.png")
    print(f"CGFX: {count} rigid frames, loop {times[-1]:.3f}s, {len(data)} bytes")


if __name__ == "__main__":
    main()
