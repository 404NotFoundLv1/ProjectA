from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import sys
import traceback
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import shiphub_blender as sb
from shiphub_contract import load_contract
from shiphub_layout import ShipHubLayout, build_layout, validate_layout


COLLECTION_NAMES = (
    "00_REFERENCE",
    "10_STRUCTURE",
    "20_NAV_TABLE",
    "30_CRYOPODS",
    "40_AIRLOCK",
    "50_WEST_BAYS",
    "60_EAST_BAYS",
    "70_CONSTRUCT_DOCKS",
    "80_CEILING",
    "90_CAMERAS",
)
MATERIAL_DEFINITIONS = (
    ("MAT_Structure", (0.76, 0.78, 0.80, 1.0)),
    ("MAT_Interactable", (0.90, 0.92, 0.94, 1.0)),
    ("MAT_Glass", (0.63, 0.72, 0.77, 0.82)),
    ("MAT_Door", (0.53, 0.56, 0.60, 1.0)),
    ("MAT_NonWalkable", (0.34, 0.36, 0.39, 1.0)),
)
EXPORT_COLLECTION_NAMES = COLLECTION_NAMES[1:9]
BLEND_FILENAME = "SM_ShipHub_Complete_White_v1.blend"
FBX_FILENAME = "SM_ShipHub_Complete_White_v1.fbx"
GLB_FILENAME = "SM_ShipHub_Complete_White_v1.glb"
MANIFEST_FILENAME = "layout-manifest.json"


def _clean_float(value: float) -> float:
    rounded = round(float(value), 6)
    return 0.0 if abs(rounded) < 0.0000005 else rounded


def _vector(values) -> list[float]:
    return [_clean_float(value) for value in values]


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
    parser = argparse.ArgumentParser(
        description="Build the ProjectRift ship-hub Blender white model."
    )
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--brief", required=True)
    parser.add_argument("--output-root", required=True)
    return parser.parse_args(argv)


def _validated_paths(arguments: argparse.Namespace) -> tuple[Path, Path, Path]:
    project_root = Path(arguments.project_root).resolve()
    if not (project_root / "ProjectA.uproject").is_file():
        raise ValueError(
            f"--project-root is not the ProjectA authority: {project_root}"
        )
    brief = _require_descendant(Path(arguments.brief), project_root, "--brief")
    expected_brief = (
        project_root
        / "SourceArt"
        / "ProjectRift"
        / "ShipHub"
        / "Briefs"
        / "ShipHubCompleteDesign_v1.json"
    ).resolve()
    if brief != expected_brief or not brief.is_file():
        raise ValueError(f"--brief must be the ProjectA ship-hub authority: {expected_brief}")

    output_root = _require_descendant(
        Path(arguments.output_root), project_root, "--output-root"
    )
    approved_output_root = (
        project_root
        / "SourceArt"
        / "ProjectRift"
        / "ShipHub"
        / "CompleteDesign"
    ).resolve()
    if output_root != approved_output_root:
        raise ValueError(f"--output-root must be exactly {approved_output_root}")
    for relative_output in (
        Path("Blender") / BLEND_FILENAME,
        Path("Exports") / FBX_FILENAME,
        Path("Exports") / GLB_FILENAME,
        Path("Reports") / MANIFEST_FILENAME,
    ):
        _require_descendant(output_root / relative_output, output_root, "output path")
    return project_root, brief, output_root


def _require_blender_52_lts() -> None:
    version = tuple(int(value) for value in bpy.app.version)
    if version[:2] != (5, 2) or "LTS" not in bpy.app.version_string:
        raise RuntimeError(
            "BuildWhiteModel requires Blender 5.2.x LTS; "
            f"found {bpy.app.version_string}."
        )


def _tag(obj, semantic_type: str):
    obj["projectrift_semantic_type"] = semantic_type
    return obj


def _create_mesh(
    name: str,
    vertices,
    faces,
    location_m,
    collection,
    material,
    semantic_type: str,
):
    return _tag(
        sb._create_mesh_object(
            name, vertices, faces, location_m, collection, material
        ),
        semantic_type,
    )


def _create_wall_segment(
    name: str,
    start,
    end,
    thickness_m: float,
    height_m: float,
    collection,
    material,
):
    delta_x = end[0] - start[0]
    delta_y = end[1] - start[1]
    length = math.hypot(delta_x, delta_y)
    if length == 0.0:
        raise ValueError(f"Wall segment is zero length: {name}")
    outward_x = delta_y / length
    outward_y = -delta_x / length
    polygon = (
        start,
        end,
        (end[0] + outward_x * thickness_m, end[1] + outward_y * thickness_m),
        (start[0] + outward_x * thickness_m, start[1] + outward_y * thickness_m),
    )
    return _tag(
        sb.create_prism_from_polygon(
            name, polygon, 0.0, height_m, collection, material
        ),
        "structural_wall",
    )


def _create_sector(
    name: str,
    radius_m: float,
    z_min_m: float,
    z_max_m: float,
    start_degrees: float,
    collection,
    material,
):
    arc_segments = 24
    angles = [
        math.radians(start_degrees + 90.0 * index / arc_segments)
        for index in range(arc_segments + 1)
    ]
    vertices = [(0.0, 0.0, z_min_m)]
    vertices.extend(
        (radius_m * math.cos(angle), radius_m * math.sin(angle), z_min_m)
        for angle in angles
    )
    top_center = len(vertices)
    vertices.append((0.0, 0.0, z_max_m))
    top_arc_start = len(vertices)
    vertices.extend(
        (radius_m * math.cos(angle), radius_m * math.sin(angle), z_max_m)
        for angle in angles
    )
    faces = []
    for index in range(arc_segments):
        faces.append((0, index + 2, index + 1))
        faces.append(
            (
                top_center,
                top_arc_start + index,
                top_arc_start + index + 1,
            )
        )
        faces.append(
            (
                index + 1,
                index + 2,
                top_arc_start + index + 1,
                top_arc_start + index,
            )
        )
    faces.append((0, 1, top_arc_start, top_center))
    faces.append(
        (
            0,
            top_center,
            top_arc_start + arc_segments,
            arc_segments + 1,
        )
    )
    return _create_mesh(
        name,
        vertices,
        faces,
        (0.0, 0.0, 0.0),
        collection,
        material,
        "navigation_table_sector",
    )


def _create_annular_prism(
    name: str,
    inner_radius_m: float,
    outer_radius_m: float,
    z_min_m: float,
    z_max_m: float,
    collection,
    material,
):
    segments = 96
    vertices = []
    for z_value in (z_min_m, z_max_m):
        for radius in (inner_radius_m, outer_radius_m):
            vertices.extend(
                (
                    radius * math.cos(2.0 * math.pi * index / segments),
                    radius * math.sin(2.0 * math.pi * index / segments),
                    z_value,
                )
                for index in range(segments)
            )
    inner_bottom = 0
    outer_bottom = segments
    inner_top = segments * 2
    outer_top = segments * 3
    faces = []
    for index in range(segments):
        next_index = (index + 1) % segments
        faces.extend(
            (
                (
                    inner_bottom + index,
                    inner_bottom + next_index,
                    outer_bottom + next_index,
                    outer_bottom + index,
                ),
                (
                    inner_top + index,
                    outer_top + index,
                    outer_top + next_index,
                    inner_top + next_index,
                ),
                (
                    inner_bottom + index,
                    inner_top + index,
                    inner_top + next_index,
                    inner_bottom + next_index,
                ),
                (
                    outer_bottom + index,
                    outer_bottom + next_index,
                    outer_top + next_index,
                    outer_top + index,
                ),
            )
        )
    return _create_mesh(
        name,
        vertices,
        faces,
        (0.0, 0.0, 0.0),
        collection,
        material,
        "ceiling_service_ring",
    )


def _create_capsule(
    name: str,
    radius_m: float,
    half_height_m: float,
    location_m,
    collection,
    material,
):
    segments = 32
    hemisphere_rings = 8
    cylinder_half = half_height_m - radius_m
    rings = []
    for index in range(1, hemisphere_rings + 1):
        latitude = -math.pi / 2.0 + (math.pi / 2.0) * index / hemisphere_rings
        rings.append((radius_m * math.cos(latitude), -cylinder_half + radius_m * math.sin(latitude)))
    for index in range(1, hemisphere_rings):
        latitude = (math.pi / 2.0) * index / hemisphere_rings
        rings.append((radius_m * math.cos(latitude), cylinder_half + radius_m * math.sin(latitude)))
    vertices = [(0.0, 0.0, -half_height_m)]
    for ring_radius, z_value in rings:
        vertices.extend(
            (
                ring_radius * math.cos(2.0 * math.pi * index / segments),
                ring_radius * math.sin(2.0 * math.pi * index / segments),
                z_value,
            )
            for index in range(segments)
        )
    top_pole = len(vertices)
    vertices.append((0.0, 0.0, half_height_m))
    faces = []
    for index in range(segments):
        next_index = (index + 1) % segments
        faces.append((0, 1 + next_index, 1 + index))
    for ring_index in range(len(rings) - 1):
        first = 1 + ring_index * segments
        second = first + segments
        for index in range(segments):
            next_index = (index + 1) % segments
            faces.append(
                (first + index, first + next_index, second + next_index, second + index)
            )
    last_ring = 1 + (len(rings) - 1) * segments
    for index in range(segments):
        next_index = (index + 1) % segments
        faces.append((last_ring + index, last_ring + next_index, top_pole))
    return _create_mesh(
        name,
        vertices,
        faces,
        location_m,
        collection,
        material,
        "character_reference",
    )


def _create_leaning_cryopod(
    name: str,
    source_dimensions_m,
    base_location_m,
    recline_degrees: float,
    collection,
    material,
):
    target_width = float(source_dimensions_m[0])
    target_depth = float(source_dimensions_m[1])
    target_height = float(source_dimensions_m[2])
    angle_radians = math.radians(recline_degrees)
    cosine = math.cos(angle_radians)
    sine = math.sin(angle_radians)
    denominator = cosine * cosine - sine * sine
    if abs(denominator) <= 0.000001:
        raise ValueError(f"Cryopod recline is not analytically solvable: {name}")
    local_depth = (
        target_depth * cosine - target_height * sine
    ) / denominator
    local_height = (
        target_height * cosine - target_depth * sine
    ) / denominator
    if local_depth <= 0.0 or local_height <= 0.0:
        raise ValueError(f"Cryopod solved dimensions are not positive: {name}")

    half_x = target_width / 2.0
    source_vertices = (
        (-half_x, -local_depth, 0.0),
        (half_x, -local_depth, 0.0),
        (half_x, 0.0, 0.0),
        (-half_x, 0.0, 0.0),
        (-half_x, -local_depth, local_height),
        (half_x, -local_depth, local_height),
        (half_x, 0.0, local_height),
        (-half_x, 0.0, local_height),
    )
    rotation = Matrix.Rotation(-angle_radians, 4, "X")
    vertices = [tuple(rotation @ Vector(vertex)) for vertex in source_vertices]
    faces = (
        (3, 2, 1, 0),
        (4, 5, 6, 7),
        (0, 1, 5, 4),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 0, 4, 7),
    )
    obj = _create_mesh(
        name,
        vertices,
        faces,
        base_location_m,
        collection,
        material,
        "cryopod",
    )
    base_axis = obj.vertex_groups.new(name="CRYOPOD_BASE_AXIS")
    base_axis.add((2, 3), 1.0, "REPLACE")
    top_axis = obj.vertex_groups.new(name="CRYOPOD_TOP_AXIS")
    top_axis.add((6, 7), 1.0, "REPLACE")
    obj["projectrift_recline_degrees"] = float(recline_degrees)
    return obj


def _build_scene(layout: ShipHubLayout) -> None:
    sb.reset_scene()
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.unit_settings.length_unit = "METERS"

    collections = {name: sb.ensure_collection(name) for name in COLLECTION_NAMES}
    materials = {
        name: sb.create_id_material(name, rgba)
        for name, rgba in MATERIAL_DEFINITIONS
    }

    reference = collections["00_REFERENCE"]
    structure = collections["10_STRUCTURE"]
    nav_table = collections["20_NAV_TABLE"]
    cryopods = collections["30_CRYOPODS"]
    airlock = collections["40_AIRLOCK"]
    west_bays = collections["50_WEST_BAYS"]
    east_bays = collections["60_EAST_BAYS"]
    docks = collections["70_CONSTRUCT_DOCKS"]
    ceiling = collections["80_CEILING"]

    structure_material = materials["MAT_Structure"]
    interactable_material = materials["MAT_Interactable"]
    glass_material = materials["MAT_Glass"]
    door_material = materials["MAT_Door"]
    nonwalkable_material = materials["MAT_NonWalkable"]

    floor = sb.create_prism_from_polygon(
        "SM_ShipHub_FloorSlab",
        layout.inner_vertices_m,
        -layout.room.floor_thickness_m,
        0.0,
        structure,
        structure_material,
    )
    _tag(floor, "floor_slab")

    wall_height = layout.clear_dimensions_m[2]
    wall_thickness = layout.room.wall_thickness_m
    south_start = layout.inner_vertices_m[0]
    south_end = layout.inner_vertices_m[1]
    opening_half_width = layout.airlock_clear_opening_m[0] / 2.0
    _create_wall_segment(
        "SM_ShipHub_Wall_SouthWest",
        south_start,
        (-opening_half_width, south_start[1]),
        wall_thickness,
        wall_height,
        structure,
        structure_material,
    )
    _create_wall_segment(
        "SM_ShipHub_Wall_SouthEast",
        (opening_half_width, south_end[1]),
        south_end,
        wall_thickness,
        wall_height,
        structure,
        structure_material,
    )
    for index in range(1, len(layout.inner_vertices_m)):
        _create_wall_segment(
            f"SM_ShipHub_Wall_{index + 1:02d}",
            layout.inner_vertices_m[index],
            layout.inner_vertices_m[(index + 1) % len(layout.inner_vertices_m)],
            wall_thickness,
            wall_height,
            structure,
            structure_material,
        )

    ceiling_shell = sb.create_prism_from_polygon(
        "SM_ShipHub_CeilingPressureShell",
        layout.inner_vertices_m,
        layout.clear_dimensions_m[2],
        layout.clear_dimensions_m[2] + wall_thickness,
        structure,
        structure_material,
    )
    _tag(ceiling_shell, "ceiling_pressure_shell")

    rib_height = layout.room.nominal_height_m - 0.4
    rib_z = rib_height / 2.0
    for side, y_value in (("North", 12.5), ("South", -12.5)):
        for x_value in (-8.0, -4.0, 0.0, 4.0, 8.0):
            if side == "South" and abs(x_value) < opening_half_width:
                continue
            rib = sb.create_box(
                f"SM_ShipHub_Rib_{side}_{x_value:+.0f}",
                (0.22, 0.6, rib_height),
                (x_value, y_value, rib_z),
                structure,
                structure_material,
            )
            _tag(rib, "structural_rib")
    for side, x_value in (("West", -14.5), ("East", 14.5)):
        for y_value in (-6.0, -2.0, 2.0, 6.0):
            rib = sb.create_box(
                f"SM_ShipHub_Rib_{side}_{y_value:+.0f}",
                (0.6, 0.22, rib_height),
                (x_value, y_value, rib_z),
                structure,
                structure_material,
            )
            _tag(rib, "structural_rib")

    for index in range(4):
        _create_sector(
            f"SM_ShipHub_NavTable_Sector_{index + 1:02d}",
            layout.navigation_table_radius_m,
            0.0,
            layout.navigation_table_height_m - 0.1,
            index * 90.0,
            nav_table,
            interactable_material,
        )
    display = sb.create_cylinder(
        "SM_ShipHub_NavTable_Display",
        layout.navigation_table.display_diameter_m / 2.0,
        0.1,
        (
            layout.navigation_table_center_m[0],
            layout.navigation_table_center_m[1],
            layout.navigation_table_height_m - 0.05,
        ),
        nav_table,
        glass_material,
        vertices=96,
    )
    _tag(display, "navigation_table_display")

    for index, pod in enumerate(layout.cryopods):
        _create_leaning_cryopod(
            f"SM_ShipHub_Cryopod_{index + 1:02d}",
            layout.cryopod_spec.bounds_m,
            pod.center_m,
            pod.recline_degrees,
            cryopods,
            interactable_material,
        )
    pod_bank_half_width = (
        max(layout.cryopod_spec.centers_x_m)
        - min(layout.cryopod_spec.centers_x_m)
    ) / 2.0
    interface_offset = (
        pod_bank_half_width
        + layout.cryopod_spec.bounds_m[0] / 2.0
        + layout.cryopod_spec.expansion_interface_width_m / 2.0
    )
    for side, x_value in (("West", -interface_offset), ("East", interface_offset)):
        interface = sb.create_box(
            f"SM_ShipHub_ExpansionInterface_{side}",
            (
                layout.cryopod_spec.expansion_interface_width_m,
                1.2,
                3.5,
            ),
            (x_value, layout.cryopod_spec.base_y_m + 0.8, 1.75),
            cryopods,
            structure_material,
        )
        _tag(interface, "expansion_interface")

    opening_width, opening_height = layout.airlock_clear_opening_m
    frame_depth = layout.airlock_depth_m
    frame_width = 0.6
    frame_height = opening_height + 0.8
    for side, x_value in (
        ("West", -opening_width / 2.0 - frame_width / 2.0),
        ("East", opening_width / 2.0 + frame_width / 2.0),
    ):
        frame = sb.create_box(
            f"SM_ShipHub_Airlock_Frame{side}",
            (frame_width, frame_depth, frame_height),
            (x_value, layout.airlock_center_m[1], frame_height / 2.0),
            airlock,
            structure_material,
        )
        _tag(frame, "airlock_frame")
    header = sb.create_box(
        "SM_ShipHub_Airlock_Header",
        (opening_width + 2.0 * frame_width, frame_depth, 0.8),
        (0.0, layout.airlock_center_m[1], opening_height + 0.4),
        airlock,
        structure_material,
    )
    _tag(header, "airlock_frame")
    door = sb.create_box(
        "SM_ShipHub_Airlock_DoorPanel",
        (opening_width - 0.2, 0.2, opening_height - 0.1),
        (0.0, layout.airlock_center_m[1], (opening_height - 0.1) / 2.0),
        airlock,
        door_material,
    )
    _tag(door, "airlock_door")

    preparation_y_min, preparation_y_max = layout.west_preparation_y_range_m
    preparation = sb.create_box(
        "SM_ShipHub_West_PreparationBay",
        (1.2, preparation_y_max - preparation_y_min, 2.4),
        (-13.2, (preparation_y_min + preparation_y_max) / 2.0, 1.2),
        west_bays,
        interactable_material,
    )
    _tag(preparation, "west_preparation_bay")
    repair = sb.create_box(
        "SM_ShipHub_West_RepairBay",
        (1.6, 3.0, 2.8),
        layout.west_repair_center_m,
        west_bays,
        interactable_material,
    )
    _tag(repair, "west_repair_bay")

    medical_y_min, medical_y_max = layout.east_medical_y_range_m
    medical = sb.create_box(
        "SM_ShipHub_East_MedicalBay",
        (1.2, medical_y_max - medical_y_min, 2.4),
        (13.2, (medical_y_min + medical_y_max) / 2.0, 1.2),
        east_bays,
        interactable_material,
    )
    _tag(medical, "east_medical_bay")
    role_y_min, role_y_max = layout.east_role_y_range_m
    role = sb.create_box(
        "SM_ShipHub_East_RoleBay",
        (1.2, role_y_max - role_y_min, 2.4),
        (13.2, (role_y_min + role_y_max) / 2.0, 1.2),
        east_bays,
        interactable_material,
    )
    _tag(role, "east_role_bay")
    jump_core = sb.create_cylinder(
        "SM_ShipHub_East_JumpCoreBay",
        1.0,
        4.0,
        layout.east_jump_core_center_m,
        east_bays,
        nonwalkable_material,
        vertices=64,
    )
    _tag(jump_core, "east_jump_core_bay")

    for index, center in enumerate(layout.dock_centers_m):
        dock = sb.create_cylinder(
            f"SM_ShipHub_ConstructDock_{index + 1:02d}",
            layout.construct_docks.diameter_m / 2.0,
            layout.construct_docks.recess_m,
            center,
            docks,
            interactable_material,
            vertices=64,
        )
        for vertex in dock.data.vertices:
            vertex.co.z -= layout.construct_docks.recess_m / 2.0
        dock.data.update()
        _tag(dock, "construct_dock")

    _create_annular_prism(
        "SM_ShipHub_CeilingServiceRing",
        layout.ceiling_ring.inner_diameter_m / 2.0,
        layout.ceiling_ring.outer_diameter_m / 2.0,
        layout.ceiling_ring.lowest_z_m,
        layout.ceiling_ring.highest_z_m,
        ceiling,
        nonwalkable_material,
    )

    envelope = sb.create_box(
        "REF_ShipHub_ClearEnvelope_28x24x7",
        layout.clear_dimensions_m,
        (0.0, 0.0, layout.clear_dimensions_m[2] / 2.0),
        reference,
        structure_material,
    )
    _tag(envelope, "clear_envelope_reference")
    envelope.display_type = "WIRE"
    envelope.hide_render = True
    capsule = _create_capsule(
        "REF_CharacterCapsule",
        layout.character_reference.capsule_radius_m,
        layout.character_reference.capsule_half_height_m,
        (0.0, -6.0, layout.character_reference.capsule_half_height_m),
        reference,
        interactable_material,
    )
    capsule.display_type = "WIRE"
    capsule.hide_render = True
    muster = sb.create_box(
        "REF_Airlock_MusterArea_8x5",
        (layout.airlock_muster_area_m[0], layout.airlock_muster_area_m[1], 0.02),
        (
            layout.airlock_center_m[0],
            layout.airlock_center_m[1] + layout.airlock_muster_area_m[1] / 2.0,
            0.01,
        ),
        reference,
        door_material,
    )
    _tag(muster, "airlock_muster_reference")
    muster.display_type = "WIRE"
    muster.hide_render = True

    for x_value in range(-12, 13, int(layout.units.structural_bay_m)):
        grid = sb.create_box(
            f"REF_Grid_X_{x_value:+03d}",
            (0.025, 24.0, 0.025),
            (float(x_value), 0.0, 0.025),
            reference,
            nonwalkable_material,
        )
        _tag(grid, "dimensional_grid_reference")
        grid.hide_render = True
    for y_value in range(-8, 9, int(layout.units.structural_bay_m)):
        grid = sb.create_box(
            f"REF_Grid_Y_{y_value:+03d}",
            (28.0, 0.025, 0.025),
            (0.0, float(y_value), 0.025),
            reference,
            nonwalkable_material,
        )
        _tag(grid, "dimensional_grid_reference")
        grid.hide_render = True

    for obj in bpy.data.objects:
        if obj.type == "MESH":
            sb.apply_object_transforms(obj)


def _object_collection_name(obj) -> str:
    collection_names = [collection.name for collection in obj.users_collection]
    if len(collection_names) != 1:
        raise ValueError(
            f"Mesh object must belong to exactly one collection: {obj.name}"
        )
    return collection_names[0]


def _production_objects() -> list:
    order = {name: index for index, name in enumerate(EXPORT_COLLECTION_NAMES)}
    result = [
        obj
        for obj in bpy.data.objects
        if obj.type == "MESH" and _object_collection_name(obj) in order
    ]
    return sorted(result, key=lambda obj: (order[_object_collection_name(obj)], obj.name))


def _measured_dimensions(bounds: dict[str, list[float]]) -> list[float]:
    return [
        _clean_float(bounds["max"][axis] - bounds["min"][axis])
        for axis in range(3)
    ]


def _object_member(obj) -> dict:
    bounds = sb.measured_bounds(obj)
    return {
        "name": obj.name,
        "location_m": _vector(obj.location),
        "measured_world_bounds_m": bounds,
        "measured_dimensions_m": _measured_dimensions(bounds),
    }


def _evaluated_world_points(obj) -> list[Vector]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        return [evaluated.matrix_world @ vertex.co.copy() for vertex in mesh.vertices]
    finally:
        evaluated.to_mesh_clear()


def _axis_group_world_centroid(obj, group_name: str) -> Vector:
    group = obj.vertex_groups.get(group_name)
    if group is None:
        raise ValueError(f"Missing cryopod axis group {group_name}: {obj.name}")
    indices = [
        vertex.index
        for vertex in obj.data.vertices
        if any(membership.group == group.index for membership in vertex.groups)
    ]
    if len(indices) != 2:
        raise ValueError(
            f"Cryopod axis group must contain exactly two vertices: {obj.name}/{group_name}"
        )
    world_points = _evaluated_world_points(obj)
    return sum((world_points[index] for index in indices), Vector()) / len(indices)


def _cryopod_member(obj) -> dict:
    member = _object_member(obj)
    base_axis = _axis_group_world_centroid(obj, "CRYOPOD_BASE_AXIS")
    top_axis = _axis_group_world_centroid(obj, "CRYOPOD_TOP_AXIS")
    axis = top_axis - base_axis
    member.update(
        {
            "base_axis_center_m": _vector(base_axis),
            "top_axis_center_m": _vector(top_axis),
            "measured_recline_degrees": _clean_float(
                math.degrees(math.atan2(axis.y, axis.z))
            ),
            "leans_toward": "+Y" if axis.y > 0.0 else "-Y",
        }
    )
    return member


def _dock_member(obj) -> dict:
    member = _object_member(obj)
    dimensions = member["measured_dimensions_m"]
    bounds = member["measured_world_bounds_m"]
    member.update(
        {
            "measured_diameter_m": _clean_float(max(dimensions[0], dimensions[1])),
            "measured_top_z_m": bounds["max"][2],
            "measured_recess_m": _clean_float(
                bounds["max"][2] - bounds["min"][2]
            ),
        }
    )
    return member


def _ring_member(obj) -> dict:
    member = _object_member(obj)
    center = obj.matrix_world.translation
    points = _evaluated_world_points(obj)
    radii = [math.hypot(point.x - center.x, point.y - center.y) for point in points]
    bounds = member["measured_world_bounds_m"]
    member.update(
        {
            "measured_inner_radius_m": _clean_float(min(radii)),
            "measured_outer_radius_m": _clean_float(max(radii)),
            "measured_z_range_m": [bounds["min"][2], bounds["max"][2]],
        }
    )
    return member


def _airlock_measurement() -> dict:
    required_names = (
        "SM_ShipHub_Airlock_DoorPanel",
        "SM_ShipHub_Airlock_FrameEast",
        "SM_ShipHub_Airlock_FrameWest",
        "SM_ShipHub_Airlock_Header",
    )
    members = [
        _object_member(bpy.data.objects[name])
        for name in required_names
        if bpy.data.objects.get(name) is not None
    ]
    west_frame = bpy.data.objects.get("SM_ShipHub_Airlock_FrameWest")
    east_frame = bpy.data.objects.get("SM_ShipHub_Airlock_FrameEast")
    header = bpy.data.objects.get("SM_ShipHub_Airlock_Header")
    west_wall = bpy.data.objects.get("SM_ShipHub_Wall_SouthWest")
    east_wall = bpy.data.objects.get("SM_ShipHub_Wall_SouthEast")
    floor = bpy.data.objects.get("SM_ShipHub_FloorSlab")
    if any(
        obj is None
        for obj in (west_frame, east_frame, header, west_wall, east_wall, floor)
    ):
        return {
            "count": 0,
            "names": [member["name"] for member in members],
            "members": members,
        }
    west_frame_bounds = sb.measured_bounds(west_frame)
    east_frame_bounds = sb.measured_bounds(east_frame)
    header_bounds = sb.measured_bounds(header)
    west_wall_bounds = sb.measured_bounds(west_wall)
    east_wall_bounds = sb.measured_bounds(east_wall)
    floor_bounds = sb.measured_bounds(floor)
    deck_z = floor_bounds["max"][2]
    frame_gap = east_frame_bounds["min"][0] - west_frame_bounds["max"][0]
    structural_gap = east_wall_bounds["min"][0] - west_wall_bounds["max"][0]
    opening_height = header_bounds["min"][2] - deck_z
    center_x = (
        west_frame_bounds["max"][0] + east_frame_bounds["min"][0]
    ) / 2.0
    center_y = (west_frame.location.y + east_frame.location.y) / 2.0
    return {
        "count": 1,
        "names": sorted(member["name"] for member in members),
        "center_m": _vector((center_x, center_y, deck_z)),
        "members": sorted(members, key=lambda member: member["name"]),
        "measured_frame_clear_opening_m": _vector((frame_gap, opening_height)),
        "measured_structural_clear_opening_m": _vector(
            (structural_gap, opening_height)
        ),
    }


def _measure_assemblies() -> dict:
    cryopod_members = [
        _cryopod_member(obj)
        for obj in sorted(
            (
                obj
                for obj in bpy.data.objects
                if obj.type == "MESH"
                and obj.get("projectrift_semantic_type") == "cryopod"
            ),
            key=lambda obj: obj.name,
        )
    ]
    dock_members = [
        _dock_member(obj)
        for obj in sorted(
            (
                obj
                for obj in bpy.data.objects
                if obj.type == "MESH"
                and obj.get("projectrift_semantic_type") == "construct_dock"
            ),
            key=lambda obj: obj.name,
        )
    ]
    table_members = [
        _object_member(obj)
        for obj in sorted(
            (
                obj
                for obj in bpy.data.objects
                if obj.type == "MESH"
                and str(obj.get("projectrift_semantic_type", "")).startswith(
                    "navigation_table_"
                )
            ),
            key=lambda obj: obj.name,
        )
    ]
    table_sectors = [
        member
        for member in table_members
        if member["name"].startswith("SM_ShipHub_NavTable_Sector_")
    ]
    ring_objects = sorted(
        (
            obj
            for obj in bpy.data.objects
            if obj.type == "MESH"
            and obj.get("projectrift_semantic_type") == "ceiling_service_ring"
        ),
        key=lambda obj: obj.name,
    )
    ring_members = [_ring_member(obj) for obj in ring_objects]
    semantic_reclines = [
        float(bpy.data.objects[member["name"]]["projectrift_recline_degrees"])
        for member in cryopod_members
    ]
    table_center = [0.0, 0.0, 0.0]
    if table_sectors:
        table_center = _vector(
            (
                sum(member["location_m"][0] for member in table_sectors)
                / len(table_sectors),
                sum(member["location_m"][1] for member in table_sectors)
                / len(table_sectors),
                min(
                    member["measured_world_bounds_m"]["min"][2]
                    for member in table_sectors
                ),
            )
        )
    return {
        "cryopods": {
            "count": len(cryopod_members),
            "names": [member["name"] for member in cryopod_members],
            "centers_m": [member["location_m"] for member in cryopod_members],
            "recline_degrees": _clean_float(
                sum(semantic_reclines) / len(semantic_reclines)
            )
            if semantic_reclines
            else 0.0,
            "members": cryopod_members,
        },
        "construct_docks": {
            "count": len(dock_members),
            "names": [member["name"] for member in dock_members],
            "centers_m": [member["location_m"] for member in dock_members],
            "members": dock_members,
        },
        "navigation_table": {
            "count": 1 if len(table_sectors) == 4 else 0,
            "sector_names": [member["name"] for member in table_sectors],
            "display_name": next(
                (
                    member["name"]
                    for member in table_members
                    if member["name"] == "SM_ShipHub_NavTable_Display"
                ),
                None,
            ),
            "center_m": table_center,
            "members": table_members,
        },
        "airlock": _airlock_measurement(),
        "ceiling_ring": {
            "count": len(ring_members),
            "names": [member["name"] for member in ring_members],
            "members": ring_members,
        },
    }


def _scene_snapshot() -> dict:
    collection_order = {name: index for index, name in enumerate(COLLECTION_NAMES)}
    all_meshes = sorted(
        (obj for obj in bpy.data.objects if obj.type == "MESH"),
        key=lambda obj: (collection_order[_object_collection_name(obj)], obj.name),
    )
    object_entries = []
    for obj in all_meshes:
        member = _object_member(obj)
        object_entries.append(
            {
                "name": obj.name,
                "collection": _object_collection_name(obj),
                "material": obj.data.materials[0].name,
                "semantic_type": obj.get("projectrift_semantic_type", "white_model"),
                "location_m": member["location_m"],
                "rotation_degrees": _vector(
                    math.degrees(value) for value in obj.rotation_euler
                ),
                "scale": _vector(obj.scale),
                "measured_world_bounds_m": member["measured_world_bounds_m"],
                "exported": False,
            }
        )
    return {
        "collections": [
            name for name in COLLECTION_NAMES if bpy.data.collections.get(name) is not None
        ],
        "materials": [
            name for name, _ in MATERIAL_DEFINITIONS if bpy.data.materials.get(name) is not None
        ],
        "objects": object_entries,
        "assemblies": _measure_assemblies(),
    }


def _validate_scene(layout: ShipHubLayout) -> list:
    issues = []
    if tuple(collection.name for collection in bpy.data.collections) != COLLECTION_NAMES:
        issues.append("Scene collections do not match the exact ten-name authority.")
    actual_material_names = {material.name for material in bpy.data.materials}
    expected_material_names = {name for name, _ in MATERIAL_DEFINITIONS}
    if (
        len(bpy.data.materials) != len(MATERIAL_DEFINITIONS)
        or actual_material_names != expected_material_names
    ):
        issues.append("Scene materials do not match the exact five-name authority.")
    if bpy.data.cameras or bpy.data.lights:
        issues.append("The white-model scene must not contain cameras or lights.")

    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    for obj in mesh_objects:
        if len(obj.data.materials) != 1:
            issues.append(f"Mesh must have exactly one ID material: {obj.name}")
        if any(abs(float(value) - 1.0) > 0.000001 for value in obj.scale):
            issues.append(f"Mesh has non-unit scale: {obj.name}")
        if _object_collection_name(obj) != "00_REFERENCE":
            bounds = sb.measured_bounds(obj)
            if bounds["max"][2] > layout.room.nominal_height_m + 0.000001:
                issues.append(f"Production mesh exceeds nominal Z 8.0: {obj.name}")

    assemblies = _measure_assemblies()
    expected_pods = tuple(
        f"SM_ShipHub_Cryopod_{index + 1:02d}"
        for index in range(layout.cryopod_spec.count)
    )
    actual_pods = tuple(assemblies["cryopods"]["names"])
    if actual_pods != expected_pods:
        issues.append("Scene cryopods do not match the five exact required names.")
    expected_pod_centers = {
        name: layout.cryopods[index].center_m
        for index, name in enumerate(expected_pods)
    }
    for member in assemblies["cryopods"]["members"]:
        name = member["name"]
        pod = bpy.data.objects.get(name)
        if pod is not None and any(
            abs(float(angle)) > 0.000001 for angle in pod.rotation_euler
        ):
            issues.append(f"Cryopod rotation is not baked: {name}")
        dimensions = member["measured_dimensions_m"]
        bounds = member["measured_world_bounds_m"]
        for axis, actual, expected in zip(
            "XYZ", dimensions, layout.cryopod_spec.bounds_m
        ):
            if abs(actual - expected) > 0.00001:
                issues.append(
                    f"Cryopod {name} measured {axis} dimension must be {expected}; got {actual}."
                )
        if bounds["min"][2] < -0.00001 or abs(bounds["min"][2]) > 0.00001:
            issues.append(f"Cryopod {name} must be grounded at Z 0.0.")
        if abs(member["measured_recline_degrees"] - layout.cryopod_spec.recline_degrees) > 0.00001:
            issues.append(
                f"Cryopod {name} measured recline must be {layout.cryopod_spec.recline_degrees}."
            )
        if member["leans_toward"] != "+Y":
            issues.append(f"Cryopod {name} must lean toward +Y.")
        if tuple(member["location_m"]) != tuple(expected_pod_centers.get(name, ())):
            issues.append(f"Cryopod {name} pivot location does not match typed layout.")

    expected_docks = tuple(
        f"SM_ShipHub_ConstructDock_{index + 1:02d}"
        for index in range(layout.construct_docks.count)
    )
    actual_docks = tuple(assemblies["construct_docks"]["names"])
    if actual_docks != expected_docks:
        issues.append("Scene construct docks do not match the four exact required names.")
    for index, member in enumerate(assemblies["construct_docks"]["members"]):
        if tuple(member["location_m"]) != tuple(layout.dock_centers_m[index]):
            issues.append(f"Construct dock location mismatch: {member['name']}")
        if abs(member["measured_diameter_m"] - layout.construct_docks.diameter_m) > 0.00001:
            issues.append(f"Construct dock diameter mismatch: {member['name']}")
        if abs(member["measured_recess_m"] - layout.construct_docks.recess_m) > 0.00001:
            issues.append(f"Construct dock recess mismatch: {member['name']}")
        if abs(member["measured_top_z_m"]) > 0.00001:
            issues.append(f"Construct dock top is not flush at Z 0: {member['name']}")

    expected_sectors = tuple(
        f"SM_ShipHub_NavTable_Sector_{index + 1:02d}" for index in range(4)
    )
    actual_sectors = tuple(assemblies["navigation_table"]["sector_names"])
    if actual_sectors != expected_sectors:
        issues.append("Navigation table must contain four exact quarter-sector meshes.")
    if assemblies["navigation_table"]["count"] != 1:
        issues.append("Navigation table semantic assembly count must be one.")
    if tuple(assemblies["navigation_table"]["center_m"]) != tuple(
        layout.navigation_table_center_m
    ):
        issues.append("Navigation table measured center does not match typed layout.")

    airlock_names = (
        "SM_ShipHub_Airlock_DoorPanel",
        "SM_ShipHub_Airlock_FrameEast",
        "SM_ShipHub_Airlock_FrameWest",
        "SM_ShipHub_Airlock_Header",
    )
    if any(bpy.data.objects.get(name) is None for name in airlock_names):
        issues.append("Airlock assembly is missing required frame or door objects.")
    airlock = assemblies["airlock"]
    if airlock["count"] != 1:
        issues.append("Airlock semantic assembly count must be one.")
    else:
        if tuple(airlock["center_m"]) != tuple(layout.airlock_center_m):
            issues.append("Airlock measured center does not match typed layout.")
        for measurement_name in (
            "measured_frame_clear_opening_m",
            "measured_structural_clear_opening_m",
        ):
            if tuple(airlock[measurement_name]) != tuple(layout.airlock_clear_opening_m):
                issues.append(
                    f"Airlock {measurement_name} does not match typed clear opening."
                )

    ring = assemblies["ceiling_ring"]
    if ring["count"] != 1:
        issues.append("Ceiling service ring semantic assembly count must be one.")
    else:
        ring_member = ring["members"][0]
        if abs(
            ring_member["measured_inner_radius_m"] * 2.0
            - layout.ceiling_ring.inner_diameter_m
        ) > 0.00001:
            issues.append("Ceiling service ring measured inner diameter mismatch.")
        if abs(
            ring_member["measured_outer_radius_m"] * 2.0
            - layout.ceiling_ring.outer_diameter_m
        ) > 0.00001:
            issues.append("Ceiling service ring measured outer diameter mismatch.")
        if tuple(ring_member["measured_z_range_m"]) != tuple(
            layout.ceiling_ring_z_range_m
        ):
            issues.append("Ceiling service ring measured Z range mismatch.")

    production = _production_objects()
    if not production:
        issues.append("Production export selection is empty.")
    return issues


def _select_for_export(objects: list) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def _manifest(
    layout: ShipHubLayout,
    brief_path: Path,
    scene_snapshot: dict,
    expected_export_names: list[str],
    fbx_readback: dict,
    glb_readback: dict,
) -> dict:
    actual_export_names = list(fbx_readback["actual_object_names"])
    actual_export_name_set = set(actual_export_names)
    for entry in scene_snapshot["objects"]:
        entry["exported"] = entry["name"] in actual_export_name_set
    return {
        "schema": "projectrift.shiphub.white-model-manifest.v1",
        "blender": {
            "version": bpy.app.version_string,
            "version_tuple": list(bpy.app.version),
            "lts": "LTS" in bpy.app.version_string,
        },
        "source_brief": {
            "relative_path": "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json",
            "sha256": hashlib.sha256(brief_path.read_bytes()).hexdigest(),
        },
        "units": {
            "blender": layout.units.blender,
            "unreal_centimeters_per_meter": layout.units.unreal_centimeters_per_meter,
        },
        "room": {
            "clear_dimensions_m": list(layout.clear_dimensions_m),
            "clear_bounds_m": {
                "min": [-layout.clear_dimensions_m[0] / 2.0, -layout.clear_dimensions_m[1] / 2.0, 0.0],
                "max": [layout.clear_dimensions_m[0] / 2.0, layout.clear_dimensions_m[1] / 2.0, layout.clear_dimensions_m[2]],
            },
            "nominal_height_m": layout.room.nominal_height_m,
        },
        "collections": scene_snapshot["collections"],
        "materials": scene_snapshot["materials"],
        "objects": scene_snapshot["objects"],
        "assemblies": scene_snapshot["assemblies"],
        "exports": {
            "expected_object_names": expected_export_names,
            "fbx": fbx_readback,
            "glb": glb_readback,
        },
        "sheet_ids": list(layout.sheet_ids),
    }


def _require_finished(result: set[str], operation: str) -> None:
    if "FINISHED" not in result:
        raise RuntimeError(f"{operation} did not return FINISHED: {sorted(result)}")


def _readback_export(path: Path, export_format: str) -> dict:
    sb.reset_scene()
    if export_format == "fbx":
        result = bpy.ops.import_scene.fbx(filepath=str(path))
    elif export_format == "glb":
        result = bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        raise ValueError(f"Unsupported export readback format: {export_format}")
    _require_finished(result, f"{export_format.upper()} readback import")
    mesh_objects = sorted(
        (obj for obj in bpy.data.objects if obj.type == "MESH"),
        key=lambda obj: obj.name,
    )
    actual_names = [obj.name for obj in mesh_objects]
    return {
        "object_count": len(actual_names),
        "objects": actual_names,
        "actual_object_names": actual_names,
        "actual_scales": [
            {"name": obj.name, "scale": _vector(obj.scale)} for obj in mesh_objects
        ],
    }


def _validate_export_readbacks(
    expected_names: list[str],
    fbx_readback: dict,
    glb_readback: dict,
) -> None:
    if not expected_names:
        raise ValueError("Expected production export object list is empty.")
    for export_format, readback in (("FBX", fbx_readback), ("GLB", glb_readback)):
        if readback["actual_object_names"] != expected_names:
            raise ValueError(
                f"{export_format} actual readback names do not match production selection."
            )
        if readback["object_count"] != len(expected_names):
            raise ValueError(f"{export_format} actual readback count mismatch.")
        non_unit = [
            entry["name"]
            for entry in readback["actual_scales"]
            if any(abs(value - 1.0) > 0.00001 for value in entry["scale"])
        ]
        if non_unit:
            raise ValueError(
                f"{export_format} readback contains non-unit object scales: {non_unit}"
            )
    if fbx_readback["actual_object_names"] != glb_readback["actual_object_names"]:
        raise ValueError("FBX and GLB actual readback object names differ.")


def _create_unique_work_directory(output_root: Path, prefix: str) -> Path:
    for attempt in range(1000):
        candidate = output_root / f".{prefix}-{os.getpid()}-{attempt:03d}"
        try:
            candidate.mkdir(parents=False, exist_ok=False)
            return candidate
        except FileExistsError:
            continue
    raise RuntimeError(f"Could not allocate unique {prefix} directory below {output_root}.")


def _validate_staged_artifacts(paths: dict[str, Path]) -> None:
    for label, path in paths.items():
        if not path.is_file() or path.stat().st_size <= 0:
            raise ValueError(f"Staged {label} artifact is missing or empty: {path}")
    blend_prefix = paths["blend"].read_bytes()[:7]
    if not (
        blend_prefix.startswith(b"BLENDER")
        or blend_prefix[:4] == bytes((0x28, 0xB5, 0x2F, 0xFD))
    ):
        raise ValueError("Staged BLEND signature is invalid.")
    if paths["fbx"].read_bytes()[:18] != b"Kaydara FBX Binary":
        raise ValueError("Staged FBX signature is invalid.")
    if paths["glb"].read_bytes()[:4] != b"glTF":
        raise ValueError("Staged GLB signature is invalid.")
    parsed_manifest = json.loads(paths["manifest"].read_text(encoding="utf-8"))
    if parsed_manifest.get("schema") != "projectrift.shiphub.white-model-manifest.v1":
        raise ValueError("Staged manifest schema is invalid.")


def _transactional_replace(
    staged_paths: dict[str, Path],
    final_paths: dict[str, Path],
    output_root: Path,
) -> None:
    backup_root = _create_unique_work_directory(output_root, "shiphub-backup")
    backups: list[tuple[Path, Path]] = []
    installed: list[Path] = []
    try:
        for label in ("blend", "fbx", "glb", "manifest"):
            final_path = final_paths[label]
            final_path.parent.mkdir(parents=True, exist_ok=True)
            if final_path.exists():
                backup_path = backup_root / label / final_path.name
                backup_path.parent.mkdir(parents=True, exist_ok=True)
                os.replace(final_path, backup_path)
                backups.append((final_path, backup_path))
        for label in ("blend", "fbx", "glb", "manifest"):
            final_path = final_paths[label]
            os.replace(staged_paths[label], final_path)
            installed.append(final_path)
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
                    os.replace(backup_path, final_path)
            except Exception as error:
                rollback_errors.append(f"restore {final_path}: {error}")
        if rollback_errors:
            raise RuntimeError(
                "Artifact transaction failed and rollback was incomplete: "
                + "; ".join(rollback_errors)
            ) from replace_error
        raise
    finally:
        shutil.rmtree(backup_root, ignore_errors=True)


def _write_manifest_atomic(path: Path, manifest: dict) -> None:
    temporary_path = path.with_name(f"{path.name}.tmp")
    try:
        with temporary_path.open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(manifest, handle, indent=2, sort_keys=True, ensure_ascii=False)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_path, path)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


def build(project_root: Path, brief_path: Path, output_root: Path) -> dict:
    del project_root
    _require_blender_52_lts()
    layout = build_layout(load_contract(brief_path))
    layout_issues = validate_layout(layout)
    if layout_issues:
        raise ValueError("Typed layout validation failed: " + "; ".join(layout_issues))

    _build_scene(layout)
    scene_issues = _validate_scene(layout)
    if scene_issues:
        raise ValueError("Actual Blender scene validation failed: " + "; ".join(scene_issues))
    production_objects = _production_objects()
    expected_export_names = sorted(obj.name for obj in production_objects)
    scene_snapshot = _scene_snapshot()

    output_root.mkdir(parents=True, exist_ok=True)
    staging_root = _create_unique_work_directory(output_root, "shiphub-staging")
    staged_paths = {
        "blend": staging_root / "Blender" / BLEND_FILENAME,
        "fbx": staging_root / "Exports" / FBX_FILENAME,
        "glb": staging_root / "Exports" / GLB_FILENAME,
        "manifest": staging_root / "Reports" / MANIFEST_FILENAME,
    }
    final_paths = {
        "blend": output_root / "Blender" / BLEND_FILENAME,
        "fbx": output_root / "Exports" / FBX_FILENAME,
        "glb": output_root / "Exports" / GLB_FILENAME,
        "manifest": output_root / "Reports" / MANIFEST_FILENAME,
    }
    try:
        for staged_path in staged_paths.values():
            staged_path.parent.mkdir(parents=True, exist_ok=True)

        save_result = bpy.ops.wm.save_as_mainfile(
            filepath=str(staged_paths["blend"]), check_existing=False
        )
        _require_finished(save_result, "BLEND staging save")

        _select_for_export(production_objects)
        fbx_result = bpy.ops.export_scene.fbx(
            filepath=str(staged_paths["fbx"]),
            check_existing=False,
            use_selection=True,
            object_types={"MESH"},
            global_scale=1.0,
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_UNITS",
            axis_forward="-Z",
            axis_up="Y",
            bake_anim=False,
            add_leaf_bones=False,
            use_mesh_modifiers=True,
            path_mode="AUTO",
        )
        _require_finished(fbx_result, "FBX staging export")

        _select_for_export(production_objects)
        glb_result = bpy.ops.export_scene.gltf(
            filepath=str(staged_paths["glb"]),
            check_existing=False,
            export_format="GLB",
            use_selection=True,
            export_apply=True,
            export_animations=False,
            export_cameras=False,
            export_lights=False,
        )
        _require_finished(glb_result, "GLB staging export")

        fbx_readback = _readback_export(staged_paths["fbx"], "fbx")
        glb_readback = _readback_export(staged_paths["glb"], "glb")
        _validate_export_readbacks(
            expected_export_names, fbx_readback, glb_readback
        )

        manifest = _manifest(
            layout,
            brief_path,
            scene_snapshot,
            expected_export_names,
            fbx_readback,
            glb_readback,
        )
        _write_manifest_atomic(staged_paths["manifest"], manifest)
        _validate_staged_artifacts(staged_paths)
        _transactional_replace(staged_paths, final_paths, output_root)
    finally:
        shutil.rmtree(staging_root, ignore_errors=True)

    summary = {
        "blender": bpy.app.version_string,
        "collections": len(scene_snapshot["collections"]),
        "materials": len(scene_snapshot["materials"]),
        "mesh_objects": len(scene_snapshot["objects"]),
        "export_objects": fbx_readback["object_count"],
        "transaction": "committed",
        "blend": str(final_paths["blend"]),
        "fbx": str(final_paths["fbx"]),
        "glb": str(final_paths["glb"]),
        "manifest": str(final_paths["manifest"]),
    }
    print("SHIPHUB_BUILD_SUMMARY=" + json.dumps(summary, sort_keys=True))
    return summary


def main(argv: list[str]) -> int:
    arguments = _parse_arguments(argv)
    project_root, brief_path, output_root = _validated_paths(arguments)
    build(project_root, brief_path, output_root)
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
