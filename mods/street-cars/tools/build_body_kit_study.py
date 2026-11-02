"""Blender study fitted to reviewed native stock geometry, never the original sources.

blender --background --python-exit-code 1 --python THIS -- REPO EMPTY_OUTPUT [--levels 1 ...]
Native: centimetres, Y up, +Z forward. Blender: metres, Z up, -Y forward.
GLB export_yup converts back to native axes in metres; adapter multiplies by 100.
Existing stock/sport/hood/decal/wheel/dynamics assets are not regenerated.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('repo', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--levels', type=int, nargs='+', choices=[1, 2, 3], default=[2, 3])
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
repo, output = args.repo, args.output
levels = sorted(set(args.levels))
sys.path.insert(0, str(repo / 'tools'))
from vinyl_pack import mesh as native

if output.exists() and any(output.iterdir()):
    raise RuntimeError('Use a fresh study directory')
output.mkdir(parents=True, exist_ok=True)
cars = [('integra_db8', 'db8'), ('bmw_e36', 'e36'), ('levin_ae92', 'ae92')]


def convert(points):
    return np.c_[points[:, 0], -points[:, 2], points[:, 1]] / 100


def material(name, colour):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = (*colour, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*colour, 1)
    bsdf.inputs['Roughness'].default_value = .34 if name.endswith('_body') else .7
    return mat


def ribbon(name, sections, mat, collection):
    vertices = [v for row in sections for v in row]
    faces = []
    for i in range(len(sections) - 1):
        for j in range(4):
            faces.append((4*i+j, 4*(i+1)+j, 4*(i+1)+(j+1)%4, 4*i+(j+1)%4))
    faces.extend([(3, 2, 1, 0), tuple(range(len(vertices)-4, len(vertices)))])
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    data.materials.append(mat)
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    if bm.calc_volume(signed=True) < 0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
    if any(not edge.is_manifold for edge in bm.edges) or bm.calc_volume() < 1e-7:
        raise RuntimeError('Non-solid kit: ' + name)
    volume = bm.calc_volume()
    bm.to_mesh(data)
    bm.free()
    bevel = obj.modifiers.new('3 mm edge radius', 'BEVEL')
    bevel.width = .003
    bevel.segments = 2
    # Export evaluated bevel geometry, not a one-sided preview-only accessory.
    return obj, volume


for model, prefix in cars:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    base = bpy.data.collections.new('REVIEWED_STOCK_REFERENCE')
    scene.collection.children.link(base)
    paint = material(model + '_body', (.08, .24, .5))
    trim = material(prefix + '_kit_trim', (.025, .025, .028))
    hashes, zones, paint_zones, report = {}, {}, {}, []
    folder = repo / 'mods/street-cars/assets/cars' / model / 'mesh'
    for zone in ['chass', 'fbump', 'rbump', 'bonnet', 'sskirt_l', 'sskirt_r', 'brklight', 'lightson']:
        path = folder / (prefix + '_' + zone + '.d8m')
        if not path.exists():
            continue
        hashes[str(path.relative_to(repo))] = hashlib.sha256(path.read_bytes()).hexdigest()
        parsed = native.read(path.read_bytes())
        vertices, faces, paint_vertices, paint_faces = [], [], [], []
        for part in parsed['parts']:
            start = len(vertices)
            positions = convert(part['streams'][0, 0])
            triangles = native.triangles(parsed, part)
            vertices.extend(map(tuple, positions))
            faces.extend(map(tuple, triangles + start))
            if part['material'] == model+'_body':
                first = len(paint_vertices)
                paint_vertices.extend(map(tuple, positions))
                paint_faces.extend(map(tuple, triangles + first))
            data = bpy.data.meshes.new(zone + '_' + part['material'])
            data.from_pydata(positions.tolist(), [], triangles.tolist())
            data.update()
            obj = bpy.data.objects.new(data.name, data)
            base.objects.link(obj)
            data.materials.append(paint if part['material'] == model+'_body' else
                                  material(part['material'], (.13, .14, .15)))
        zones[zone] = (vertices, faces)
        paint_zones[zone] = (paint_vertices, paint_faces)
    wheel_report = json.loads((repo/'mods/street-cars/assets/reports'/(model+'.json')).read_text())
    exhaust_path = repo/'mods/street-cars/assets/carmesh'/(prefix+'_exhaust.d8m')
    hierarchy = repo/'mods/street-cars/assets/cars'/model/'models'/(model+'_stock.boh')
    mount = next(matrix for name, _, matrix in native.boh(hierarchy.read_bytes()) if name == 'EXHAUST_S')
    exhaust = native.read(exhaust_path.read_bytes())
    exhaust_points = []
    for part in exhaust['parts']:
        points = part['streams'][0, 0] + mount[:3, 3]
        exhaust_points.extend(points)
        data = bpy.data.meshes.new('Stock_exhaust')
        data.from_pydata(convert(points).tolist(), [], native.triangles(exhaust, part).tolist())
        data.update()
        obj = bpy.data.objects.new(data.name, data)
        base.objects.link(obj)
        data.materials.append(material('Exhaust_reference', (.55, .55, .58)))
    exhaust_points = np.asarray(exhaust_points)
    tips = exhaust_points[exhaust_points[:, 2] < exhaust_points[:, 2].min()+12]
    exhaust_notch = (float(tips[:, 0].min())/100-.025, float(tips[:, 0].max())/100+.025)
    hashes[str(exhaust_path.relative_to(repo))] = hashlib.sha256(exhaust_path.read_bytes()).hexdigest()
    hashes[str(hierarchy.relative_to(repo))] = hashlib.sha256(hierarchy.read_bytes()).hexdigest()
    wheel = native.read((folder/(prefix+'_wheel.d8m')).read_bytes())
    for corner, centre in wheel_report['wheels'].items():
        for part in wheel['parts']:
            points = part['streams'][0, 0].copy()
            if corner in ['FR', 'RR']:
                points[:, 0] *= -1
            points += centre
            data = bpy.data.meshes.new('Wheel_'+corner)
            data.from_pydata(convert(points).tolist(), [], native.triangles(wheel, part).tolist())
            data.update()
            obj = bpy.data.objects.new(data.name, data)
            base.objects.link(obj)
            data.materials.append(trim)
    front = wheel_report['wheels']['FL'][2] / 100
    rear = wheel_report['wheels']['RL'][2] / 100
    # Actual wheel mesh tyre extent is more conservative than guessed radius.
    allwheel = np.concatenate([p['streams'][0, 0] for p in wheel['parts']]) / 100
    half = float(np.max(np.abs(allwheel[:, 2])))
    skirt_start, skirt_end = -front + half + .055, -rear - half - .055
    for zone in ['sskirt_l', 'sskirt_r']:
        points = np.asarray(zones[zone][0])
        skirt_start = max(skirt_start, float(points[:, 1].min()) + .025)
        skirt_end = min(skirt_end, float(points[:, 1].max()) - .025)
    for level in levels:
        collection = bpy.data.collections.new('KIT_'+str(level))
        scene.collection.children.link(collection)
        kit_material = paint if level in [1, 2] else trim
        for tag, zone, sign in [('FRONT', 'fbump', -1), ('REAR', 'rbump', 1)]:
            # Fit the lower painted bumper, not inset grille/plate triangles
            # whose depth discontinuities would make the new lip corrugated.
            vertices, faces = paint_zones[zone] if tag == 'FRONT' else zones[zone]
            pts = np.asarray(vertices)
            tree = BVHTree.FromPolygons(vertices, faces)
            end = pts[:, 1].min() if sign < 0 else pts[:, 1].max()
            width = min(abs(pts[:, 0].min()), abs(pts[:, 0].max())) * .94
            height = (max(.20, float(pts[:, 2].min()) + .035) if tag == 'FRONT' else
                      max(.23, float(pts[:, 2].min()) + .085))
            samples = []
            for x in np.linspace(-width, width, 33):
                hit = None
                for z in np.arange(height, .475, .005):
                    hit = tree.ray_cast(Vector((x, end + sign*.8, z)), Vector((0, -sign, 0)), 1.3)[0]
                    if hit is not None:
                        break
                if hit is None:
                    raise RuntimeError(f'Bumper mount missing {model} {tag} {x}')
                samples.append(hit)
            sections = []
            for p in samples:
                x, y, z = p
                taper = max(.2, 1 - (abs(x)/width)**6)
                extension = (.012 if level == 1 else .035 if level == 2 else .065) * taper
                drop = .022 if level == 1 else .055 if level == 2 else .022
                if tag == 'FRONT':
                    # The new visible edge stays below *all* original front
                    # geometry, including low number plates and intake trim.
                    edge_top = float(np.asarray(zones[zone][0])[:, 2].min()) - .008
                    edge_bottom = edge_top - (.01 if level == 1 else .035 if level == 2 else .018)
                else:
                    edge_top, edge_bottom = height-.012, height-drop
                # Mount follows the source; the visible lip has a deliberate
                # continuous lower edge rather than copying noisy ray heights.
                sections.append([(x, y-sign*.022, z+.012), (x, y+sign*extension, edge_top),
                                 (x, y+sign*extension, edge_bottom), (x, y-sign*.022, edge_bottom)])
            groups = [[]]
            for section in sections:
                x = section[0][0]
                if tag == 'REAR' and exhaust_notch[0] < x < exhaust_notch[1]:
                    if groups[-1]:
                        groups.append([])
                else:
                    groups[-1].append(section)
            volume = 0
            for i, group in enumerate(groups):
                if len(group) >= 2:
                    _, solid = ribbon(tag+'_VALANCE_'+str(level)+'_'+str(i), group, kit_material, collection)
                    volume += solid
            report.append(dict(level=level, part=tag, contacts=len(samples), volume_m3=volume,
                               mount_search_start_m=height, mount_points_m=[list(p) for p in samples],
                               exhaust_clearance_x_m=exhaust_notch if tag == 'REAR' else None))
        for side, zone in [(-1, 'sskirt_l'), (1, 'sskirt_r')]:
            vertices, faces = zones[zone]
            pts = np.asarray(vertices)
            tree = BVHTree.FromPolygons(vertices, faces)
            height = max(.195, float(pts[:, 2].min()) + .055)
            sections = []
            for y in np.linspace(skirt_start, skirt_end, 25):
                p = None
                # Rocker ends rise into the wheel arches; use their real local
                # lower surface instead of bridging empty space at a fixed Z.
                for z in np.arange(height, .365, .005):
                    p = tree.ray_cast(Vector((side*1.5, y, z)), Vector((-side, 0, 0)), 1.2)[0]
                    if p is not None:
                        break
                if p is None:
                    raise RuntimeError(f'Skirt mount missing {model} {zone} {y}')
                x, y, z = p
                shape = max(.25, math.sin(math.pi * (y-skirt_start)/(skirt_end-skirt_start)))
                extension = (.012 if level == 1 else .035 if level == 2 else .06) * shape
                # Keep the outer wall thicker than the evaluated3mm bevel on
                # both edges; a3mm wall collapses into degenerate export faces.
                drop = .025 if level == 1 else .045 if level == 2 else .022
                sections.append([(x-side*.022, y, z+.01), (x+side*extension, y, height-.012),
                                 (x+side*extension, y, height-drop), (x-side*.022, y, height-drop)])
            obj, volume = ribbon('SKIRT_'+('L' if side<0 else 'R')+'_'+str(level), sections,
                                 kit_material, collection)
            report.append(dict(level=level, part=zone, contacts=len(sections), volume_m3=volume,
                               mount_height_m=height, wheel_clearance_m=.055))
        bpy.ops.object.select_all(action='DESELECT')
        for obj in collection.objects:
            obj.select_set(True)
        path = output / (model+'-kit'+str(level)+'.glb')
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True,
                                  export_apply=True, export_yup=True, export_texcoords=True)
        collection.hide_render = collection.hide_viewport = True
    bpy.ops.wm.save_as_mainfile(filepath=str(output/(model+'-kit-study.blend')))
    (output/(model+'-kit-study.json')).write_text(json.dumps(dict(
        status='candidate: native-render review required', units='metres, Blender Z-up / -Y-forward',
        adapter='GLB Y-up positions *100; no handedness flip', stock_sha256=hashes, parts=report), indent=2)+'\n')
    print('STUDY', model, flush=True)
