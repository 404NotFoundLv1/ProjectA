from __future__ import annotations

import argparse
import binascii
import json
import math
import os
import shutil
import struct
import sys
import time
import traceback
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector


WIDTH = 4961
HEIGHT = 3508
PIXELS_PER_METER_300_DPI = 11811
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
TEMP_COLLECTION_NAME = "DRAWING_TEMP"
TEMP_WORLD_NAME = "DRAWING_TEMP_WORLD"
SECTION_MATERIAL_NAME = "DRAWING_TEMP_SECTION_RED"

SHEET_IDS = (
    "A01_FloorPlan",
    "A02_ReflectedCeilingPlan",
    "A03_NorthElevation",
    "A04_SouthElevation",
    "A05_WestElevation",
    "A06_EastElevation",
    "A07_LongitudinalSection",
    "A08_TransverseSection",
    "A09_ExplodedModulePlan",
    "A10_PerspectiveSheet",
    "D01_Cryopod",
    "D02_NavigationTable",
    "D03_MainAirlock",
    "D04_ConstructDock",
    "D05_WallBayInterface",
)

ORTHO_CAMERAS = {
    "A01_FloorPlan": ((0, 0, 30), (0, 0, 0), 34.0),
    "A02_ReflectedCeilingPlan": ((0, 0, -12), (0, 0, 7), 34.0),
    "A03_NorthElevation": ((0, -32, 3.5), (0, 10, 3.5), 32.0),
    "A04_SouthElevation": ((0, 32, 3.5), (0, -10, 3.5), 32.0),
    "A05_WestElevation": ((32, 0, 3.5), (-14, 0, 3.5), 28.0),
    "A06_EastElevation": ((-32, 0, 3.5), (14, 0, 3.5), 28.0),
}

# The exact camera directions establish the drawing coordinate contract:
# North looks +Y, South looks -Y, West looks -X, and East looks +X.
SECTION_CAMERAS = {
    "A07_LongitudinalSection": ((32, 0, 3.5), (0, 0, 3.5), 28.0),
    "A08_TransverseSection": ((0, -32, 3.5), (0, 0, 3.5), 32.0),
}

HIGH_OVERVIEW = ((18.0, -20.0, 18.0), (0.0, 0.0, 2.7), 52.0)
PERSPECTIVE_CAMERAS = {
    "front.png": ((0.0, -34.0, 8.0), (0.0, 0.0, 3.2), 52.0),
    "reverse.png": ((0.0, 34.0, 8.0), (0.0, 0.0, 3.2), 52.0),
    "west-oblique.png": ((11.0, -10.0, 6.0), (0.0, 0.0, 2.5), 48.0),
    "east-oblique.png": ((-11.0, -10.0, 6.0), (0.0, 0.0, 2.5), 48.0),
    "high-overview.png": HIGH_OVERVIEW,
    "ceiling-low-angle.png": ((0.0, -11.0, 0.8), (0.0, 0.0, 6.8), 14.0),
}

PERSPECTIVE_HIDDEN_NAMES = {
    "A10_PerspectiveSheet_Base.png": {
        "SM_ShipHub_CeilingPressureShell",
        "SM_ShipHub_CeilingServiceRing",
    },
    "west-oblique.png": {"SM_ShipHub_CeilingPressureShell"},
    "east-oblique.png": {"SM_ShipHub_CeilingPressureShell"},
    "high-overview.png": {
        "SM_ShipHub_CeilingPressureShell",
        "SM_ShipHub_CeilingServiceRing",
    },
    "ceiling-low-angle.png": {"SM_ShipHub_CeilingPressureShell"},
}


def _parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--blend", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--probe-sheet-visibility", choices=tuple(ORTHO_CAMERAS))
    return parser.parse_args(argv)


def _contained_path(candidate: Path, allowed_root: Path) -> bool:
    candidate_resolved = candidate.resolve()
    root_resolved = allowed_root.resolve()
    try:
        candidate_resolved.relative_to(root_resolved)
    except ValueError:
        return False
    return candidate_resolved != root_resolved


def _validated_paths(arguments: argparse.Namespace) -> tuple[Path, Path, Path, Path]:
    project_root = Path(arguments.project_root).resolve()
    blend_path = Path(arguments.blend).resolve()
    manifest_path = Path(arguments.manifest).resolve()
    output_root = Path(arguments.output_root).resolve()
    if not project_root.is_dir():
        raise ValueError(f"Project root is missing: {project_root}")
    complete_design_root = (
        project_root / "SourceArt" / "ProjectRift" / "ShipHub" / "CompleteDesign"
    ).resolve()
    expected_blend = (
        complete_design_root / "Blender" / "SM_ShipHub_Complete_White_v1.blend"
    ).resolve()
    expected_manifest = (complete_design_root / "Reports" / "layout-manifest.json").resolve()
    expected_output = (complete_design_root / "Drawings" / "PNG").resolve()
    if blend_path != expected_blend or not blend_path.is_file():
        raise ValueError(f"Authoritative BLEND path is missing or unexpected: {blend_path}")
    if manifest_path != expected_manifest or not manifest_path.is_file():
        raise ValueError(f"Authoritative manifest path is missing or unexpected: {manifest_path}")
    if output_root != expected_output or not _contained_path(output_root, complete_design_root):
        raise ValueError(f"Drawing output path is outside the exact approved location: {output_root}")
    return project_root, blend_path, manifest_path, output_root


def _require_blender_52_lts() -> None:
    if tuple(bpy.app.version[:2]) != (5, 2) or "LTS" not in bpy.app.version_string:
        raise RuntimeError(
            f"RenderDrawings requires Blender 5.2.x LTS; got {bpy.app.version_string}."
        )


def _load_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("schema") != "projectrift.shiphub.white-model-manifest.v1":
        raise ValueError("Authoritative manifest schema is invalid.")
    if tuple(manifest.get("sheet_ids", ())) != SHEET_IDS:
        raise ValueError("Authoritative manifest sheet_ids do not match the exact fifteen sheets.")
    blender = manifest.get("blender", {})
    if tuple(blender.get("version_tuple", ()))[:2] != (5, 2) or not blender.get("lts"):
        raise ValueError("Authoritative manifest was not produced by Blender 5.2.x LTS.")
    names = tuple(manifest.get("exports", {}).get("expected_object_names", ()))
    if not names:
        raise ValueError("Authoritative manifest has no expected production objects.")
    return manifest


def look_at(camera, target) -> None:
    direction = Vector(target) - camera.location
    if direction.length_squared == 0.0:
        raise ValueError("Camera location and target must differ.")
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def _configure_render(scene) -> None:
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = WIDTH
    scene.render.resolution_y = HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = 1.0
    scene.render.pixel_aspect_y = 1.0
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 35
    scene.render.use_file_extension = True
    scene.render.film_transparent = True
    scene.render.use_freestyle = True
    scene.render.line_thickness = 1.15
    scene.render.dither_intensity = 0.0
    scene.eevee.taa_render_samples = 64
    scene.eevee.use_shadows = True
    scene.eevee.use_raytracing = False
    scene.eevee.use_fast_gi = True
    scene.eevee.fast_gi_method = "AMBIENT_OCCLUSION_ONLY"
    scene.eevee.fast_gi_ray_count = 8
    scene.eevee.fast_gi_step_count = 16
    scene.eevee.fast_gi_distance = 12.0
    scene.view_settings.look = "AgX - Medium High Contrast"


def _ensure_temp_collection(scene):
    if bpy.data.collections.get(TEMP_COLLECTION_NAME) is not None:
        raise ValueError(f"Authoritative BLEND already contains reserved collection {TEMP_COLLECTION_NAME}.")
    collection = bpy.data.collections.new(TEMP_COLLECTION_NAME)
    scene.collection.children.link(collection)
    return collection


def _remove_temp_objects(collection) -> None:
    for obj in tuple(collection.objects):
        data = obj.data
        data_name = getattr(data, "name", "") if data is not None else ""
        data_type = obj.type
        bpy.data.objects.remove(obj, do_unlink=True)
        if data is not None and data.users == 0 and data_name.startswith(TEMP_COLLECTION_NAME):
            if data_type == "MESH":
                bpy.data.meshes.remove(data)
            elif data_type == "CAMERA":
                bpy.data.cameras.remove(data)
            elif data_type == "LIGHT":
                bpy.data.lights.remove(data)


def _snapshot_sources(objects: tuple) -> dict:
    return {
        obj.name: {
            "hide_render": obj.hide_render,
            "hide_viewport": obj.hide_viewport,
            "hide": obj.hide_get(),
            "matrix_world": obj.matrix_world.copy(),
            "materials": tuple(obj.data.materials) if obj.type == "MESH" else (),
        }
        for obj in objects
    }


def _restore_sources(objects: tuple, snapshot: dict) -> None:
    for obj in objects:
        state = snapshot[obj.name]
        obj.hide_render = state["hide_render"]
        obj.hide_viewport = state["hide_viewport"]
        obj.hide_set(state["hide"])
        obj.matrix_world = state["matrix_world"]
        if obj.type == "MESH":
            obj.data.materials.clear()
            for material in state["materials"]:
                obj.data.materials.append(material)


def _set_source_visibility(objects: tuple, visible_names: set[str]) -> None:
    for obj in objects:
        obj.hide_render = obj.name not in visible_names


def _configure_orthographic_source_visibility(
    sheet_id: str, objects: tuple, visible_names: set[str]
) -> dict[str, list[str]]:
    _set_source_visibility(objects, visible_names)
    hidden_for_view = {
        "A01_FloorPlan": {
            "SM_ShipHub_CeilingPressureShell",
            "SM_ShipHub_CeilingServiceRing",
        },
        "A02_ReflectedCeilingPlan": {"SM_ShipHub_FloorSlab"},
    }.get(sheet_id, set())
    for name in hidden_for_view:
        obj = bpy.data.objects.get(name)
        if obj is None or obj not in objects:
            raise ValueError(f"Orthographic visibility source is missing: {name}")
        obj.hide_render = True
    return {
        "sheet_id": sheet_id,
        "visible": sorted(obj.name for obj in objects if not obj.hide_render),
        "hidden": sorted(obj.name for obj in objects if obj.hide_render),
    }


def _create_world(scene):
    if bpy.data.worlds.get(TEMP_WORLD_NAME) is not None:
        raise ValueError(f"Authoritative BLEND already contains reserved world {TEMP_WORLD_NAME}.")
    world = bpy.data.worlds.new(TEMP_WORLD_NAME)
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.055, 0.065, 0.08, 1.0)
    background.inputs["Strength"].default_value = 0.7
    scene.world = world
    return world


def _add_light(collection, name: str, location, energy: float, size: float) -> None:
    data = bpy.data.lights.new(f"{TEMP_COLLECTION_NAME}_{name}_Data", "AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(f"{TEMP_COLLECTION_NAME}_{name}", data)
    obj.location = location
    collection.objects.link(obj)
    look_at(obj, (0.0, 0.0, 2.5))


def _add_camera(collection, location, target, ortho_scale: float | None, lens: float):
    data = bpy.data.cameras.new(f"{TEMP_COLLECTION_NAME}_Camera_Data")
    camera = bpy.data.objects.new(f"{TEMP_COLLECTION_NAME}_Camera", data)
    collection.objects.link(camera)
    camera.location = location
    look_at(camera, target)
    if ortho_scale is None:
        data.type = "PERSP"
        data.lens = lens
    else:
        data.type = "ORTHO"
        data.ortho_scale = ortho_scale
    data.clip_start = 0.05
    data.clip_end = 250.0
    return camera


def _prepare_view(scene, collection, location, target, ortho_scale, lens, studio: bool) -> None:
    _remove_temp_objects(collection)
    camera = _add_camera(collection, location, target, ortho_scale, lens)
    scene.camera = camera
    _add_light(collection, "Key", (18.0, -16.0, 28.0), 2600.0, 12.0)
    _add_light(collection, "Fill", (-20.0, -6.0, 18.0), 1700.0, 10.0)
    _add_light(collection, "Rim", (0.0, 22.0, 20.0), 2200.0, 10.0)
    scene.render.film_transparent = not studio


def _linked_duplicate(source, collection, name: str, transform: Matrix):
    duplicate = source.copy()
    duplicate.data = source.data
    duplicate.name = f"{TEMP_COLLECTION_NAME}_{name}_{source.name}"
    collection.objects.link(duplicate)
    duplicate.matrix_world = transform @ source.matrix_world
    duplicate.hide_render = False
    duplicate.hide_viewport = False
    return duplicate


def _group_transform(source_center, destination, rotation: Matrix | None = None) -> Matrix:
    rotation_matrix = rotation if rotation is not None else Matrix.Identity(4)
    return (
        Matrix.Translation(Vector(destination))
        @ rotation_matrix
        @ Matrix.Translation(-Vector(source_center))
    )


def _section_duplicate(source, collection, red_material, plane_normal, clear_outer, clear_inner):
    duplicate = source.copy()
    duplicate.data = source.data.copy()
    duplicate.data.name = f"{TEMP_COLLECTION_NAME}_SectionMesh_{source.name}"
    duplicate.name = f"{TEMP_COLLECTION_NAME}_Section_{source.name}"
    collection.objects.link(duplicate)
    duplicate.matrix_world = source.matrix_world.copy()
    duplicate.hide_render = False
    duplicate.hide_viewport = False

    local_plane_co = duplicate.matrix_world.inverted() @ Vector((0.0, 0.0, 0.0))
    local_plane_no = duplicate.matrix_world.to_3x3().transposed() @ Vector(plane_normal)
    local_plane_no.normalize()
    bm = bmesh.new()
    try:
        bm.from_mesh(duplicate.data)
        result = bmesh.ops.bisect_plane(
            bm,
            geom=tuple(bm.verts) + tuple(bm.edges) + tuple(bm.faces),
            dist=0.000001,
            plane_co=local_plane_co,
            plane_no=local_plane_no,
            use_snap_center=False,
            clear_outer=clear_outer,
            clear_inner=clear_inner,
        )
        cut_edges = [item for item in result.get("geom_cut", ()) if isinstance(item, bmesh.types.BMEdge)]
        if cut_edges:
            duplicate.data.materials.append(red_material)
            cap_result = bmesh.ops.holes_fill(bm, edges=cut_edges, sides=0)
            cap_index = len(duplicate.data.materials) - 1
            for face in cap_result.get("faces", ()):
                face.material_index = cap_index
        bm.to_mesh(duplicate.data)
        duplicate.data.update()
    finally:
        bm.free()
    if len(duplicate.data.vertices) == 0:
        data = duplicate.data
        bpy.data.objects.remove(duplicate, do_unlink=True)
        bpy.data.meshes.remove(data)
        return None
    return duplicate


def _render(scene, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(output_path)
    result = bpy.ops.render.render(write_still=True)
    if "FINISHED" not in result:
        raise RuntimeError(f"Blender render did not return FINISHED for {output_path}: {sorted(result)}")
    if not output_path.is_file() or output_path.stat().st_size <= 0:
        raise RuntimeError(f"Blender did not write a nonempty PNG: {output_path}")
    _write_300dpi_phys(output_path)
    _validate_png(output_path)


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    payload = chunk_type + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", binascii.crc32(payload) & 0xFFFFFFFF)


def _read_png_chunks(data: bytes) -> list[tuple[bytes, bytes]]:
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("PNG signature is invalid.")
    chunks = []
    offset = len(PNG_SIGNATURE)
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        end = offset + 12 + length
        if end > len(data):
            raise ValueError("PNG chunk extends beyond the file.")
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        stored_crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        actual_crc = binascii.crc32(chunk_type + payload) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            raise ValueError(f"PNG chunk CRC mismatch: {chunk_type!r}")
        chunks.append((chunk_type, payload))
        offset = end
        if chunk_type == b"IEND":
            if offset != len(data):
                raise ValueError("PNG contains data after IEND.")
            return chunks
    raise ValueError("PNG is missing IEND.")


def _write_300dpi_phys(path: Path) -> None:
    chunks = _read_png_chunks(path.read_bytes())
    rebuilt = bytearray(PNG_SIGNATURE)
    inserted = False
    for chunk_type, payload in chunks:
        if chunk_type == b"pHYs":
            continue
        if chunk_type == b"tEXt" and payload.partition(b"\x00")[0] in {
            b"Date",
            b"RenderTime",
        }:
            continue
        rebuilt.extend(_png_chunk(chunk_type, payload))
        if chunk_type == b"IHDR":
            phys = struct.pack(">IIB", PIXELS_PER_METER_300_DPI, PIXELS_PER_METER_300_DPI, 1)
            rebuilt.extend(_png_chunk(b"pHYs", phys))
            inserted = True
    if not inserted:
        raise ValueError(f"Cannot add pHYs because IHDR is missing: {path}")
    temporary = path.with_name(f"{path.name}.phys.tmp")
    try:
        temporary.write_bytes(rebuilt)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _validate_png(path: Path) -> dict:
    if not path.is_file() or path.stat().st_size <= 0:
        raise ValueError(f"PNG is missing or empty: {path}")
    chunks = _read_png_chunks(path.read_bytes())
    ihdr = [payload for chunk_type, payload in chunks if chunk_type == b"IHDR"]
    phys = [payload for chunk_type, payload in chunks if chunk_type == b"pHYs"]
    if len(ihdr) != 1 or len(ihdr[0]) != 13:
        raise ValueError(f"PNG must contain one valid IHDR: {path}")
    width, height = struct.unpack(">II", ihdr[0][:8])
    if (width, height) != (WIDTH, HEIGHT):
        raise ValueError(f"PNG dimensions must be {WIDTH}x{HEIGHT}: {path}")
    if len(phys) != 1 or len(phys[0]) != 9:
        raise ValueError(f"PNG must contain one valid pHYs: {path}")
    x_ppm, y_ppm, unit = struct.unpack(">IIB", phys[0])
    if (x_ppm, y_ppm, unit) != (
        PIXELS_PER_METER_300_DPI,
        PIXELS_PER_METER_300_DPI,
        1,
    ):
        raise ValueError(f"PNG pHYs must encode 300 dpi in meters: {path}")
    return {
        "path": str(path),
        "bytes": path.stat().st_size,
        "width": width,
        "height": height,
        "pixels_per_meter": x_ppm,
        "unit": unit,
    }


def _unique_work_directory(output_root: Path, prefix: str) -> Path:
    output_root.mkdir(parents=True, exist_ok=True)
    for attempt in range(1000):
        candidate = output_root / f".{prefix}-{os.getpid()}-{attempt:03d}"
        try:
            candidate.mkdir(parents=False, exist_ok=False)
            return candidate
        except FileExistsError:
            continue
    raise RuntimeError(f"Could not allocate unique {prefix} directory below {output_root}.")


def _expected_relative_paths() -> tuple[Path, ...]:
    bases = tuple(Path(f"{sheet_id}_Base.png") for sheet_id in SHEET_IDS)
    perspectives = tuple(Path("Perspectives") / name for name in PERSPECTIVE_CAMERAS)
    return bases + perspectives


def _transactional_replace(staging_root: Path, output_root: Path, relative_paths: tuple[Path, ...]) -> None:
    backup_root = _unique_work_directory(output_root, "shiphub-render-backup")
    backups: list[tuple[Path, Path]] = []
    installed: list[Path] = []
    cleanup_backup = True
    try:
        unexpected_root_files = {
            path.name for path in output_root.glob("*.png")
        } - {path.name for path in relative_paths if len(path.parts) == 1}
        perspectives_root = output_root / "Perspectives"
        unexpected_perspectives = (
            {path.name for path in perspectives_root.glob("*.png")}
            - {path.name for path in relative_paths if path.parts[0] == "Perspectives"}
            if perspectives_root.is_dir()
            else set()
        )
        if unexpected_root_files or unexpected_perspectives:
            raise ValueError(
                "Refusing to remove unowned PNGs: "
                + ", ".join(sorted(unexpected_root_files | unexpected_perspectives))
            )
        for relative_path in relative_paths:
            final_path = output_root / relative_path
            if final_path.exists():
                backup_path = backup_root / relative_path
                backup_path.parent.mkdir(parents=True, exist_ok=True)
                os.replace(final_path, backup_path)
                backups.append((final_path, backup_path))
        for relative_path in relative_paths:
            staged_path = staging_root / relative_path
            final_path = output_root / relative_path
            final_path.parent.mkdir(parents=True, exist_ok=True)
            os.replace(staged_path, final_path)
            installed.append(final_path)
        for relative_path in relative_paths:
            _validate_png(output_root / relative_path)
    except Exception as replace_error:
        rollback_errors = []
        for final_path in reversed(installed):
            try:
                if final_path.exists():
                    final_path.unlink()
            except Exception as error:
                rollback_errors.append(f"remove {final_path}: {error}")
        for final_path, backup_path in reversed(backups):
            try:
                if backup_path.exists():
                    final_path.parent.mkdir(parents=True, exist_ok=True)
                    os.replace(backup_path, final_path)
            except Exception as error:
                rollback_errors.append(f"restore {final_path}: {error}")
        if rollback_errors:
            cleanup_backup = False
            raise RuntimeError(
                "Drawing transaction failed and rollback was incomplete: "
                + "; ".join(rollback_errors)
                + f". Preserved backup directory: {backup_root}"
            ) from replace_error
        raise
    finally:
        if cleanup_backup:
            shutil.rmtree(backup_root, ignore_errors=True)


def _render_orthographic_bases(scene, collection, sources, visible_names, staging_root) -> None:
    for sheet_id, (location, target, scale) in ORTHO_CAMERAS.items():
        _configure_orthographic_source_visibility(sheet_id, sources, visible_names)
        _prepare_view(scene, collection, location, target, scale, 52.0, studio=False)
        _render(scene, staging_root / f"{sheet_id}_Base.png")


def _render_sections(scene, collection, sources, red_material, staging_root) -> None:
    section_specs = {
        "A07_LongitudinalSection": ((1.0, 0.0, 0.0), True, False),
        "A08_TransverseSection": ((0.0, 1.0, 0.0), False, True),
    }
    for sheet_id, (normal, clear_outer, clear_inner) in section_specs.items():
        _set_source_visibility(sources, set())
        for source in sources:
            _section_duplicate(source, collection, red_material, normal, clear_outer, clear_inner)
        location, target, scale = SECTION_CAMERAS[sheet_id]
        camera = _add_camera(collection, location, target, scale, 52.0)
        scene.camera = camera
        _add_light(collection, "Key", (18.0, -16.0, 28.0), 2600.0, 12.0)
        _add_light(collection, "Fill", (-20.0, -6.0, 18.0), 1700.0, 10.0)
        _add_light(collection, "Rim", (0.0, 22.0, 20.0), 2200.0, 10.0)
        scene.render.film_transparent = True
        _render(scene, staging_root / f"{sheet_id}_Base.png")
        _remove_temp_objects(collection)


def _render_exploded(scene, collection, sources, staging_root) -> None:
    _set_source_visibility(sources, set())
    collection_order = (
        "10_STRUCTURE",
        "20_NAV_TABLE",
        "30_CRYOPODS",
        "40_AIRLOCK",
        "50_WEST_BAYS",
        "60_EAST_BAYS",
        "70_CONSTRUCT_DOCKS",
        "80_CEILING",
    )
    offsets = {name: index * 1.5 for index, name in enumerate(collection_order)}
    _prepare_view(scene, collection, (32.0, -34.0, 30.0), (0.0, 0.0, 7.0), 38.0, 52.0, studio=False)
    for source in sources:
        if source.name == "SM_ShipHub_CeilingPressureShell":
            continue
        source_collection = next((item.name for item in source.users_collection if item.name in offsets), None)
        transform = Matrix.Translation((0.0, 0.0, offsets.get(source_collection, 0.0)))
        _linked_duplicate(source, collection, "Exploded", transform)
    _render(scene, staging_root / "A09_ExplodedModulePlan_Base.png")


def _duplicate_named_group(collection, names, center, destination, rotation, tag) -> None:
    transform = _group_transform(center, destination, rotation)
    for name in names:
        source = bpy.data.objects.get(name)
        if source is None:
            raise ValueError(f"Detail view source object is missing: {name}")
        _linked_duplicate(source, collection, tag, transform)


def _render_details(scene, collection, sources, staging_root) -> None:
    _set_source_visibility(sources, set())

    _prepare_view(scene, collection, (0.0, -36.0, 4.0), (0.0, 0.0, 3.0), 22.0, 52.0, studio=False)
    cryopod = ("SM_ShipHub_Cryopod_01",)
    cryopod_center = (-4.0, 9.8, 0.0)
    _duplicate_named_group(collection, cryopod, cryopod_center, (-8.0, 0.0, 0.0), Matrix.Identity(4), "CryopodFront")
    _duplicate_named_group(collection, cryopod, cryopod_center, (0.0, 0.0, 0.0), Matrix.Rotation(math.radians(90.0), 4, "Z"), "CryopodSide")
    _duplicate_named_group(collection, cryopod, cryopod_center, (8.0, 0.0, 3.0), Matrix.Rotation(math.radians(90.0), 4, "X"), "CryopodTop")
    _render(scene, staging_root / "D01_Cryopod_Base.png")

    _prepare_view(scene, collection, (0.0, -42.0, 5.0), (0.0, 0.0, 3.5), 27.0, 52.0, studio=False)
    nav_names = (
        "SM_ShipHub_NavTable_Display",
        "SM_ShipHub_NavTable_Sector_01",
        "SM_ShipHub_NavTable_Sector_02",
        "SM_ShipHub_NavTable_Sector_03",
        "SM_ShipHub_NavTable_Sector_04",
    )
    _duplicate_named_group(collection, nav_names, (0.0, 0.0, 0.0), (-9.0, 0.0, 4.0), Matrix.Rotation(math.radians(90.0), 4, "X"), "NavTop")
    _duplicate_named_group(collection, nav_names, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), Matrix.Identity(4), "NavSide")
    breakdown = _group_transform((0.0, 0.0, 0.0), (9.0, 0.0, 4.0), Matrix.Rotation(math.radians(90.0), 4, "X"))
    display = bpy.data.objects.get("SM_ShipHub_NavTable_Display")
    _linked_duplicate(display, collection, "NavBreakdown", breakdown)
    for index, name in enumerate(nav_names[1:]):
        source = bpy.data.objects.get(name)
        x_sign = 1.0 if index in (0, 3) else -1.0
        z_sign = 1.0 if index in (0, 1) else -1.0
        offset = Matrix.Translation((0.7 * x_sign, 0.0, 0.7 * z_sign))
        _linked_duplicate(source, collection, "NavBreakdown", offset @ breakdown)
    _render(scene, staging_root / "D02_NavigationTable_Base.png")

    _prepare_view(scene, collection, (0.0, -42.0, 5.0), (0.0, 0.0, 2.5), 30.0, 52.0, studio=False)
    airlock_names = (
        "SM_ShipHub_Airlock_DoorPanel",
        "SM_ShipHub_Airlock_FrameEast",
        "SM_ShipHub_Airlock_FrameWest",
        "SM_ShipHub_Airlock_Header",
    )
    _duplicate_named_group(collection, airlock_names, (0.0, -12.0, 0.0), (-10.0, 0.0, 0.0), Matrix.Identity(4), "AirlockFront")
    _duplicate_named_group(collection, airlock_names, (0.0, -12.0, 0.0), (0.0, 0.0, 0.0), Matrix.Rotation(math.radians(90.0), 4, "Z"), "AirlockSection")
    plan_rotation = Matrix.Rotation(math.radians(90.0), 4, "X")
    plan_destination = (10.0, 0.0, 4.5)
    _duplicate_named_group(
        collection,
        airlock_names,
        (0.0, -12.0, 0.0),
        plan_destination,
        plan_rotation,
        "AirlockEnvelope",
    )
    door = bpy.data.objects.get("SM_ShipHub_Airlock_DoorPanel")
    hinge = Vector((-1.9, -12.0, 0.0))
    envelope_transform = _group_transform(
        (0.0, -12.0, 0.0), plan_destination, plan_rotation
    )
    swing = Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(45.0), 4, "Z") @ Matrix.Translation(-hinge)
    _linked_duplicate(door, collection, "AirlockDoorSwing", envelope_transform @ swing)
    _render(scene, staging_root / "D03_MainAirlock_Base.png")

    _prepare_view(scene, collection, (0.0, -30.0, 3.0), (0.0, 0.0, 1.5), 15.0, 52.0, studio=False)
    dock_names = ("SM_ShipHub_ConstructDock_01",)
    dock_center = (-5.3, -5.3, 0.0)
    _duplicate_named_group(collection, dock_names, dock_center, (-4.0, 0.0, 2.0), Matrix.Rotation(math.radians(90.0), 4, "X"), "DockPlan")
    _duplicate_named_group(collection, dock_names, dock_center, (4.0, 0.0, 0.0), Matrix.Identity(4), "DockSection")
    _render(scene, staging_root / "D04_ConstructDock_Base.png")

    _prepare_view(scene, collection, (0.0, -44.0, 5.0), (0.0, 0.0, 3.0), 30.0, 52.0, studio=False)
    bay_specs = (
        ("SM_ShipHub_East_RoleBay", (13.2, -3.0, 1.2), 1.0, -10.0),
        ("SM_ShipHub_East_MedicalBay", (13.2, -6.0, 1.2), 2.0, 0.0),
        ("SM_ShipHub_West_RepairBay", (-14.0, 6.0, 1.4), 4.0, 10.0),
    )
    for name, center, nominal_width, destination_x in bay_specs:
        source = bpy.data.objects.get(name)
        if source is None:
            raise ValueError(f"Wall-bay authority is missing: {name}")
        authoritative_span = float(source.dimensions.y)
        if authoritative_span <= 0.0:
            raise ValueError(f"Wall-bay authority has no Y span: {name}")
        width_scale = nominal_width / authoritative_span
        scale_y = Matrix.Diagonal((1.0, width_scale, 1.0, 1.0))
        front_transform = (
            Matrix.Translation((destination_x, 0.0, 1.2))
            @ Matrix.Rotation(math.radians(90.0), 4, "Z")
            @ scale_y
            @ Matrix.Translation(-Vector(center))
        )
        side_transform = (
            Matrix.Translation((destination_x, 0.0, 6.0))
            @ scale_y
            @ Matrix.Translation(-Vector(center))
        )
        width_label = f"{int(nominal_width)}m"
        _linked_duplicate(source, collection, f"WallBay{width_label}Front", front_transform)
        _linked_duplicate(source, collection, f"WallBay{width_label}Side", side_transform)
    _render(scene, staging_root / "D05_WallBayInterface_Base.png")


def _render_perspectives(scene, collection, sources, visible_names, staging_root) -> None:
    def configure_visibility(view_name: str) -> None:
        _set_source_visibility(sources, visible_names)
        for hidden_name in PERSPECTIVE_HIDDEN_NAMES.get(view_name, set()):
            hidden_object = bpy.data.objects.get(hidden_name)
            if hidden_object is None or hidden_object not in sources:
                raise ValueError(f"Perspective visibility source is missing: {hidden_name}")
            hidden_object.hide_render = True

    configure_visibility("A10_PerspectiveSheet_Base.png")
    location, target, lens = HIGH_OVERVIEW
    _prepare_view(scene, collection, location, target, None, lens, studio=True)
    _render(scene, staging_root / "A10_PerspectiveSheet_Base.png")
    for filename, (location, target, lens) in PERSPECTIVE_CAMERAS.items():
        configure_visibility(filename)
        _prepare_view(scene, collection, location, target, None, lens, studio=True)
        _render(scene, staging_root / "Perspectives" / filename)


def render_drawings(blend_path: Path, manifest: dict, output_root: Path) -> dict:
    open_result = bpy.ops.wm.open_mainfile(filepath=str(blend_path))
    if "FINISHED" not in open_result:
        raise RuntimeError(f"Could not open authoritative BLEND: {sorted(open_result)}")
    if Path(bpy.data.filepath).resolve() != blend_path:
        raise RuntimeError("Opened Blender filepath does not match the authoritative BLEND.")

    scene = bpy.context.scene
    _configure_render(scene)
    expected_names = tuple(manifest["exports"]["expected_object_names"])
    missing_names = [name for name in expected_names if bpy.data.objects.get(name) is None]
    if missing_names:
        raise ValueError(f"Authoritative BLEND is missing manifest objects: {missing_names}")
    sources = tuple(bpy.data.objects[name] for name in expected_names)
    if any(obj.type != "MESH" for obj in sources):
        raise ValueError("Every manifest production object must be a mesh.")
    visible_names = set(expected_names)
    source_snapshot = _snapshot_sources(sources)
    original_world = scene.world
    original_camera = scene.camera
    temp_collection = None
    temp_world = None
    red_material = None
    staging_root = _unique_work_directory(output_root, "shiphub-render-staging")
    relative_paths = _expected_relative_paths()
    start_time = time.monotonic()
    try:
        temp_collection = _ensure_temp_collection(scene)
        temp_world = _create_world(scene)
        red_material = bpy.data.materials.new(SECTION_MATERIAL_NAME)
        red_material.diffuse_color = (0.8, 0.025, 0.02, 1.0)
        principled = red_material.node_tree.nodes.get("Principled BSDF")
        principled.inputs["Base Color"].default_value = (0.8, 0.025, 0.02, 1.0)
        principled.inputs["Roughness"].default_value = 0.58

        _render_orthographic_bases(scene, temp_collection, sources, visible_names, staging_root)
        _restore_sources(sources, source_snapshot)
        _remove_temp_objects(temp_collection)
        _render_sections(scene, temp_collection, sources, red_material, staging_root)
        _restore_sources(sources, source_snapshot)
        _remove_temp_objects(temp_collection)
        _render_exploded(scene, temp_collection, sources, staging_root)
        _restore_sources(sources, source_snapshot)
        _remove_temp_objects(temp_collection)
        _render_details(scene, temp_collection, sources, staging_root)
        _restore_sources(sources, source_snapshot)
        _remove_temp_objects(temp_collection)
        _render_perspectives(scene, temp_collection, sources, visible_names, staging_root)

        staged_metadata = [_validate_png(staging_root / relative_path) for relative_path in relative_paths]
        _transactional_replace(staging_root, output_root, relative_paths)
        final_metadata = [_validate_png(output_root / relative_path) for relative_path in relative_paths]
        return {
            "blender": bpy.app.version_string,
            "engine": scene.render.engine,
            "samples": scene.eevee.taa_render_samples,
            "render_count": len(final_metadata),
            "resolution": [WIDTH, HEIGHT],
            "pixels_per_meter": PIXELS_PER_METER_300_DPI,
            "elapsed_seconds": round(time.monotonic() - start_time, 3),
            "transaction": "committed",
            "staged_count": len(staged_metadata),
            "outputs": final_metadata,
        }
    finally:
        _restore_sources(sources, source_snapshot)
        if temp_collection is not None:
            _remove_temp_objects(temp_collection)
            if temp_collection.name in {child.name for child in scene.collection.children}:
                scene.collection.children.unlink(temp_collection)
            bpy.data.collections.remove(temp_collection)
        scene.camera = original_camera
        scene.world = original_world
        if red_material is not None and red_material.users == 0:
            bpy.data.materials.remove(red_material)
        if temp_world is not None and temp_world.users == 0:
            bpy.data.worlds.remove(temp_world)
        shutil.rmtree(staging_root, ignore_errors=True)


def probe_sheet_visibility(blend_path: Path, manifest: dict, sheet_id: str) -> dict:
    open_result = bpy.ops.wm.open_mainfile(filepath=str(blend_path))
    if "FINISHED" not in open_result:
        raise RuntimeError(f"Could not open authoritative BLEND: {sorted(open_result)}")
    expected_names = tuple(manifest["exports"]["expected_object_names"])
    missing_names = [name for name in expected_names if bpy.data.objects.get(name) is None]
    if missing_names:
        raise ValueError(f"Authoritative BLEND is missing manifest objects: {missing_names}")
    sources = tuple(bpy.data.objects[name] for name in expected_names)
    snapshot = _snapshot_sources(sources)
    try:
        return _configure_orthographic_source_visibility(sheet_id, sources, set(expected_names))
    finally:
        _restore_sources(sources, snapshot)


def main(argv: list[str]) -> int:
    _require_blender_52_lts()
    arguments = _parse_arguments(argv)
    _, blend_path, manifest_path, output_root = _validated_paths(arguments)
    manifest = _load_manifest(manifest_path)
    if arguments.probe_sheet_visibility:
        probe = probe_sheet_visibility(
            blend_path, manifest, arguments.probe_sheet_visibility
        )
        print("SHIPHUB_VISIBILITY_PROBE=" + json.dumps(probe, sort_keys=True))
        return 0
    summary = render_drawings(blend_path, manifest, output_root)
    print("SHIPHUB_RENDER_SUMMARY=" + json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        separator_index = sys.argv.index("--")
        script_arguments = sys.argv[separator_index + 1 :]
        raise SystemExit(main(script_arguments))
    except SystemExit:
        raise
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
