from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path


Point2 = tuple[float, float]
Point3 = tuple[float, float, float]

_REQUIRED_PATHS = (
    "Units.Blender",
    "Units.UnrealCentimetersPerMeter",
    "Units.SnapM",
    "Units.StructuralBayM",
    "Axes.East",
    "Axes.NorthPods",
    "Axes.Up",
    "Axes.Origin",
    "Room.ClearWidthM",
    "Room.ClearDepthM",
    "Room.ClearHeightM",
    "Room.NominalHeightM",
    "Room.FloorThicknessM",
    "Room.WallThicknessM",
    "Room.InnerVerticesM",
    "Room.MainPathMinWidthM",
    "NavigationTable.CenterM",
    "NavigationTable.DiameterM",
    "NavigationTable.HeightM",
    "NavigationTable.DisplayDiameterM",
    "Cryopods.Count",
    "Cryopods.CentersXM",
    "Cryopods.BaseYM",
    "Cryopods.BoundsM",
    "Cryopods.ReclineDegrees",
    "Cryopods.DoorOpenDegrees",
    "Cryopods.DoorEnvelopeSouthM",
    "Cryopods.InteractionClearanceM",
    "Cryopods.ExpansionInterfaceWidthM",
    "Airlock.CenterM",
    "Airlock.ClearOpeningM",
    "Airlock.DepthM",
    "Airlock.MusterAreaM",
    "WestBays.PreparationYRangeM",
    "WestBays.RepairCenterM",
    "EastBays.MedicalYRangeM",
    "EastBays.RoleYRangeM",
    "EastBays.JumpCoreCenterM",
    "ConstructDocks.Count",
    "ConstructDocks.DiameterM",
    "ConstructDocks.RecessM",
    "ConstructDocks.CentersM",
    "CeilingRing.InnerDiameterM",
    "CeilingRing.OuterDiameterM",
    "CeilingRing.LowestZM",
    "CeilingRing.HighestZM",
    "CeilingRing.Walkable",
    "CharacterReference.CapsuleRadiusM",
    "CharacterReference.CapsuleHalfHeightM",
    "CharacterReference.CameraArmM",
    "Deliverables.Sheets",
    "Deliverables.Formats",
)


@dataclass(frozen=True)
class UnitsContract:
    blender: str
    unreal_centimeters_per_meter: int
    snap_m: float
    structural_bay_m: float


@dataclass(frozen=True)
class AxesContract:
    east: str
    north_pods: str
    up: str
    origin: str


@dataclass(frozen=True)
class RoomContract:
    clear_dimensions_m: Point3
    nominal_height_m: float
    floor_thickness_m: float
    wall_thickness_m: float
    inner_vertices_m: tuple[Point2, ...]
    main_path_min_width_m: float


@dataclass(frozen=True)
class NavigationTableContract:
    center_m: Point3
    diameter_m: float
    height_m: float
    display_diameter_m: float


@dataclass(frozen=True)
class CryopodContract:
    count: int
    centers_x_m: tuple[float, ...]
    base_y_m: float
    bounds_m: Point3
    recline_degrees: float
    door_open_degrees: float
    door_envelope_south_m: float
    interaction_clearance_m: Point2
    expansion_interface_width_m: float


@dataclass(frozen=True)
class AirlockContract:
    center_m: Point3
    clear_opening_m: Point2
    depth_m: float
    muster_area_m: Point2


@dataclass(frozen=True)
class DockContract:
    count: int
    diameter_m: float
    recess_m: float
    centers_m: tuple[Point3, ...]


@dataclass(frozen=True)
class CeilingRingContract:
    inner_diameter_m: float
    outer_diameter_m: float
    lowest_z_m: float
    highest_z_m: float
    walkable: bool


@dataclass(frozen=True)
class CharacterReferenceContract:
    capsule_radius_m: float
    capsule_half_height_m: float
    camera_arm_m: float


@dataclass(frozen=True)
class ShipHubContract:
    schema: str
    units: UnitsContract
    axes: AxesContract
    room: RoomContract
    navigation_table: NavigationTableContract
    cryopods: CryopodContract
    airlock: AirlockContract
    docks: DockContract
    ceiling_ring: CeilingRingContract
    character_reference: CharacterReferenceContract
    west_preparation_y_range_m: Point2
    west_repair_center_m: Point3
    east_medical_y_range_m: Point2
    east_role_y_range_m: Point2
    east_jump_core_center_m: Point3
    sheet_ids: tuple[str, ...]
    deliverable_formats: tuple[str, ...]


def _number(value: object, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{path} must be a finite number.")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{path} must be a finite number.")
    return result


def _integer(value: object, path: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{path} must be a finite integer.")
    return value


def _array(values: object, path: str) -> list[object]:
    if not isinstance(values, list):
        raise ValueError(f"{path} must be an array.")
    return values


def _boolean(value: object, path: str) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{path} must be a boolean.")
    return value


def _string(value: object, path: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{path} must be a string.")
    return value


def _string_tuple(values: object, path: str) -> tuple[str, ...]:
    result = []
    for index, value in enumerate(_array(values, path)):
        if not isinstance(value, str):
            raise ValueError(f"{path}[{index}] must be a string.")
        result.append(value)
    return tuple(result)


def _numeric_tuple(values: object, length: int, path: str) -> tuple[float, ...]:
    array = _array(values, path)
    if len(array) != length:
        raise ValueError(f"{path} must contain exactly {length} finite numbers.")
    return tuple(
        _number(value, f"{path}[{index}]") for index, value in enumerate(array)
    )


def _point2(values: object, path: str) -> Point2:
    result = _numeric_tuple(values, 2, path)
    return (result[0], result[1])


def _point3(values: object, path: str) -> Point3:
    result = _numeric_tuple(values, 3, path)
    return (result[0], result[1], result[2])


def _require_paths(data: dict[str, object]) -> None:
    if "Schema" not in data:
        raise ValueError("Missing required key: Schema")
    for dotted_path in _REQUIRED_PATHS:
        current: object = data
        for key in dotted_path.split("."):
            if not isinstance(current, dict) or key not in current:
                raise ValueError(f"Missing required key: {dotted_path}")
            current = current[key]


def load_contract(path: Path) -> ShipHubContract:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Root JSON must be an object.")
    _require_paths(data)
    if data.get("Schema") != "projectrift.shiphub.complete-design.v1":
        raise ValueError(
            "Schema must be 'projectrift.shiphub.complete-design.v1'."
        )
    units = data["Units"]
    axes = data["Axes"]
    room = data["Room"]
    table = data["NavigationTable"]
    cryopods = data["Cryopods"]
    airlock = data["Airlock"]
    west_bays = data["WestBays"]
    east_bays = data["EastBays"]
    docks = data["ConstructDocks"]
    ring = data["CeilingRing"]
    character_reference = data["CharacterReference"]
    deliverables = data["Deliverables"]

    contract = ShipHubContract(
        schema=str(data["Schema"]),
        units=UnitsContract(
            blender=_string(units["Blender"], "Units.Blender"),
            unreal_centimeters_per_meter=_integer(
                units["UnrealCentimetersPerMeter"],
                "Units.UnrealCentimetersPerMeter",
            ),
            snap_m=_number(units["SnapM"], "Units.SnapM"),
            structural_bay_m=_number(units["StructuralBayM"], "Units.StructuralBayM"),
        ),
        axes=AxesContract(
            east=_string(axes["East"], "Axes.East"),
            north_pods=_string(axes["NorthPods"], "Axes.NorthPods"),
            up=_string(axes["Up"], "Axes.Up"),
            origin=_string(axes["Origin"], "Axes.Origin"),
        ),
        room=RoomContract(
            clear_dimensions_m=(
                _number(room["ClearWidthM"], "Room.ClearWidthM"),
                _number(room["ClearDepthM"], "Room.ClearDepthM"),
                _number(room["ClearHeightM"], "Room.ClearHeightM"),
            ),
            nominal_height_m=_number(room["NominalHeightM"], "Room.NominalHeightM"),
            floor_thickness_m=_number(room["FloorThicknessM"], "Room.FloorThicknessM"),
            wall_thickness_m=_number(room["WallThicknessM"], "Room.WallThicknessM"),
            inner_vertices_m=tuple(
                _point2(vertex, f"Room.InnerVerticesM[{index}]")
                for index, vertex in enumerate(
                    _array(room["InnerVerticesM"], "Room.InnerVerticesM")
                )
            ),
            main_path_min_width_m=_number(
                room["MainPathMinWidthM"], "Room.MainPathMinWidthM"
            ),
        ),
        navigation_table=NavigationTableContract(
            center_m=_point3(table["CenterM"], "NavigationTable.CenterM"),
            diameter_m=_number(table["DiameterM"], "NavigationTable.DiameterM"),
            height_m=_number(table["HeightM"], "NavigationTable.HeightM"),
            display_diameter_m=_number(
                table["DisplayDiameterM"], "NavigationTable.DisplayDiameterM"
            ),
        ),
        cryopods=CryopodContract(
            count=_integer(cryopods["Count"], "Cryopods.Count"),
            centers_x_m=tuple(
                _number(value, f"Cryopods.CentersXM[{index}]")
                for index, value in enumerate(
                    _array(cryopods["CentersXM"], "Cryopods.CentersXM")
                )
            ),
            base_y_m=_number(cryopods["BaseYM"], "Cryopods.BaseYM"),
            bounds_m=_point3(cryopods["BoundsM"], "Cryopods.BoundsM"),
            recline_degrees=_number(
                cryopods["ReclineDegrees"], "Cryopods.ReclineDegrees"
            ),
            door_open_degrees=_number(
                cryopods["DoorOpenDegrees"], "Cryopods.DoorOpenDegrees"
            ),
            door_envelope_south_m=_number(
                cryopods["DoorEnvelopeSouthM"], "Cryopods.DoorEnvelopeSouthM"
            ),
            interaction_clearance_m=_point2(
                cryopods["InteractionClearanceM"],
                "Cryopods.InteractionClearanceM",
            ),
            expansion_interface_width_m=_number(
                cryopods["ExpansionInterfaceWidthM"],
                "Cryopods.ExpansionInterfaceWidthM",
            ),
        ),
        airlock=AirlockContract(
            center_m=_point3(airlock["CenterM"], "Airlock.CenterM"),
            clear_opening_m=_point2(airlock["ClearOpeningM"], "Airlock.ClearOpeningM"),
            depth_m=_number(airlock["DepthM"], "Airlock.DepthM"),
            muster_area_m=_point2(airlock["MusterAreaM"], "Airlock.MusterAreaM"),
        ),
        docks=DockContract(
            count=_integer(docks["Count"], "ConstructDocks.Count"),
            diameter_m=_number(docks["DiameterM"], "ConstructDocks.DiameterM"),
            recess_m=_number(docks["RecessM"], "ConstructDocks.RecessM"),
            centers_m=tuple(
                _point3(center, f"ConstructDocks.CentersM[{index}]")
                for index, center in enumerate(
                    _array(docks["CentersM"], "ConstructDocks.CentersM")
                )
            ),
        ),
        ceiling_ring=CeilingRingContract(
            inner_diameter_m=_number(ring["InnerDiameterM"], "CeilingRing.InnerDiameterM"),
            outer_diameter_m=_number(ring["OuterDiameterM"], "CeilingRing.OuterDiameterM"),
            lowest_z_m=_number(ring["LowestZM"], "CeilingRing.LowestZM"),
            highest_z_m=_number(ring["HighestZM"], "CeilingRing.HighestZM"),
            walkable=_boolean(ring["Walkable"], "CeilingRing.Walkable"),
        ),
        character_reference=CharacterReferenceContract(
            capsule_radius_m=_number(
                character_reference["CapsuleRadiusM"],
                "CharacterReference.CapsuleRadiusM",
            ),
            capsule_half_height_m=_number(
                character_reference["CapsuleHalfHeightM"],
                "CharacterReference.CapsuleHalfHeightM",
            ),
            camera_arm_m=_number(
                character_reference["CameraArmM"],
                "CharacterReference.CameraArmM",
            ),
        ),
        west_preparation_y_range_m=_point2(
            west_bays["PreparationYRangeM"], "WestBays.PreparationYRangeM"
        ),
        west_repair_center_m=_point3(
            west_bays["RepairCenterM"], "WestBays.RepairCenterM"
        ),
        east_medical_y_range_m=_point2(
            east_bays["MedicalYRangeM"], "EastBays.MedicalYRangeM"
        ),
        east_role_y_range_m=_point2(
            east_bays["RoleYRangeM"], "EastBays.RoleYRangeM"
        ),
        east_jump_core_center_m=_point3(
            east_bays["JumpCoreCenterM"], "EastBays.JumpCoreCenterM"
        ),
        sheet_ids=_string_tuple(
            deliverables["Sheets"],
            "Deliverables.Sheets",
        ),
        deliverable_formats=_string_tuple(
            deliverables["Formats"],
            "Deliverables.Formats",
        ),
    )
    issues = validate_contract(contract)
    if issues:
        raise ValueError("; ".join(issues))
    return contract


def validate_contract(contract: ShipHubContract) -> list[str]:
    issues: list[str] = []
    expected_vertices = (
        (-10.0, -12.0),
        (10.0, -12.0),
        (14.0, -8.0),
        (14.0, 8.0),
        (10.0, 12.0),
        (-10.0, 12.0),
        (-14.0, 8.0),
        (-14.0, -8.0),
    )
    expected_dock_centers = (
        (-5.3, -5.3, 0.0),
        (5.3, -5.3, 0.0),
        (-5.3, 5.3, 0.0),
        (5.3, 5.3, 0.0),
    )
    expected_sheets = (
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
    expected_formats = ("blend", "fbx", "glb", "pdf", "png", "svg", "json", "txt")
    exact_values = (
        ("Schema", contract.schema, "projectrift.shiphub.complete-design.v1"),
        ("Units.Blender", contract.units.blender, "m"),
        ("Units.UnrealCentimetersPerMeter", contract.units.unreal_centimeters_per_meter, 100),
        ("Units.SnapM", contract.units.snap_m, 0.5),
        ("Units.StructuralBayM", contract.units.structural_bay_m, 4.0),
        ("Axes.East", contract.axes.east, "+X"),
        ("Axes.NorthPods", contract.axes.north_pods, "+Y"),
        ("Axes.Up", contract.axes.up, "+Z"),
        ("Axes.Origin", contract.axes.origin, "Deck center"),
        ("Room.ClearWidthM", contract.room.clear_dimensions_m[0], 28.0),
        ("Room.ClearDepthM", contract.room.clear_dimensions_m[1], 24.0),
        ("Room.ClearHeightM", contract.room.clear_dimensions_m[2], 7.0),
        ("Room.NominalHeightM", contract.room.nominal_height_m, 8.0),
        ("Room.FloorThicknessM", contract.room.floor_thickness_m, 0.4),
        ("Room.WallThicknessM", contract.room.wall_thickness_m, 0.4),
        ("Room.InnerVerticesM", contract.room.inner_vertices_m, expected_vertices),
        ("Room.MainPathMinWidthM", contract.room.main_path_min_width_m, 5.0),
        ("NavigationTable.CenterM", contract.navigation_table.center_m, (0.0, 0.0, 0.0)),
        ("NavigationTable.DiameterM", contract.navigation_table.diameter_m, 8.0),
        ("NavigationTable.HeightM", contract.navigation_table.height_m, 1.1),
        ("NavigationTable.DisplayDiameterM", contract.navigation_table.display_diameter_m, 6.0),
        ("Cryopods.Count", contract.cryopods.count, 5),
        ("Cryopods.CentersXM", contract.cryopods.centers_x_m, (-4.0, -2.0, 0.0, 2.0, 4.0)),
        ("Cryopods.BaseYM", contract.cryopods.base_y_m, 9.8),
        ("Cryopods.BoundsM", contract.cryopods.bounds_m, (1.6, 1.6, 3.0)),
        ("Cryopods.ReclineDegrees", contract.cryopods.recline_degrees, 18.0),
        ("Cryopods.DoorOpenDegrees", contract.cryopods.door_open_degrees, 75.0),
        ("Cryopods.DoorEnvelopeSouthM", contract.cryopods.door_envelope_south_m, 1.2),
        ("Cryopods.InteractionClearanceM", contract.cryopods.interaction_clearance_m, (2.0, 2.5)),
        ("Cryopods.ExpansionInterfaceWidthM", contract.cryopods.expansion_interface_width_m, 2.0),
        ("Airlock.CenterM", contract.airlock.center_m, (0.0, -12.0, 0.0)),
        ("Airlock.ClearOpeningM", contract.airlock.clear_opening_m, (4.0, 3.5)),
        ("Airlock.DepthM", contract.airlock.depth_m, 1.2),
        ("Airlock.MusterAreaM", contract.airlock.muster_area_m, (8.0, 5.0)),
        ("WestBays.PreparationYRangeM", contract.west_preparation_y_range_m, (-8.0, 0.0)),
        ("WestBays.RepairCenterM", contract.west_repair_center_m, (-14.0, 6.0, 1.4)),
        ("EastBays.MedicalYRangeM", contract.east_medical_y_range_m, (-8.0, -4.0)),
        ("EastBays.RoleYRangeM", contract.east_role_y_range_m, (-4.0, -2.0)),
        ("EastBays.JumpCoreCenterM", contract.east_jump_core_center_m, (14.0, 6.0, 3.0)),
        ("ConstructDocks.Count", contract.docks.count, 4),
        ("ConstructDocks.DiameterM", contract.docks.diameter_m, 1.0),
        ("ConstructDocks.RecessM", contract.docks.recess_m, 0.08),
        ("ConstructDocks.CentersM", contract.docks.centers_m, expected_dock_centers),
        ("CeilingRing.InnerDiameterM", contract.ceiling_ring.inner_diameter_m, 10.0),
        ("CeilingRing.OuterDiameterM", contract.ceiling_ring.outer_diameter_m, 16.0),
        ("CeilingRing.LowestZM", contract.ceiling_ring.lowest_z_m, 6.2),
        ("CeilingRing.HighestZM", contract.ceiling_ring.highest_z_m, 8.0),
        ("CeilingRing.Walkable", contract.ceiling_ring.walkable, False),
        ("CharacterReference.CapsuleRadiusM", contract.character_reference.capsule_radius_m, 0.42),
        ("CharacterReference.CapsuleHalfHeightM", contract.character_reference.capsule_half_height_m, 0.96),
        ("CharacterReference.CameraArmM", contract.character_reference.camera_arm_m, 4.0),
    )
    for path, actual, expected in exact_values:
        if actual != expected:
            category_label = ""
            if path.startswith("NavigationTable."):
                category_label = " (navigation table)"
            elif path.startswith("CeilingRing."):
                category_label = " (ceiling ring)"
            issues.append(f"{path}{category_label} must be exactly {expected!r}.")

    if contract.sheet_ids != expected_sheets:
        issues.append("Deliverables.Sheets must match the exact ordered 15 sheet IDs.")
    if contract.deliverable_formats != expected_formats:
        issues.append(
            "Deliverables.Formats must be exactly ordered as "
            "('blend', 'fbx', 'glb', 'pdf', 'png', 'svg', 'json', 'txt')."
        )

    for index, start in enumerate(contract.room.inner_vertices_m):
        end = contract.room.inner_vertices_m[
            (index + 1) % len(contract.room.inner_vertices_m)
        ]
        if start == end:
            issues.append(
                f"Room.InnerVerticesM contains a zero-length segment at edge {index}."
            )
            break
    if contract.cryopods.count != len(contract.cryopods.centers_x_m):
        issues.append("Cryopods.Count must match Cryopods.CentersXM length.")
    if contract.docks.count != len(contract.docks.centers_m):
        issues.append("ConstructDocks.Count must match ConstructDocks.CentersM length.")
    if len(contract.sheet_ids) != 15:
        issues.append("Deliverables.Sheets must contain exactly 15 sheet IDs.")
    if len(set(contract.sheet_ids)) != len(contract.sheet_ids):
        issues.append("Deliverables.Sheets must not contain duplicate sheet IDs.")
    return issues


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate the ProjectRift ship-hub contract."
    )
    parser.add_argument("--brief", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    arguments = parser.parse_args(argv)
    try:
        contract = load_contract(arguments.brief)
        if __package__:
            from .shiphub_layout import build_layout, validate_layout
        else:
            from shiphub_layout import build_layout, validate_layout
        layout = build_layout(contract)
        issues = validate_layout(layout)
        if issues:
            raise ValueError("; ".join(issues))
        preview = {
            "contract": {
                "schema": contract.schema,
                "units": {
                    "blender": layout.units.blender,
                    "unreal_centimeters_per_meter": layout.units.unreal_centimeters_per_meter,
                    "snap_m": layout.units.snap_m,
                    "structural_bay_m": layout.units.structural_bay_m,
                },
                "axes": {
                    "east": layout.axes.east,
                    "north_pods": layout.axes.north_pods,
                    "up": layout.axes.up,
                    "origin": layout.axes.origin,
                },
                "room": {
                    "clear_dimensions_m": layout.room.clear_dimensions_m,
                    "nominal_height_m": layout.room.nominal_height_m,
                    "floor_thickness_m": layout.room.floor_thickness_m,
                    "wall_thickness_m": layout.room.wall_thickness_m,
                    "inner_vertices_m": layout.room.inner_vertices_m,
                    "main_path_min_width_m": layout.room.main_path_min_width_m,
                },
                "navigation_table": {
                    "center_m": layout.navigation_table.center_m,
                    "diameter_m": layout.navigation_table.diameter_m,
                    "height_m": layout.navigation_table.height_m,
                    "display_diameter_m": layout.navigation_table.display_diameter_m,
                },
                "cryopods": {
                    "count": layout.cryopod_spec.count,
                    "centers_x_m": layout.cryopod_spec.centers_x_m,
                    "base_y_m": layout.cryopod_spec.base_y_m,
                    "bounds_m": layout.cryopod_spec.bounds_m,
                    "recline_degrees": layout.cryopod_spec.recline_degrees,
                    "door_open_degrees": layout.cryopod_spec.door_open_degrees,
                    "door_envelope_south_m": layout.cryopod_spec.door_envelope_south_m,
                    "interaction_clearance_m": layout.cryopod_spec.interaction_clearance_m,
                    "expansion_interface_width_m": layout.cryopod_spec.expansion_interface_width_m,
                },
                "airlock": {
                    "center_m": layout.airlock.center_m,
                    "clear_opening_m": layout.airlock.clear_opening_m,
                    "depth_m": layout.airlock.depth_m,
                    "muster_area_m": layout.airlock.muster_area_m,
                },
                "west_bays": {
                    "preparation_y_range_m": layout.west_preparation_y_range_m,
                    "repair_center_m": layout.west_repair_center_m,
                },
                "east_bays": {
                    "medical_y_range_m": layout.east_medical_y_range_m,
                    "role_y_range_m": layout.east_role_y_range_m,
                    "jump_core_center_m": layout.east_jump_core_center_m,
                },
                "construct_docks": {
                    "count": layout.construct_docks.count,
                    "diameter_m": layout.construct_docks.diameter_m,
                    "recess_m": layout.construct_docks.recess_m,
                    "centers_m": layout.construct_docks.centers_m,
                },
                "ceiling_ring": {
                    "inner_diameter_m": layout.ceiling_ring.inner_diameter_m,
                    "outer_diameter_m": layout.ceiling_ring.outer_diameter_m,
                    "lowest_z_m": layout.ceiling_ring.lowest_z_m,
                    "highest_z_m": layout.ceiling_ring.highest_z_m,
                    "walkable": layout.ceiling_ring.walkable,
                },
                "character_reference": {
                    "capsule_radius_m": layout.character_reference.capsule_radius_m,
                    "capsule_half_height_m": layout.character_reference.capsule_half_height_m,
                    "camera_arm_m": layout.character_reference.camera_arm_m,
                },
                "deliverables": {
                    "sheets": layout.sheet_ids,
                    "formats": layout.deliverable_formats,
                },
            },
            "clear_dimensions_m": layout.clear_dimensions_m,
            "inner_vertices_m": layout.inner_vertices_m,
            "cryopods": [
                {
                    "center_m": pod.center_m,
                    "recline_degrees": pod.recline_degrees,
                }
                for pod in layout.cryopods
            ],
            "dock_centers_m": layout.dock_centers_m,
            "navigation_table": {
                "center_m": layout.navigation_table_center_m,
                "radius_m": layout.navigation_table_radius_m,
                "height_m": layout.navigation_table_height_m,
            },
            "minimum_main_path_width_m": layout.minimum_main_path_width_m(),
            "airlock": {
                "center_m": layout.airlock_center_m,
                "clear_opening_m": layout.airlock_clear_opening_m,
                "depth_m": layout.airlock_depth_m,
                "muster_area_m": layout.airlock_muster_area_m,
            },
            "west_bays": {
                "preparation_y_range_m": layout.west_preparation_y_range_m,
                "repair_center_m": layout.west_repair_center_m,
            },
            "east_bays": {
                "medical_y_range_m": layout.east_medical_y_range_m,
                "role_y_range_m": layout.east_role_y_range_m,
                "jump_core_center_m": layout.east_jump_core_center_m,
            },
            "ceiling_ring": {
                "z_range_m": layout.ceiling_ring_z_range_m,
                "diameters_m": layout.ceiling_ring_diameters_m,
            },
            "objects": [
                {
                    "object_id": layout_object.object_id,
                    "object_type": layout_object.object_type,
                    "center_m": layout_object.center_m,
                }
                for layout_object in layout.objects
            ],
            "sheet_ids": layout.sheet_ids,
        }
        arguments.out.parent.mkdir(parents=True, exist_ok=True)
        arguments.out.write_text(
            json.dumps(preview, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"Ship-hub contract validation failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
