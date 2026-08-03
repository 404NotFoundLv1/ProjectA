"""Validate the complete ProjectRift Ship Hub design package."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

from PIL import Image
from pypdf import PdfReader


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
PERSPECTIVE_NAMES = (
    "ceiling-low-angle.png",
    "east-oblique.png",
    "front.png",
    "high-overview.png",
    "reverse.png",
    "west-oblique.png",
)
EXPECTED = {
    "room_clear_m": [28.0, 24.0, 7.0],
    "nominal_height_m": 8.0,
    "navigation_table_diameter_m": 8.0,
    "cryopod_count": 5,
    "cryopod_recline_degrees": 18.0,
    "construct_dock_count": 4,
    "navigation_table_count": 1,
    "minimum_main_path_width_m": 5.0,
    "ceiling_ring_lowest_z_m": 6.2,
}
PNG_SIZE = (4961, 3508)
PDF_PAGE_COUNT = 15
BOUNDS_TOLERANCE_M = 0.01
UNRESOLVED_MARKERS = re.compile(
    r"\b(?:TODO|TBD|FIXME|PLACEHOLDER|UNRESOLVED)\b|\{\{|\}\}|<%|%>",
    re.IGNORECASE,
)


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--brief", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--export-report", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--sha256", required=True)
    return parser.parse_args()


def _resolve_exact_paths(arguments: argparse.Namespace) -> dict[str, Path]:
    project_root = Path(arguments.project_root).resolve()
    expected_output_root = (
        project_root / "SourceArt/ProjectRift/ShipHub/CompleteDesign"
    ).resolve()
    expected_brief = (
        project_root
        / "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json"
    ).resolve()
    paths = {
        "project_root": project_root,
        "output_root": Path(arguments.output_root).resolve(),
        "brief": Path(arguments.brief).resolve(),
        "export_report": Path(arguments.export_report).resolve(),
        "report": Path(arguments.report).resolve(),
        "sha256": Path(arguments.sha256).resolve(),
    }
    exact = {
        "output_root": expected_output_root,
        "brief": expected_brief,
        "export_report": expected_output_root / "Reports/export-validation.json",
        "report": expected_output_root / "Reports/validation-report.json",
        "sha256": expected_output_root / "Reports/SHA256SUMS.txt",
    }
    if not project_root.is_dir():
        raise ValueError(f"Project root is missing: {project_root}")
    for label, expected_path in exact.items():
        expected_path = expected_path.resolve()
        if paths[label] != expected_path:
            raise ValueError(
                f"Unexpected {label} path: {paths[label]}; expected {expected_path}"
            )
    for label in ("export_report", "report", "sha256"):
        try:
            paths[label].relative_to(expected_output_root)
        except ValueError as error:
            raise ValueError(
                f"{label} path escapes the approved output root: {paths[label]}"
            ) from error
    return paths


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _validate_input_hashes(
    export_report: dict, artifacts: dict[str, Path], issues: list[dict]
) -> None:
    recorded_hashes = export_report.get("InputSHA256", {})
    for label, path in artifacts.items():
        actual_hash = _sha256(path) if path.is_file() else None
        expected_hash = recorded_hashes.get(label)
        if expected_hash != actual_hash:
            _issue(
                issues,
                f"EXPORT_EVIDENCE_SHA256_{label.upper()}",
                expected_hash,
                actual_hash,
                path.name,
            )


def _issue(issues: list[dict], rule_id: str, expected, actual, artifact: str) -> None:
    issues.append(
        {
            "RuleId": rule_id,
            "Expected": expected,
            "Actual": actual,
            "Artifact": artifact,
        }
    )


def _expected_artifacts(output_root: Path, canonical_hashes: dict[str, str]) -> set[str]:
    artifacts = {
        "Blender/SM_ShipHub_Complete_White_v1.blend",
        "Exports/SM_ShipHub_Complete_White_v1.fbx",
        "Exports/SM_ShipHub_Complete_White_v1.glb",
        "Reports/layout-manifest.json",
        "Reports/export-validation.json",
        "Reports/validation-report.json",
        "Reports/SHA256SUMS.txt",
        "Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf",
        "Drawings/ProjectRift_ShipHub_ContactSheet_v1.png",
        (
            "Drawings/Handoff/ProjectRift_ShipHub_CompleteDesign_v1r2_"
            f"{canonical_hashes['pdf'][:8]}.pdf"
        ),
        (
            "Drawings/Handoff/ProjectRift_ShipHub_ContactSheet_v1r2_"
            f"{canonical_hashes['contact'][:8]}.png"
        ),
    }
    artifacts.update(f"Drawings/PNG/{sheet_id}_Base.png" for sheet_id in SHEET_IDS)
    artifacts.update(
        f"Drawings/PNG/Perspectives/{name}" for name in PERSPECTIVE_NAMES
    )
    artifacts.update(f"Drawings/SVG/{sheet_id}.svg" for sheet_id in SHEET_IDS)
    artifacts.update(f"Drawings/FinalPNG/{sheet_id}.png" for sheet_id in SHEET_IDS)
    return artifacts


def _manifest_export_bounds(manifest: dict, issues: list[dict]) -> dict[str, list[float]]:
    exported_objects = [entry for entry in manifest.get("objects", []) if entry.get("exported")]
    expected_names = manifest.get("exports", {}).get("expected_object_names", [])
    actual_names = sorted(entry.get("name") for entry in exported_objects)
    if actual_names != sorted(expected_names):
        _issue(
            issues,
            "MANIFEST_EXPORT_SELECTION",
            sorted(expected_names),
            actual_names,
            "Reports/layout-manifest.json",
        )
    try:
        return {
            "Min": [
                min(float(entry["measured_world_bounds_m"]["min"][axis]) for entry in exported_objects)
                for axis in range(3)
            ],
            "Max": [
                max(float(entry["measured_world_bounds_m"]["max"][axis]) for entry in exported_objects)
                for axis in range(3)
            ],
        }
    except (KeyError, TypeError, ValueError) as error:
        _issue(
            issues,
            "MANIFEST_EXPORT_BOUNDS",
            "measured bounds for every exported object",
            str(error),
            "Reports/layout-manifest.json",
        )
        return {"Min": [0.0, 0.0, 0.0], "Max": [0.0, 0.0, 0.0]}


def _bounds_match(actual, expected) -> bool:
    try:
        return all(
            abs(float(actual[key][axis]) - float(expected[key][axis]))
            <= BOUNDS_TOLERANCE_M
            for key in ("Min", "Max")
            for axis in range(3)
        )
    except (KeyError, TypeError, ValueError, IndexError):
        return False


def _navigation_diameter(manifest: dict) -> float | None:
    members = manifest.get("assemblies", {}).get("navigation_table", {}).get("members", [])
    try:
        minimum_x = min(float(entry["measured_world_bounds_m"]["min"][0]) for entry in members)
        maximum_x = max(float(entry["measured_world_bounds_m"]["max"][0]) for entry in members)
        minimum_y = min(float(entry["measured_world_bounds_m"]["min"][1]) for entry in members)
        maximum_y = max(float(entry["measured_world_bounds_m"]["max"][1]) for entry in members)
        return max(maximum_x - minimum_x, maximum_y - minimum_y)
    except (KeyError, TypeError, ValueError):
        return None


def _navigation_table_count(manifest: dict, issues: list[dict]) -> int:
    navigation = manifest.get("assemblies", {}).get("navigation_table", {})
    reported_count = navigation.get("count")
    expected_count = EXPECTED["navigation_table_count"]
    if reported_count != expected_count:
        _issue(
            issues,
            "NAVIGATION_TABLE_REPORTED_COUNT",
            expected_count,
            reported_count,
            "Reports/layout-manifest.json",
        )
    display_name = navigation.get("display_name")
    sector_names = list(navigation.get("sector_names", []))
    member_names = [entry.get("name") for entry in navigation.get("members", [])]
    scene_names = [entry.get("name") for entry in manifest.get("objects", [])]
    required_names = [display_name, *sector_names]
    valid_membership = (
        isinstance(display_name, str)
        and len(sector_names) == 4
        and len(set(sector_names)) == 4
        and len(member_names) == 5
        and len(set(member_names)) == 5
        and set(member_names) == set(required_names)
        and all(scene_names.count(name) == 1 for name in required_names)
    )
    if not valid_membership:
        _issue(
            issues,
            "NAVIGATION_TABLE_MEMBERSHIP",
            {
                "DisplayCount": 1,
                "SectorCount": 4,
                "MemberCount": 5,
                "SceneObjectCountPerMember": 1,
            },
            {
                "DisplayName": display_name,
                "SectorNames": sector_names,
                "MemberNames": member_names,
                "SceneObjectCounts": {
                    str(name): scene_names.count(name) for name in required_names
                },
            },
            "Reports/layout-manifest.json",
        )
    return 1 if valid_membership else 0


def _actual_semantics(manifest: dict, brief: dict, issues: list[dict]) -> dict:
    assemblies = manifest.get("assemblies", {})
    ceiling_members = assemblies.get("ceiling_ring", {}).get("members", [])
    ceiling_lowest = None
    if ceiling_members:
        ceiling_lowest = ceiling_members[0].get("measured_world_bounds_m", {}).get("min", [None, None, None])[2]
    return {
        "room_clear_m": manifest.get("room", {}).get("clear_dimensions_m"),
        "nominal_height_m": manifest.get("room", {}).get("nominal_height_m"),
        "navigation_table_diameter_m": _navigation_diameter(manifest),
        "cryopod_count": assemblies.get("cryopods", {}).get("count"),
        "cryopod_recline_degrees": assemblies.get("cryopods", {}).get("recline_degrees"),
        "construct_dock_count": assemblies.get("construct_docks", {}).get("count"),
        "navigation_table_count": _navigation_table_count(manifest, issues),
        "minimum_main_path_width_m": brief.get("Room", {}).get("MainPathMinWidthM"),
        "ceiling_ring_lowest_z_m": ceiling_lowest,
    }


def _validate_semantics(actual: dict, issues: list[dict]) -> None:
    artifacts = {
        "minimum_main_path_width_m": "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json"
    }
    for rule_name, expected in EXPECTED.items():
        observed = actual.get(rule_name)
        if observed != expected:
            _issue(
                issues,
                f"SEMANTIC_{rule_name.upper()}",
                expected,
                observed,
                artifacts.get(rule_name, "Reports/layout-manifest.json"),
            )


def _validate_png(path: Path, relative: str, issues: list[dict]) -> None:
    try:
        with Image.open(path) as image:
            image.load()
            observed_size = image.size
            observed_format = image.format
        if observed_format != "PNG":
            _issue(issues, "PNG_FORMAT", "PNG", observed_format, relative)
        if observed_size != PNG_SIZE:
            _issue(issues, "PNG_DIMENSIONS", list(PNG_SIZE), list(observed_size), relative)
    except Exception as error:
        _issue(issues, "PNG_DECODE", "Pillow-decodable PNG", str(error), relative)


def _validate_svg(path: Path, relative: str, issues: list[dict]) -> str:
    try:
        root = ElementTree.parse(path).getroot()
        if root.tag.rsplit("}", 1)[-1] != "svg":
            _issue(issues, "SVG_ROOT", "svg", root.tag, relative)
        return ElementTree.tostring(root, encoding="unicode")
    except Exception as error:
        _issue(issues, "SVG_PARSE", "ElementTree-parseable SVG", str(error), relative)
        return ""


def _validate_pdf(path: Path, relative: str, issues: list[dict]) -> tuple[int, str]:
    try:
        reader = PdfReader(path)
        page_count = len(reader.pages)
        if page_count != PDF_PAGE_COUNT:
            _issue(issues, "PDF_PAGE_COUNT", PDF_PAGE_COUNT, page_count, relative)
        text = "\n".join(page.extract_text() or "" for page in reader.pages)
        return page_count, text
    except Exception as error:
        _issue(issues, "PDF_PARSE", "pypdf-parseable PDF", str(error), relative)
        return 0, ""


def _write_json(path: Path, value: dict) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def _write_hashes(output_root: Path, sha_path: Path, artifacts: set[str]) -> int:
    hashed_paths = sorted(
        relative
        for relative in artifacts
        if relative != "Reports/SHA256SUMS.txt" and (output_root / relative).is_file()
    )
    lines = [f"{_sha256(output_root / relative)}  {relative}" for relative in hashed_paths]
    temporary = sha_path.with_name(sha_path.name + ".tmp")
    temporary.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    temporary.replace(sha_path)
    return len(lines)


def main() -> int:
    paths = _resolve_exact_paths(_arguments())
    output_root = paths["output_root"]
    issues: list[dict] = []
    for label in ("brief", "export_report"):
        path = paths[label]
        if not path.is_file() or path.stat().st_size <= 0:
            raise ValueError(f"Required {label} input is missing or empty: {path}")

    manifest_path = output_root / "Reports/layout-manifest.json"
    if not manifest_path.is_file() or manifest_path.stat().st_size <= 0:
        raise ValueError(f"Required manifest is missing or empty: {manifest_path}")
    brief = json.loads(paths["brief"].read_text(encoding="utf-8"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    export_report = json.loads(paths["export_report"].read_text(encoding="utf-8"))
    _validate_input_hashes(
        export_report,
        {
            "BLEND": output_root / "Blender/SM_ShipHub_Complete_White_v1.blend",
            "Manifest": manifest_path,
            "FBX": output_root / "Exports/SM_ShipHub_Complete_White_v1.fbx",
            "GLB": output_root / "Exports/SM_ShipHub_Complete_White_v1.glb",
        },
        issues,
    )

    canonical_pdf = output_root / "Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf"
    canonical_contact = output_root / "Drawings/ProjectRift_ShipHub_ContactSheet_v1.png"
    canonical_hashes = {
        "pdf": _sha256(canonical_pdf) if canonical_pdf.is_file() else "missing",
        "contact": _sha256(canonical_contact) if canonical_contact.is_file() else "missing",
    }
    expected_artifacts = _expected_artifacts(output_root, canonical_hashes)
    generated_during_validation = {
        "Reports/validation-report.json",
        "Reports/SHA256SUMS.txt",
    }
    for relative in sorted(expected_artifacts - generated_during_validation):
        path = output_root / relative
        if not path.is_file():
            _issue(issues, "REQUIRED_FILE", "present regular file", "missing", relative)
        elif path.stat().st_size <= 0:
            _issue(issues, "REQUIRED_FILE_LENGTH", "> 0", path.stat().st_size, relative)

    actual_files = {
        path.relative_to(output_root).as_posix()
        for path in output_root.rglob("*")
        if path.is_file()
    }
    for relative in sorted(actual_files - expected_artifacts):
        _issue(issues, "UNRECOGNIZED_FILE", "recognized final artifact", relative, relative)

    handoff_pdf = next(
        output_root / relative
        for relative in expected_artifacts
        if relative.startswith("Drawings/Handoff/") and relative.endswith(".pdf")
    )
    handoff_contact = next(
        output_root / relative
        for relative in expected_artifacts
        if relative.startswith("Drawings/Handoff/") and relative.endswith(".png")
    )
    if handoff_pdf.is_file() and canonical_pdf.is_file() and _sha256(handoff_pdf) != canonical_hashes["pdf"]:
        _issue(issues, "HANDOFF_PDF_BYTES", canonical_hashes["pdf"], _sha256(handoff_pdf), handoff_pdf.relative_to(output_root).as_posix())
    if handoff_contact.is_file() and canonical_contact.is_file() and _sha256(handoff_contact) != canonical_hashes["contact"]:
        _issue(issues, "HANDOFF_CONTACT_BYTES", canonical_hashes["contact"], _sha256(handoff_contact), handoff_contact.relative_to(output_root).as_posix())

    png_paths = sorted(relative for relative in expected_artifacts if relative.endswith(".png"))
    for relative in png_paths:
        path = output_root / relative
        if path.is_file():
            _validate_png(path, relative, issues)

    marker_sources: list[tuple[str, str]] = []
    svg_paths = sorted(relative for relative in expected_artifacts if relative.endswith(".svg"))
    for relative in svg_paths:
        path = output_root / relative
        if path.is_file():
            marker_sources.append((relative, _validate_svg(path, relative, issues)))

    pdf_page_count = 0
    for relative in sorted(relative for relative in expected_artifacts if relative.endswith(".pdf")):
        path = output_root / relative
        if path.is_file():
            page_count, text = _validate_pdf(path, relative, issues)
            marker_sources.append((relative, text))
            if relative == "Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf":
                pdf_page_count = page_count

    sheet_ids = list(manifest.get("sheet_ids", []))
    if len(sheet_ids) != len(set(sheet_ids)):
        _issue(issues, "DUPLICATE_SHEET_IDS", "fifteen unique IDs", sheet_ids, "Reports/layout-manifest.json")
    if sheet_ids != list(SHEET_IDS):
        _issue(issues, "SHEET_IDS", list(SHEET_IDS), sheet_ids, "Reports/layout-manifest.json")

    actual_brief_hash = _sha256(paths["brief"])
    manifest_brief_hash = manifest.get("source_brief", {}).get("sha256")
    if manifest_brief_hash != actual_brief_hash:
        _issue(issues, "BRIEF_SHA256", actual_brief_hash, manifest_brief_hash, "Reports/layout-manifest.json")

    manifest_bounds = _manifest_export_bounds(manifest, issues)
    source_validation_bounds = export_report.get("Source", {}).get("BoundsM")
    if not _bounds_match(source_validation_bounds, manifest_bounds):
        _issue(issues, "SOURCE_BOUNDS", {"BoundsM": manifest_bounds, "ToleranceM": BOUNDS_TOLERANCE_M}, source_validation_bounds, "Reports/export-validation.json")
    expected_export_count = len(manifest.get("exports", {}).get("expected_object_names", []))
    for export_format in ("FBX", "GLB"):
        export_data = export_report.get(export_format, {})
        relative = f"Exports/SM_ShipHub_Complete_White_v1.{export_format.lower()}"
        if export_data.get("Passed") is not True:
            _issue(issues, f"{export_format}_REIMPORT_PASSED", True, export_data.get("Passed"), "Reports/export-validation.json")
        if export_data.get("ObjectCount") != expected_export_count:
            _issue(issues, f"{export_format}_OBJECT_COUNT", expected_export_count, export_data.get("ObjectCount"), "Reports/export-validation.json")
        if not _bounds_match(export_data.get("BoundsM"), manifest_bounds):
            _issue(issues, f"{export_format}_BOUNDS", {"BoundsM": manifest_bounds, "ToleranceM": BOUNDS_TOLERANCE_M}, export_data.get("BoundsM"), relative)
        export_path = output_root / relative
        if not export_path.is_file() or export_path.stat().st_size <= 0:
            _issue(issues, f"{export_format}_FILE", "non-empty", "missing or empty", relative)
    if export_report.get("Passed") is not True:
        _issue(issues, "EXPORT_VALIDATION_PASSED", True, export_report.get("Passed"), "Reports/export-validation.json")

    actual_semantics = _actual_semantics(manifest, brief, issues)
    _validate_semantics(actual_semantics, issues)

    marker_sources.extend(
        (
            relative,
            (output_root / relative).read_text(encoding="utf-8"),
        )
        for relative in ("Reports/layout-manifest.json", "Reports/export-validation.json")
    )
    marker_sources.append((paths["brief"].relative_to(paths["project_root"]).as_posix(), paths["brief"].read_text(encoding="utf-8")))
    for relative, text in marker_sources:
        match = UNRESOLVED_MARKERS.search(text)
        if match:
            _issue(issues, "UNRESOLVED_MARKER", "no unresolved marker strings", match.group(0), relative)

    dimensions = {
        "RoomClearM": actual_semantics["room_clear_m"],
        "NominalHeightM": actual_semantics["nominal_height_m"],
        "NavigationTableDiameterM": actual_semantics["navigation_table_diameter_m"],
        "CryopodReclineDegrees": actual_semantics["cryopod_recline_degrees"],
        "MinimumMainPathWidthM": actual_semantics["minimum_main_path_width_m"],
        "CeilingRingLowestZM": actual_semantics["ceiling_ring_lowest_z_m"],
    }
    report = {
        "Schema": "projectrift.shiphub.package-validation.v1",
        "Passed": len(issues) == 0,
        "IssueCount": len(issues),
        "Issues": issues,
        "SheetCount": len(sheet_ids),
        "PdfPageCount": pdf_page_count,
        "CryopodCount": actual_semantics["cryopod_count"],
        "ConstructDockCount": actual_semantics["construct_dock_count"],
        "NavigationTableCount": actual_semantics["navigation_table_count"],
        "Dimensions": dimensions,
        "PngCount": len(png_paths),
        "SvgCount": len(svg_paths),
        "ExpectedArtifactCount": len(expected_artifacts),
        "BriefSHA256": actual_brief_hash,
        "ManifestExportBoundsM": manifest_bounds,
        "ExportValidationPassed": export_report.get("Passed") is True,
    }
    _write_json(paths["report"], report)
    hash_count = _write_hashes(output_root, paths["sha256"], expected_artifacts)
    print(
        f"ShipHub package validation: {'PASS' if report['Passed'] else 'FAIL'} "
        f"({report['IssueCount']} issues; {hash_count} SHA-256 entries)."
    )
    return 0 if report["Passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"ShipHub package validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
