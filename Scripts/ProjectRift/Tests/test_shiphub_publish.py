import hashlib
import json
import re
import subprocess
import sys
import tempfile
import unittest
from math import cos, radians, sin
from pathlib import Path
from unittest.mock import patch
from xml.etree import ElementTree

from PIL import Image, ImageChops
from pypdf import PdfReader

from Scripts.ProjectRift.ArtPipeline.shiphub import publish_shiphub_drawings as publisher
from Scripts.ProjectRift.ArtPipeline.shiphub.shiphub_dimensions import AngularDimension, LinearDimension, ScheduleNote, SHEET_ANNOTATIONS, annotation_specs
from Scripts.ProjectRift.ArtPipeline.shiphub.publish_shiphub_drawings import _commit, publish_package


SHEET_IDS = (
    "A01_FloorPlan", "A02_ReflectedCeilingPlan", "A03_NorthElevation",
    "A04_SouthElevation", "A05_WestElevation", "A06_EastElevation",
    "A07_LongitudinalSection", "A08_TransverseSection", "A09_ExplodedModulePlan",
    "A10_PerspectiveSheet", "D01_Cryopod", "D02_NavigationTable",
    "D03_MainAirlock", "D04_ConstructDock", "D05_WallBayInterface",
)


class ShipHubPublishTests(unittest.TestCase):
    def test_publisher_script_supports_the_runner_file_path_contract(self) -> None:
        """Removing direct-script import compatibility must break the runner contract."""
        script = (
            Path(__file__).parents[1]
            / "ArtPipeline"
            / "shiphub"
            / "publish_shiphub_drawings.py"
        )
        result = subprocess.run(
            [sys.executable, str(script), "--help"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn("Publish ProjectRift Ship Hub A3 drawing package.", result.stdout)

    @staticmethod
    def _image_size(path: Path) -> tuple[int, int]:
        with Image.open(path) as image:
            return image.size

    @staticmethod
    def _annotation_orange_pixels(
        path: Path, view_box: tuple[float, float, float, float] | None = None
    ) -> int:
        with Image.open(path) as source:
            image = source.convert("RGB")
        if view_box is not None:
            scale_x, scale_y = image.width / 420.0, image.height / 297.0
            image = image.crop(
                tuple(
                    round(value * (scale_x if index % 2 == 0 else scale_y))
                    for index, value in enumerate(view_box)
                )
            )
        red, green, blue = image.split()
        red = red.point(lambda value: 255 if value == 239 else 0)
        green = green.point(lambda value: 255 if value == 176 else 0)
        blue = blue.point(lambda value: 255 if value == 90 else 0)
        mask = ImageChops.multiply(ImageChops.multiply(red, green), blue)
        return mask.histogram()[255]

    @staticmethod
    def _jpeg_annotation_orange_pixels(
        image: Image.Image, view_box: tuple[float, float, float, float]
    ) -> int:
        rgb = image.convert("RGB")
        scale_x, scale_y = rgb.width / 420.0, rgb.height / 297.0
        rgb = rgb.crop(
            tuple(
                round(value * (scale_x if index % 2 == 0 else scale_y))
                for index, value in enumerate(view_box)
            )
        )
        red, green, blue = rgb.split()
        masks = (
            red.point(lambda value: 255 if value >= 180 else 0),
            green.point(lambda value: 255 if 90 <= value <= 220 else 0),
            blue.point(lambda value: 255 if value <= 150 else 0),
            ImageChops.subtract(red, green).point(
                lambda value: 255 if value >= 25 else 0
            ),
            ImageChops.subtract(green, blue).point(
                lambda value: 255 if value >= 20 else 0
            ),
        )
        mask = masks[0]
        for component in masks[1:]:
            mask = ImageChops.multiply(mask, component)
        return mask.histogram()[255]

    @staticmethod
    def _write_inputs(root: Path) -> tuple[Path, Path, Path]:
        brief_path = root / "brief.json"
        manifest_path = root / "layout-manifest.json"
        drawings_root = root / "Drawings"
        png_root = drawings_root / "PNG"
        perspectives_root = png_root / "Perspectives"
        perspectives_root.mkdir(parents=True)
        brief_path.write_text('{"name": "synthetic"}', encoding="utf-8")
        manifest_path.write_text(
            json.dumps({"sheet_ids": list(SHEET_IDS)}), encoding="utf-8"
        )
        for sheet_id in SHEET_IDS:
            Image.new("RGB", (1600, 900), "#17212b").save(
                png_root / f"{sheet_id}_Base.png"
            )
        for name in (
            "front.png", "reverse.png", "west-oblique.png", "east-oblique.png",
            "high-overview.png", "ceiling-low-angle.png",
        ):
            Image.new("RGB", (1600, 900), "#17212b").save(perspectives_root / name)
        return brief_path, manifest_path, drawings_root

    @staticmethod
    def _write_commit_fixture(root: Path) -> tuple[Path, Path, dict[str, str]]:
        drawings_root = root / "Drawings"
        drawings_root.mkdir()
        stage = drawings_root / ".stage"
        old_values = {
            "SVG": "old-svg",
            "FinalPNG": "old-png",
            "ProjectRift_ShipHub_CompleteDesign_v1.pdf": "old-pdf",
            "ProjectRift_ShipHub_ContactSheet_v1.png": "old-contact",
            "Handoff": "old-handoff",
        }
        new_values = {
            "SVG": "new-svg",
            "FinalPNG": "new-png",
            "ProjectRift_ShipHub_CompleteDesign_v1.pdf": "new-pdf",
            "ProjectRift_ShipHub_ContactSheet_v1.png": "new-contact",
            "Handoff": "new-handoff",
        }
        for name, value in old_values.items():
            old_path, new_path = drawings_root / name, stage / name
            if name in {"SVG", "FinalPNG", "Handoff"}:
                old_path.mkdir(parents=True)
                new_path.mkdir(parents=True)
                (old_path / "payload.txt").write_text(value, encoding="utf-8")
                (new_path / "payload.txt").write_text(new_values[name], encoding="utf-8")
            else:
                new_path.parent.mkdir(parents=True, exist_ok=True)
                old_path.write_text(value, encoding="utf-8")
                new_path.write_text(new_values[name], encoding="utf-8")
        return stage, drawings_root, old_values

    def _assert_old_commit_fixture(self, drawings_root: Path, old_values: dict[str, str]) -> None:
        for name, value in old_values.items():
            path = drawings_root / name
            if path.is_dir():
                path = path / "payload.txt"
            self.assertEqual(value, path.read_text(encoding="utf-8"), name)

    def test_all_sheets_use_semantically_oriented_annotations(self) -> None:
        """Replacing any explicit sheet layout with the generic grid must fail."""
        expected = {
            "A02_ReflectedCeilingPlan": (("ring-outer", "diameter"), ("ring-inner", "diameter")),
            "A03_NorthElevation": (("overall-width", "horizontal"), ("clear-height", "vertical")),
            "A04_SouthElevation": (("overall-width", "horizontal"), ("clear-height", "vertical")),
            "A05_WestElevation": (("overall-depth", "horizontal"), ("clear-height", "vertical")),
            "A06_EastElevation": (("overall-depth", "horizontal"), ("clear-height", "vertical")),
            "A07_LongitudinalSection": (("section-length", "horizontal"), ("clear-height", "vertical")),
            "A08_TransverseSection": (("section-length", "horizontal"), ("clear-height", "vertical")),
            "A09_ExplodedModulePlan": (("ring-lift", "vertical"), ("structural-bay", "horizontal")),
            "A10_PerspectiveSheet": (),
            "D02_NavigationTable": (("table-diameter", "diameter"), ("display-diameter", "diameter"), ("table-height", "vertical")),
            "D03_MainAirlock": (("opening-width", "horizontal"), ("opening-height", "vertical"), ("airlock-depth", "horizontal")),
            "D04_ConstructDock": (("dock-diameter", "diameter"), ("recess-depth", "vertical")),
            "D05_WallBayInterface": (("bay-width-chain", "horizontal"), ("wall-thickness", "horizontal")),
        }
        for sheet_id, wanted in expected.items():
            actual = tuple(
                (spec.dimension_id, spec.orientation)
                for spec in annotation_specs(sheet_id)
                if isinstance(spec, LinearDimension)
            )
            self.assertEqual(wanted, actual, sheet_id)

    def test_d01_angles_are_arcs_and_a10_has_no_dimension_chains(self) -> None:
        """Flattening D01 angles to horizontal chains must fail this semantic contract."""
        d01 = annotation_specs("D01_Cryopod")
        self.assertEqual(
            {"recline-angle", "door-angle"},
            {item.dimension_id for item in d01 if isinstance(item, AngularDimension)},
        )
        self.assertFalse(annotation_specs("A10_PerspectiveSheet"))

    def test_explicit_annotation_artifacts_stay_in_their_zones_and_avoid_duplicates(self) -> None:
        """Duplicating a label or returning to grid/title-block dimensions must fail."""
        ns = "{http://www.w3.org/2000/svg}"
        fixed_positions = {
            "A02_ReflectedCeilingPlan": {"ring-outer": ("150", "135", "270", "135"), "ring-inner": ("172", "145", "248", "145")},
            "A03_NorthElevation": {"overall-width": ("68", "238", "352", "238"), "clear-height": ("43", "58", "43", "218")},
            "A04_SouthElevation": {"overall-width": ("68", "238", "352", "238"), "clear-height": ("43", "58", "43", "218")},
            "A05_WestElevation": {"overall-depth": ("76", "238", "344", "238"), "clear-height": ("43", "58", "43", "218")},
            "A06_EastElevation": {"overall-depth": ("76", "238", "344", "238"), "clear-height": ("43", "58", "43", "218")},
            "A07_LongitudinalSection": {"section-length": ("72", "238", "348", "238"), "clear-height": ("43", "58", "43", "218")},
            "A08_TransverseSection": {"section-length": ("72", "238", "348", "238"), "clear-height": ("43", "58", "43", "218")},
            "A09_ExplodedModulePlan": {"structural-bay": ("82", "228", "146", "228"), "ring-lift": ("365", "68", "365", "210")},
            "D01_Cryopod": {"pod-width": ("74", "222", "132", "222"), "pod-length": ("158", "222", "262", "222"), "pod-height": ("54", "82", "54", "202")},
            "D02_NavigationTable": {"table-diameter": ("54", "136", "154", "136"), "display-diameter": ("280", "136", "362", "136"), "table-height": ("204", "114", "204", "190")},
            "D03_MainAirlock": {"opening-width": ("54", "218", "142", "218"), "airlock-depth": ("185", "218", "244", "218"), "opening-height": ("42", "94", "42", "196")},
            "D04_ConstructDock": {"dock-diameter": ("148", "146", "204", "146"), "recess-depth": ("270", "142", "270", "183")},
            "D05_WallBayInterface": {"bay-width-chain": ("68", "224", "330", "224"), "wall-thickness": ("338", "145", "382", "145")},
        }
        perimeter_sheets = {
            "A03_NorthElevation", "A04_SouthElevation", "A05_WestElevation",
            "A06_EastElevation", "A07_LongitudinalSection", "A08_TransverseSection",
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
            publish_package(brief, manifest, drawings)
            self.assertEqual(set(SHEET_IDS), set(SHEET_ANNOTATIONS))
            self.assertIn("A10_PerspectiveSheet", SHEET_ANNOTATIONS)
            self.assertEqual((), annotation_specs("A10_PerspectiveSheet"))
            self.assertEqual((), annotation_specs("Unknown_Sheet"))
            for sheet_id in SHEET_IDS:
                sheet = ElementTree.parse(drawings / "SVG" / f"{sheet_id}.svg").getroot()
                rendered_text = "".join(sheet.itertext())
                self.assertNotIn("\u8134", rendered_text, sheet_id)
                self.assertNotIn("\u63b3", rendered_text, sheet_id)
                self.assertNotIn("\ufffd", rendered_text, sheet_id)
                self.assertLessEqual(len(sheet.findall(f".//{ns}g[@data-kind='overall']")), 2, sheet_id)
                self.assertEqual(len({group.attrib["data-label"] for group in sheet.findall(f".//{ns}g[@class='dimension-chain']")}), len(sheet.findall(f".//{ns}g[@class='dimension-chain']")), sheet_id)
                self.assertLessEqual(len(sheet.findall(f".//{ns}g[@class='dimension-chain']")), 4, sheet_id)
                title_block = sheet.find(f".//{ns}rect[@id='title-block']")
                self.assertIsNotNone(title_block)
                title_top = float(title_block.attrib["y"])
                dimension_text = sheet.findall(f".//{ns}text[@class='dimension']")
                self.assertTrue(all(float(text.attrib["y"]) < title_top for text in dimension_text), sheet_id)
                if sheet_id in perimeter_sheets:
                    self.assertEqual(2, len(sheet.findall(f".//{ns}g[@class='dimension-chain']")), sheet_id)
                for dimension_id, expected_line in fixed_positions.get(sheet_id, {}).items():
                    group = sheet.find(f".//{ns}g[@data-dimension-id='{dimension_id}']")
                    self.assertIsNotNone(group, f"{sheet_id}:{dimension_id}")
                    line = group.find(f"{ns}line[@class='dimension-line']")
                    self.assertIsNotNone(line, f"{sheet_id}:{dimension_id}")
                    self.assertEqual(expected_line, tuple(line.attrib[key] for key in ("x1", "y1", "x2", "y2")))

            a01 = ElementTree.parse(drawings / "SVG" / "A01_FloorPlan.svg").getroot()
            a10 = ElementTree.parse(drawings / "SVG" / "A10_PerspectiveSheet.svg").getroot()
            d01 = ElementTree.parse(drawings / "SVG" / "D01_Cryopod.svg").getroot()
            self.assertEqual(1, "".join(a01.itertext()).count("28 m OVERALL WIDTH"))
            self.assertEqual(0, len(a10.findall(f".//{ns}g[@class='dimension-chain']")))
            d05 = ElementTree.parse(drawings / "SVG" / "D05_WallBayInterface.svg").getroot()
            bay_ticks = d05.findall(f".//{ns}g[@data-dimension-id='bay-width-chain']/{ns}line[@class='chain-tick']")
            self.assertEqual(
                (("105.4", "221", "105.4", "227"), ("180.3", "221", "180.3", "227")),
                tuple(tuple(tick.attrib[key] for key in ("x1", "y1", "x2", "y2")) for tick in bay_ticks),
            )
            for tick in bay_ticks:
                self.assertIn("stroke", tick.attrib)
                self.assertIn("stroke-width", tick.attrib)
                self.assertNotEqual("none", tick.attrib.get("stroke"))
                self.assertGreater(float(tick.attrib["stroke-width"]), 0)
            tick_xs = (68.0, *(float(tick.attrib["x1"]) for tick in bay_ticks), 330.0)
            tick_segments = tuple(tick_xs[index + 1] - tick_xs[index] for index in range(3))
            self.assertAlmostEqual(2.0, tick_segments[1] / tick_segments[0], places=2)
            self.assertAlmostEqual(4.0, tick_segments[2] / tick_segments[0], places=2)

            d02 = ElementTree.parse(drawings / "SVG" / "D02_NavigationTable.svg").getroot()
            d03 = ElementTree.parse(drawings / "SVG" / "D03_MainAirlock.svg").getroot()
            d04 = ElementTree.parse(drawings / "SVG" / "D04_ConstructDock.svg").getroot()
            self.assertIsNone(d02.find(f".//{ns}g[@id='keyed-notes']"))
            self.assertEqual(0, len(d02.findall(f".//{ns}g[@class='schedule-note']")))
            for sheet, note_id, key, label in (
                (d03, "muster-area", "M", "8 x 5 m muster area"),
                (d04, "dock-locations", "D1-D4", "4 dock locations"),
            ):
                note = sheet.find(f".//{ns}g[@data-dimension-id='{note_id}']")
                self.assertIsNotNone(note)
                self.assertEqual("schedule", note.attrib["data-kind"])
                note_text = "".join(note.itertext())
                self.assertIn(key, note_text)
                self.assertIn(label, note_text)

            for angle_id, spec in (("recline-angle", next(item for item in annotation_specs("D01_Cryopod") if isinstance(item, AngularDimension) and item.dimension_id == "recline-angle")), ("door-angle", next(item for item in annotation_specs("D01_Cryopod") if isinstance(item, AngularDimension) and item.dimension_id == "door-angle"))):
                arc = d01.find(f".//{ns}path[@data-dimension-id='{angle_id}']")
                self.assertIsNotNone(arc)
                self.assertEqual("angular", arc.attrib["data-kind"])
                self.assertEqual("angular", arc.attrib["data-orientation"])
                self.assertIn("stroke", arc.attrib)
                self.assertIn("stroke-width", arc.attrib)
                self.assertNotEqual("none", arc.attrib.get("stroke"))
                self.assertGreater(float(arc.attrib["stroke-width"]), 0)
                self.assertEqual((f"{spec.center[0]:g}", f"{spec.center[1]:g}"), (arc.attrib["data-center-x"], arc.attrib["data-center-y"]))
                path = re.fullmatch(r"M ([^ ]+) ([^ ]+) A ([^ ]+) ([^ ]+) 0 ([01]) ([01]) ([^ ]+) ([^ ]+)", arc.attrib["d"])
                self.assertIsNotNone(path)
                start_x, start_y, radius_x, radius_y, large_arc, sweep, end_x, end_y = path.groups()
                self.assertAlmostEqual(spec.radius, float(radius_x), places=3)
                self.assertAlmostEqual(spec.radius, float(radius_y), places=3)
                self.assertEqual(str(int(abs(spec.end_degrees - spec.start_degrees) > 180)), large_arc)
                self.assertEqual(str(int(spec.end_degrees > spec.start_degrees)), sweep)
                expected_start = (spec.center[0] + spec.radius * cos(radians(spec.start_degrees)), spec.center[1] + spec.radius * sin(radians(spec.start_degrees)))
                expected_end = (spec.center[0] + spec.radius * cos(radians(spec.end_degrees)), spec.center[1] + spec.radius * sin(radians(spec.end_degrees)))
                self.assertAlmostEqual(expected_start[0], float(start_x), places=3)
                self.assertAlmostEqual(expected_start[1], float(start_y), places=3)
                self.assertAlmostEqual(expected_end[0], float(end_x), places=3)
                self.assertAlmostEqual(expected_end[1], float(end_y), places=3)

    def test_publisher_creates_parseable_fifteen_sheet_book_from_base_renders(self) -> None:
        """Removing any drawing-sheet publish step must leave this package incomplete."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            brief_path, manifest_path, drawings_root = self._write_inputs(root)

            result = publish_package(brief_path, manifest_path, drawings_root)

            self.assertEqual(15, len(result.svg_paths))
            self.assertEqual(15, len(result.png_paths))
            self.assertEqual(15, len(PdfReader(result.pdf_path).pages))
            for path in result.svg_paths:
                self.assertEqual(
                    "{http://www.w3.org/2000/svg}svg", ElementTree.parse(path).getroot().tag
                )

    def test_publisher_uses_the_frozen_pdf_and_contact_sheet_filenames(self) -> None:
        """Changing either frozen delivery filename must make the published result fail this contract."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief_path, manifest_path, drawings_root = self._write_inputs(Path(temporary_directory))
            result = publish_package(brief_path, manifest_path, drawings_root)

            self.assertEqual("ProjectRift_ShipHub_CompleteDesign_v1.pdf", result.pdf_path.name)
            self.assertEqual("ProjectRift_ShipHub_ContactSheet_v1.png", result.contact_sheet_path.name)
            self.assertTrue(result.pdf_path.is_file())
            self.assertTrue(result.contact_sheet_path.is_file())

    def test_publish_creates_stable_content_hashed_handoff_copies(self) -> None:
        """Changing output bytes, names, or handoff copies must fail this delivery contract."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
            first = publish_package(brief, manifest, drawings)
            first_pdf_bytes = first.pdf_path.read_bytes()
            first_png_bytes = first.contact_sheet_path.read_bytes()
            first_names = (first.handoff_pdf_path.name, first.handoff_contact_sheet_path.name)

            second = publish_package(brief, manifest, drawings)
            self.assertEqual(first_pdf_bytes, second.pdf_path.read_bytes())
            self.assertEqual(first_png_bytes, second.contact_sheet_path.read_bytes())
            self.assertEqual(first_names, (second.handoff_pdf_path.name, second.handoff_contact_sheet_path.name))

            pdf_sha8 = hashlib.sha256(first_pdf_bytes).hexdigest()[:8]
            png_sha8 = hashlib.sha256(first_png_bytes).hexdigest()[:8]
            self.assertEqual(f"ProjectRift_ShipHub_CompleteDesign_v1r2_{pdf_sha8}.pdf", first_names[0])
            self.assertEqual(f"ProjectRift_ShipHub_ContactSheet_v1r2_{png_sha8}.png", first_names[1])
            self.assertEqual(first_pdf_bytes, first.handoff_pdf_path.read_bytes())
            self.assertEqual(first_png_bytes, first.handoff_contact_sheet_path.read_bytes())
            self.assertEqual(2, len(list((drawings / "Handoff").iterdir())))

    def test_publisher_emits_dimensioned_a3_contract_artifacts(self) -> None:
        """Removing dimension graphics or contractual A3 metadata must fail this published package."""
        namespace = "{http://www.w3.org/2000/svg}"
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief_path, manifest_path, drawings_root = self._write_inputs(Path(temporary_directory))
            result = publish_package(brief_path, manifest_path, drawings_root)

            a01 = ElementTree.parse(drawings_root / "SVG" / "A01_FloorPlan.svg").getroot()
            d01 = ElementTree.parse(drawings_root / "SVG" / "D01_Cryopod.svg").getroot()
            a10 = ElementTree.parse(drawings_root / "SVG" / "A10_PerspectiveSheet.svg").getroot()
            self.assertEqual("0 0 420 297", a01.attrib["viewBox"])
            self.assertEqual("../PNG/A01_FloorPlan_Base.png", a01.find(f"{namespace}image").attrib["href"])
            self.assertIsNotNone(a01.find(f".//{namespace}marker[@id='dimension-arrow']"))
            self.assertEqual(4, len(a01.findall(f".//{namespace}line[@class='extension-line']")))
            self.assertEqual(3, len(a01.findall(f".//{namespace}line[@class='dimension-line']")))
            self.assertEqual(6, len(d01.findall(f".//{namespace}line[@class='extension-line']")))
            self.assertEqual(3, len(d01.findall(f".//{namespace}line[@class='dimension-line']")))
            self.assertEqual(2, len(d01.findall(f".//{namespace}path[@class='angular-dimension']")))
            a01_text = "".join(a01.itertext())
            d01_text = "".join(d01.itertext())
            for literal in ("28 m OVERALL WIDTH", "24 m OVERALL DEPTH", "8 m NAVIGATION TABLE", "MAIN PATH CLEARANCE: 5 m", "P1-P5", "D1-D4"):
                self.assertIn(literal, a01_text)
            for literal in ("1.6 x 1.6 x 3 m", "18 degrees recline", "75-degree door angle", "1.2 m envelope"):
                self.assertIn(literal, d01_text)
            perspective_images = [
                image for image in a10.findall(f"{namespace}image")
                if image.attrib["href"].startswith("../PNG/Perspectives/")
            ]
            self.assertEqual(6, len(perspective_images))
            self.assertEqual(("20", "38"), (perspective_images[0].attrib["x"], perspective_images[0].attrib["y"]))
            self.assertEqual(("270", "118"), (perspective_images[5].attrib["x"], perspective_images[5].attrib["y"]))
            self.assertTrue(all(self._image_size(path) == (4961, 3508) for path in result.png_paths))
            self.assertEqual((4961, 3508), self._image_size(result.contact_sheet_path))
            final_pngs = {path.stem: path for path in result.png_paths}
            for sheet_id, region in {
                "A01_FloorPlan": (40, 40, 408, 252),
                "A02_ReflectedCeilingPlan": (145, 120, 275, 165),
                "D01_Cryopod": (40, 75, 365, 252),
                "D03_MainAirlock": (30, 85, 330, 252),
            }.items():
                self.assertGreater(
                    self._annotation_orange_pixels(final_pngs[sheet_id], region),
                    1000,
                    sheet_id,
                )
            self.assertGreater(
                self._annotation_orange_pixels(
                    final_pngs["A01_FloorPlan"], (260, 218, 408, 239)
                ),
                100,
                "A01 schedule layer",
            )
            self.assertGreater(
                self._annotation_orange_pixels(
                    final_pngs["A01_FloorPlan"], (60, 240, 360, 250)
                ),
                1000,
                "A01 overall-width layer",
            )
            self.assertEqual(
                0,
                self._annotation_orange_pixels(final_pngs["A10_PerspectiveSheet"]),
                "A10 must not gain an engineering annotation layer",
            )
            reader = PdfReader(result.pdf_path)
            self.assertEqual("ProjectRift Ship Hub Complete Modeling Drawings v1", reader.metadata.title)
            self.assertEqual("ProjectRift Project-Owned Art Pipeline", reader.metadata.author)

    def test_a01_dimensions_stay_outside_the_plan_and_use_keyed_schedules(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
            publish_package(brief, manifest, drawings)
            root = ElementTree.parse(drawings / "SVG" / "A01_FloorPlan.svg").getroot()
            ns = "{http://www.w3.org/2000/svg}"

            width = root.find(f".//{ns}g[@data-dimension-id='room-width']")
            depth = root.find(f".//{ns}g[@data-dimension-id='room-depth']")
            table = root.find(f".//{ns}g[@data-dimension-id='table-diameter']")
            self.assertIsNotNone(width)
            self.assertIsNotNone(depth)
            self.assertIsNotNone(table)

            width_line = width.find(f"{ns}line[@class='dimension-line']")
            depth_line = depth.find(f"{ns}line[@class='dimension-line']")
            self.assertEqual(width_line.attrib["y1"], width_line.attrib["y2"])
            self.assertGreater(float(width_line.attrib["y1"]), 218.0)
            self.assertEqual(depth_line.attrib["x1"], depth_line.attrib["x2"])
            self.assertLess(float(depth_line.attrib["x1"]), 48.0)
            self.assertEqual("diameter", table.attrib["data-orientation"])

            schedule_baselines = [
                float(group.findall(f"{ns}text")[-1].attrib["y"])
                for group in root.findall(f".//{ns}g[@class='schedule-note']")
            ]
            self.assertGreaterEqual(
                float(width_line.attrib["y1"]) - max(schedule_baselines),
                11.0,
            )

            source_font, _ = publisher._font()
            annotation_font = source_font.font_variant(size=round(4.0 * 3508 / 297))
            pixels_per_a3_unit = 4961 / 420.0
            schedule_specs = [
                specification
                for specification in annotation_specs("A01_FloorPlan")
                if isinstance(specification, ScheduleNote)
            ]
            right_extension_x = float(width_line.attrib["x2"])
            for specification in schedule_specs:
                rendered_width = annotation_font.getlength(
                    f"{specification.key}: {specification.label}"
                ) / pixels_per_a3_unit
                self.assertLessEqual(
                    specification.text_at[0] + rendered_width,
                    right_extension_x - 8.0,
                    specification.note_id,
                )

            chains = " ".join(group.attrib.get("data-label", "") for group in root.findall(f".//{ns}g[@class='dimension-chain']"))
            self.assertNotIn("Pod centers", chains)
            self.assertNotIn("Dock coordinates", chains)
            schedule_text = "".join(root.find(f".//{ns}g[@id='keyed-notes']").itertext())
            self.assertIn("P1-P5", schedule_text)
            self.assertIn("D1-D4", schedule_text)

    def test_a01_navigation_table_uses_the_standard_diameter_symbol(self) -> None:
        """Replacing the diameter symbol with a CJK glyph must fail the drawing contract."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
            publish_package(brief, manifest, drawings)
            root = ElementTree.parse(drawings / "SVG" / "A01_FloorPlan.svg").getroot()
            ns = "{http://www.w3.org/2000/svg}"

            table = root.find(f".//{ns}g[@data-dimension-id='table-diameter']")
            self.assertIsNotNone(table)
            table_text = "".join(table.itertext())
            self.assertIn("Ø 8 m NAVIGATION TABLE", table_text)
            self.assertNotIn("脴", table_text)

    def test_a01_keyed_notes_clear_the_title_block(self) -> None:
        """Lowering the title block over a keyed-note baseline must fail the layout contract."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
            publish_package(brief, manifest, drawings)
            root = ElementTree.parse(drawings / "SVG" / "A01_FloorPlan.svg").getroot()
            ns = "{http://www.w3.org/2000/svg}"

            keyed_notes = root.find(f".//{ns}g[@id='keyed-notes']")
            title_block = root.find(f".//{ns}rect[@id='title-block']")
            self.assertIsNotNone(keyed_notes)
            self.assertIsNotNone(title_block)
            title_top = float(title_block.attrib["y"])
            title_bottom = title_top + float(title_block.attrib["height"])
            keyed_note_ys = [
                float(text.attrib["y"])
                for text in keyed_notes.findall(f".//{ns}text")
            ]
            self.assertGreaterEqual(title_top - max(keyed_note_ys), 22.0)

            title_text_ys = [
                float(text.attrib["y"])
                for text in root.findall(f".//{ns}text[@class='t']")
                if text.attrib.get("x") in {"15", "150"} and float(text.attrib["y"]) >= title_top
            ]
            self.assertEqual([263.0, 276.0, 276.0, 286.0], title_text_ys)
            self.assertTrue(all(title_top < y < title_bottom for y in title_text_ys))

    def test_publisher_emits_a_wps_compatible_compact_pdf(self) -> None:
        """Re-embedding full-resolution PNGs with ASCII85/Flate must fail the WPS compatibility contract."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief_path, manifest_path, drawings_root = self._write_inputs(Path(temporary_directory))
            result = publish_package(brief_path, manifest_path, drawings_root)

            reader = PdfReader(result.pdf_path, strict=True)
            self.assertEqual(15, len(reader.pages))
            for page in reader.pages:
                images = [
                    reference.get_object()
                    for reference in page["/Resources"].get("/XObject", {}).values()
                    if reference.get_object().get("/Subtype") == "/Image"
                ]
                self.assertEqual(1, len(images))
                filters = images[0].get("/Filter")
                if not isinstance(filters, list):
                    filters = [filters]
                self.assertEqual(["/DCTDecode"], [str(value) for value in filters])
            for page_index, sheet_id, region in (
                (0, "A01_FloorPlan", (40, 130, 408, 252)),
                (10, "D01_Cryopod", (40, 75, 365, 252)),
            ):
                embedded_images = list(reader.pages[page_index].images)
                self.assertEqual(1, len(embedded_images), sheet_id)
                self.assertGreater(
                    self._jpeg_annotation_orange_pixels(
                        embedded_images[0].image, region
                    ),
                    1000,
                    sheet_id,
                )
            self.assertLess(result.pdf_path.stat().st_size, 32 * 1024 * 1024)

    def test_publisher_uses_arial_fallback_for_ascii_sheet_titles(self) -> None:
        """A missing YaHei font must publish real artwork with the allowed Arial fallback."""
        original_paths = publisher.FONT_PATHS
        with tempfile.TemporaryDirectory() as temporary_directory:
            brief_path, manifest_path, drawings_root = self._write_inputs(Path(temporary_directory))
            publisher.FONT_PATHS = (Path(temporary_directory) / "missing-yahei.ttc", Path(r"C:\Windows\Fonts\arial.ttf"))
            try:
                publish_package(brief_path, manifest_path, drawings_root)
            finally:
                publisher.FONT_PATHS = original_paths
            contents = (drawings_root / "SVG" / "A01_FloorPlan.svg").read_text(encoding="utf-8")
            self.assertIn("FLOOR PLAN", contents)
            self.assertNotIn("总平面图", contents)

    def test_commit_failure_restores_every_preexisting_output(self) -> None:
        """A failed Handoff replacement must restore every old published artifact."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            drawings_root = Path(temporary_directory) / "Drawings"
            drawings_root.mkdir()
            stage = drawings_root / ".stage"
            (stage / "SVG").mkdir(parents=True)
            (stage / "SVG" / "new.svg").write_text("new", encoding="utf-8")
            (stage / "FinalPNG").mkdir()
            (stage / "FinalPNG" / "new.png").write_text("new", encoding="utf-8")
            (stage / "ProjectRift_ShipHub_CompleteDesign_v1.pdf").write_text("new-pdf", encoding="utf-8")
            (stage / "ProjectRift_ShipHub_ContactSheet_v1.png").write_text("new-contact", encoding="utf-8")
            (drawings_root / "SVG").mkdir()
            (drawings_root / "SVG" / "old.svg").write_text("old-svg", encoding="utf-8")
            (drawings_root / "FinalPNG").mkdir()
            (drawings_root / "FinalPNG" / "old.png").write_text("old-png", encoding="utf-8")
            (drawings_root / "ProjectRift_ShipHub_CompleteDesign_v1.pdf").write_text("old-pdf", encoding="utf-8")
            (drawings_root / "ProjectRift_ShipHub_ContactSheet_v1.png").write_text("old-contact", encoding="utf-8")
            (stage / "Handoff").mkdir()
            (stage / "Handoff" / "new.pdf").write_text("new-handoff-pdf", encoding="utf-8")
            (stage / "Handoff" / "new.png").write_text("new-handoff-png", encoding="utf-8")
            (drawings_root / "Handoff").mkdir()
            (drawings_root / "Handoff" / "old.pdf").write_text("old-handoff-pdf", encoding="utf-8")
            (drawings_root / "Handoff" / "old.png").write_text("old-handoff-png", encoding="utf-8")
            real_replace = publisher.os.replace

            def fail_handoff_install(source: Path, destination: Path) -> None:
                if source == stage / "Handoff" and destination == drawings_root / "Handoff":
                    raise OSError("injected Handoff install failure")
                real_replace(source, destination)

            with patch.object(publisher.os, "replace", side_effect=fail_handoff_install):
                with self.assertRaisesRegex(OSError, "injected Handoff install failure"):
                    _commit(stage, drawings_root)

            self.assertEqual("old-svg", (drawings_root / "SVG" / "old.svg").read_text(encoding="utf-8"))
            self.assertEqual("old-png", (drawings_root / "FinalPNG" / "old.png").read_text(encoding="utf-8"))
            self.assertEqual("old-pdf", (drawings_root / "ProjectRift_ShipHub_CompleteDesign_v1.pdf").read_text(encoding="utf-8"))
            self.assertEqual("old-contact", (drawings_root / "ProjectRift_ShipHub_ContactSheet_v1.png").read_text(encoding="utf-8"))
            self.assertEqual("old-handoff-pdf", (drawings_root / "Handoff" / "old.pdf").read_text(encoding="utf-8"))
            self.assertEqual("old-handoff-png", (drawings_root / "Handoff" / "old.png").read_text(encoding="utf-8"))
            self.assertFalse(list(drawings_root.glob(".shiphub-publish-backup-*")))

    def test_commit_backup_move_failure_restores_all_old_outputs_without_backup_leak(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            stage, drawings_root, old_values = self._write_commit_fixture(Path(temporary_directory))
            real_replace = publisher.os.replace

            def fail_final_png_backup(source: Path, destination: Path) -> None:
                if source == drawings_root / "FinalPNG":
                    raise OSError("injected backup failure")
                real_replace(source, destination)

            with patch.object(publisher.os, "replace", side_effect=fail_final_png_backup):
                with self.assertRaisesRegex(OSError, "injected backup failure"):
                    _commit(stage, drawings_root)

            self._assert_old_commit_fixture(drawings_root, old_values)
            self.assertFalse(list(drawings_root.glob(".shiphub-publish-backup-*")))

    def test_commit_preserves_transaction_backup_when_installed_output_cannot_be_removed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            stage, drawings_root, _old_values = self._write_commit_fixture(Path(temporary_directory))
            real_replace = publisher.os.replace

            def fail_install_then_remove(source: Path, destination: Path) -> None:
                if source == stage / "FinalPNG" and destination == drawings_root / "FinalPNG":
                    raise OSError("injected install failure")
                if source == drawings_root / "SVG" and destination == stage / "SVG":
                    raise OSError("injected installed-output removal failure")
                real_replace(source, destination)

            with patch.object(publisher.os, "replace", side_effect=fail_install_then_remove):
                with self.assertRaisesRegex(RuntimeError, "Preserved backup directory"):
                    _commit(stage, drawings_root)

            backups = list(drawings_root.glob(".shiphub-publish-backup-*"))
            self.assertEqual(1, len(backups))
            self.assertEqual("old-svg", (backups[0] / "SVG" / "payload.txt").read_text(encoding="utf-8"))

    def test_commit_preserves_transaction_backup_when_backup_restore_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            stage, drawings_root, _old_values = self._write_commit_fixture(Path(temporary_directory))
            real_replace = publisher.os.replace
            backup_path: Path | None = None

            def fail_install_then_restore(source: Path, destination: Path) -> None:
                nonlocal backup_path
                if source == stage / "FinalPNG" and destination == drawings_root / "FinalPNG":
                    raise OSError("injected install failure")
                if source.name == "SVG" and source.parent.name.startswith(".shiphub-publish-backup-"):
                    backup_path = source.parent
                    raise OSError("injected backup restore failure")
                real_replace(source, destination)

            with patch.object(publisher.os, "replace", side_effect=fail_install_then_restore):
                with self.assertRaisesRegex(RuntimeError, "Preserved backup directory") as raised:
                    _commit(stage, drawings_root)

            self.assertIsNotNone(backup_path)
            self.assertIn(str(backup_path), str(raised.exception))
            self.assertEqual("old-svg", (backup_path / "SVG" / "payload.txt").read_text(encoding="utf-8"))

if __name__ == "__main__":
    unittest.main()
