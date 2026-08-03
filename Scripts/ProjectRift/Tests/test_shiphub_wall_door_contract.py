"""Behavioral tests for the ShipHub G3 wall-door contract."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from Scripts.ProjectRift.ArtPipeline.shiphub.wall_door_contract import (
    load_contract,
    validate_contract,
)


PROJECT_ROOT = Path(__file__).resolve().parents[3]
CONTRACT_PATH = (
    PROJECT_ROOT
    / "SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs"
    / "SM_ShipHub_WallDoor_400_A.asset.json"
)
MODULE_PATH = (
    PROJECT_ROOT / "Scripts/ProjectRift/ArtPipeline/shiphub/wall_door_contract.py"
)


class WallDoorContractTests(unittest.TestCase):
    """The G3 contract's consumer-visible validation and preview behavior."""

    def read_contract_json(self) -> dict[str, object]:
        return json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))

    def write_fixture(self, directory: Path, payload: object) -> Path:
        fixture_path = directory / "contract.json"
        fixture_path.write_text(json.dumps(payload), encoding="utf-8")
        return fixture_path

    def test_load_contract_exposes_the_approved_g3_values(self) -> None:
        """A changed approved wall-door value must be rejected by consumers."""
        contract = load_contract(CONTRACT_PATH)

        self.assertEqual(contract.asset_id, "SM_ShipHub_WallDoor_400_A")
        self.assertEqual(contract.bounds_cm, (400.0, 30.0, 400.0))
        self.assertEqual(contract.opening_cm, (240.0, 280.0))
        self.assertEqual(contract.snap_cm, 50.0)
        self.assertEqual(contract.pivot, "BottomLeftBack")
        self.assertEqual(contract.material_slot_limit, 2)
        self.assertEqual(contract.texture_size, 2048)
        self.assertEqual(contract.states, ("Damaged", "Patched", "Online"))
        self.assertEqual(validate_contract(contract, PROJECT_ROOT), [])

    def test_validate_contract_reports_each_mutated_approved_value(self) -> None:
        """Changing any locked G3 field must identify its JSON path."""
        mutations = (
            ("BoundsCm", lambda payload: payload.__setitem__("BoundsCm", [399, 30, 400])),
            (
                "DoorOpeningCm",
                lambda payload: payload.__setitem__("DoorOpeningCm", [239, 280]),
            ),
            ("SnapCm", lambda payload: payload.__setitem__("SnapCm", 25)),
            ("Pivot", lambda payload: payload.__setitem__("Pivot", "Center")),
            ("TextureSize", lambda payload: payload.__setitem__("TextureSize", 8192)),
            (
                "MaterialSlotLimit",
                lambda payload: payload.__setitem__("MaterialSlotLimit", 3),
            ),
            (
                "States",
                lambda payload: payload.__setitem__(
                    "States", ["Online", "Patched", "Damaged"]
                ),
            ),
        )

        for expected_path, mutate in mutations:
            with self.subTest(path=expected_path), tempfile.TemporaryDirectory() as temp:
                payload = self.read_contract_json()
                mutate(payload)
                contract = load_contract(self.write_fixture(Path(temp), payload))
                issues = validate_contract(contract)
                self.assertTrue(
                    any(expected_path in issue for issue in issues),
                    issues,
                )

    def test_validate_contract_rejects_every_other_locked_value(self) -> None:
        """Schema, geometry, material, and reference drift must be observable."""
        mutations = (
            ("Schema", lambda payload: payload.__setitem__("Schema", "wrong")),
            ("Stage", lambda payload: payload.__setitem__("Stage", "G4")),
            ("Units", lambda payload: payload.__setitem__("Units", "m")),
            ("BaseMesh", lambda payload: payload.__setitem__("BaseMesh", "Other")),
            (
                "TextureChannels",
                lambda payload: payload.__setitem__(
                    "TextureChannels", ["N", "BC", "ORM", "StateMask"]
                ),
            ),
            (
                "CollisionPieces",
                lambda payload: payload.__setitem__(
                    "CollisionPieces", ["RightJamb", "LeftJamb", "Lintel"]
                ),
            ),
            (
                "References",
                lambda payload: payload.__setitem__("References", payload["References"][:1]),
            ),
        )

        for expected_path, mutate in mutations:
            with self.subTest(path=expected_path), tempfile.TemporaryDirectory() as temp:
                payload = self.read_contract_json()
                mutate(payload)
                contract = load_contract(self.write_fixture(Path(temp), payload))
                self.assertTrue(
                    any(expected_path in issue for issue in validate_contract(contract)),
                )

    def test_load_contract_rejects_invalid_reference_syntax(self) -> None:
        """References escaping the ShipHub SourceArt root must never load."""
        invalid_references = (
            "C:/outside.blend",
            "../outside.blend",
            "SourceArt//ProjectRift/ShipHub/CompleteDesign/file.blend",
            "SourceArt\\ProjectRift\\ShipHub\\CompleteDesign\\file.blend",
            "SourceArt/ProjectRift/Other/file.blend",
        )

        for reference in invalid_references:
            with self.subTest(reference=reference), tempfile.TemporaryDirectory() as temp:
                payload = self.read_contract_json()
                payload["References"] = [reference, payload["References"][1]]
                with self.assertRaisesRegex(ValueError, "References"):
                    load_contract(self.write_fixture(Path(temp), payload))

    def test_load_contract_normalizes_json_root_and_decode_failures_to_value_error(self) -> None:
        """Malformed contract input must not leak parser or mapping exceptions."""
        with tempfile.TemporaryDirectory() as temp:
            fixture = Path(temp) / "contract.json"
            fixture.write_text("[1, 2, 3]", encoding="utf-8")
            with self.assertRaises(ValueError):
                load_contract(fixture)

            fixture.write_text("{not json", encoding="utf-8")
            with self.assertRaises(ValueError):
                load_contract(fixture)

    def test_load_contract_normalizes_field_type_failures_to_value_error(self) -> None:
        """A malformed numeric field must not leak an implementation exception."""
        with tempfile.TemporaryDirectory() as temp:
            payload = self.read_contract_json()
            payload["SnapCm"] = "50"
            with self.assertRaises(ValueError):
                load_contract(self.write_fixture(Path(temp), payload))

    def test_load_contract_aggregates_independent_structural_failures(self) -> None:
        """One malformed field must not hide another required contract defect."""
        with tempfile.TemporaryDirectory() as temp:
            payload = self.read_contract_json()
            del payload["Schema"]
            payload["BoundsCm"] = "not-an-array"
            with self.assertRaises(ValueError) as raised:
                load_contract(self.write_fixture(Path(temp), payload))

            self.assertIn("Schema", str(raised.exception))
            self.assertIn("BoundsCm", str(raised.exception))

    def test_validation_reports_missing_project_reference(self) -> None:
        """A missing existing-design reference must be reported when a root is supplied."""
        with tempfile.TemporaryDirectory() as temp:
            payload = self.read_contract_json()
            payload["References"][0] = (
                "SourceArt/ProjectRift/ShipHub/CompleteDesign/Blender/missing.blend"
            )
            contract = load_contract(self.write_fixture(Path(temp), payload))
            issues = validate_contract(contract, PROJECT_ROOT)
            self.assertTrue(any("References[0]" in issue for issue in issues), issues)

    def test_cli_writes_only_a_deterministic_preview_for_a_valid_contract(self) -> None:
        """The CLI must emit a stable G3 preview rather than DCC artifacts."""
        with tempfile.TemporaryDirectory() as temp:
            preview_path = Path(temp) / "contract-preview.json"
            command = [
                sys.executable,
                str(MODULE_PATH),
                "--contract",
                str(CONTRACT_PATH),
                "--project-root",
                str(PROJECT_ROOT),
                "--out",
                str(preview_path),
            ]
            completed = subprocess.run(command, capture_output=True, text=True, check=False)

            self.assertEqual(completed.returncode, 0, completed.stderr)
            preview = json.loads(preview_path.read_text(encoding="utf-8"))
            self.assertEqual(
                list(preview),
                [
                    "Schema",
                    "AssetId",
                    "Stage",
                    "BoundsCm",
                    "DoorOpeningCm",
                    "DoorOpeningMinCm",
                    "SnapCm",
                    "Pivot",
                    "MaterialSlotLimit",
                    "TextureSize",
                    "States",
                    "References",
                ],
            )
            self.assertEqual(preview["References"], self.read_contract_json()["References"])

    def test_cli_writes_no_preview_when_validation_fails(self) -> None:
        """Invalid inputs must fail before creating an automation preview."""
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            payload = self.read_contract_json()
            payload["SnapCm"] = 25
            contract_path = self.write_fixture(temp_path, payload)
            preview_path = temp_path / "contract-preview.json"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(MODULE_PATH),
                    "--contract",
                    str(contract_path),
                    "--project-root",
                    str(PROJECT_ROOT),
                    "--out",
                    str(preview_path),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse(preview_path.exists())


if __name__ == "__main__":
    unittest.main()
