from __future__ import annotations

import math
from dataclasses import dataclass

try:
    from .shiphub_contract import (
        AirlockContract,
        AxesContract,
        CeilingRingContract,
        CharacterReferenceContract,
        CryopodContract,
        DockContract,
        NavigationTableContract,
        Point2,
        Point3,
        RoomContract,
        ShipHubContract,
        UnitsContract,
        validate_contract,
    )
except ImportError:
    from shiphub_contract import (
        AirlockContract,
        AxesContract,
        CeilingRingContract,
        CharacterReferenceContract,
        CryopodContract,
        DockContract,
        NavigationTableContract,
        Point2,
        Point3,
        RoomContract,
        ShipHubContract,
        UnitsContract,
        validate_contract,
    )


@dataclass(frozen=True)
class LayoutObject:
    object_id: str
    object_type: str
    center_m: Point3


@dataclass(frozen=True)
class CryopodPlacement:
    center_m: Point3
    recline_degrees: float


@dataclass(frozen=True)
class ShipHubLayout:
    schema: str
    units: UnitsContract
    axes: AxesContract
    room: RoomContract
    navigation_table: NavigationTableContract
    cryopod_spec: CryopodContract
    airlock: AirlockContract
    construct_docks: DockContract
    ceiling_ring: CeilingRingContract
    character_reference: CharacterReferenceContract
    deliverable_formats: tuple[str, ...]
    clear_dimensions_m: Point3
    inner_vertices_m: tuple[Point2, ...]
    cryopods: tuple[CryopodPlacement, ...]
    dock_centers_m: tuple[Point3, ...]
    navigation_table_center_m: Point3
    navigation_table_radius_m: float
    navigation_table_height_m: float
    main_path_min_width_m: float
    airlock_center_m: Point3
    airlock_clear_opening_m: Point2
    airlock_depth_m: float
    airlock_muster_area_m: Point2
    west_preparation_y_range_m: Point2
    west_repair_center_m: Point3
    east_medical_y_range_m: Point2
    east_role_y_range_m: Point2
    east_jump_core_center_m: Point3
    ceiling_ring_z_range_m: Point2
    ceiling_ring_diameters_m: Point2
    sheet_ids: tuple[str, ...]
    objects: tuple[LayoutObject, ...]

    def minimum_main_path_width_m(self) -> float:
        edge_distances = []
        table_center = (
            self.navigation_table_center_m[0],
            self.navigation_table_center_m[1],
        )
        for index, start in enumerate(self.inner_vertices_m):
            end = self.inner_vertices_m[(index + 1) % len(self.inner_vertices_m)]
            edge_distances.append(_segment_to_point_distance(start, end, table_center))
        return min(edge_distances) - self.navigation_table_radius_m


def _segment_to_point_distance(start: Point2, end: Point2, point: Point2) -> float:
    delta_x = end[0] - start[0]
    delta_y = end[1] - start[1]
    squared_length = delta_x * delta_x + delta_y * delta_y
    if squared_length == 0.0:
        raise ValueError("Polygon edge is a zero-length segment.")
    projection = (
        (point[0] - start[0]) * delta_x + (point[1] - start[1]) * delta_y
    ) / squared_length
    clamped_projection = min(1.0, max(0.0, projection))
    nearest_x = start[0] + clamped_projection * delta_x
    nearest_y = start[1] + clamped_projection * delta_y
    return math.hypot(nearest_x - point[0], nearest_y - point[1])


def build_layout(contract: ShipHubContract) -> ShipHubLayout:
    cryopods = tuple(
        CryopodPlacement(
            center_m=(center_x, contract.cryopods.base_y_m, 0.0),
            recline_degrees=contract.cryopods.recline_degrees,
        )
        for center_x in contract.cryopods.centers_x_m
    )
    objects = (
        LayoutObject(
            "navigation_table", "navigation_table", contract.navigation_table.center_m
        ),
        *(
            LayoutObject(f"cryopod_{index + 1}", "cryopod", pod.center_m)
            for index, pod in enumerate(cryopods)
        ),
        *(
            LayoutObject(f"construct_dock_{index + 1}", "construct_dock", center)
            for index, center in enumerate(contract.docks.centers_m)
        ),
    )
    return ShipHubLayout(
        schema=contract.schema,
        units=contract.units,
        axes=contract.axes,
        room=contract.room,
        navigation_table=contract.navigation_table,
        cryopod_spec=contract.cryopods,
        airlock=contract.airlock,
        construct_docks=contract.docks,
        ceiling_ring=contract.ceiling_ring,
        character_reference=contract.character_reference,
        deliverable_formats=contract.deliverable_formats,
        clear_dimensions_m=contract.room.clear_dimensions_m,
        inner_vertices_m=contract.room.inner_vertices_m,
        cryopods=cryopods,
        dock_centers_m=contract.docks.centers_m,
        navigation_table_center_m=contract.navigation_table.center_m,
        navigation_table_radius_m=contract.navigation_table.diameter_m / 2.0,
        navigation_table_height_m=contract.navigation_table.height_m,
        main_path_min_width_m=contract.room.main_path_min_width_m,
        airlock_center_m=contract.airlock.center_m,
        airlock_clear_opening_m=contract.airlock.clear_opening_m,
        airlock_depth_m=contract.airlock.depth_m,
        airlock_muster_area_m=contract.airlock.muster_area_m,
        west_preparation_y_range_m=contract.west_preparation_y_range_m,
        west_repair_center_m=contract.west_repair_center_m,
        east_medical_y_range_m=contract.east_medical_y_range_m,
        east_role_y_range_m=contract.east_role_y_range_m,
        east_jump_core_center_m=contract.east_jump_core_center_m,
        ceiling_ring_z_range_m=(
            contract.ceiling_ring.lowest_z_m,
            contract.ceiling_ring.highest_z_m,
        ),
        ceiling_ring_diameters_m=(
            contract.ceiling_ring.inner_diameter_m,
            contract.ceiling_ring.outer_diameter_m,
        ),
        sheet_ids=contract.sheet_ids,
        objects=objects,
    )


def validate_layout(layout: ShipHubLayout) -> list[str]:
    layout_contract = ShipHubContract(
        schema=layout.schema,
        units=layout.units,
        axes=layout.axes,
        room=layout.room,
        navigation_table=layout.navigation_table,
        cryopods=layout.cryopod_spec,
        airlock=layout.airlock,
        docks=layout.construct_docks,
        ceiling_ring=layout.ceiling_ring,
        character_reference=layout.character_reference,
        west_preparation_y_range_m=layout.west_preparation_y_range_m,
        west_repair_center_m=layout.west_repair_center_m,
        east_medical_y_range_m=layout.east_medical_y_range_m,
        east_role_y_range_m=layout.east_role_y_range_m,
        east_jump_core_center_m=layout.east_jump_core_center_m,
        sheet_ids=layout.sheet_ids,
        deliverable_formats=layout.deliverable_formats,
    )
    issues: list[str] = validate_contract(layout_contract)
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
    if (
        layout.clear_dimensions_m != (28.0, 24.0, 7.0)
        or layout.inner_vertices_m != expected_vertices
    ):
        issues.append("Room.ClearWidthM/ClearDepthM/ClearHeightM and Room.InnerVerticesM must match the frozen layout.")

    has_zero_length_segment = any(
        start == layout.inner_vertices_m[(index + 1) % len(layout.inner_vertices_m)]
        for index, start in enumerate(layout.inner_vertices_m)
    )
    if has_zero_length_segment:
        issues.append("Room inner vertices contain a zero-length segment.")

    expected_pod_centers = (
        (-4.0, 9.8, 0.0),
        (-2.0, 9.8, 0.0),
        (0.0, 9.8, 0.0),
        (2.0, 9.8, 0.0),
        (4.0, 9.8, 0.0),
    )
    if (
        tuple(pod.center_m for pod in layout.cryopods) != expected_pod_centers
        or any(pod.recline_degrees != 18.0 for pod in layout.cryopods)
    ):
        issues.append("Cryopods.Count, Cryopods.CentersXM/BaseYM, and Cryopods.ReclineDegrees must match the frozen layout.")

    expected_docks = (
        (-5.3, -5.3, 0.0),
        (5.3, -5.3, 0.0),
        (-5.3, 5.3, 0.0),
        (5.3, 5.3, 0.0),
    )
    if layout.dock_centers_m != expected_docks:
        issues.append("ConstructDocks.CentersM must match the four frozen flush docks.")

    if (
        layout.navigation_table_center_m != (0.0, 0.0, 0.0)
        or layout.navigation_table_radius_m != 4.0
        or layout.navigation_table_height_m != 1.1
    ):
        issues.append("NavigationTable.CenterM, DiameterM, and HeightM (navigation table) must match the frozen layout.")

    if len(layout.inner_vertices_m) >= 2 and not has_zero_length_segment:
        if layout.minimum_main_path_width_m() < layout.main_path_min_width_m:
            issues.append("Navigation-table clearance is below the contract minimum.")

    if (
        layout.airlock_clear_opening_m != (4.0, 3.5)
        or layout.airlock_muster_area_m != (8.0, 5.0)
    ):
        issues.append("Airlock.ClearOpeningM and Airlock.MusterAreaM must match the frozen layout.")

    if (
        layout.west_preparation_y_range_m != (-8.0, 0.0)
        or layout.west_repair_center_m != (-14.0, 6.0, 1.4)
        or layout.east_medical_y_range_m != (-8.0, -4.0)
        or layout.east_role_y_range_m != (-4.0, -2.0)
        or layout.east_jump_core_center_m != (14.0, 6.0, 3.0)
    ):
        issues.append("WestBays/EastBays layout values must match the frozen layout.")

    if (
        layout.ceiling_ring_z_range_m != (6.2, 8.0)
        or layout.ceiling_ring_diameters_m != (10.0, 16.0)
    ):
        issues.append("CeilingRing.LowestZM/HighestZM and diameters (ceiling ring) must match the frozen layout.")

    if len(layout.sheet_ids) != 15:
        issues.append("Sheet list must contain exactly 15 IDs.")
    if len(set(layout.sheet_ids)) != len(layout.sheet_ids):
        issues.append("Sheet IDs must be unique.")
    return issues
