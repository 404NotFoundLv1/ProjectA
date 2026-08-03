"""Focused regression tests for Ship Hub package-validation evidence."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
SOURCE_OUTPUT_ROOT = PROJECT_ROOT / "SourceArt/ProjectRift/ShipHub/CompleteDesign"
SOURCE_BRIEF = (
    PROJECT_ROOT / "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json"
)
VALIDATOR = (
    PROJECT_ROOT
    / "Scripts/ProjectRift/ArtPipeline/shiphub/validate_shiphub_package.py"
)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ShipHubValidationEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        fixture_parent = PROJECT_ROOT / "Saved/Automation/ProjectRiftShipHubDesign"
        fixture_parent.mkdir(parents=True, exist_ok=True)
        self.fixture_root = Path(
            tempfile.mkdtemp(prefix="validation-evidence-test-", dir=fixture_parent)
        )
        self.output_root = (
            self.fixture_root / "SourceArt/ProjectRift/ShipHub/CompleteDesign"
        )
        shutil.copytree(SOURCE_OUTPUT_ROOT, self.output_root, copy_function=os.link)
        for generated in (
            self.output_root / "Reports/validation-report.json",
            self.output_root / "Reports/SHA256SUMS.txt",
        ):
            generated.unlink(missing_ok=True)
        self.brief_path = (
            self.fixture_root
            / "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json"
        )
        self.brief_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(SOURCE_BRIEF, self.brief_path)

    def tearDown(self) -> None:
        shutil.rmtree(self.fixture_root, ignore_errors=True)

    def _run_validator(self) -> subprocess.CompletedProcess[str]:
        reports = self.output_root / "Reports"
        return subprocess.run(
            [
                sys.executable,
                str(VALIDATOR),
                "--project-root",
                str(self.fixture_root),
                "--brief",
                str(self.brief_path),
                "--output-root",
                str(self.output_root),
                "--export-report",
                str(reports / "export-validation.json"),
                "--report",
                str(reports / "validation-report.json"),
                "--sha256",
                str(reports / "SHA256SUMS.txt"),
            ],
            text=True,
            capture_output=True,
            check=False,
        )

    def _write_current_evidence_hashes(self) -> None:
        evidence_path = self.output_root / "Reports/export-validation.json"
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        evidence_path.unlink()
        evidence["InputSHA256"] = {
            "BLEND": _sha256(
                self.output_root / "Blender/SM_ShipHub_Complete_White_v1.blend"
            ),
            "Manifest": _sha256(self.output_root / "Reports/layout-manifest.json"),
            "FBX": _sha256(
                self.output_root / "Exports/SM_ShipHub_Complete_White_v1.fbx"
            ),
            "GLB": _sha256(
                self.output_root / "Exports/SM_ShipHub_Complete_White_v1.glb"
            ),
        }
        evidence_path.write_text(
            json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    def test_package_validator_rejects_export_changed_after_blender_evidence(self) -> None:
        """Replacing an export after Blender validation must invalidate stale evidence."""
        self._write_current_evidence_hashes()
        fbx_path = self.output_root / "Exports/SM_ShipHub_Complete_White_v1.fbx"
        original_bytes = fbx_path.read_bytes()
        fbx_path.unlink()
        fbx_path.write_bytes(original_bytes + b"stale-evidence-mutation")

        result = self._run_validator()
        report = json.loads(
            (self.output_root / "Reports/validation-report.json").read_text(
                encoding="utf-8"
            )
        )

        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        matching = [
            issue
            for issue in report["Issues"]
            if issue["RuleId"] == "EXPORT_EVIDENCE_SHA256_FBX"
        ]
        self.assertEqual(len(matching), 1)
        self.assertEqual(matching[0]["Actual"], _sha256(fbx_path))

    def test_package_validator_rejects_navigation_table_count_mutation(self) -> None:
        """A manifest scalar count other than one must produce a structured issue."""
        manifest_path = self.output_root / "Reports/layout-manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["assemblies"]["navigation_table"]["count"] = 2
        manifest_path.unlink()
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

        self._write_current_evidence_hashes()

        result = self._run_validator()
        report = json.loads(
            (self.output_root / "Reports/validation-report.json").read_text(
                encoding="utf-8"
            )
        )

        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        matching = [
            issue
            for issue in report["Issues"]
            if issue["RuleId"] == "NAVIGATION_TABLE_REPORTED_COUNT"
        ]
        self.assertEqual(len(matching), 1)
        self.assertEqual(matching[0]["Expected"], 1)
        self.assertEqual(matching[0]["Actual"], 2)


if __name__ == "__main__":
    unittest.main()
