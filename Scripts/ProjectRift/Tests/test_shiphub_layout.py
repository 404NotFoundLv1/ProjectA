import json
import tempfile
import unittest
from contextlib import redirect_stderr
from dataclasses import replace
from io import StringIO
from pathlib import Path

from Scripts.ProjectRift.ArtPipeline.shiphub.shiphub_contract import load_contract, main
from Scripts.ProjectRift.ArtPipeline.shiphub.shiphub_layout import (
    build_layout,
    validate_layout,
)


PROJECT_ROOT = Path(__file__).resolve().parents[3]
BRIEF_PATH = (
    PROJECT_ROOT
    / "SourceArt"
    / "ProjectRift"
    / "ShipHub"
    / "Briefs"
    / "ShipHubCompleteDesign_v1.json"
)


class ShipHubLayoutTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_contract(BRIEF_PATH)
        self.layout = build_layout(self.contract)

    @staticmethod
    def _mutated_brief(dotted_path: str, value: object) -> dict[str, object]:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        section, key = dotted_path.split(".")
        data[section][key] = value
        return data

    def test_real_contract_preserves_every_frozen_authority_category(self) -> None:
        self.assertEqual(self.contract.units.blender, "m")
        self.assertEqual(self.contract.units.unreal_centimeters_per_meter, 100)
        self.assertEqual(self.contract.units.snap_m, 0.5)
        self.assertEqual(self.contract.units.structural_bay_m, 4.0)
        self.assertEqual(self.contract.axes.east, "+X")
        self.assertEqual(self.contract.axes.north_pods, "+Y")
        self.assertEqual(self.contract.axes.up, "+Z")
        self.assertEqual(self.contract.axes.origin, "Deck center")
        self.assertEqual(self.contract.room.clear_dimensions_m, (28.0, 24.0, 7.0))
        self.assertEqual(self.contract.room.nominal_height_m, 8.0)
        self.assertEqual(self.contract.room.floor_thickness_m, 0.4)
        self.assertEqual(self.contract.room.wall_thickness_m, 0.4)
        self.assertEqual(self.contract.room.main_path_min_width_m, 5.0)
        self.assertEqual(self.contract.navigation_table.center_m, (0.0, 0.0, 0.0))
        self.assertEqual(self.contract.navigation_table.diameter_m, 8.0)
        self.assertEqual(self.contract.navigation_table.height_m, 1.1)
        self.assertEqual(self.contract.navigation_table.display_diameter_m, 6.0)
        self.assertEqual(self.contract.cryopods.count, 5)
        self.assertEqual(self.contract.cryopods.centers_x_m, (-4.0, -2.0, 0.0, 2.0, 4.0))
        self.assertEqual(self.contract.cryopods.base_y_m, 9.8)
        self.assertEqual(self.contract.cryopods.bounds_m, (1.6, 1.6, 3.0))
        self.assertEqual(self.contract.cryopods.recline_degrees, 18.0)
        self.assertEqual(self.contract.cryopods.door_open_degrees, 75.0)
        self.assertEqual(self.contract.cryopods.door_envelope_south_m, 1.2)
        self.assertEqual(self.contract.cryopods.interaction_clearance_m, (2.0, 2.5))
        self.assertEqual(self.contract.cryopods.expansion_interface_width_m, 2.0)
        self.assertEqual(self.contract.airlock.center_m, (0.0, -12.0, 0.0))
        self.assertEqual(self.contract.airlock.clear_opening_m, (4.0, 3.5))
        self.assertEqual(self.contract.airlock.depth_m, 1.2)
        self.assertEqual(self.contract.airlock.muster_area_m, (8.0, 5.0))
        self.assertEqual(self.contract.west_preparation_y_range_m, (-8.0, 0.0))
        self.assertEqual(self.contract.west_repair_center_m, (-14.0, 6.0, 1.4))
        self.assertEqual(self.contract.east_medical_y_range_m, (-8.0, -4.0))
        self.assertEqual(self.contract.east_role_y_range_m, (-4.0, -2.0))
        self.assertEqual(self.contract.east_jump_core_center_m, (14.0, 6.0, 3.0))
        self.assertEqual(self.contract.docks.count, 4)
        self.assertEqual(self.contract.docks.diameter_m, 1.0)
        self.assertEqual(self.contract.docks.recess_m, 0.08)
        self.assertEqual(
            self.contract.docks.centers_m,
            ((-5.3, -5.3, 0.0), (5.3, -5.3, 0.0), (-5.3, 5.3, 0.0), (5.3, 5.3, 0.0)),
        )
        self.assertEqual(self.contract.ceiling_ring.inner_diameter_m, 10.0)
        self.assertEqual(self.contract.ceiling_ring.outer_diameter_m, 16.0)
        self.assertEqual(self.contract.ceiling_ring.lowest_z_m, 6.2)
        self.assertEqual(self.contract.ceiling_ring.highest_z_m, 8.0)
        self.assertIs(self.contract.ceiling_ring.walkable, False)
        self.assertEqual(self.contract.character_reference.capsule_radius_m, 0.42)
        self.assertEqual(self.contract.character_reference.capsule_half_height_m, 0.96)
        self.assertEqual(self.contract.character_reference.camera_arm_m, 4.0)
        self.assertEqual(
            self.contract.deliverable_formats,
            ("blend", "fbx", "glb", "pdf", "png", "svg", "json", "txt"),
        )

    def test_layout_preserves_every_frozen_authority_category(self) -> None:
        self.assertEqual(self.layout.units, self.contract.units)
        self.assertEqual(self.layout.axes, self.contract.axes)
        self.assertEqual(self.layout.room, self.contract.room)
        self.assertEqual(self.layout.navigation_table, self.contract.navigation_table)
        self.assertEqual(self.layout.cryopod_spec, self.contract.cryopods)
        self.assertEqual(self.layout.airlock, self.contract.airlock)
        self.assertEqual(self.layout.west_preparation_y_range_m, (-8.0, 0.0))
        self.assertEqual(self.layout.west_repair_center_m, (-14.0, 6.0, 1.4))
        self.assertEqual(self.layout.east_medical_y_range_m, (-8.0, -4.0))
        self.assertEqual(self.layout.east_role_y_range_m, (-4.0, -2.0))
        self.assertEqual(self.layout.east_jump_core_center_m, (14.0, 6.0, 3.0))
        self.assertEqual(self.layout.construct_docks, self.contract.docks)
        self.assertEqual(self.layout.ceiling_ring, self.contract.ceiling_ring)
        self.assertEqual(self.layout.character_reference, self.contract.character_reference)
        self.assertEqual(self.layout.deliverable_formats, self.contract.deliverable_formats)

    def test_loader_requires_units_axes_and_character_reference_with_strict_types(self) -> None:
        cases = (
            ("Units", None, None, r"Missing required key: Units\.Blender"),
            ("Axes", None, None, r"Missing required key: Axes\.East"),
            (
                "CharacterReference",
                None,
                None,
                r"Missing required key: CharacterReference\.CapsuleRadiusM",
            ),
            ("Units", "Blender", 1, r"Units\.Blender.*string"),
            ("Units", "UnrealCentimetersPerMeter", True, r"Units\.UnrealCentimetersPerMeter.*integer"),
            ("Units", "UnrealCentimetersPerMeter", 100.0, r"Units\.UnrealCentimetersPerMeter.*integer"),
            ("Axes", "East", 1, r"Axes\.East.*string"),
            ("CharacterReference", "CapsuleRadiusM", "wide", r"CharacterReference\.CapsuleRadiusM.*finite number"),
        )
        for section, key, value, expected_error in cases:
            with self.subTest(section=section, key=key):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                if key is None:
                    del data[section]
                else:
                    data[section][key] = value
                with tempfile.TemporaryDirectory() as temporary_directory:
                    invalid_path = Path(temporary_directory) / "invalid-authority.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, expected_error):
                        load_contract(invalid_path)

    def test_loader_rejects_every_same_schema_frozen_value_mutation(self) -> None:
        cases = (
            ("Units.Blender", "cm"),
            ("Units.UnrealCentimetersPerMeter", 99),
            ("Units.SnapM", 0.25),
            ("Units.StructuralBayM", 3.0),
            ("Axes.East", "-X"),
            ("Axes.NorthPods", "-Y"),
            ("Axes.Up", "-Z"),
            ("Axes.Origin", "World origin"),
            ("Room.ClearWidthM", 27.0),
            ("Room.ClearDepthM", 23.0),
            ("Room.ClearHeightM", 6.0),
            ("Room.NominalHeightM", 7.5),
            ("Room.FloorThicknessM", 0.3),
            ("Room.WallThicknessM", 0.3),
            (
                "Room.InnerVerticesM",
                [[-9, -12], [10, -12], [14, -8], [14, 8], [10, 12], [-10, 12], [-14, 8], [-14, -8]],
            ),
            ("Room.MainPathMinWidthM", 4.5),
            ("NavigationTable.CenterM", [0, 1, 0]),
            ("NavigationTable.DiameterM", 7.0),
            ("NavigationTable.HeightM", 1.0),
            ("NavigationTable.DisplayDiameterM", 5.0),
            ("Cryopods.Count", 4),
            ("Cryopods.CentersXM", [-5, -2, 0, 2, 4]),
            ("Cryopods.BaseYM", 9.7),
            ("Cryopods.BoundsM", [1.5, 1.6, 3.0]),
            ("Cryopods.ReclineDegrees", 17.0),
            ("Cryopods.DoorOpenDegrees", 74.0),
            ("Cryopods.DoorEnvelopeSouthM", 1.1),
            ("Cryopods.InteractionClearanceM", [1.9, 2.5]),
            ("Cryopods.ExpansionInterfaceWidthM", 1.8),
            ("Airlock.CenterM", [0, -11, 0]),
            ("Airlock.ClearOpeningM", [3.5, 3.5]),
            ("Airlock.DepthM", 1.0),
            ("Airlock.MusterAreaM", [7.0, 5.0]),
            ("WestBays.PreparationYRangeM", [-7, 0]),
            ("WestBays.RepairCenterM", [-13, 6, 1.4]),
            ("EastBays.MedicalYRangeM", [-7, -4]),
            ("EastBays.RoleYRangeM", [-3, -2]),
            ("EastBays.JumpCoreCenterM", [13, 6, 3.0]),
            ("ConstructDocks.Count", 3),
            ("ConstructDocks.DiameterM", 0.9),
            ("ConstructDocks.RecessM", 0.07),
            (
                "ConstructDocks.CentersM",
                [[-5.2, -5.3, 0], [5.3, -5.3, 0], [-5.3, 5.3, 0], [5.3, 5.3, 0]],
            ),
            ("CeilingRing.InnerDiameterM", 9.0),
            ("CeilingRing.OuterDiameterM", 15.0),
            ("CeilingRing.LowestZM", 6.1),
            ("CeilingRing.HighestZM", 7.9),
            ("CeilingRing.Walkable", True),
            ("CharacterReference.CapsuleRadiusM", 0.4),
            ("CharacterReference.CapsuleHalfHeightM", 0.9),
            ("CharacterReference.CameraArmM", 3.5),
            (
                "Deliverables.Sheets",
                [
                    "A02_ReflectedCeilingPlan", "A01_FloorPlan", "A03_NorthElevation",
                    "A04_SouthElevation", "A05_WestElevation", "A06_EastElevation",
                    "A07_LongitudinalSection", "A08_TransverseSection", "A09_ExplodedModulePlan",
                    "A10_PerspectiveSheet", "D01_Cryopod", "D02_NavigationTable",
                    "D03_MainAirlock", "D04_ConstructDock", "D05_WallBayInterface",
                ],
            ),
            ("Deliverables.Formats", ["fbx", "blend", "glb", "pdf", "png", "svg", "json", "txt"]),
        )
        for dotted_path, value in cases:
            with self.subTest(dotted_path=dotted_path):
                data = self._mutated_brief(dotted_path, value)
                with tempfile.TemporaryDirectory() as temporary_directory:
                    invalid_path = Path(temporary_directory) / "same-schema-drift.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, dotted_path.replace(".", r"\.")):
                        load_contract(invalid_path)

    def test_cli_rejects_representative_mutation_of_every_category_without_preview(self) -> None:
        cases = (
            ("Units.Blender", "cm"),
            ("Axes.East", "-X"),
            ("Room.NominalHeightM", 7.5),
            ("NavigationTable.DisplayDiameterM", 5.0),
            ("Cryopods.DoorOpenDegrees", 74.0),
            ("Airlock.DepthM", 1.0),
            ("WestBays.RepairCenterM", [-13, 6, 1.4]),
            ("EastBays.RoleYRangeM", [-3, -2]),
            ("ConstructDocks.RecessM", 0.07),
            ("CeilingRing.Walkable", True),
            ("CharacterReference.CameraArmM", 3.5),
            ("Deliverables.Formats", ["fbx", "blend", "glb", "pdf", "png", "svg", "json", "txt"]),
        )
        for dotted_path, value in cases:
            with self.subTest(dotted_path=dotted_path):
                data = self._mutated_brief(dotted_path, value)
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary_path = Path(temporary_directory)
                    invalid_path = temporary_path / "same-schema-drift.json"
                    output_path = temporary_path / "layout-preview.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    stderr = StringIO()
                    with redirect_stderr(stderr):
                        exit_code = main(["--brief", str(invalid_path), "--out", str(output_path)])
                    self.assertEqual(exit_code, 2)
                    self.assertFalse(output_path.exists())
                    self.assertIn(dotted_path, stderr.getvalue())

    def test_validate_layout_reports_every_layout_owned_frozen_category(self) -> None:
        cases = (
            (replace(self.layout, units=replace(self.layout.units, snap_m=0.25)), "Units.SnapM"),
            (replace(self.layout, axes=replace(self.layout.axes, east="-X")), "Axes.East"),
            (replace(self.layout, room=replace(self.layout.room, nominal_height_m=7.5)), "Room.NominalHeightM"),
            (
                replace(
                    self.layout,
                    navigation_table=replace(self.layout.navigation_table, display_diameter_m=5.0),
                ),
                "NavigationTable.DisplayDiameterM",
            ),
            (
                replace(self.layout, cryopod_spec=replace(self.layout.cryopod_spec, door_open_degrees=74.0)),
                "Cryopods.DoorOpenDegrees",
            ),
            (replace(self.layout, airlock=replace(self.layout.airlock, depth_m=1.0)), "Airlock.DepthM"),
            (replace(self.layout, west_repair_center_m=(-13.0, 6.0, 1.4)), "WestBays.RepairCenterM"),
            (replace(self.layout, east_role_y_range_m=(-3.0, -2.0)), "EastBays.RoleYRangeM"),
            (
                replace(self.layout, construct_docks=replace(self.layout.construct_docks, recess_m=0.07)),
                "ConstructDocks.RecessM",
            ),
            (
                replace(self.layout, ceiling_ring=replace(self.layout.ceiling_ring, walkable=True)),
                "CeilingRing.Walkable",
            ),
            (
                replace(
                    self.layout,
                    character_reference=replace(self.layout.character_reference, camera_arm_m=3.5),
                ),
                "CharacterReference.CameraArmM",
            ),
            (
                replace(self.layout, deliverable_formats=("fbx", "blend", "glb", "pdf", "png", "svg", "json", "txt")),
                "Deliverables.Formats",
            ),
        )
        for invalid_layout, expected_path in cases:
            with self.subTest(expected_path=expected_path):
                self.assertTrue(
                    any(expected_path in issue for issue in validate_layout(invalid_layout)),
                    validate_layout(invalid_layout),
                )

    def test_real_contract_preserves_exact_room_geometry(self) -> None:
        self.assertEqual(self.contract.room.clear_dimensions_m, (28.0, 24.0, 7.0))
        self.assertEqual(
            self.contract.room.inner_vertices_m,
            (
                (-10.0, -12.0),
                (10.0, -12.0),
                (14.0, -8.0),
                (14.0, 8.0),
                (10.0, 12.0),
                (-10.0, 12.0),
                (-14.0, 8.0),
                (-14.0, -8.0),
            ),
        )

    def test_layout_has_exact_pod_and_flush_dock_placements(self) -> None:
        self.assertEqual(
            tuple(pod.center_m for pod in self.layout.cryopods),
            (
                (-4.0, 9.8, 0.0),
                (-2.0, 9.8, 0.0),
                (0.0, 9.8, 0.0),
                (2.0, 9.8, 0.0),
                (4.0, 9.8, 0.0),
            ),
        )
        self.assertEqual(
            tuple(pod.recline_degrees for pod in self.layout.cryopods),
            (18.0, 18.0, 18.0, 18.0, 18.0),
        )
        self.assertEqual(
            self.layout.dock_centers_m,
            (
                (-5.3, -5.3, 0.0),
                (5.3, -5.3, 0.0),
                (-5.3, 5.3, 0.0),
                (5.3, 5.3, 0.0),
            ),
        )

    def test_main_path_clearance_is_analytically_derived(self) -> None:
        self.assertEqual(self.layout.minimum_main_path_width_m(), 8.0)
        self.assertGreaterEqual(
            self.layout.minimum_main_path_width_m(),
            5.0,
        )

    def test_offset_table_clearance_uses_actual_table_center(self) -> None:
        offset_layout = replace(
            self.layout,
            navigation_table_center_m=(0.0, 2.0, 0.0),
        )
        self.assertEqual(offset_layout.minimum_main_path_width_m(), 6.0)

    def test_layout_preserves_airlock_bays_ring_and_ordered_sheets(self) -> None:
        self.assertEqual(self.layout.airlock_muster_area_m, (8.0, 5.0))
        self.assertEqual(self.layout.ceiling_ring_z_range_m, (6.2, 8.0))
        self.assertEqual(self.layout.west_preparation_y_range_m, (-8.0, 0.0))
        self.assertEqual(self.layout.west_repair_center_m, (-14.0, 6.0, 1.4))
        self.assertEqual(self.layout.east_medical_y_range_m, (-8.0, -4.0))
        self.assertEqual(self.layout.east_role_y_range_m, (-4.0, -2.0))
        self.assertEqual(self.layout.east_jump_core_center_m, (14.0, 6.0, 3.0))
        self.assertEqual(
            self.layout.sheet_ids,
            (
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
            ),
        )

    def test_real_layout_has_no_semantic_issues(self) -> None:
        self.assertEqual(validate_layout(self.layout), [])

    def test_loader_rejects_wrong_schema(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Schema"] = "projectrift.shiphub.complete-design.v0"
        with tempfile.TemporaryDirectory() as temporary_directory:
            invalid_path = Path(temporary_directory) / "wrong-schema.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"Schema.*complete-design\.v1"):
                load_contract(invalid_path)

    def test_loader_identifies_missing_required_key_path(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        del data["Room"]["ClearHeightM"]
        with tempfile.TemporaryDirectory() as temporary_directory:
            invalid_path = Path(temporary_directory) / "missing-key.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"Room\.ClearHeightM"):
                load_contract(invalid_path)

    def test_loader_rejects_non_finite_boolean_and_non_numeric_numbers(self) -> None:
        invalid_values = (float("nan"), float("inf"), True, "wide")
        for invalid_value in invalid_values:
            with self.subTest(invalid_value=invalid_value):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                data["Room"]["ClearWidthM"] = invalid_value
                with tempfile.TemporaryDirectory() as temporary_directory:
                    invalid_path = Path(temporary_directory) / "invalid-number.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, r"Room\.ClearWidthM.*finite number"):
                        load_contract(invalid_path)

    def test_loader_rejects_boolean_count(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Cryopods"]["Count"] = True
        with tempfile.TemporaryDirectory() as temporary_directory:
            invalid_path = Path(temporary_directory) / "boolean-count.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"Cryopods\.Count.*finite integer"):
                load_contract(invalid_path)

    def test_cli_rejects_malformed_root_and_collection_types(self) -> None:
        root_cases = (None, True, "not-an-object", [])
        for invalid_root in root_cases:
            with self.subTest(root=invalid_root):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary_path = Path(temporary_directory)
                    invalid_path = temporary_path / "invalid-root.json"
                    output_path = temporary_path / "layout-preview.json"
                    invalid_path.write_text(json.dumps(invalid_root), encoding="utf-8")
                    stderr = StringIO()
                    with redirect_stderr(stderr):
                        exit_code = main(
                            ["--brief", str(invalid_path), "--out", str(output_path)]
                        )
                    self.assertEqual(exit_code, 2)
                    self.assertFalse(output_path.exists())
                    self.assertIn("Root JSON must be an object", stderr.getvalue())

        collection_cases = (
            ("Cryopods", "CentersXM"),
            ("ConstructDocks", "CentersM"),
            ("Deliverables", "Sheets"),
            ("Deliverables", "Formats"),
        )
        malformed_values = (True, None, "not-an-array", {"value": 1})
        for section, key in collection_cases:
            for malformed_value in malformed_values:
                with self.subTest(section=section, key=key, value=malformed_value):
                    data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                    data[section][key] = malformed_value
                    with tempfile.TemporaryDirectory() as temporary_directory:
                        temporary_path = Path(temporary_directory)
                        invalid_path = temporary_path / "invalid-collection.json"
                        output_path = temporary_path / "layout-preview.json"
                        invalid_path.write_text(json.dumps(data), encoding="utf-8")
                        stderr = StringIO()
                        with redirect_stderr(stderr):
                            exit_code = main(
                                [
                                    "--brief",
                                    str(invalid_path),
                                    "--out",
                                    str(output_path),
                                ]
                            )
                        self.assertEqual(exit_code, 2)
                        self.assertFalse(output_path.exists())
                        self.assertIn(
                            f"{section}.{key} must be an array",
                            stderr.getvalue(),
                        )

    def test_loader_rejects_wrong_or_mismatched_pod_and_dock_counts(self) -> None:
        cases = (
            ("Cryopods", "Count", 4, r"Cryopods\.Count.*exactly 5"),
            ("Cryopods", "CentersXM", [-4, -2, 0, 2], r"Cryopods\.Count.*CentersXM"),
            ("ConstructDocks", "Count", 3, r"ConstructDocks\.Count.*exactly 4"),
            (
                "ConstructDocks",
                "CentersM",
                [[-5.3, -5.3, 0], [5.3, -5.3, 0], [-5.3, 5.3, 0]],
                r"ConstructDocks\.Count.*CentersM",
            ),
        )
        for section, key, value, message in cases:
            with self.subTest(section=section, key=key):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                data[section][key] = value
                with tempfile.TemporaryDirectory() as temporary_directory:
                    invalid_path = Path(temporary_directory) / "wrong-count.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        load_contract(invalid_path)

    def test_loader_rejects_non_frozen_cryopod_recline(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Cryopods"]["ReclineDegrees"] = 17.5
        with tempfile.TemporaryDirectory() as temporary_directory:
            invalid_path = Path(temporary_directory) / "wrong-recline.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"Cryopods\.ReclineDegrees.*18\.0"):
                load_contract(invalid_path)

    def test_loader_rejects_zero_length_room_segment(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Room"]["InnerVerticesM"][1] = data["Room"]["InnerVerticesM"][0]
        with tempfile.TemporaryDirectory() as temporary_directory:
            invalid_path = Path(temporary_directory) / "zero-length-edge.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError,
                r"Room\.InnerVerticesM.*zero-length segment",
            ):
                load_contract(invalid_path)

    def test_loader_rejects_duplicate_or_wrong_sheet_count(self) -> None:
        cases = (
            (lambda sheets: sheets.__setitem__(14, sheets[0]), r"Deliverables\.Sheets.*duplicate"),
            (lambda sheets: sheets.pop(), r"Deliverables\.Sheets.*exactly 15"),
        )
        for mutate_sheets, message in cases:
            with self.subTest(message=message):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                mutate_sheets(data["Deliverables"]["Sheets"])
                with tempfile.TemporaryDirectory() as temporary_directory:
                    invalid_path = Path(temporary_directory) / "invalid-sheets.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        load_contract(invalid_path)

    def test_cli_returns_two_and_does_not_write_invalid_preview(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Schema"] = "wrong.schema"
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_path = Path(temporary_directory)
            invalid_path = temporary_path / "invalid.json"
            output_path = temporary_path / "layout-preview.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            stderr = StringIO()
            with redirect_stderr(stderr):
                exit_code = main(
                    ["--brief", str(invalid_path), "--out", str(output_path)]
                )
            self.assertEqual(exit_code, 2)
            self.assertFalse(output_path.exists())
            self.assertIn("Schema", stderr.getvalue())

    def test_cli_writes_utf8_serializable_layout_preview_for_valid_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_path = Path(temporary_directory) / "nested" / "layout-preview.json"
            exit_code = main(
                ["--brief", str(BRIEF_PATH), "--out", str(output_path)]
            )
            self.assertEqual(exit_code, 0)
            preview = json.loads(output_path.read_bytes().decode("utf-8"))
            self.assertEqual(preview["clear_dimensions_m"], [28.0, 24.0, 7.0])
            self.assertEqual(preview["inner_vertices_m"][0], [-10.0, -12.0])
            self.assertEqual(preview["navigation_table"]["center_m"], [0.0, 0.0, 0.0])
            self.assertEqual(preview["navigation_table"]["radius_m"], 4.0)
            self.assertEqual(preview["navigation_table"]["height_m"], 1.1)
            self.assertEqual(preview["minimum_main_path_width_m"], 8.0)
            self.assertEqual(preview["airlock"]["muster_area_m"], [8.0, 5.0])
            self.assertEqual(preview["west_bays"]["repair_center_m"], [-14.0, 6.0, 1.4])
            self.assertEqual(preview["east_bays"]["jump_core_center_m"], [14.0, 6.0, 3.0])
            self.assertEqual(preview["ceiling_ring"]["z_range_m"], [6.2, 8.0])
            self.assertEqual(len(preview["objects"]), 10)
            self.assertEqual(len(preview["sheet_ids"]), 15)
            self.assertEqual(preview["sheet_ids"][0], "A01_FloorPlan")
            self.assertEqual(preview["sheet_ids"][-1], "D05_WallBayInterface")
            self.assertEqual(
                preview["contract"],
                {
                    "schema": "projectrift.shiphub.complete-design.v1",
                    "units": {
                        "blender": "m",
                        "unreal_centimeters_per_meter": 100,
                        "snap_m": 0.5,
                        "structural_bay_m": 4.0,
                    },
                    "axes": {
                        "east": "+X",
                        "north_pods": "+Y",
                        "up": "+Z",
                        "origin": "Deck center",
                    },
                    "room": {
                        "clear_dimensions_m": [28.0, 24.0, 7.0],
                        "nominal_height_m": 8.0,
                        "floor_thickness_m": 0.4,
                        "wall_thickness_m": 0.4,
                        "inner_vertices_m": [
                            [-10.0, -12.0], [10.0, -12.0], [14.0, -8.0],
                            [14.0, 8.0], [10.0, 12.0], [-10.0, 12.0],
                            [-14.0, 8.0], [-14.0, -8.0],
                        ],
                        "main_path_min_width_m": 5.0,
                    },
                    "navigation_table": {
                        "center_m": [0.0, 0.0, 0.0],
                        "diameter_m": 8.0,
                        "height_m": 1.1,
                        "display_diameter_m": 6.0,
                    },
                    "cryopods": {
                        "count": 5,
                        "centers_x_m": [-4.0, -2.0, 0.0, 2.0, 4.0],
                        "base_y_m": 9.8,
                        "bounds_m": [1.6, 1.6, 3.0],
                        "recline_degrees": 18.0,
                        "door_open_degrees": 75.0,
                        "door_envelope_south_m": 1.2,
                        "interaction_clearance_m": [2.0, 2.5],
                        "expansion_interface_width_m": 2.0,
                    },
                    "airlock": {
                        "center_m": [0.0, -12.0, 0.0],
                        "clear_opening_m": [4.0, 3.5],
                        "depth_m": 1.2,
                        "muster_area_m": [8.0, 5.0],
                    },
                    "west_bays": {
                        "preparation_y_range_m": [-8.0, 0.0],
                        "repair_center_m": [-14.0, 6.0, 1.4],
                    },
                    "east_bays": {
                        "medical_y_range_m": [-8.0, -4.0],
                        "role_y_range_m": [-4.0, -2.0],
                        "jump_core_center_m": [14.0, 6.0, 3.0],
                    },
                    "construct_docks": {
                        "count": 4,
                        "diameter_m": 1.0,
                        "recess_m": 0.08,
                        "centers_m": [
                            [-5.3, -5.3, 0.0], [5.3, -5.3, 0.0],
                            [-5.3, 5.3, 0.0], [5.3, 5.3, 0.0],
                        ],
                    },
                    "ceiling_ring": {
                        "inner_diameter_m": 10.0,
                        "outer_diameter_m": 16.0,
                        "lowest_z_m": 6.2,
                        "highest_z_m": 8.0,
                        "walkable": False,
                    },
                    "character_reference": {
                        "capsule_radius_m": 0.42,
                        "capsule_half_height_m": 0.96,
                        "camera_arm_m": 4.0,
                    },
                    "deliverables": {
                        "sheets": list(self.layout.sheet_ids),
                        "formats": ["blend", "fbx", "glb", "pdf", "png", "svg", "json", "txt"],
                    },
                },
            )

    def test_cli_rejects_same_schema_wrong_navigation_table_geometry(self) -> None:
        cases = (
            ("CenterM", [0.0, 2.0, 0.0]),
            ("DiameterM", 7.0),
            ("HeightM", 1.2),
        )
        for key, value in cases:
            with self.subTest(key=key):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                data["NavigationTable"][key] = value
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary_path = Path(temporary_directory)
                    invalid_path = temporary_path / "invalid-table.json"
                    output_path = temporary_path / "layout-preview.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    stderr = StringIO()
                    with redirect_stderr(stderr):
                        exit_code = main(
                            ["--brief", str(invalid_path), "--out", str(output_path)]
                        )
                    self.assertEqual(exit_code, 2)
                    self.assertFalse(output_path.exists())
                    self.assertIn("navigation table", stderr.getvalue().lower())

    def test_cli_rejects_zero_length_room_segment_without_output(self) -> None:
        data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
        data["Room"]["InnerVerticesM"][1] = data["Room"]["InnerVerticesM"][0]
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_path = Path(temporary_directory)
            invalid_path = temporary_path / "zero-length-edge.json"
            output_path = temporary_path / "layout-preview.json"
            invalid_path.write_text(json.dumps(data), encoding="utf-8")
            stderr = StringIO()
            with redirect_stderr(stderr):
                exit_code = main(
                    ["--brief", str(invalid_path), "--out", str(output_path)]
                )
            self.assertEqual(exit_code, 2)
            self.assertFalse(output_path.exists())
            self.assertIn("zero-length segment", stderr.getvalue())

    def test_cli_rejects_nonboolean_and_nonstring_metadata(self) -> None:
        cases = (
            (
                "CeilingRing",
                "Walkable",
                None,
                "false",
                "CeilingRing.Walkable must be a boolean",
            ),
            (
                "Deliverables",
                "Sheets",
                0,
                True,
                "Deliverables.Sheets[0] must be a string",
            ),
            (
                "Deliverables",
                "Formats",
                0,
                123,
                "Deliverables.Formats[0] must be a string",
            ),
        )
        for section, key, index, invalid_value, expected_error in cases:
            with self.subTest(section=section, key=key):
                data = json.loads(BRIEF_PATH.read_text(encoding="utf-8"))
                if index is None:
                    data[section][key] = invalid_value
                else:
                    data[section][key][index] = invalid_value
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary_path = Path(temporary_directory)
                    invalid_path = temporary_path / "invalid-metadata.json"
                    output_path = temporary_path / "layout-preview.json"
                    invalid_path.write_text(json.dumps(data), encoding="utf-8")
                    stderr = StringIO()
                    with redirect_stderr(stderr):
                        exit_code = main(
                            ["--brief", str(invalid_path), "--out", str(output_path)]
                        )
                    self.assertEqual(exit_code, 2)
                    self.assertFalse(output_path.exists())
                    self.assertIn(expected_error, stderr.getvalue())

    def test_validate_layout_reports_wrong_room_geometry(self) -> None:
        wrong_dimensions = replace(self.layout, clear_dimensions_m=(27.0, 24.0, 7.0))
        wrong_vertices = replace(self.layout, inner_vertices_m=self.layout.inner_vertices_m[:-1])
        moved_vertex = replace(
            self.layout,
            inner_vertices_m=((-9.0, -12.0),) + self.layout.inner_vertices_m[1:],
        )
        for invalid_layout in (wrong_dimensions, wrong_vertices, moved_vertex):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("room" in issue.lower() for issue in validate_layout(invalid_layout))
                )

    def test_validate_layout_reports_zero_length_segment_without_dividing(self) -> None:
        duplicate_vertices = (
            self.layout.inner_vertices_m[0],
            self.layout.inner_vertices_m[0],
            *self.layout.inner_vertices_m[2:],
        )
        invalid_layout = replace(
            self.layout,
            inner_vertices_m=duplicate_vertices,
        )
        issues = validate_layout(invalid_layout)
        self.assertTrue(any("zero-length segment" in issue for issue in issues))

    def test_validate_layout_reports_wrong_pod_count_positions_or_recline(self) -> None:
        wrong_count = replace(self.layout, cryopods=self.layout.cryopods[:-1])
        wrong_position = replace(
            self.layout,
            cryopods=(
                replace(self.layout.cryopods[0], center_m=(-3.5, 9.8, 0.0)),
                *self.layout.cryopods[1:],
            ),
        )
        wrong_recline = replace(
            self.layout,
            cryopods=(
                replace(self.layout.cryopods[0], recline_degrees=17.0),
                *self.layout.cryopods[1:],
            ),
        )
        for invalid_layout in (wrong_count, wrong_position, wrong_recline):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("cryopod" in issue.lower() for issue in validate_layout(invalid_layout))
                )

    def test_validate_layout_reports_wrong_dock_coordinates(self) -> None:
        invalid_layout = replace(
            self.layout,
            dock_centers_m=self.layout.dock_centers_m[:-1],
        )
        self.assertTrue(any("dock" in issue.lower() for issue in validate_layout(invalid_layout)))

    def test_validate_layout_reports_table_clearance_below_contract_minimum(self) -> None:
        invalid_layout = replace(self.layout, navigation_table_radius_m=8.0)
        self.assertTrue(
            any("clearance" in issue.lower() for issue in validate_layout(invalid_layout))
        )

    def test_validate_layout_reports_wrong_navigation_table_geometry(self) -> None:
        wrong_center = replace(
            self.layout,
            navigation_table_center_m=(0.0, 1.0, 0.0),
        )
        wrong_radius = replace(self.layout, navigation_table_radius_m=3.5)
        wrong_height = replace(self.layout, navigation_table_height_m=1.2)
        for invalid_layout in (wrong_center, wrong_radius, wrong_height):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any(
                        "navigation table" in issue.lower()
                        for issue in validate_layout(invalid_layout)
                    )
                )

    def test_validate_layout_reports_wrong_airlock_opening_or_muster(self) -> None:
        wrong_opening = replace(self.layout, airlock_clear_opening_m=(3.0, 3.5))
        wrong_muster = replace(self.layout, airlock_muster_area_m=(7.0, 5.0))
        for invalid_layout in (wrong_opening, wrong_muster):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("airlock" in issue.lower() for issue in validate_layout(invalid_layout))
                )

    def test_validate_layout_reports_wrong_west_or_east_bay_data(self) -> None:
        wrong_west = replace(self.layout, west_preparation_y_range_m=(-7.0, 0.0))
        wrong_east = replace(self.layout, east_jump_core_center_m=(13.0, 6.0, 3.0))
        for invalid_layout in (wrong_west, wrong_east):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("bay" in issue.lower() for issue in validate_layout(invalid_layout))
                )

    def test_validate_layout_reports_wrong_ceiling_ring_bounds(self) -> None:
        wrong_z = replace(self.layout, ceiling_ring_z_range_m=(6.1, 8.0))
        wrong_diameter = replace(self.layout, ceiling_ring_diameters_m=(9.0, 16.0))
        for invalid_layout in (wrong_z, wrong_diameter):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("ceiling ring" in issue.lower() for issue in validate_layout(invalid_layout))
                )

    def test_validate_layout_reports_duplicate_or_wrong_sheet_count(self) -> None:
        wrong_count = replace(self.layout, sheet_ids=self.layout.sheet_ids[:-1])
        duplicate = replace(
            self.layout,
            sheet_ids=self.layout.sheet_ids[:-1] + (self.layout.sheet_ids[0],),
        )
        for invalid_layout in (wrong_count, duplicate):
            with self.subTest(invalid_layout=invalid_layout):
                self.assertTrue(
                    any("sheet" in issue.lower() for issue in validate_layout(invalid_layout))
                )


if __name__ == "__main__":
    unittest.main()
