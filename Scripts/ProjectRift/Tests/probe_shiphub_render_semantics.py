from __future__ import annotations

import argparse
import importlib.util
import json
import math
import os
import shutil
import sys
from pathlib import Path

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--blend", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output-root", required=True)
    return parser.parse_args(argv)


def load_renderer(project_root: Path):
    path = (
        project_root
        / "Scripts"
        / "ProjectRift"
        / "ArtPipeline"
        / "shiphub"
        / "render_shiphub_drawings.py"
    )
    spec = importlib.util.spec_from_file_location("projectrift_shiphub_renderer_probe", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load renderer module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def world_bounds(obj) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return (
        Vector(tuple(min(point[axis] for point in points) for axis in range(3))),
        Vector(tuple(max(point[axis] for point in points) for axis in range(3))),
    )


def projected_width_axis(scene, camera, obj) -> Vector:
    x_coordinates = [corner[0] for corner in obj.bound_box]
    center = Vector((0.0, 0.0, 0.0))
    left = center.copy()
    right = center.copy()
    left.x = min(x_coordinates)
    right.x = max(x_coordinates)
    left_view = world_to_camera_view(scene, camera, obj.matrix_world @ left)
    right_view = world_to_camera_view(scene, camera, obj.matrix_world @ right)
    axis = Vector((right_view.x - left_view.x, right_view.y - left_view.y))
    if axis.length_squared == 0.0:
        raise ValueError(f"Door width axis collapses in camera projection: {obj.name}")
    return axis.normalized()


def projected_angle_degrees(first: Vector, second: Vector) -> float:
    dot = max(-1.0, min(1.0, abs(first.dot(second))))
    return math.degrees(math.acos(dot))


def evaluated_mesh_projection(scene, camera, obj) -> dict:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        projected = [
            world_to_camera_view(scene, camera, evaluated.matrix_world @ vertex.co)
            for vertex in mesh.vertices
        ]
    finally:
        evaluated.to_mesh_clear()
    if not projected:
        raise ValueError(f"Cannot project empty evaluated mesh: {obj.name}")
    positive = [point for point in projected if point.z > 0.0]
    if not positive:
        raise ValueError(f"Evaluated mesh is entirely behind the camera: {obj.name}")
    min_x = min(point.x for point in positive)
    max_x = max(point.x for point in positive)
    min_y = min(point.y for point in positive)
    max_y = max(point.y for point in positive)
    return {
        "ndc_bounds": [round(float(value), 6) for value in (min_x, max_x, min_y, max_y)],
        "bbox_coverage": round(float((max_x - min_x) * (max_y - min_y)), 6),
        "positive_depth_fraction": round(float(len(positive) / len(projected)), 6),
        "vertex_count": len(projected),
    }


def projected_context_centers(scene, camera, objects) -> dict:
    result = {}
    for obj in objects:
        minimum, maximum = world_bounds(obj)
        center = (minimum + maximum) * 0.5
        projected = world_to_camera_view(scene, camera, center)
        result[obj.name] = {
            "ndc": [round(float(value), 6) for value in projected],
            "in_frame": bool(
                projected.z > 0.0
                and 0.04 <= projected.x <= 0.96
                and 0.04 <= projected.y <= 0.96
            ),
        }
    return result


def find_temp_object(collection, token: str):
    matches = [obj for obj in collection.objects if token in obj.name]
    if len(matches) != 1:
        raise ValueError(f"Expected one temp object containing {token!r}; got {[obj.name for obj in matches]}")
    return matches[0]


def probe_scene_semantics(renderer, blend_path: Path, manifest: dict, output_root: Path) -> dict:
    result = bpy.ops.wm.open_mainfile(filepath=str(blend_path))
    if "FINISHED" not in result:
        raise RuntimeError(f"Could not open authoritative BLEND: {sorted(result)}")
    scene = bpy.context.scene
    renderer._configure_render(scene)
    expected_names = tuple(manifest["exports"]["expected_object_names"])
    sources = tuple(bpy.data.objects[name] for name in expected_names)
    visible_names = set(expected_names)
    snapshot = renderer._snapshot_sources(sources)
    original_world = scene.world
    original_camera = scene.camera
    collection = renderer._ensure_temp_collection(scene)
    temp_world = renderer._create_world(scene)
    original_render = renderer._render
    original_prepare = renderer._prepare_view
    semantics: dict = {}
    try:
        renderer._render = lambda _scene, _path: None
        renderer._render_exploded(scene, collection, sources, output_root)
        exploded = {}
        for source_name in (
            "SM_ShipHub_CeilingPressureShell",
            "SM_ShipHub_CeilingServiceRing",
            "SM_ShipHub_NavTable_Display",
            "SM_ShipHub_Cryopod_01",
            "SM_ShipHub_Airlock_DoorPanel",
            "SM_ShipHub_West_PreparationBay",
            "SM_ShipHub_East_MedicalBay",
            "SM_ShipHub_ConstructDock_01",
        ):
            matches = [
                obj
                for obj in collection.objects
                if "Exploded" in obj.name and obj.name.endswith(source_name)
            ]
            source = bpy.data.objects[source_name]
            exploded[source_name] = {
                "visible_duplicate": len(matches) == 1,
                "offset_z_m": (
                    round(float(matches[0].matrix_world.translation.z - source.matrix_world.translation.z), 6)
                    if len(matches) == 1
                    else None
                ),
            }
        semantics["a09"] = exploded

        renderer._restore_sources(sources, snapshot)
        renderer._remove_temp_objects(collection)
        perspective_records = []
        pending_view = {}

        def capture_prepare(_scene, _collection, location, target, ortho_scale, lens, studio):
            original_prepare(_scene, _collection, location, target, ortho_scale, lens, studio)
            pending_view.clear()
            pending_view.update(
                {
                    "location": [round(float(value), 6) for value in location],
                    "target": [round(float(value), 6) for value in target],
                    "studio": bool(studio),
                }
            )

        def capture_perspective(_scene, path):
            record = dict(pending_view)
            record["name"] = path.name
            record["hidden"] = sorted(obj.name for obj in sources if obj.hide_render)
            if path.name == "ceiling-low-angle.png":
                bpy.context.view_layer.update()
                record["service_ring_projection"] = evaluated_mesh_projection(
                    scene,
                    scene.camera,
                    bpy.data.objects["SM_ShipHub_CeilingServiceRing"],
                )
                record["wall_context"] = projected_context_centers(
                    scene,
                    scene.camera,
                    tuple(
                        bpy.data.objects[name]
                        for name in (
                            "SM_ShipHub_Wall_04",
                            "SM_ShipHub_Wall_05",
                            "SM_ShipHub_Wall_06",
                        )
                    ),
                )
            perspective_records.append(record)

        renderer._prepare_view = capture_prepare
        renderer._render = capture_perspective
        renderer._render_perspectives(scene, collection, sources, visible_names, output_root)
        semantics["perspectives"] = perspective_records

        renderer._restore_sources(sources, snapshot)
        renderer._remove_temp_objects(collection)
        detail_records = {}

        def capture_detail(_scene, path):
            if path.name == "D03_MainAirlock_Base.png":
                bpy.context.view_layer.update()
                closed = find_temp_object(collection, "AirlockEnvelope_SM_ShipHub_Airlock_DoorPanel")
                swung = find_temp_object(collection, "AirlockDoorSwing_SM_ShipHub_Airlock_DoorPanel")
                closed_axis = projected_width_axis(scene, scene.camera, closed)
                swung_axis = projected_width_axis(scene, scene.camera, swung)
                camera_forward = (scene.camera.matrix_world.to_quaternion() @ Vector((0.0, 0.0, -1.0))).normalized()
                door_height_axis = (closed.matrix_world.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()
                detail_records["d03"] = {
                    "projected_door_axis_angle_degrees": round(
                        projected_angle_degrees(closed_axis, swung_axis), 6
                    ),
                    "door_height_axis_camera_alignment": round(
                        abs(float(door_height_axis.dot(camera_forward))), 6
                    ),
                    "closed_visible": not closed.hide_render,
                    "swung_visible": not swung.hide_render,
                }
            elif path.name == "D05_WallBayInterface_Base.png":
                front_objects = sorted(
                    (
                        obj
                        for obj in collection.objects
                        if "WallBay" in obj.name and "Front" in obj.name
                    ),
                    key=lambda obj: obj.name,
                )
                side_objects = [
                    obj
                    for obj in collection.objects
                    if "WallBay" in obj.name and "Side" in obj.name
                ]
                front_widths = []
                for obj in front_objects:
                    minimum, maximum = world_bounds(obj)
                    front_widths.append(round(float(maximum.x - minimum.x), 6))
                detail_records["d05"] = {
                    "front_widths_m": sorted(front_widths),
                    "front_count": len(front_objects),
                    "side_count": len(side_objects),
                    "all_linked": all(
                        any(obj.data is source.data for source in sources)
                        for obj in front_objects + side_objects
                    ),
                }

        renderer._prepare_view = original_prepare
        renderer._render = capture_detail
        renderer._render_details(scene, collection, sources, output_root)
        semantics["details"] = detail_records
        return semantics
    finally:
        renderer._render = original_render
        renderer._prepare_view = original_prepare
        renderer._restore_sources(sources, snapshot)
        renderer._remove_temp_objects(collection)
        if collection.name in {child.name for child in scene.collection.children}:
            scene.collection.children.unlink(collection)
        bpy.data.collections.remove(collection)
        scene.camera = original_camera
        scene.world = original_world
        if temp_world.users == 0:
            bpy.data.worlds.remove(temp_world)


def probe_rollback_preservation(renderer, project_root: Path) -> dict:
    test_root = (
        project_root
        / "Saved"
        / "Automation"
        / "ProjectRiftShipHubDesign"
        / f"transaction-fault-{os.getpid()}"
    )
    output_root = test_root / "output"
    staging_root = test_root / "staging"
    relative_paths = (Path("A.png"), Path("B.png"))
    output_root.mkdir(parents=True, exist_ok=False)
    staging_root.mkdir(parents=True, exist_ok=False)
    (output_root / "A.png").write_bytes(b"accepted-a")
    (output_root / "B.png").write_bytes(b"accepted-b")
    (staging_root / "A.png").write_bytes(b"new-a")
    (staging_root / "B.png").write_bytes(b"new-b")

    real_replace = renderer.os.replace
    captured_backup_root = None
    install_failed = False
    restore_failed = False

    def faulting_replace(source, destination):
        nonlocal captured_backup_root, install_failed, restore_failed
        source_path = Path(source)
        destination_path = Path(destination)
        backup_parts = [part for part in destination_path.parts if part.startswith(".shiphub-render-backup-")]
        if backup_parts:
            captured_backup_root = next(
                parent
                for parent in destination_path.parents
                if parent.name.startswith(".shiphub-render-backup-")
            )
        if source_path == staging_root / "B.png" and destination_path == output_root / "B.png":
            install_failed = True
            raise OSError("injected install failure")
        if (
            source_path.name == "B.png"
            and any(part.startswith(".shiphub-render-backup-") for part in source_path.parts)
            and destination_path == output_root / "B.png"
        ):
            restore_failed = True
            raise OSError("injected restore failure")
        real_replace(source, destination)

    renderer.os.replace = faulting_replace
    error_text = ""
    try:
        renderer._transactional_replace(staging_root, output_root, relative_paths)
    except Exception as error:
        error_text = str(error)
    finally:
        renderer.os.replace = real_replace

    preserved = bool(captured_backup_root and captured_backup_root.is_dir())
    preserved_files = (
        sorted(str(path.relative_to(captured_backup_root)) for path in captured_backup_root.rglob("*") if path.is_file())
        if preserved
        else []
    )
    result = {
        "install_failed": install_failed,
        "restore_failed": restore_failed,
        "backup_preserved": preserved,
        "backup_path": str(captured_backup_root) if captured_backup_root else None,
        "preserved_files": preserved_files,
        "error": error_text,
        "error_reports_backup_path": bool(
            captured_backup_root and str(captured_backup_root) in error_text
        ),
    }
    shutil.rmtree(test_root, ignore_errors=True)
    return result


def validate_semantics(payload: dict) -> list[str]:
    issues = []
    a09 = payload["scene"]["a09"]
    if a09["SM_ShipHub_CeilingPressureShell"]["visible_duplicate"]:
        issues.append("A09 pressure shell must be hidden so it cannot occlude exploded layers.")
    expected_offsets = {
        "SM_ShipHub_CeilingServiceRing": 10.5,
        "SM_ShipHub_NavTable_Display": 1.5,
        "SM_ShipHub_Cryopod_01": 3.0,
        "SM_ShipHub_Airlock_DoorPanel": 4.5,
        "SM_ShipHub_West_PreparationBay": 6.0,
        "SM_ShipHub_East_MedicalBay": 7.5,
        "SM_ShipHub_ConstructDock_01": 9.0,
    }
    for name, expected in expected_offsets.items():
        actual = a09[name]["offset_z_m"]
        if actual != expected:
            issues.append(f"A09 {name} offset must be {expected}; got {actual}.")

    perspectives = {record["name"]: record for record in payload["scene"]["perspectives"]}
    low = perspectives["ceiling-low-angle.png"]
    if not (
        abs(low["location"][0]) < 14.0
        and abs(low["location"][1]) < 12.0
        and 0.0 < low["location"][2] < 8.0
        and low["target"][2] >= 6.2
    ):
        issues.append(f"Ceiling low-angle camera must be inside and target the ceiling; got {low}.")
    ring_projection = low["service_ring_projection"]
    min_x, max_x, min_y, max_y = ring_projection["ndc_bounds"]
    if ring_projection["positive_depth_fraction"] != 1.0:
        issues.append(f"The complete service ring must remain in front of the low camera; got {ring_projection}.")
    if not (min_x >= 0.04 and max_x <= 0.96 and min_y >= 0.04 and max_y <= 0.96):
        issues.append(f"The complete service ring must fit inside a 4% NDC safety margin; got {ring_projection}.")
    if not (0.08 <= ring_projection["bbox_coverage"] <= 0.65):
        issues.append(f"Service-ring low-view bbox coverage must be readable without dominating the frame; got {ring_projection}.")
    context_count = sum(item["in_frame"] for item in low["wall_context"].values())
    if context_count < 1:
        issues.append(f"The low ceiling view must retain structural wall context; got {low['wall_context']}.")
    for name in (
        "west-oblique.png",
        "east-oblique.png",
        "high-overview.png",
        "ceiling-low-angle.png",
        "A10_PerspectiveSheet_Base.png",
    ):
        if "SM_ShipHub_CeilingPressureShell" not in perspectives[name]["hidden"]:
            issues.append(f"{name} must hide the pressure shell for a complementary interior view.")
    a10 = perspectives["A10_PerspectiveSheet_Base.png"]
    overview = perspectives["high-overview.png"]
    for name in ("A10_PerspectiveSheet_Base.png", "high-overview.png"):
        if "SM_ShipHub_CeilingServiceRing" not in perspectives[name]["hidden"]:
            issues.append(f"{name} must hide the ceiling service ring so the high overview is not roof-dominated.")
    if any(a10[key] != overview[key] for key in ("location", "target", "hidden", "studio")):
        issues.append("A10 must use the exact high-overview camera/world/visibility configuration.")
    source_names = [record["name"] for record in payload["scene"]["perspectives"] if record["name"] != "A10_PerspectiveSheet_Base.png"]
    if len(source_names) != 6 or len({tuple(perspectives[name]["location"]) for name in source_names}) != 6:
        issues.append("The six perspective sources must have six distinct camera locations.")

    details = payload["scene"]["details"]
    d03 = details["d03"]
    if d03["door_height_axis_camera_alignment"] < 0.95:
        issues.append(f"D03 envelope must use a plan projection; got {d03}.")
    if d03["projected_door_axis_angle_degrees"] < 25.0:
        issues.append(f"D03 door swing must be distinguishable in projection; got {d03}.")
    d05 = details["d05"]
    if d05["front_widths_m"] != [1.0, 2.0, 4.0]:
        issues.append(f"D05 temp front widths must be exact 1/2/4m; got {d05['front_widths_m']}.")
    if d05["front_count"] != 3 or d05["side_count"] != 3 or not d05["all_linked"]:
        issues.append(f"D05 must contain three linked front/side pairs; got {d05}.")

    rollback = payload["rollback"]
    if not rollback["install_failed"] or not rollback["restore_failed"]:
        issues.append(f"Transaction fault injection did not reach both failures: {rollback}.")
    if not rollback["backup_preserved"]:
        issues.append("Rollback-incomplete must preserve the backup directory.")
    if not rollback["error_reports_backup_path"]:
        issues.append("Rollback-incomplete error must report the preserved backup path.")
    if "B.png" not in rollback["preserved_files"]:
        issues.append(f"Rollback-incomplete backup must retain accepted B.png; got {rollback['preserved_files']}.")
    return issues


def main(argv: list[str]) -> int:
    arguments = parse_arguments(argv)
    project_root = Path(arguments.project_root).resolve()
    blend_path = Path(arguments.blend).resolve()
    manifest_path = Path(arguments.manifest).resolve()
    output_root = Path(arguments.output_root).resolve()
    renderer = load_renderer(project_root)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    payload = {
        "scene": probe_scene_semantics(renderer, blend_path, manifest, output_root),
        "rollback": probe_rollback_preservation(renderer, project_root),
    }
    print("SHIPHUB_RENDER_SEMANTIC_PROBE=" + json.dumps(payload, sort_keys=True))
    issues = validate_semantics(payload)
    if issues:
        for issue in issues:
            print("SEMANTIC_PROBE_FAILURE=" + issue)
        return 1
    return 0


if __name__ == "__main__":
    separator = sys.argv.index("--")
    raise SystemExit(main(sys.argv[separator + 1 :]))
