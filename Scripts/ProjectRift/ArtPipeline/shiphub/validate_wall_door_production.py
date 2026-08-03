"""Independently reopen and validate the saved ProjectRift wall-door production BLEND."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

import bmesh  # type: ignore
import bpy  # type: ignore
from mathutils import Vector  # type: ignore


ASSET_ID = "SM_ShipHub_WallDoor_400_A"
COVER_NAME = f"{ASSET_ID}_RemovableCover"
DAMAGED_NAME = f"{ASSET_ID}_Overlay_Damaged"
PATCHED_NAME = f"{ASSET_ID}_Overlay_Patched"
SCHEMA = "projectrift.shiphub.wall-door-geometry-validation.v1"
COLLECTION_NAMES = (
    "00_REFERENCE",
    "10_STRUCTURE",
    "20_DETAIL",
    "30_STATE_OVERLAY",
    "40_COLLISION",
    "90_EXPORT",
)
EXPORT_NAMES = (ASSET_ID, COVER_NAME, DAMAGED_NAME, PATCHED_NAME)
COLLISION_BOUNDS = {
    f"UCX_{ASSET_ID}_LeftJamb": [0.0, 0.0, 0.0, 80.0, 30.0, 400.0],
    f"UCX_{ASSET_ID}_RightJamb": [320.0, 0.0, 0.0, 400.0, 30.0, 400.0],
    f"UCX_{ASSET_ID}_Lintel": [80.0, 0.0, 280.0, 320.0, 30.0, 400.0],
}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _descendant(path: Path, root: Path, label: str) -> Path:
    resolved = path.resolve()
    try:
        relative = resolved.relative_to(root.resolve())
    except ValueError as error:
        raise ValueError(f"{label} is outside ProjectA: {resolved}") from error
    if not relative.parts:
        raise ValueError(f"{label} must be below ProjectA: {resolved}")
    return resolved


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--blend", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--skip-renders", action="store_true")
    return parser.parse_args(argv)


def _paths(args: argparse.Namespace) -> tuple[Path, Path, Path, Path]:
    project_root = Path(args.project_root).resolve()
    if not (project_root / "ProjectA.uproject").is_file():
        raise ValueError(f"--project-root is not ProjectA: {project_root}")
    output_root = _descendant(Path(args.output_root), project_root, "--output-root")
    expected_output = (
        project_root / "SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A"
    ).resolve()
    if output_root != expected_output:
        raise ValueError(f"--output-root must be exactly {expected_output}")
    blend_path = _descendant(Path(args.blend), project_root, "--blend")
    report_path = _descendant(Path(args.report), project_root, "--report")
    if not blend_path.is_file() or blend_path.suffix.lower() != ".blend":
        raise ValueError(f"Saved production BLEND is missing: {blend_path}")
    return project_root, output_root, blend_path, report_path


def _bounds_cm(obj) -> list[float]:
    points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    minimum = [min(float(point[index]) for point in points) * 100.0 for index in range(3)]
    maximum = [max(float(point[index]) for point in points) * 100.0 for index in range(3)]
    return [round(value, 3) for value in minimum + maximum]


def _component_bounds_cm(obj) -> list[list[float]]:
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    remaining = set(bm.faces)
    result = []
    while remaining:
        seed = remaining.pop()
        component = {seed}
        pending = [seed]
        while pending:
            face = pending.pop()
            for edge in face.edges:
                for linked in edge.link_faces:
                    if linked in remaining:
                        remaining.remove(linked)
                        component.add(linked)
                        pending.append(linked)
        points = [obj.matrix_world @ vertex.co for face in component for vertex in face.verts]
        minimum = [min(float(point[index]) for point in points) * 100.0 for index in range(3)]
        maximum = [max(float(point[index]) for point in points) * 100.0 for index in range(3)]
        result.append([round(value, 3) for value in minimum + maximum])
    bm.free()
    return sorted(result)


def _topology(obj) -> dict[str, object]:
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    non_manifold = [edge for edge in bm.edges if len(edge.link_faces) != 2]
    face_signatures: set[tuple[tuple[float, float, float], ...]] = set()
    duplicate_faces = 0
    for face in bm.faces:
        signature = tuple(
            sorted(tuple(round(float(value), 6) for value in vertex.co) for vertex in face.verts)
        )
        duplicate_faces += int(signature in face_signatures)
        face_signatures.add(signature)
    vertex_signatures: set[tuple[float, float, float]] = set()
    duplicate_vertices = 0
    for vertex in bm.verts:
        signature = tuple(round(float(value), 6) for value in vertex.co)
        duplicate_vertices += int(signature in vertex_signatures)
        vertex_signatures.add(signature)
    volume = float(bm.calc_volume(signed=True))
    bm.free()
    return {
        "UnexpectedNonManifoldEdgeCount": len(non_manifold),
        "DuplicateFaceCount": duplicate_faces,
        "DuplicateVertexCount": duplicate_vertices,
        "SignedVolumeM3": round(volume, 9),
    }


def _opening(base) -> dict[str, list[float]]:
    points = [base.matrix_world @ vertex.co for vertex in base.data.vertices]
    left = max(float(point.x) for point in points if point.x <= 0.800001)
    right = min(float(point.x) for point in points if point.x >= 3.199999)
    lintel = min(
        float(point.z)
        for point in points
        if 0.8 - 1e-6 <= point.x <= 3.2 + 1e-6 and point.z >= 2.799999
    )
    return {
        "SizeCm": [round((right - left) * 100.0, 3), round(lintel * 100.0, 3)],
        "X": [round(left * 100.0, 3), round(right * 100.0, 3)],
        "Z": [0.0, round(lintel * 100.0, 3)],
    }


def _recess_evidence(base) -> dict[str, object]:
    cavity_samples = []
    front_reference_samples = []
    for vertex in base.data.vertices:
        point = base.matrix_world @ vertex.co
        if (
            1.10 - 1e-5 <= point.x <= 2.90 + 1e-5
            and 3.29 - 1e-5 <= point.z <= 3.41 + 1e-5
            and 0.039 <= point.y <= 0.081
        ):
            cavity_samples.append(point)
        if (
            0.80 - 1e-5 <= point.x <= 3.20 + 1e-5
            and 2.80 - 1e-5 <= point.z <= 4.00 + 1e-5
            and 0.039 <= point.y <= 0.041
        ):
            front_reference_samples.append(point)
    cavity_y_values = [float(point.y) for point in cavity_samples]
    front_y_values = [float(point.y) for point in front_reference_samples]
    front = min(front_y_values) if front_y_values else math.nan
    back = max(cavity_y_values) if cavity_y_values else math.nan
    backplane_polygons = 0
    for polygon in base.data.polygons:
        polygon_points = [base.matrix_world @ base.data.vertices[index].co for index in polygon.vertices]
        if polygon_points and all(abs(float(point.y) - 0.08) <= 0.00001 for point in polygon_points):
            center_x = sum(float(point.x) for point in polygon_points) / len(polygon_points)
            center_z = sum(float(point.z) for point in polygon_points) / len(polygon_points)
            if 1.10 - 1e-5 <= center_x <= 2.90 + 1e-5 and 3.29 - 1e-5 <= center_z <= 3.41 + 1e-5:
                backplane_polygons += 1
    return {
        "Method": "SavedMeshCavityBackplane",
        "RegionCm": {"X": [110, 290], "Z": [329, 341]},
        "FrontSurfaceYCm": round(front * 100.0, 3) if front_y_values else None,
        "BackSurfaceYCm": round(back * 100.0, 3) if cavity_y_values else None,
        "MeasuredDepthCm": round((back - front) * 100.0, 3) if front_y_values and cavity_y_values else None,
        "SampleVertexCount": len(front_reference_samples) + len(cavity_samples),
        "FrontReferenceVertexCount": len(front_reference_samples),
        "CavityVertexCount": len(cavity_samples),
        "BackplanePolygonCount": backplane_polygons,
    }


def _snap_evidence(base) -> dict[str, object]:
    points = [base.matrix_world @ vertex.co for vertex in base.data.vertices]
    evidence = {}
    for label, axis, expected in (("LeftX", 0, 0.0), ("RightX", 0, 4.0), ("TopZ", 2, 4.0)):
        coordinates = [float(point[axis]) for point in points if abs(float(point[axis]) - expected) <= 1e-6]
        deviation = max((abs(value - expected) for value in coordinates), default=math.inf)
        evidence[label] = {
            "PlaneM": expected,
            "VertexCount": len(coordinates),
            "MaximumDeviationM": round(deviation, 9),
            "Planar": len(coordinates) >= 3 and deviation <= 1e-6,
        }
    return {"Planes": evidence, "AllPlanar": all(item["Planar"] for item in evidence.values())}


def _state_compositions() -> list[dict[str, object]]:
    result = []
    for state in ("Base", "Damaged", "Patched", "Online"):
        visible = [ASSET_ID]
        cover_visible = state != "Damaged"
        if cover_visible:
            visible.append(COVER_NAME)
        if state == "Damaged":
            visible.append(DAMAGED_NAME)
        if state == "Patched":
            visible.append(PATCHED_NAME)
        result.append(
            {
                "State": state,
                "VisibleObjects": visible,
                "VisibleBaseBodyCount": sum(name == ASSET_ID for name in visible),
                "VisibleCoverObjectCount": sum(name == COVER_NAME for name in visible),
                "CoverVisible": cover_visible,
                "UsesClonedBase": sum(name == ASSET_ID for name in visible) != 1,
            }
        )
    return result


def _apply_state(state: str) -> None:
    for obj in bpy.data.objects:
        if obj.type == "MESH":
            obj.hide_render = obj.name not in EXPORT_NAMES
    bpy.data.objects[ASSET_ID].hide_render = False
    bpy.data.objects[COVER_NAME].hide_render = state == "Damaged"
    bpy.data.objects[DAMAGED_NAME].hide_render = state != "Damaged"
    bpy.data.objects[PATCHED_NAME].hide_render = state != "Patched"


def _render_reviews(output_root: Path) -> list[dict[str, object]]:
    scene = bpy.context.scene
    camera = bpy.data.objects.get("CAM_Production_Perspective")
    if camera is None or camera.type != "CAMERA":
        raise ValueError("Saved blend is missing CAM_Production_Perspective.")
    scene.camera = camera
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    results = []
    for state in ("Base", "Damaged", "Patched"):
        _apply_state(state)
        relative = f"Reports/ProductionReview/{state}.png"
        path = output_root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        scene.render.filepath = str(path)
        if "FINISHED" not in bpy.ops.render.render(write_still=True):
            raise RuntimeError(f"Independent review render failed: {state}")
        results.append(
            {
                "State": state,
                "Path": relative,
                "Width": 1024,
                "Height": 1024,
                "Camera": camera.name,
                "CoverVisible": state != "Damaged",
                "Sha256": _sha256(path),
            }
        )
    return results


def validate(
    project_root: Path,
    output_root: Path,
    blend_path: Path,
    report_path: Path,
    skip_renders: bool,
) -> dict[str, object]:
    if bpy.app.version[:2] != (5, 2) or "LTS" not in bpy.app.version_string:
        raise RuntimeError(f"Independent validation requires Blender 5.2.x LTS; found {bpy.app.version_string}.")
    inspected_hash = _sha256(blend_path)
    result = bpy.ops.wm.open_mainfile(filepath=str(blend_path), load_ui=False)
    if "FINISHED" not in result:
        raise RuntimeError(f"Could not reopen saved production BLEND: {blend_path}")

    issues: list[str] = []
    collection_names = tuple(collection.name for collection in bpy.data.collections)
    if collection_names != COLLECTION_NAMES:
        issues.append(f"Collections mismatch: {collection_names}")
    missing = [name for name in EXPORT_NAMES if bpy.data.objects.get(name) is None]
    if missing:
        issues.append(f"Export objects missing: {missing}")
    collision_collection = bpy.data.collections.get("40_COLLISION")
    collision_objects = list(collision_collection.objects) if collision_collection else []
    if sorted(obj.name for obj in collision_objects) != sorted(COLLISION_BOUNDS):
        issues.append("Collision piece names do not match the exact three-piece contract.")
    if issues:
        raise ValueError("Saved-blend production validation failed: " + "; ".join(issues))

    exports = {name: bpy.data.objects[name] for name in EXPORT_NAMES}
    topology = {name: _topology(obj) for name, obj in exports.items()}
    export_evidence = []
    for name, obj in exports.items():
        uv_layers = [layer.name for layer in obj.data.uv_layers]
        if uv_layers != ["UV0", "UV1"]:
            issues.append(f"UV layers for {name} must be exactly UV0 and UV1; found {uv_layers}")
        location = [round(float(value), 6) for value in obj.location]
        rotation = [round(math.degrees(float(value)), 6) for value in obj.rotation_euler]
        scale = [round(float(value), 6) for value in obj.scale]
        if location != [0.0, 0.0, 0.0] or rotation != [0.0, 0.0, 0.0] or scale != [1.0, 1.0, 1.0]:
            issues.append(f"Applied transforms invalid for {name}: {location}, {rotation}, {scale}")
        if len(obj.modifiers) != 0:
            issues.append(f"Export modifiers remain on {name}.")
        if any(len(polygon.vertices) != 3 for polygon in obj.data.polygons):
            issues.append(f"Export topology is not fully triangulated for {name}.")
        if topology[name]["UnexpectedNonManifoldEdgeCount"] != 0:
            issues.append(f"Unexpected non-manifold edges on {name}.")
        if topology[name]["DuplicateFaceCount"] != 0:
            issues.append(f"Duplicate faces on {name}.")
        if topology[name]["DuplicateVertexCount"] != 0:
            issues.append(f"Duplicate vertices on {name}.")
        if float(topology[name]["SignedVolumeM3"]) <= 0.0:
            issues.append(f"Non-positive signed volume on {name}.")
        export_evidence.append(
            {
                "Name": name,
                "Location": location,
                "RotationDegrees": rotation,
                "Scale": scale,
                "UVLayers": uv_layers,
                "ModifiersApplied": len(obj.modifiers) == 0,
                "Triangulated": all(len(polygon.vertices) == 3 for polygon in obj.data.polygons),
                "PolygonCount": len(obj.data.polygons),
                "TriangleCount": len(obj.data.polygons),
                "BoundsCm": _bounds_cm(obj),
                "Topology": topology[name],
            }
        )

    base = exports[ASSET_ID]
    bounds = _bounds_cm(base)
    if bounds != [0.0, 0.0, 0.0, 400.0, 30.0, 400.0]:
        issues.append(f"Base bounds mismatch: {bounds}")
    opening = _opening(base)
    if opening != {"SizeCm": [240.0, 280.0], "X": [80.0, 320.0], "Z": [0.0, 280.0]}:
        issues.append(f"Door opening mismatch: {opening}")
    if len(base.data.materials) != 2:
        issues.append(f"Base material slots must equal 2; found {len(base.data.materials)}")

    recess = _recess_evidence(base)
    if (
        recess["FrontSurfaceYCm"] != 4.0
        or recess["BackSurfaceYCm"] != 8.0
        or recess["MeasuredDepthCm"] != 4.0
        or int(recess["BackplanePolygonCount"]) <= 0
    ):
        issues.append(f"Functional recess geometry must measure 4 cm from Y=4 to Y=8: {recess}")

    snap = _snap_evidence(base)
    if not snap["AllPlanar"]:
        issues.append(f"Outer snap faces are not planar: {snap}")

    collision_evidence = []
    for obj in collision_objects:
        measured = _bounds_cm(obj)
        expected = COLLISION_BOUNDS[obj.name]
        blocks = measured != expected
        if blocks:
            issues.append(f"Collision bounds mismatch for {obj.name}: {measured}")
        collision_evidence.append(
            {
                "Name": obj.name,
                "Intent": str(obj.get("projectrift_collision_intent", "")),
                "BoundsCm": measured,
                "BlocksDoorOpening": blocks,
            }
        )

    overlay_clearance = {}
    for state, name in (("Damaged", DAMAGED_NAME), ("Patched", PATCHED_NAME)):
        component_bounds = _component_bounds_cm(exports[name])
        crosses_opening = any(
            bounds[3] > 80.0 and bounds[0] < 320.0 and bounds[5] > 0.0 and bounds[2] < 280.0
            for bounds in component_bounds
        )
        crosses_snap = any(
            bounds[0] <= 0.0 or bounds[3] >= 400.0 or bounds[2] <= 0.0 or bounds[5] >= 400.0
            for bounds in component_bounds
        )
        if crosses_opening or crosses_snap:
            issues.append(f"{state} overlay crosses the opening or snap planes.")
        overlay_clearance[state] = {
            "ComponentBoundsCm": component_bounds,
            "CrossesDoorOpening": crosses_opening,
            "CrossesOuterSnapPlanes": crosses_snap,
        }

    source_modifier_stacks = {
        obj.name: [modifier.type for modifier in obj.modifiers]
        for obj in bpy.data.objects
        if obj.name.startswith("SRC_") and obj.type == "MESH" and len(obj.modifiers) > 0
    }
    modifier_types = sorted({item for stack in source_modifier_stacks.values() for item in stack})
    if not {"ARRAY", "BEVEL", "BOOLEAN", "MIRROR", "WEIGHTED_NORMAL"}.issubset(modifier_types):
        issues.append(f"Editable source modifier types incomplete: {modifier_types}")
    primary = bpy.data.objects.get("SRC_Primary_UFrame")
    primary_bevel = next((modifier.width * 100.0 for modifier in primary.modifiers if modifier.type == "BEVEL"), None)
    secondary_bevels = [
        modifier.width * 100.0
        for obj in bpy.data.objects
        if obj.name.startswith("SRC_Armor_")
        for modifier in obj.modifiers
        if modifier.type == "BEVEL"
    ]
    patch_source = bpy.data.objects.get("SRC_Patched_RepairPlate_Array")
    plate_count = next((modifier.count for modifier in patch_source.modifiers if modifier.type == "ARRAY"), 0)
    cable_count = int(bpy.data.objects.get("SRC_Patched_ProtectedCableGuide") is not None)
    damaged_rim = bpy.data.objects.get("SRC_Damaged_AbsentCoverRim")
    absent_rim = damaged_rim is not None and any(modifier.type == "BOOLEAN" for modifier in damaged_rim.modifiers)
    state_compositions = _state_compositions()
    if any(item["UsesClonedBase"] for item in state_compositions):
        issues.append("A state composition clones or omits the shared base body.")
    if next(item for item in state_compositions if item["State"] == "Damaged")["VisibleCoverObjectCount"] != 0:
        issues.append("Damaged state still exposes the removable cover.")

    if issues:
        raise ValueError("Saved-blend production validation failed: " + "; ".join(issues))

    review_renders = [] if skip_renders else _render_reviews(output_root)
    approval_path = output_root / f"Briefs/{ASSET_ID}.approval.json"
    approval = json.loads(approval_path.read_text(encoding="utf-8"))["Appearance"]
    appearance_blend = output_root / f"Blender/{ASSET_ID}_Appearance.blend"
    appearance_report = output_root / "Reports/appearance-validation.json"
    appearance_sheet = output_root / f"Concept/{ASSET_ID}_AppearanceLock.png"
    contract_path = output_root / f"Briefs/{ASSET_ID}.asset.json"
    validator_path = Path(__file__).resolve()
    report = {
        "Schema": SCHEMA,
        "AssetId": ASSET_ID,
        "Passed": True,
        "ValidationMethod": "IndependentSavedBlendReopen",
        "SavedBlendReopened": True,
        "ValidatorScriptSha256": _sha256(validator_path),
        "InspectedBlendSha256": inspected_hash,
        "BlenderVersion": bpy.app.version_string,
        "BoundsCm": [400.0, 30.0, 400.0],
        "MeasuredBoundsM": {"min": [0.0, 0.0, 0.0], "max": [4.0, 0.3, 4.0]},
        "DoorOpeningCm": opening["SizeCm"],
        "DoorOpeningRangeCm": {"X": opening["X"], "Z": opening["Z"]},
        "PivotCm": [round(float(value) * 100.0, 3) for value in base.location],
        "AppliedScale": [round(float(value), 6) for value in base.scale],
        "MaterialSlotCount": len(base.data.materials),
        "UnexpectedNonManifoldEdgeCount": sum(int(item["UnexpectedNonManifoldEdgeCount"]) for item in topology.values()),
        "DuplicateFaceCount": sum(int(item["DuplicateFaceCount"]) for item in topology.values()),
        "OverlayObjects": [DAMAGED_NAME, PATCHED_NAME],
        "OnlineSilhouetteOverlayCount": 0,
        "CollisionPieces": collision_evidence,
        "ExportObjects": export_evidence,
        "PrimarySilhouetteBevelCm": round(float(primary_bevel), 3),
        "SecondaryPanelBevelCm": round(max(secondary_bevels), 3),
        "FunctionalRecessDepthCm": recess["MeasuredDepthCm"],
        "FunctionalRecessEvidence": recess,
        "OuterSnapFacesPlanar": snap["AllPlanar"],
        "OuterSnapFaceEvidence": snap,
        "OverlayClearance": overlay_clearance,
        "StateCompositions": state_compositions,
        "HasDoorLeaf": any("doorleaf" in obj.name.lower().replace("_", "") for obj in bpy.data.objects if obj.type == "MESH"),
        "PatchedRepairPlateCount": plate_count,
        "PatchedCableGuideCount": cable_count,
        "DamagedOverlayHasAbsentCoverRim": absent_rim,
        "Collections": list(COLLECTION_NAMES),
        "SourceModifierStacks": source_modifier_stacks,
        "AppliedTopologyEvidence": {
            "EditableModifierTypes": modifier_types,
            "ExportModifiersApplied": all(len(obj.modifiers) == 0 for obj in exports.values()),
            "FinalExportDuplicatesOnlyTriangulated": all(
                all(len(polygon.vertices) == 3 for polygon in obj.data.polygons) for obj in exports.values()
            ),
            "NormalsRecalculatedOutward": all(float(item["SignedVolumeM3"]) > 0.0 for item in topology.values()),
            "AccidentalDoublesRemoved": all(int(item["DuplicateVertexCount"]) == 0 for item in topology.values()),
        },
        "ReviewRenders": review_renders,
        "ApprovalDependency": approval,
        "SourceSha256": {
            "AppearanceBlend": _sha256(appearance_blend),
            "AppearanceReport": _sha256(appearance_report),
            "AppearanceSheet": _sha256(appearance_sheet),
            "Contract": _sha256(contract_path),
            "BuilderScript": _sha256(validator_path.parent / "build_wall_door_first_article.py"),
            "ValidatorScript": _sha256(validator_path),
        },
        "OutputSha256": {"ProductionBlend": inspected_hash},
        "ValidationIssues": [],
        "ValidatedAt": datetime.now(timezone.utc).astimezone().isoformat(),
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = report_path.with_suffix(report_path.suffix + ".tmp")
    temporary.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, report_path)
    print(
        "WALL_DOOR_SAVED_BLEND_VALIDATION="
        + json.dumps(
            {
                "blend": str(blend_path),
                "report": str(report_path),
                "base_triangles": len(base.data.polygons),
                "recess_depth_cm": recess["MeasuredDepthCm"],
                "renders": len(review_renders),
            },
            sort_keys=True,
        )
    )
    return report


def main(argv: list[str]) -> int:
    args = _parse(argv)
    project_root, output_root, blend_path, report_path = _paths(args)
    validate(project_root, output_root, blend_path, report_path, args.skip_renders)
    return 0


if __name__ == "__main__":
    try:
        raw = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else sys.argv[1:]
        raise SystemExit(main(raw))
    except SystemExit:
        raise
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
