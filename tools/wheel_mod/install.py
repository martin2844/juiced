#!/usr/bin/env python3
"""Install the user's Eagle wheel archive as a validated, non-destructive overlay."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import sys
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'car_mod'))
from retail import Retail


def blocks(text):
    text = '\n'.join(line for line in text.splitlines() if not line.lstrip().startswith(';'))
    return re.findall(r'\[BeginMod\].*?\[EndMod\]', text, re.I | re.S)


def value(block, key):
    match = re.search(r'^\s*' + key + r'\s+([^\r\n;]+)', block, re.I | re.M)
    return match.group(1).strip().strip('"') if match else ''


def mesh_materials(data):
    """Walk exactly the native D8M layout, rejecting malformed mesh buffers."""
    offset = 0
    def u32():
        nonlocal offset
        v, = struct.unpack_from('<I', data, offset)
        offset += 4
        return v
    offset = 4 + struct.unpack_from('<I', data)[0] * 2
    materials = []
    for _ in range(u32()):
        materials.append(data[offset:offset + 32].split(b'\0')[0].decode('ascii').lower())
        offset += 32 + 12 + 44 + 12
        declaration_count = u32()
        if declaration_count > 64:
            raise ValueError('Invalid mesh declaration')
        offset += declaration_count * 8
        stride, count = u32(), u32()
        offset += stride * count
        if offset > len(data):
            raise ValueError('Truncated mesh')
    if offset + 44 != len(data):
        raise ValueError('Unexpected mesh layout')
    return materials


def normalize_mesh_colours(data):
    """Supply explicit COLOR0 and TEXCOORD1 for legacy wheel shaders on Metal."""
    offset = 4 + struct.unpack_from('<I', data)[0] * 2
    count, = struct.unpack_from('<I', data, offset)
    offset += 4
    output = bytearray(data[:offset])
    changed = 0
    for _ in range(count):
        header = data[offset:offset + 100]
        offset += 100
        declaration_count, = struct.unpack_from('<I', data, offset)
        offset += 4
        declarations = [struct.unpack_from('<HHBBBB', data, offset + i * 8) for i in range(declaration_count)]
        offset += declaration_count * 8
        stride, vertices = struct.unpack_from('<II', data, offset)
        offset += 8
        raw = data[offset:offset + stride * vertices]
        offset += stride * vertices
        if not any(e[4] == 10 and e[5] == 0 and e[0] != 255 for e in declarations):
            # Add after existing data: existing normal/UV offsets remain exact.
            declarations.insert(len(declarations) - 1, (0, stride, 4, 0, 10, 0))
            raw = b''.join(raw[i * stride:(i + 1) * stride] + b'\xff\xff\xff\xff' for i in range(vertices))
            stride += 4
            changed += 1
        if not any(e[4] == 5 and e[5] == 1 and e[0] != 255 for e in declarations):
            uv = next((e for e in declarations if e[4] == 5 and e[5] == 0), None)
            if uv is not None:
                declarations.insert(len(declarations) - 1, (*uv[:5], 1))
                changed += 1
        output += header + struct.pack('<I', len(declarations))
        output += b''.join(struct.pack('<HHBBBB', *e) for e in declarations)
        output += struct.pack('<II', stride, vertices) + raw
    output += data[offset:]
    return bytes(output), changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base-overlay', type=Path, help='Read existing catalogue and colours here before retail')
    args = parser.parse_args()
    output = args.output.resolve()
    if output == args.game.resolve() or output == args.archive.resolve().parent:
        parser.error('Output must be a separate mod directory')
    if output.exists() and any(output.iterdir()):
        parser.error('Output must be absent or empty; use a new directory to reinstall')
    output.mkdir(parents=True, exist_ok=True)
    retail = Retail(args.game)
    def read_base(name):
        if args.base_overlay and (args.base_overlay / name).is_file():
            return (args.base_overlay / name).read_bytes()
        return retail.read(name)
    original = read_base('scripts/wheels.txt').decode('cp1252')
    source_scripts = {}
    file_count = 0
    total_bytes = 0
    normalized_parts = 0
    proc = subprocess.Popen(['bsdtar', '-cf', '-', '--format', 'pax', '@' + str(args.archive.resolve())], stdout=subprocess.PIPE)
    try:
        with tarfile.open(fileobj=proc.stdout, mode='r|') as archive:
            for entry in archive:
                path = PurePosixPath(entry.name.replace('\\', '/'))
                if path.is_absolute() or '..' in path.parts or entry.issym() or entry.islnk():
                    raise ValueError('Unsafe archive path: ' + entry.name)
                if not entry.isfile():
                    continue
                name = path.as_posix().lower()
                if name == 'readme install.txt':
                    (output / 'EAGLE-README.txt').write_bytes(archive.extractfile(entry).read())
                    continue
                if not name.startswith('juiced/'):
                    continue
                name = name[len('juiced/'):]
                if name.startswith('scripts/'):
                    source_scripts[name] = archive.extractfile(entry).read().decode('cp1252')
                    continue
                # Keep the native UI and tire catalogue. The pack's Windows menu is unnecessary.
                if not name.startswith(('carmesh/', 'cartextures/', 'ui/textures/')):
                    continue
                target = output / name
                target.parent.mkdir(parents=True, exist_ok=True)
                data = archive.extractfile(entry).read()
                if name.endswith('.d8m'):
                    data, changed = normalize_mesh_colours(data)
                    normalized_parts += changed
                if target.exists() and target.read_bytes() != data:
                    raise ValueError('Case-colliding asset: ' + name)
                target.write_bytes(data)
                file_count += 1
                total_bytes += len(data)
        if proc.wait() != 0:
            raise ValueError('Could not extract wheel archive')
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait()
    original_blocks = blocks(original)
    existing = {value(b, 'SHAPE').lower() for b in original_blocks}
    added = [b for b in blocks(source_scripts['scripts/wheels.txt']) if value(b, 'SHAPE').lower() not in existing]
    if len(original_blocks) + len(added) > 255:
        raise ValueError('Wheel catalogue exceeds native save byte capacity')
    scripts = output / 'scripts'
    scripts.mkdir(exist_ok=True)
    (scripts / 'wheels.txt').write_text(original.rstrip() + '\n\n; Eagle Ultimate Wheels: appended, original indices preserved.\n' + '\n\n'.join(added) + '\n')
    cardefs = read_base('scripts/cardefs.txt').decode('cp1252')
    colours = re.findall(r'^\s*\[WheelColour\]\s+(\d+\s+\d+\s+\d+)', cardefs, re.I | re.M)
    known = {tuple(map(int, c.split())) for c in colours}
    extra = []
    for c in re.findall(r'^\s*\[WheelColour\]\s+(\d+\s+\d+\s+\d+)', source_scripts.get('scripts/extra wheel colors.txt', ''), re.I | re.M):
        key = tuple(map(int, c.split()))
        if key not in known:
            known.add(key)
            extra.append('[WheelColour] ' + ' '.join(map(str, key)))
    (scripts / 'cardefs.txt').write_text(cardefs.rstrip() + '\n\n; Eagle extra wheel colours\n' + '\n'.join(extra) + '\n')
    missing = set()
    checked_materials = set()
    def available(name):
        if (output / name).is_file():
            return (output / name).read_bytes()
        try:
            return read_base(name)
        except FileNotFoundError:
            return None
    for b in added:
        for suffix in ('', '_blur'):
            name = 'carmesh/' + value(b, 'SHAPE').lower() + suffix + '.d8m'
            data = available(name)
            if data is None:
                missing.add(name)
                continue
            for mat in mesh_materials(data):
                if mat in checked_materials:
                    continue
                checked_materials.add(mat)
                pmdname = 'carmesh/mats/' + mat + '.pmd'
                pmd = available(pmdname)
                if pmd is None:
                    missing.add(pmdname)
                    continue
                for line in pmd.decode('cp1252').splitlines():
                    if '=' not in line:
                        continue
                    key, tex = map(str.strip, line.split('=', 1))
                    if key.lower() not in ('paint work', 'scratch & reflect', 'texture', 'texture 1', 'texture 2', 'reflection map', 'alpha texture', 'alpha test texture', 'opaque texture'):
                        continue
                    tex = PurePosixPath(tex.replace('\\', '/')).stem.lower()
                    if tex in ('', 'none'):
                        continue
                    if not any(available(folder + tex + '.ddt') is not None for folder in ('cartextures/', 'carmesh/textures/')):
                        missing.add('texture:' + tex)
    report = dict(archive=args.archive.name, sha256=hashlib.sha256(args.archive.read_bytes()).hexdigest(),
                  original_wheels=len(original_blocks), added_wheels=len(added), total_wheels=len(original_blocks)+len(added),
                  extra_colours=len(extra), assets=file_count, asset_bytes=total_bytes,
                  materials_checked=len(checked_materials), normalized_vertex_attributes=normalized_parts, missing_dependencies=sorted(missing),
                  original_indices_preserved=True, stock_wheels_preserved=True,
                  tyres_changed=False, native_ui_preserved=True)
    (output / 'wheel-mod-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if missing:
        raise SystemExit('Missing assets: inspect wheel-mod-report.json before using this overlay')

if __name__ == '__main__':
    main()
