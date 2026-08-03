"""Build and finalize the ProjectRift G3 wall-door appearance authority."""

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

try:
    import bpy  # type: ignore
    from mathutils import Vector  # type: ignore
except ImportError:
    bpy = None
    Vector = None


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

from wall_door_contract import load_contract, validate_contract


ASSET_ID = "SM_ShipHub_WallDoor_400_A"
BLEND_FILENAME = f"{ASSET_ID}_Appearance.blend"
REPORT_SCHEMA = "projectrift.shiphub.wall-door-appearance-validation.v1"
PRODUCTION_BLEND_FILENAME = f"{ASSET_ID}.blend"
PRODUCTION_REPORT_SCHEMA = "projectrift.shiphub.wall-door-geometry-validation.v1"
PRODUCTION_OVERLAY_NAMES = (
    f"{ASSET_ID}_Overlay_Damaged",
    f"{ASSET_ID}_Overlay_Patched",
)
PRODUCTION_COVER_NAME = f"{ASSET_ID}_RemovableCover"
COLLECTION_NAMES = (
    "00_REFERENCE",
    "10_STRUCTURE",
    "20_DETAIL",
    "30_STATE_OVERLAY",
    "40_COLLISION",
    "90_EXPORT",
)
AUTHORITY_VIEWS = (
    "Concept/Orthographic/Front.png",
    "Concept/Orthographic/Back.png",
    "Concept/Orthographic/Left.png",
    "Concept/Orthographic/Right.png",
    "Concept/Orthographic/Perspective.png",
)
CANDIDATE_PATHS = (
    "Concept/Candidates/Base.png",
    "Concept/Candidates/Damaged.png",
    "Concept/Candidates/Patched.png",
    "Concept/Candidates/Online.png",
)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _require_descendant(candidate: Path, root: Path, label: str) -> Path:
    resolved_candidate = candidate.resolve()
    resolved_root = root.resolve()
    try:
        relative = resolved_candidate.relative_to(resolved_root)
    except ValueError as error:
        raise ValueError(f"{label} is outside {resolved_root}: {resolved_candidate}") from error
    if not relative.parts:
        raise ValueError(f"{label} must be below, not equal to, {resolved_root}.")
    return resolved_candidate


def _parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", required=True)
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--finalize", action="store_true")
    parser.add_argument("--production", action="store_true")
    return parser.parse_args(argv)


def _validated_paths(arguments: argparse.Namespace) -> tuple[Path, Path, Path]:
    project_root = Path(arguments.project_root).resolve()
    if not (project_root / "ProjectA.uproject").is_file():
        raise ValueError(f"--project-root is not ProjectA: {project_root}")
    contract_path = _require_descendant(Path(arguments.contract), project_root, "--contract")
    output_root = _require_descendant(Path(arguments.output_root), project_root, "--output-root")
    expected_output_root = (
        project_root
        / "SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A"
    ).resolve()
    expected_contract = (
        expected_output_root
        / "Briefs/SM_ShipHub_WallDoor_400_A.asset.json"
    ).resolve()
    if output_root != expected_output_root:
        raise ValueError(f"--output-root must be exactly {expected_output_root}")
    if contract_path != expected_contract or not contract_path.is_file():
        raise ValueError(f"--contract must be exactly {expected_contract}")
    return project_root, contract_path, output_root


def _require_contract(contract_path: Path, project_root: Path):
    contract = load_contract(contract_path)
    issues = validate_contract(contract, project_root)
    if issues:
        raise ValueError("Wall-door contract validation failed: " + "; ".join(issues))
    return contract


def _validated_appearance_approval(output_root: Path) -> dict[str, object]:
    approval_path = output_root / "Briefs/SM_ShipHub_WallDoor_400_A.approval.json"
    try:
        approval = json.loads(approval_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"Appearance approval ledger cannot be read: {error}") from None
    if not isinstance(approval, dict):
        raise ValueError("Appearance approval ledger root must be an object.")
    if approval.get("Schema") != "projectrift.shiphub.wall-door-approval.v1":
        raise ValueError("Appearance approval ledger schema is invalid.")
    if approval.get("AssetId") != ASSET_ID:
        raise ValueError("Appearance approval ledger asset ID is invalid.")
    appearance = approval.get("Appearance")
    if not isinstance(appearance, dict):
        raise ValueError("Appearance approval entry must be an object.")
    status = appearance.get("Status")
    if status not in {"Pending", "Approved"}:
        raise ValueError(f"Unsupported Appearance.Status: {status!r}")
    evidence = appearance.get("Evidence")
    if not isinstance(evidence, list) or any(not isinstance(item, str) for item in evidence):
        raise ValueError("Appearance.Evidence must be a list of strings.")
    date = appearance.get("Date")
    if status == "Pending":
        if evidence:
            raise ValueError("Pending appearance approval must not contain evidence.")
        if date is not None:
            raise ValueError("Pending appearance approval must not contain a date.")
    else:
        if not isinstance(date, str):
            raise ValueError("Approved appearance requires an ISO-8601 calendar date.")
        try:
            parsed_date = datetime.strptime(date, "%Y-%m-%d")
        except ValueError:
            raise ValueError("Approved appearance date must use YYYY-MM-DD.") from None
        if parsed_date.strftime("%Y-%m-%d") != date:
            raise ValueError("Approved appearance date must use YYYY-MM-DD.")
        expected_evidence = [
            "Concept/SM_ShipHub_WallDoor_400_A_AppearanceLock.png",
            "Reports/appearance-validation.json",
        ]
        if evidence != expected_evidence:
            raise ValueError(
                "Approved appearance evidence must list the final sheet and validation report."
            )
    return {"Status": status, "Date": date, "Evidence": list(evidence)}


def _create_material(
    name: str,
    color: tuple[float, float, float, float],
    roughness: float,
    metallic: float = 0.72,
    emission: tuple[float, float, float, float] | None = None,
):
    material = bpy.data.materials.new(name=name)
    material.diffuse_color = color
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission is not None:
        emission_input = shader.inputs.get("Emission Color") or shader.inputs.get("Emission")
        if emission_input is not None:
            emission_input.default_value = emission
        strength_input = shader.inputs.get("Emission Strength")
        if strength_input is not None:
            strength_input.default_value = 4.0
    else:
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        noise = nodes.new("ShaderNodeTexNoise")
        noise.name = f"{name}_RestrainedRoughness"
        noise.inputs["Scale"].default_value = 7.0
        noise.inputs["Detail"].default_value = 2.0
        noise.inputs["Roughness"].default_value = 0.62
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.name = f"{name}_RoughnessRange"
        ramp.color_ramp.elements[0].color = (max(0.0, roughness - 0.08),) * 3 + (1.0,)
        ramp.color_ramp.elements[1].color = (min(1.0, roughness + 0.08),) * 3 + (1.0,)
        links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], shader.inputs["Roughness"])
    return material


def _link_object(obj, collection) -> None:
    for existing in tuple(obj.users_collection):
        existing.objects.unlink(obj)
    collection.objects.link(obj)


def _create_box(name: str, bounds: tuple[float, float, float, float, float, float], collection, material, bevel=0.0):
    x0, x1, y0, y1, z0, z1 = bounds
    bpy.ops.mesh.primitive_cube_add(location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (x1 - x0, y1 - y0, z1 - z0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    _link_object(obj, collection)
    obj.data.materials.append(material)
    if bevel > 0.0:
        modifier = obj.modifiers.new(name="ControlledChamfer", type="BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        modifier.limit_method = "ANGLE"
    return obj


def _create_root_frame(collection, material):
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int, int]] = []

    def add_box(x0, x1, y0, y1, z0, z1):
        start = len(vertices)
        vertices.extend(
            ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))
        )
        faces.extend(
            tuple(start + index for index in face)
            for face in ((3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4),
                         (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))
        )

    add_box(0.0, 0.8, 0.04, 0.3, 0.0, 4.0)
    add_box(3.2, 4.0, 0.04, 0.3, 0.0, 4.0)
    add_box(0.8, 3.2, 0.04, 0.3, 2.8, 4.0)
    mesh = bpy.data.meshes.new(f"{ASSET_ID}_Mesh")
    mesh.from_pydata(vertices, (), faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(ASSET_ID, mesh)
    collection.objects.link(obj)
    mesh.materials.append(material)
    obj.location = (0.0, 0.0, 0.0)
    obj["projectrift_pivot"] = "BottomLeftBack"
    obj["projectrift_geometry_authority"] = True
    modifier = obj.modifiers.new(name="OuterChamfer_5cm", type="BEVEL")
    modifier.width = 0.05
    modifier.segments = 3
    modifier.limit_method = "ANGLE"
    return obj


def _look_at(obj, target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def _create_camera(name: str, location, target, reference_collection, orthographic=False):
    camera_data = bpy.data.cameras.new(name=name)
    camera_data.type = "ORTHO" if orthographic else "PERSP"
    if orthographic:
        camera_data.ortho_scale = 5.2
    else:
        camera_data.lens = 50.0
    camera = bpy.data.objects.new(name, camera_data)
    reference_collection.objects.link(camera)
    camera.location = location
    _look_at(camera, target)
    return camera


def _create_area_light(name: str, location, energy: float, size: float, reference_collection, target=(2.0, 0.15, 2.0)):
    data = bpy.data.lights.new(name=name, type="AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    reference_collection.objects.link(obj)
    obj.location = location
    _look_at(obj, target)
    return obj


def _build_scene() -> dict[str, object]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.unit_settings.length_unit = "METERS"
    # Blender 5.2 exposes Eevee Next through the BLENDER_EEVEE RNA identifier.
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 2048
    scene.render.resolution_y = 2048
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world = bpy.data.worlds.new("ProjectRift_NeutralGrayWorld")
    scene.world.color = (0.045, 0.05, 0.06)
    scene.render.image_settings.compression = 15

    collections = {}
    for name in COLLECTION_NAMES:
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        collections[name] = collection

    dark_steel = _create_material("MAT_Appearance_DarkSteel", (0.055, 0.075, 0.095, 1.0), 0.38)
    armor = _create_material("MAT_Appearance_CoolGrayArmor", (0.29, 0.34, 0.39, 1.0), 0.5)
    seal = _create_material("MAT_Appearance_BlackSeal", (0.012, 0.016, 0.02, 1.0), 0.72)
    soot = _create_material("MAT_State_DamagedSoot", (0.018, 0.019, 0.02, 1.0), 0.9, metallic=0.05)
    abrasion = _create_material("MAT_State_DamagedAbrasion", (0.18, 0.19, 0.20, 1.0), 0.64, metallic=0.42)
    patch = _create_material("MAT_State_PatchPlate", (0.16, 0.18, 0.20, 1.0), 0.48)
    amber = _create_material("MAT_State_Amber", (0.85, 0.27, 0.025, 1.0), 0.32, metallic=0.1, emission=(1.0, 0.18, 0.01, 1.0))
    cyan = _create_material("MAT_State_Cyan", (0.01, 0.52, 0.72, 1.0), 0.3, metallic=0.1, emission=(0.01, 0.65, 1.0, 1.0))
    cold_white = _create_material("MAT_State_ColdWhite", (0.72, 0.86, 1.0, 1.0), 0.25, metallic=0.05, emission=(0.72, 0.86, 1.0, 1.0))

    structure = collections["10_STRUCTURE"]
    detail = collections["20_DETAIL"]
    root = _create_root_frame(structure, dark_steel)

    # Calm broad armor faces. Gaps expose the structural frame and define service access.
    panels = (
        ("Armor_LeftLower", (0.06, 0.68, 0.008, 0.035, 0.12, 1.82)),
        ("Armor_LeftUpper", (0.06, 0.68, 0.008, 0.035, 1.94, 3.82)),
        ("Armor_RightLower", (3.32, 3.94, 0.008, 0.035, 0.12, 1.70)),
        ("Armor_RightUpper", (3.32, 3.94, 0.008, 0.035, 1.82, 3.82)),
        ("Armor_LintelLeft", (0.92, 1.92, 0.008, 0.035, 2.90, 3.82)),
        ("Armor_LintelRight", (2.08, 3.08, 0.008, 0.035, 2.90, 3.82)),
    )
    for name, bounds in panels:
        obj = _create_box(name, bounds, detail, armor, bevel=0.015)
        obj.parent = root

    # Four-centimeter black reveal, two eight-centimeter service channels,
    # a twelve-centimeter lintel maintenance recess, and one asymmetric cover.
    detail_boxes = (
        ("Reveal_Left", (0.76, 0.80, 0.0, 0.006, 0.0, 2.80), seal),
        ("Reveal_Right", (3.20, 3.24, 0.0, 0.006, 0.0, 2.80), seal),
        ("Reveal_Lintel", (0.80, 3.20, 0.0, 0.006, 2.80, 2.84), seal),
        ("ServiceChannel_Left_8cm", (0.32, 0.40, 0.0, 0.006, 0.34, 3.62), seal),
        ("ServiceChannel_Right_8cm", (3.60, 3.68, 0.0, 0.006, 0.34, 3.62), seal),
        ("LintelMaintenanceRecess_12cm", (1.10, 2.90, 0.0, 0.006, 3.29, 3.41), seal),
        ("AsymmetricRemovableCover", (0.47, 0.69, 0.003, 0.007, 2.18, 2.62), armor),
    )
    for name, bounds, material in detail_boxes:
        obj = _create_box(name, bounds, detail, material, bevel=0.008 if material == armor else 0.0)
        obj.parent = root

    overlay_collection = collections["30_STATE_OVERLAY"]
    overlay_specs = {
        "Damaged": (
            ("Damaged_Soot_Localized", (0.10, 0.29, 0.001, 0.006, 3.46, 3.62), soot),
            ("Damaged_Abrasion_Localized", (3.73, 3.90, 0.001, 0.006, 2.89, 2.94), abrasion),
        ),
        "Patched": (
            ("Patched_RepairPlate_A", (0.10, 0.27, 0.002, 0.007, 1.12, 1.45), patch),
            ("Patched_RepairPlate_B", (3.73, 3.90, 0.002, 0.007, 2.12, 2.45), patch),
            ("Patched_ProtectedCableGuide", (3.48, 3.55, 0.002, 0.007, 1.86, 2.24), patch),
            ("Patched_AmberStrip_A", (0.13, 0.24, 0.0, 0.0015, 1.24, 1.29), amber),
            ("Patched_AmberStrip_B", (3.76, 3.87, 0.0, 0.0015, 2.24, 2.29), amber),
        ),
        "Online": (
            ("Online_CyanStrip_Left", (0.13, 0.25, 0.0, 0.0015, 2.32, 2.38), cyan),
            ("Online_CyanStrip_Right", (3.75, 3.87, 0.0, 0.0015, 1.50, 1.56), cyan),
            ("Online_ColdWhiteWorkLight", (1.78, 2.22, 0.0, 0.0015, 2.86, 2.91), cold_white),
        ),
    }
    overlays: dict[str, list[object]] = {}
    for state_name, specifications in overlay_specs.items():
        overlays[state_name] = []
        for name, bounds, material in specifications:
            obj = _create_box(name, bounds, overlay_collection, material, bevel=0.004)
            obj.parent = root
            obj.hide_render = True
            obj["projectrift_appearance_only"] = True
            obj["projectrift_state"] = state_name
            overlays[state_name].append(obj)

    reference = collections["00_REFERENCE"]
    cameras = {
        "Front": _create_camera("CAM_Front", (2.0, -8.0, 2.0), (2.0, 0.15, 2.0), reference, True),
        "Back": _create_camera("CAM_Back", (2.0, 8.0, 2.0), (2.0, 0.15, 2.0), reference, True),
        "Left": _create_camera("CAM_Left", (-6.0, 0.15, 2.0), (2.0, 0.15, 2.0), reference, True),
        "Right": _create_camera("CAM_Right", (10.0, 0.15, 2.0), (2.0, 0.15, 2.0), reference, True),
        "Perspective": _create_camera("CAM_Perspective", (6.3, -7.2, 5.1), (2.0, 0.15, 2.0), reference, False),
    }
    _create_area_light("KEY_Neutral", (-1.0, -4.5, 6.5), 1050.0, 4.0, reference)
    _create_area_light("FILL_Neutral", (6.0, -2.0, 3.5), 720.0, 3.0, reference)
    _create_area_light("RIM_Neutral", (2.0, 4.0, 5.0), 900.0, 3.0, reference)
    return {
        "collections": collections,
        "root": root,
        "cameras": cameras,
        "overlays": overlays,
        "removable_cover": bpy.data.objects["AsymmetricRemovableCover"],
    }


def _measured_bounds(objects) -> dict[str, list[float]]:
    points = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            points.extend(evaluated.matrix_world @ vertex.co for vertex in mesh.vertices)
        finally:
            evaluated.to_mesh_clear()
    return {
        "min": [round(min(point[index] for point in points), 6) for index in range(3)],
        "max": [round(max(point[index] for point in points), 6) for index in range(3)],
    }


def _validate_scene(scene_data: dict[str, object]) -> tuple[list[str], dict[str, list[float]]]:
    issues: list[str] = []
    if tuple(collection.name for collection in bpy.data.collections) != COLLECTION_NAMES:
        issues.append("Collections do not match the exact appearance authority.")
    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    bounds = _measured_bounds(meshes)
    expected_bounds = {"min": [0.0, 0.0, 0.0], "max": [4.0, 0.3, 4.0]}
    if bounds != expected_bounds:
        issues.append(f"Measured envelope mismatch: {bounds}")
    root = scene_data["root"]
    if tuple(round(value, 6) for value in root.location) != (0.0, 0.0, 0.0):
        issues.append("Asset pivot is not bottom-left-back at (0,0,0).")
    required_frame_bounds = {
        ASSET_ID: expected_bounds,
    }
    if root.name not in required_frame_bounds:
        issues.append("Geometry authority root is missing.")
    # The modeled boxes are all analytically outside X 0.8..3.2 below Z 2.8.
    for obj in meshes:
        if obj == root:
            continue
        corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
        minimum = [min(point[index] for point in corners) for index in range(3)]
        maximum = [max(point[index] for point in corners) for index in range(3)]
        overlaps_x = maximum[0] > 0.8 + 1e-6 and minimum[0] < 3.2 - 1e-6
        overlaps_z = maximum[2] > 0.0 + 1e-6 and minimum[2] < 2.8 - 1e-6
        if overlaps_x and overlaps_z:
            issues.append(f"Detail crosses the clear opening: {obj.name}")
    return issues, bounds


def _set_candidate_state(scene_data: dict[str, object], state_name: str) -> None:
    for state_overlays in scene_data["overlays"].values():
        for obj in state_overlays:
            obj.hide_render = True
    scene_data["removable_cover"].hide_render = state_name == "Damaged"
    if state_name != "Base":
        for obj in scene_data["overlays"][state_name]:
            obj.hide_render = False


def _camera_transform_lens_hash(camera) -> str:
    payload = {
        "Name": camera.name,
        "Location": [round(float(value), 9) for value in camera.location],
        "RotationEuler": [round(float(value), 9) for value in camera.rotation_euler],
        "Type": camera.data.type,
        "LensMm": round(float(camera.data.lens), 9),
        "SensorWidthMm": round(float(camera.data.sensor_width), 9),
        "ShiftX": round(float(camera.data.shift_x), 9),
        "ShiftY": round(float(camera.data.shift_y), 9),
        "Resolution": [
            int(bpy.context.scene.render.resolution_x),
            int(bpy.context.scene.render.resolution_y),
            int(bpy.context.scene.render.resolution_percentage),
        ],
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _write_report(
    output_root: Path,
    bounds: dict[str, list[float]],
    passed: bool,
    candidate_metadata: list[dict[str, object]],
) -> Path:
    ledger_path = output_root / "Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json"
    approval = _validated_appearance_approval(output_root)
    report = {
        "Schema": REPORT_SCHEMA,
        "AssetId": ASSET_ID,
        "Passed": bool(passed),
        "BlenderVersion": bpy.app.version_string if bpy is not None else "Blender 5.2 LTS",
        "MeasuredBoundsM": bounds,
        "BoundsCm": {"Width": 400, "Depth": 30, "Height": 400},
        "DoorOpeningCm": {"Width": 240, "Height": 280, "MinX": 80, "MaxX": 320},
        "PivotAtBottomLeftBack": True,
        "ApprovalStatus": approval["Status"],
        "ApprovalDate": approval["Date"],
        "ApprovalEvidence": approval["Evidence"],
        "Method": "BlenderAuthoritativeStyleReconstruction",
        "ImageGenRole": "StyleReferenceOnly",
        "ImageGenStyleReference": "Concept/References/BaseStyleReference.png",
        "CandidateCamera": "CAM_Perspective",
        "CandidateCameraDriftPixels": 0,
        "Collections": list(COLLECTION_NAMES),
        "AuthorityViews": list(AUTHORITY_VIEWS),
        "Candidates": list(CANDIDATE_PATHS),
        "GenerationLedgerSha256": _sha256(ledger_path),
        "CandidateMetadata": candidate_metadata,
        "RenderProvenance": {
            "Tool": "Blender 5.2 LTS",
            "Engine": "Eevee Next",
            "GeometryAuthority": ASSET_ID,
            "StateOverlayCollection": "30_STATE_OVERLAY",
            "AppearanceOnly": True,
        },
        "ValidatedAt": datetime.now(timezone.utc).astimezone().isoformat(),
    }
    report_path = output_root / "Reports/appearance-validation.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = report_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, report_path)
    return report_path


def build(project_root: Path, contract_path: Path, output_root: Path) -> dict[str, str]:
    if bpy is None:
        raise RuntimeError("Deterministic appearance build must run inside Blender.")
    if bpy.app.version[:2] != (5, 2) or "LTS" not in bpy.app.version_string:
        raise RuntimeError(f"BuildAppearance requires Blender 5.2.x LTS; found {bpy.app.version_string}.")
    _require_contract(contract_path, project_root)
    scene_data = _build_scene()
    issues, bounds = _validate_scene(scene_data)
    if issues:
        raise ValueError("Appearance scene validation failed: " + "; ".join(issues))

    for relative_path in AUTHORITY_VIEWS:
        _set_candidate_state(scene_data, "Base")
        output_path = output_root / relative_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        view_name = output_path.stem
        bpy.context.scene.camera = scene_data["cameras"][view_name]
        bpy.context.scene.render.filepath = str(output_path)
        result = bpy.ops.render.render(write_still=True)
        if "FINISHED" not in result:
            raise RuntimeError(f"Authority render failed: {view_name}")

    perspective_camera = scene_data["cameras"]["Perspective"]
    authority_camera_hash = _camera_transform_lens_hash(perspective_camera)
    candidate_metadata: list[dict[str, object]] = []
    for state_name, relative_path in zip(("Base", "Damaged", "Patched", "Online"), CANDIDATE_PATHS):
        _set_candidate_state(scene_data, state_name)
        candidate_path = output_root / relative_path
        candidate_path.parent.mkdir(parents=True, exist_ok=True)
        bpy.context.scene.camera = perspective_camera
        camera_hash = _camera_transform_lens_hash(perspective_camera)
        if camera_hash != authority_camera_hash:
            raise ValueError(f"Candidate camera drift detected before rendering {state_name}.")
        bpy.context.scene.render.filepath = str(candidate_path)
        result = bpy.ops.render.render(write_still=True)
        if "FINISHED" not in result:
            raise RuntimeError(f"Candidate render failed: {state_name}")
        if _camera_transform_lens_hash(perspective_camera) != authority_camera_hash:
            raise ValueError(f"Candidate camera drift detected after rendering {state_name}.")
        candidate_metadata.append(
            {
                "State": state_name,
                "Path": relative_path,
                "Width": 2048,
                "Height": 2048,
                "Sha256": _sha256(candidate_path),
                "Camera": "CAM_Perspective",
                "CameraTransformLensSha256": camera_hash,
                "CameraDriftPixels": 0,
                "Provenance": "BlenderProceduralMaterialAndAppearanceOnlyOverlay",
            }
        )

    if len({entry["CameraTransformLensSha256"] for entry in candidate_metadata}) != 1:
        raise ValueError("Candidate camera transform/lens hashes differ.")

    blend_path = output_root / "Blender" / BLEND_FILENAME
    blend_path.parent.mkdir(parents=True, exist_ok=True)
    save_result = bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), check_existing=False)
    if "FINISHED" not in save_result:
        raise RuntimeError("Appearance BLEND save failed.")
    report_path = _write_report(output_root, bounds, passed=False, candidate_metadata=candidate_metadata)
    summary = {
        "blender": bpy.app.version_string,
        "blend": str(blend_path),
        "report": str(report_path),
        "authority_views": str(len(AUTHORITY_VIEWS)),
        "candidates": str(len(candidate_metadata)),
    }
    print("WALL_DOOR_APPEARANCE_SUMMARY=" + json.dumps(summary, sort_keys=True))
    return summary


def _png_metadata(path: Path) -> dict[str, object]:
    from PIL import Image

    with Image.open(path) as image:
        image.load()
        return {
            "Path": path.as_posix(),
            "Width": image.width,
            "Height": image.height,
            "Mode": image.mode,
            "Sha256": _sha256(path),
        }


def finalize(project_root: Path, contract_path: Path, output_root: Path) -> dict[str, str]:
    from PIL import Image, ImageDraw, ImageFont

    _require_contract(contract_path, project_root)
    approval = _validated_appearance_approval(output_root)
    ledger_path = output_root / "Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json"
    ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
    expected_ids = ["base-v1", "base-v2"]
    if [entry.get("Id") for entry in ledger.get("Entries", [])] != expected_ids:
        raise ValueError("Generation ledger must contain the two actual ordered ImageGen calls.")
    if ledger["Entries"][0].get("Status") != "RejectedCameraDrift":
        raise ValueError("base-v1 must remain honestly recorded as RejectedCameraDrift.")
    if ledger["Entries"][1].get("Status") != "StyleReferenceOnly":
        raise ValueError("base-v2 must remain honestly recorded as StyleReferenceOnly.")
    style_reference_path = output_root / "Concept/References/BaseStyleReference.png"
    if not style_reference_path.is_file():
        raise ValueError("The approved ImageGen style reference is missing.")
    if _sha256(style_reference_path) != ledger["Entries"][1].get("OutputSha256"):
        raise ValueError("The ImageGen style-reference hash does not match base-v2.")

    all_paths = [output_root / relative for relative in AUTHORITY_VIEWS + CANDIDATE_PATHS]
    for path in all_paths:
        if not path.is_file() or path.stat().st_size <= 24:
            raise ValueError(f"Required appearance raster is missing or empty: {path}")
    authority_metadata = [_png_metadata(path) for path in all_paths[:5]]
    measured_candidate_metadata = [_png_metadata(path) for path in all_paths[5:]]
    if any((item["Width"], item["Height"]) != (2048, 2048) for item in authority_metadata):
        raise ValueError("Authority views must all be exactly 2048x2048.")
    candidate_sizes = {(item["Width"], item["Height"]) for item in measured_candidate_metadata}
    if candidate_sizes != {(2048, 2048)}:
        raise ValueError("Final Blender candidates must all be exactly 2048x2048.")

    report_path = output_root / "Reports/appearance-validation.json"
    previous_report = json.loads(report_path.read_text(encoding="utf-8"))
    if previous_report.get("Method") != "BlenderAuthoritativeStyleReconstruction":
        raise ValueError("Preliminary report does not declare the approved Blender-authoritative method.")
    if previous_report.get("ImageGenRole") != "StyleReferenceOnly":
        raise ValueError("Preliminary report does not constrain ImageGen to StyleReferenceOnly.")
    if previous_report.get("CandidateCamera") != "CAM_Perspective":
        raise ValueError("Final candidates were not rendered from CAM_Perspective.")
    preliminary_metadata = previous_report.get("CandidateMetadata", [])
    camera_hashes = {entry.get("CameraTransformLensSha256") for entry in preliminary_metadata}
    if len(preliminary_metadata) != 4 or len(camera_hashes) != 1 or None in camera_hashes:
        raise ValueError("Final candidate camera transform/lens provenance is incomplete or inconsistent.")
    if any(entry.get("CameraDriftPixels") != 0 for entry in preliminary_metadata):
        raise ValueError("Final candidate camera drift is not zero.")
    measured_by_name = {
        Path(str(entry["Path"])).name: entry for entry in measured_candidate_metadata
    }
    candidate_metadata = []
    for entry in preliminary_metadata:
        measured = measured_by_name.get(Path(str(entry["Path"])).name)
        if measured is None:
            raise ValueError(f"Missing measured metadata for {entry['Path']}")
        if measured["Sha256"] != entry.get("Sha256"):
            raise ValueError(
                f"Candidate changed after the Blender render report was written: {entry['Path']}"
            )
        merged = dict(entry)
        merged.update(
            {
                "Width": measured["Width"],
                "Height": measured["Height"],
                "Mode": measured["Mode"],
                "Sha256": measured["Sha256"],
            }
        )
        candidate_metadata.append(merged)

    background = (19, 22, 27, 255)
    panel_size = 960
    gutter = 20
    margin = 30
    title_height = 100
    label_height = 42
    footer_height = 92
    sheet_width = margin * 2 + panel_size * 5 + gutter * 4
    sheet_height = title_height + (panel_size + label_height) * 2 + gutter + footer_height + margin
    sheet = Image.new("RGBA", (sheet_width, sheet_height), background)
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=26)
    title_font = ImageFont.load_default(size=42)
    footer_font = ImageFont.load_default(size=23)
    # Pillow's bundled bitmap font lacks an em-dash glyph, so draw the exact
    # title in two text runs with the em dash rendered deterministically.
    title_left = f"{ASSET_ID} "
    title_right = " G3.0 Appearance Lock"
    draw.text((margin, 26), title_left, fill=(235, 239, 244, 255), font=title_font)
    dash_x = margin + int(draw.textlength(title_left, font=title_font))
    draw.rectangle((dash_x, 50, dash_x + 30, 53), fill=(235, 239, 244, 255))
    draw.text((dash_x + 30, 26), title_right, fill=(235, 239, 244, 255), font=title_font)

    panels = [
        ("Front - Blender authority", all_paths[0], 0, 0),
        ("Back - Blender authority", all_paths[1], 1, 0),
        ("Left - Blender authority", all_paths[2], 2, 0),
        ("Right - Blender authority", all_paths[3], 3, 0),
        ("Perspective - Blender authority", all_paths[4], 0, 1),
        ("Base surface candidate", all_paths[5], 1, 1),
        ("Damaged state", all_paths[6], 2, 1),
        ("Patched state", all_paths[7], 3, 1),
        ("Online state", all_paths[8], 4, 1),
    ]
    for label, path, column, row in panels:
        x = margin + column * (panel_size + gutter)
        y = title_height + row * (panel_size + label_height + gutter)
        with Image.open(path) as source:
            source = source.convert("RGB")
            source.thumbnail((panel_size, panel_size), Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", (panel_size, panel_size), (31, 35, 41))
            offset = ((panel_size - source.width) // 2, (panel_size - source.height) // 2)
            canvas.paste(source, offset)
            sheet.paste(canvas.convert("RGBA"), (x, y))
        draw.rectangle((x, y, x + panel_size - 1, y + panel_size - 1), outline=(77, 86, 99, 255), width=2)
        draw.text((x + 8, y + panel_size + 7), label, fill=(210, 217, 226, 255), font=font)

    footer_y = sheet_height - footer_height
    footer = (
        "Bounds: 400 x 30 x 400 cm   |   Door opening: 240 x 280 cm at X 80..320 cm, Z 0..280 cm   |   "
        "Snap: 50 cm   |   Pivot: bottom-left-back (0,0,0)\n"
        f"Appearance approval: {approval['Status']}   |   Geometry authority: Blender 5.2 LTS"
    )
    draw.text((margin, footer_y), footer, fill=(186, 196, 208, 255), font=footer_font, spacing=8)
    sheet_path = output_root / f"Concept/{ASSET_ID}_AppearanceLock.png"
    sheet_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(sheet_path, format="PNG", optimize=False, compress_level=6)

    previous_report.update(
        {
            "Passed": True,
            "ApprovalStatus": approval["Status"],
            "ApprovalDate": approval["Date"],
            "ApprovalEvidence": approval["Evidence"],
            "Candidates": list(CANDIDATE_PATHS),
            "GenerationLedgerSha256": _sha256(ledger_path),
            "AuthorityMetadata": authority_metadata,
            "CandidateMetadata": candidate_metadata,
            "AppearanceSheet": {
                "Path": f"Concept/{ASSET_ID}_AppearanceLock.png",
                "Width": sheet.width,
                "Height": sheet.height,
                "Sha256": _sha256(sheet_path),
            },
            "ValidatedAt": datetime.now(timezone.utc).astimezone().isoformat(),
        }
    )
    temporary = report_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(previous_report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, report_path)
    return {"sheet": str(sheet_path), "report": str(report_path)}


def _require_production_inputs(output_root: Path) -> dict[str, object]:
    approval = _validated_appearance_approval(output_root)
    if approval["Status"] != "Approved":
        raise ValueError("BuildProduction requires explicit Appearance approval.")
    required = {
        "AppearanceBlend": output_root / "Blender" / BLEND_FILENAME,
        "AppearanceReport": output_root / "Reports/appearance-validation.json",
        "AppearanceSheet": output_root / f"Concept/{ASSET_ID}_AppearanceLock.png",
    }
    for label, path in required.items():
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"BuildProduction requires {label}: {path}")
    try:
        appearance_report = json.loads(required["AppearanceReport"].read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"Approved appearance report cannot be read: {error}") from None
    if (
        appearance_report.get("Schema") != REPORT_SCHEMA
        or appearance_report.get("AssetId") != ASSET_ID
        or appearance_report.get("Passed") is not True
        or appearance_report.get("ApprovalStatus") != "Approved"
    ):
        raise ValueError("BuildProduction requires the clean, approved Task 10 appearance report.")
    return {
        "Approval": approval,
        "Paths": required,
        "Hashes": {label: _sha256(path) for label, path in required.items()},
    }


def _create_prism_from_xz_outline(
    name: str,
    outline: tuple[tuple[float, float], ...],
    y0: float,
    y1: float,
    collection,
    material,
):
    count = len(outline)
    vertices = [(x, y0, z) for x, z in outline] + [(x, y1, z) for x, z in outline]
    faces: list[tuple[int, ...]] = [tuple(range(count)), tuple(reversed(range(count, count * 2)))]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, index + count, following + count, following))
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, (), faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    mesh.materials.append(material)
    return obj


def _add_source_modifiers(obj, bevel_width: float, *, weighted=True) -> None:
    if bevel_width > 0.0:
        bevel = obj.modifiers.new(name=f"Bevel_{round(bevel_width * 100)}cm", type="BEVEL")
        bevel.width = bevel_width
        bevel.segments = 3
        bevel.limit_method = "ANGLE"
    if weighted:
        weighted_normal = obj.modifiers.new(name="WeightedNormal_Source", type="WEIGHTED_NORMAL")
        weighted_normal.keep_sharp = True


def _add_boolean_difference(obj, cutter, name: str) -> None:
    modifier = obj.modifiers.new(name=name, type="BOOLEAN")
    modifier.operation = "DIFFERENCE"
    modifier.solver = "EXACT"
    modifier.object = cutter


def _create_production_source_scene() -> dict[str, object]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.unit_settings.length_unit = "METERS"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 15
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world = bpy.data.worlds.new("ProjectRift_ProductionNeutralWorld")
    scene.world.color = (0.035, 0.04, 0.05)
    bpy.context.preferences.filepaths.save_version = 0

    collections = {}
    for name in COLLECTION_NAMES:
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        collections[name] = collection

    structure_material = _create_material(
        "MAT_WallDoor_Structure", (0.045, 0.065, 0.085, 1.0), 0.42
    )
    secondary_material = _create_material(
        "MAT_WallDoor_Secondary", (0.27, 0.32, 0.37, 1.0), 0.52
    )
    source_structure = collections["10_STRUCTURE"]
    source_detail = collections["20_DETAIL"]
    source_overlay = collections["30_STATE_OVERLAY"]
    collision_collection = collections["40_COLLISION"]
    export_collection = collections["90_EXPORT"]

    outline = (
        (0.0, 0.0),
        (0.8, 0.0),
        (0.8, 2.8),
        (3.2, 2.8),
        (3.2, 0.0),
        (4.0, 0.0),
        (4.0, 4.0),
        (0.0, 4.0),
    )
    frame = _create_prism_from_xz_outline(
        "SRC_Primary_UFrame", outline, 0.04, 0.30, source_structure, structure_material
    )
    recess_cutters = {
        "LeftServiceChannel": _create_box(
            "SRC_CUTTER_Recess_LeftServiceChannel",
            (0.32, 0.40, 0.0, 0.08, 0.34, 3.62),
            source_detail,
            structure_material,
        ),
        "RightServiceChannel": _create_box(
            "SRC_CUTTER_Recess_RightServiceChannel",
            (3.60, 3.68, 0.0, 0.08, 0.34, 3.62),
            source_detail,
            structure_material,
        ),
        "LintelMaintenance": _create_box(
            "SRC_CUTTER_Recess_LintelMaintenance",
            (1.10, 2.90, 0.0, 0.08, 3.29, 3.41),
            source_detail,
            structure_material,
        ),
    }
    for cutter in recess_cutters.values():
        cutter.hide_render = True
        cutter.display_type = "WIRE"
        cutter["projectrift_recess_cutter"] = True
    for recess_name, cutter in recess_cutters.items():
        _add_boolean_difference(frame, cutter, f"Boolean_Recess_{recess_name}")
    _add_source_modifiers(frame, 0.05)
    frame["projectrift_modifier_intent"] = "NonDestructivePrimaryBevel+WeightedNormal"
    frame["projectrift_primary_silhouette_bevel_cm"] = 5.0

    mirror_anchor = bpy.data.objects.new("SRC_MirrorAnchor_CenterX", None)
    source_detail.objects.link(mirror_anchor)
    mirror_anchor.location = (2.0, 0.0, 0.0)
    base_sources: list[tuple[object, int]] = [(frame, 0)]
    for name, bounds in (
        ("SRC_Armor_LeftLower", (0.06, 0.68, 0.008, 0.035, 0.12, 1.82)),
        ("SRC_Armor_LeftUpper", (0.06, 0.68, 0.008, 0.035, 1.94, 3.82)),
    ):
        obj = _create_box(name, bounds, source_detail, secondary_material)
        mirror = obj.modifiers.new(name="Mirror_Across_CenterX", type="MIRROR")
        mirror.use_axis[0] = True
        mirror.use_clip = False
        mirror.mirror_object = mirror_anchor
        _add_boolean_difference(
            obj, recess_cutters["LeftServiceChannel"], "Boolean_Recess_LeftServiceChannel"
        )
        _add_boolean_difference(
            obj, recess_cutters["RightServiceChannel"], "Boolean_Recess_RightServiceChannel"
        )
        _add_source_modifiers(obj, 0.012)
        obj["projectrift_secondary_panel_bevel_cm"] = 1.2
        base_sources.append((obj, 1))

    for name, bounds in (
        ("SRC_Armor_LintelLeft", (0.92, 1.92, 0.008, 0.035, 2.90, 3.82)),
        ("SRC_Armor_LintelRight", (2.08, 3.08, 0.008, 0.035, 2.90, 3.82)),
    ):
        obj = _create_box(name, bounds, source_detail, secondary_material)
        _add_boolean_difference(
            obj, recess_cutters["LintelMaintenance"], "Boolean_Recess_LintelMaintenance"
        )
        _add_source_modifiers(obj, 0.012)
        obj["projectrift_secondary_panel_bevel_cm"] = 1.2
        base_sources.append((obj, 1))

    removable_cover = _create_box(
        "SRC_AsymmetricRemovableCover",
        (0.47, 0.69, 0.003, 0.035, 2.18, 2.62),
        source_detail,
        secondary_material,
    )
    _add_source_modifiers(removable_cover, 0.012)
    removable_cover["projectrift_secondary_panel_bevel_cm"] = 1.2
    removable_cover["projectrift_base_component"] = "RemovableCover"
    removable_cover["projectrift_hidden_in_state"] = "Damaged"

    reveal = _create_prism_from_xz_outline(
        "SRC_DoorReveal_U",
        (
            (0.76, 0.0),
            (0.80, 0.0),
            (0.80, 2.80),
            (3.20, 2.80),
            (3.20, 0.0),
            (3.24, 0.0),
            (3.24, 2.84),
            (0.76, 2.84),
        ),
        0.0,
        0.006,
        source_detail,
        structure_material,
    )
    base_sources.append((reveal, 0))
    frame["projectrift_recess_front_y_m"] = 0.04
    frame["projectrift_recess_back_y_m"] = 0.08

    damaged_rim = _create_box(
        "SRC_Damaged_AbsentCoverRim",
        (0.45, 0.71, 0.0, 0.018, 2.16, 2.64),
        source_overlay,
        secondary_material,
    )
    damaged_cutter = _create_box(
        "SRC_Damaged_AbsentCoverRim_Cutter",
        (0.49, 0.67, -0.01, 0.03, 2.20, 2.60),
        source_overlay,
        secondary_material,
    )
    boolean = damaged_rim.modifiers.new(name="Boolean_AbsentCover", type="BOOLEAN")
    boolean.operation = "DIFFERENCE"
    boolean.solver = "EXACT"
    boolean.object = damaged_cutter
    _add_source_modifiers(damaged_rim, 0.01)
    damaged_cutter.hide_render = True
    damaged_insert = _create_box(
        "SRC_Damaged_LocalizedEdgeInsert",
        (3.73, 3.90, 0.001, 0.02, 2.89, 2.96),
        source_overlay,
        secondary_material,
    )
    _add_source_modifiers(damaged_insert, 0.01)

    patch_plate = _create_box(
        "SRC_Patched_RepairPlate_Array",
        (0.10, 0.27, 0.002, 0.02, 1.12, 1.45),
        source_overlay,
        secondary_material,
    )
    plate_array = patch_plate.modifiers.new(name="Array_ExactlyTwoRepairPlates", type="ARRAY")
    plate_array.count = 2
    plate_array.use_relative_offset = False
    plate_array.use_constant_offset = True
    plate_array.constant_offset_displace = (3.63, 0.0, 1.0)
    _add_source_modifiers(patch_plate, 0.01)
    cable_guide = _create_box(
        "SRC_Patched_ProtectedCableGuide",
        (3.48, 3.55, 0.002, 0.02, 1.86, 2.24),
        source_overlay,
        secondary_material,
    )
    _add_source_modifiers(cable_guide, 0.01)

    source_groups = {
        "Base": base_sources,
        "Cover": [(removable_cover, 1)],
        "Damaged": [(damaged_rim, 1), (damaged_insert, 1)],
        "Patched": [(patch_plate, 1), (cable_guide, 1)],
    }

    collision_specs = (
        (f"UCX_{ASSET_ID}_LeftJamb", (0.0, 0.8, 0.0, 0.30, 0.0, 4.0), "LeftJamb"),
        (f"UCX_{ASSET_ID}_RightJamb", (3.2, 4.0, 0.0, 0.30, 0.0, 4.0), "RightJamb"),
        (f"UCX_{ASSET_ID}_Lintel", (0.8, 3.2, 0.0, 0.30, 2.8, 4.0), "Lintel"),
    )
    collisions = []
    for name, bounds, intent in collision_specs:
        obj = _create_box(name, bounds, collision_collection, structure_material)
        obj.hide_render = True
        obj.display_type = "WIRE"
        obj["projectrift_collision_intent"] = intent
        collisions.append(obj)

    reference = collections["00_REFERENCE"]
    cameras = {
        "Base": _create_camera("CAM_Production_Perspective", (6.3, -7.2, 5.1), (2.0, 0.15, 2.0), reference),
        "Front": _create_camera("CAM_Production_Front", (2.0, -8.0, 2.0), (2.0, 0.15, 2.0), reference, True),
    }
    _create_area_light("Production_KEY", (-1.0, -4.5, 6.5), 1050.0, 4.0, reference)
    _create_area_light("Production_FILL", (6.0, -2.0, 3.5), 720.0, 3.0, reference)
    _create_area_light("Production_RIM", (2.0, 4.0, 5.0), 900.0, 3.0, reference)
    return {
        "collections": collections,
        "materials": (structure_material, secondary_material),
        "source_groups": source_groups,
        "collisions": collisions,
        "cameras": cameras,
        "export_collection": export_collection,
        "damaged_cutter": damaged_cutter,
        "recess_cutters": recess_cutters,
    }


def _join_evaluated_sources(name: str, sources, collection, materials):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    material_indices: list[int] = []
    for source, material_index in sources:
        evaluated = source.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        try:
            offset = len(vertices)
            vertices.extend(
                tuple(float(value) for value in (evaluated.matrix_world @ vertex.co))
                for vertex in mesh.vertices
            )
            for polygon in mesh.polygons:
                faces.append(tuple(offset + index for index in polygon.vertices))
                material_indices.append(material_index)
        finally:
            evaluated.to_mesh_clear()
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, (), faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    for material in materials:
        mesh.materials.append(material)
    for polygon, material_index in zip(mesh.polygons, material_indices):
        polygon.material_index = material_index
    obj.location = (0.0, 0.0, 0.0)
    obj.rotation_euler = (0.0, 0.0, 0.0)
    obj.scale = (1.0, 1.0, 1.0)
    return obj


def _finalize_export_mesh(obj) -> None:
    import bmesh

    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.000001)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update(calc_edges=True)

    triangulate = obj.modifiers.new(name="Triangulate_FinalExportOnly", type="TRIANGULATE")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    result = bpy.ops.object.modifier_apply(modifier=triangulate.name)
    obj.select_set(False)
    if "FINISHED" not in result:
        raise RuntimeError(f"Failed to apply final export triangulation: {obj.name}")

    for uv_name, margin_factor in (("UV0", 0.08), ("UV1", 0.14)):
        uv_layer = mesh.uv_layers.new(name=uv_name)
        grid = max(1, math.ceil(math.sqrt(len(mesh.polygons))))
        cell = 1.0 / grid
        for index, polygon in enumerate(mesh.polygons):
            column = index % grid
            row = index // grid
            inset = margin_factor * cell
            corners = (
                (column * cell + inset, row * cell + inset),
                ((column + 1) * cell - inset, row * cell + inset),
                (column * cell + inset, (row + 1) * cell - inset),
            )
            for loop_offset, loop_index in enumerate(polygon.loop_indices):
                uv_layer.data[loop_index].uv = corners[min(loop_offset, 2)]
    obj["projectrift_modifiers_applied"] = True
    obj["projectrift_final_export_triangulated"] = True
    obj["projectrift_uv_contract"] = "UV0,UV1"


def _mesh_topology_evidence(obj) -> dict[str, object]:
    import bmesh

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    non_manifold_edges = [edge for edge in bm.edges if len(edge.link_faces) != 2]
    unexpected_non_manifold = len(non_manifold_edges)
    face_signatures: set[tuple[tuple[float, float, float], ...]] = set()
    vertex_signatures: set[tuple[float, float, float]] = set()
    duplicate_vertices = 0
    for vertex in bm.verts:
        signature = tuple(round(float(value), 6) for value in vertex.co)
        if signature in vertex_signatures:
            duplicate_vertices += 1
        vertex_signatures.add(signature)
    duplicates = 0
    for face in bm.faces:
        signature = tuple(
            sorted(tuple(round(float(value), 6) for value in vertex.co) for vertex in face.verts)
        )
        if signature in face_signatures:
            duplicates += 1
        face_signatures.add(signature)
    non_manifold_coordinates = [
            [
                [round(float(value), 6) for value in edge.verts[0].co],
                [round(float(value), 6) for value in edge.verts[1].co],
                len(edge.link_faces),
            ]
            for edge in non_manifold_edges
        ]
    signed_volume = float(bm.calc_volume(signed=True))
    bm.free()
    return {
        "UnexpectedNonManifoldEdgeCount": unexpected_non_manifold,
        "DuplicateFaceCount": duplicates,
        "DuplicateVertexCount": duplicate_vertices,
        "SignedVolumeM3": round(signed_volume, 9),
        "NonManifoldEdgeCoordinates": non_manifold_coordinates,
    }


def _object_bounds_cm(obj) -> list[float]:
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    minimum = [min(point[index] for point in points) * 100.0 for index in range(3)]
    maximum = [max(point[index] for point in points) * 100.0 for index in range(3)]
    return [round(value, 3) for value in minimum + maximum]


def _mesh_component_bounds_cm(obj) -> list[list[float]]:
    import bmesh

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    remaining = set(bm.faces)
    bounds = []
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
        bounds.append([round(value, 3) for value in minimum + maximum])
    bm.free()
    return sorted(bounds)


def _opening_from_export_geometry(obj) -> dict[str, list[float]]:
    coordinates = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    left = max(point.x for point in coordinates if point.x <= 0.800001)
    right = min(point.x for point in coordinates if point.x >= 3.199999)
    lintel = min(
        point.z
        for point in coordinates
        if 0.8 - 1e-6 <= point.x <= 3.2 + 1e-6 and point.z >= 2.799999
    )
    return {
        "SizeCm": [round((right - left) * 100.0, 3), round(lintel * 100.0, 3)],
        "X": [round(left * 100.0, 3), round(right * 100.0, 3)],
        "Z": [0.0, round(lintel * 100.0, 3)],
    }


def _outer_snap_face_evidence(obj) -> dict[str, object]:
    vertices = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    planes = (("LeftX", 0, 0.0), ("RightX", 0, 4.0), ("TopZ", 2, 4.0))
    evidence = {}
    for label, axis, expected in planes:
        coordinates = [float(vertex[axis]) for vertex in vertices if abs(float(vertex[axis]) - expected) <= 0.000001]
        maximum_deviation = max((abs(value - expected) for value in coordinates), default=math.inf)
        evidence[label] = {
            "PlaneM": expected,
            "VertexCount": len(coordinates),
            "MaximumDeviationM": round(maximum_deviation, 9),
            "Planar": len(coordinates) >= 3 and maximum_deviation <= 0.000001,
        }
    return {"Planes": evidence, "AllPlanar": all(item["Planar"] for item in evidence.values())}


def _render_production_reviews(scene_data: dict[str, object], output_root: Path) -> list[dict[str, object]]:
    exports = scene_data["exports"]
    for source_group in scene_data["source_groups"].values():
        for source, _ in source_group:
            source.hide_render = True
    scene_data["damaged_cutter"].hide_render = True
    for collision in scene_data["collisions"]:
        collision.hide_render = True

    relative_paths = (
        "Reports/ProductionReview/Base.png",
        "Reports/ProductionReview/Damaged.png",
        "Reports/ProductionReview/Patched.png",
    )
    metadata = []
    for state_name, relative_path in zip(("Base", "Damaged", "Patched"), relative_paths):
        exports["Cover"].hide_render = state_name == "Damaged"
        exports["Damaged"].hide_render = state_name != "Damaged"
        exports["Patched"].hide_render = state_name != "Patched"
        bpy.context.scene.camera = scene_data["cameras"]["Base"]
        output_path = output_root / relative_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        bpy.context.scene.render.filepath = str(output_path)
        result = bpy.ops.render.render(write_still=True)
        if "FINISHED" not in result:
            raise RuntimeError(f"Production review render failed: {state_name}")
        metadata.append(
            {
                "State": state_name,
                "Path": relative_path,
                "Width": 1024,
                "Height": 1024,
                "Camera": "CAM_Production_Perspective",
                "Sha256": _sha256(output_path),
            }
        )
    exports["Damaged"].hide_render = True
    exports["Patched"].hide_render = True
    return metadata


def build_production(project_root: Path, contract_path: Path, output_root: Path) -> dict[str, str]:
    if bpy is None:
        raise RuntimeError("BuildProduction must run inside Blender.")
    if bpy.app.version[:2] != (5, 2) or "LTS" not in bpy.app.version_string:
        raise RuntimeError(f"BuildProduction requires Blender 5.2.x LTS; found {bpy.app.version_string}.")
    contract = _require_contract(contract_path, project_root)
    _require_production_inputs(output_root)
    final_report_path = output_root / "Reports/geometry-validation.json"
    final_report_path.unlink(missing_ok=True)
    scene_data = _create_production_source_scene()

    exports = {
        "Base": _join_evaluated_sources(
            ASSET_ID,
            scene_data["source_groups"]["Base"],
            scene_data["export_collection"],
            scene_data["materials"],
        ),
        "Cover": _join_evaluated_sources(
            PRODUCTION_COVER_NAME,
            scene_data["source_groups"]["Cover"],
            scene_data["export_collection"],
            scene_data["materials"],
        ),
        "Damaged": _join_evaluated_sources(
            PRODUCTION_OVERLAY_NAMES[0],
            scene_data["source_groups"]["Damaged"],
            scene_data["export_collection"],
            scene_data["materials"],
        ),
        "Patched": _join_evaluated_sources(
            PRODUCTION_OVERLAY_NAMES[1],
            scene_data["source_groups"]["Patched"],
            scene_data["export_collection"],
            scene_data["materials"],
        ),
    }
    scene_data["exports"] = exports
    for obj in exports.values():
        _finalize_export_mesh(obj)
    exports["Base"]["projectrift_pivot"] = "BottomLeftBack"
    exports["Base"]["projectrift_online_silhouette_overlay_count"] = 0
    exports["Base"]["projectrift_base_body"] = True
    exports["Cover"]["projectrift_base_component"] = "RemovableCover"
    exports["Cover"]["projectrift_hidden_in_state"] = "Damaged"

    bounds = _measured_bounds([exports["Base"]])
    expected_bounds = {"min": [0.0, 0.0, 0.0], "max": [4.0, 0.3, 4.0]}
    opening = _opening_from_export_geometry(exports["Base"])
    topology = {name: _mesh_topology_evidence(obj) for name, obj in exports.items()}
    unexpected_non_manifold = sum(item["UnexpectedNonManifoldEdgeCount"] for item in topology.values())
    duplicate_faces = sum(item["DuplicateFaceCount"] for item in topology.values())
    if bounds != expected_bounds:
        raise ValueError(f"Production bounds mismatch: {bounds}")
    if opening != {"SizeCm": [240.0, 280.0], "X": [80.0, 320.0], "Z": [0.0, 280.0]}:
        raise ValueError(f"Production opening mismatch: {opening}")
    if unexpected_non_manifold != 0 or duplicate_faces != 0:
        raise ValueError(
            f"Export topology is not clean: non-manifold={unexpected_non_manifold}, "
            f"duplicates={duplicate_faces}, per-object={topology}"
        )
    if len(exports["Base"].data.materials) != contract.material_slot_limit:
        raise ValueError("Production base must use exactly two material slots.")

    collision_evidence = []
    for collision in scene_data["collisions"]:
        piece_bounds = _object_bounds_cm(collision)
        min_x, min_y, min_z, max_x, max_y, max_z = piece_bounds
        blocks_opening = (
            max_x > 80.0
            and min_x < 320.0
            and max_z > 0.0
            and min_z < 280.0
            and max_y > 0.0
            and min_y < 30.0
        )
        collision_evidence.append(
            {
                "Name": collision.name,
                "Intent": collision["projectrift_collision_intent"],
                "BoundsCm": piece_bounds,
                "BlocksDoorOpening": blocks_opening,
            }
        )
    if len(collision_evidence) != 3 or any(item["BlocksDoorOpening"] for item in collision_evidence):
        raise ValueError("Collision intent must contain three pieces which preserve the opening.")

    base_bounds_cm = [
        round((bounds["max"][index] - bounds["min"][index]) * 100.0, 3)
        for index in range(3)
    ]
    pivot_cm = [round(float(value) * 100.0, 3) for value in exports["Base"].location]
    applied_scale = [round(float(value), 6) for value in exports["Base"].scale]
    overlay_objects = [exports["Damaged"].name, exports["Patched"].name]
    online_overlay_count = sum(
        1 for obj in scene_data["export_collection"].objects if "Online" in obj.name
    )
    outer_snap_evidence = _outer_snap_face_evidence(exports["Base"])
    if not outer_snap_evidence["AllPlanar"]:
        raise ValueError(f"Outer snap faces are not planar: {outer_snap_evidence}")
    overlay_clearance = {}
    for state_name in ("Damaged", "Patched"):
        component_bounds = _mesh_component_bounds_cm(exports[state_name])
        crosses_opening = any(
            bounds[3] > 80.0
            and bounds[0] < 320.0
            and bounds[5] > 0.0
            and bounds[2] < 280.0
            for bounds in component_bounds
        )
        crosses_snap_planes = any(
            bounds[0] <= 0.0
            or bounds[3] >= 400.0
            or bounds[2] <= 0.0
            or bounds[5] >= 400.0
            for bounds in component_bounds
        )
        overlay_clearance[state_name] = {
            "ComponentBoundsCm": component_bounds,
            "CrossesDoorOpening": crosses_opening,
            "CrossesOuterSnapPlanes": crosses_snap_planes,
        }
    if any(
        evidence["CrossesDoorOpening"] or evidence["CrossesOuterSnapPlanes"]
        for evidence in overlay_clearance.values()
    ):
        raise ValueError(f"State overlay clearance failed: {overlay_clearance}")

    frame_source = scene_data["source_groups"]["Base"][0][0]
    primary_bevel_cm = round(
        next(modifier.width for modifier in frame_source.modifiers if modifier.type == "BEVEL") * 100.0,
        3,
    )
    secondary_bevels_cm = [
        round(modifier.width * 100.0, 3)
        for source, _ in scene_data["source_groups"]["Base"]
        for modifier in source.modifiers
        if modifier.type == "BEVEL" and source != frame_source
    ]
    secondary_bevel_cm = max(secondary_bevels_cm)
    functional_recess_depth_cm = round(
        (
            float(frame_source["projectrift_recess_back_y_m"])
            - float(frame_source["projectrift_recess_front_y_m"])
        )
        * 100.0,
        3,
    )
    patch_plate_source = scene_data["source_groups"]["Patched"][0][0]
    repair_plate_count = next(
        modifier.count for modifier in patch_plate_source.modifiers if modifier.type == "ARRAY"
    )
    cable_guide_count = sum(
        1 for source, _ in scene_data["source_groups"]["Patched"] if "CableGuide" in source.name
    )
    damaged_rim_sources = [
        source for source, _ in scene_data["source_groups"]["Damaged"] if "AbsentCoverRim" in source.name
    ]
    damaged_has_absent_cover_rim = len(damaged_rim_sources) == 1 and any(
        modifier.type == "BOOLEAN" for modifier in damaged_rim_sources[0].modifiers
    )
    has_door_leaf = any(
        "doorleaf" in obj.name.lower().replace("_", "")
        for obj in bpy.data.objects
        if obj.type == "MESH"
    )

    source_modifier_stacks = {
        source.name: [modifier.type for modifier in source.modifiers]
        for group in scene_data["source_groups"].values()
        for source, _ in group
    }
    modifier_types = sorted({item for stack in source_modifier_stacks.values() for item in stack})
    required_modifier_types = {"ARRAY", "BEVEL", "BOOLEAN", "MIRROR", "WEIGHTED_NORMAL"}
    if not required_modifier_types.issubset(modifier_types):
        raise ValueError(f"Required editable modifier evidence is incomplete: {modifier_types}")

    production_blend = output_root / "Blender" / PRODUCTION_BLEND_FILENAME
    production_blend.parent.mkdir(parents=True, exist_ok=True)
    save_result = bpy.ops.wm.save_as_mainfile(filepath=str(production_blend), check_existing=False)
    if "FINISHED" not in save_result:
        raise RuntimeError("Production BLEND save failed.")

    summary = {
        "blender": bpy.app.version_string,
        "blend": str(production_blend),
        "base_triangles": str(len(exports["Base"].data.polygons)),
        "export_objects": str(len(exports)),
        "final_report_writer": "IndependentSavedBlendValidator",
    }
    print("WALL_DOOR_PRODUCTION_SUMMARY=" + json.dumps(summary, sort_keys=True))
    return summary


def main(argv: list[str]) -> int:
    arguments = _parse_arguments(argv)
    project_root, contract_path, output_root = _validated_paths(arguments)
    if arguments.finalize and arguments.production:
        raise ValueError("--finalize and --production are mutually exclusive.")
    if arguments.production:
        build_production(project_root, contract_path, output_root)
    elif arguments.finalize:
        print(json.dumps(finalize(project_root, contract_path, output_root), sort_keys=True))
    else:
        build(project_root, contract_path, output_root)
    return 0


if __name__ == "__main__":
    try:
        raw_arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else sys.argv[1:]
        raise SystemExit(main(raw_arguments))
    except SystemExit:
        raise
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
