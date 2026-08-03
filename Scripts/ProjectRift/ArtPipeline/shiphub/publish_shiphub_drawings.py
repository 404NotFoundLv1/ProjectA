from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from reportlab import rl_config
from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas

if __package__:
    from .shiphub_dimensions import draw_sheet_annotations, render_sheet_annotations
else:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from shiphub_dimensions import draw_sheet_annotations, render_sheet_annotations


SHEET_IDS = (
    "A01_FloorPlan", "A02_ReflectedCeilingPlan", "A03_NorthElevation",
    "A04_SouthElevation", "A05_WestElevation", "A06_EastElevation",
    "A07_LongitudinalSection", "A08_TransverseSection", "A09_ExplodedModulePlan",
    "A10_PerspectiveSheet", "D01_Cryopod", "D02_NavigationTable",
    "D03_MainAirlock", "D04_ConstructDock", "D05_WallBayInterface",
)
MAX_PDF_BYTES = 32 * 1024 * 1024
PERSPECTIVE_NAMES = (
    "front.png", "reverse.png", "west-oblique.png", "east-oblique.png",
    "high-overview.png", "ceiling-low-angle.png",
)
SHEET_TITLES = {
    "A01_FloorPlan": ("总平面图", "FLOOR PLAN"), "A02_ReflectedCeilingPlan": ("反射天花平面图", "REFLECTED CEILING PLAN"),
    "A03_NorthElevation": ("北立面图", "NORTH ELEVATION"), "A04_SouthElevation": ("南立面图", "SOUTH ELEVATION"),
    "A05_WestElevation": ("西立面图", "WEST ELEVATION"), "A06_EastElevation": ("东立面图", "EAST ELEVATION"),
    "A07_LongitudinalSection": ("纵剖面图", "LONGITUDINAL SECTION"), "A08_TransverseSection": ("横剖面图", "TRANSVERSE SECTION"),
    "A09_ExplodedModulePlan": ("模块分解图", "EXPLODED MODULE PLAN"), "A10_PerspectiveSheet": ("透视图集", "PERSPECTIVE SHEET"),
    "D01_Cryopod": ("冷冻舱详图", "CRYOPOD DETAIL"), "D02_NavigationTable": ("导航台详图", "NAVIGATION TABLE DETAIL"),
    "D03_MainAirlock": ("主气闸详图", "MAIN AIRLOCK DETAIL"), "D04_ConstructDock": ("建造停靠位详图", "CONSTRUCT DOCK DETAIL"),
    "D05_WallBayInterface": ("墙体舱位接口详图", "WALL BAY INTERFACE DETAIL"),
}
SHEET_SCALES = {
    "A01_FloorPlan": "1:100", "A02_ReflectedCeilingPlan": "1:100", "A03_NorthElevation": "1:100",
    "A04_SouthElevation": "1:100", "A05_WestElevation": "1:100", "A06_EastElevation": "1:100",
    "A07_LongitudinalSection": "1:100", "A08_TransverseSection": "1:100", "A09_ExplodedModulePlan": "1:100",
    "A10_PerspectiveSheet": "NTS", "D01_Cryopod": "1:20", "D02_NavigationTable": "1:25",
    "D03_MainAirlock": "1:25", "D04_ConstructDock": "1:20", "D05_WallBayInterface": "1:20",
}
SECTION_MARKS = {
    "A01_FloorPlan": "SECTION A-A / B-B", "A02_ReflectedCeilingPlan": "CEILING SECTION C-C", "A03_NorthElevation": "NORTH DATUM +0.000",
    "A04_SouthElevation": "SOUTH DATUM +0.000", "A05_WestElevation": "WEST DATUM +0.000", "A06_EastElevation": "EAST DATUM +0.000",
    "A07_LongitudinalSection": "SECTION A-A", "A08_TransverseSection": "SECTION B-B", "A09_ExplodedModulePlan": "EXPLODED AXIS Z+",
    "A10_PerspectiveSheet": "PERSPECTIVE INDEX", "D01_Cryopod": "DETAIL 01", "D02_NavigationTable": "DETAIL 02",
    "D03_MainAirlock": "DETAIL 03", "D04_ConstructDock": "DETAIL 04", "D05_WallBayInterface": "DETAIL 05",
}
CANVAS_SIZE = (4961, 3508)
FONT_PATHS = (Path(r"C:\Windows\Fonts\msyh.ttc"), Path(r"C:\Windows\Fonts\arial.ttf"))


@dataclass(frozen=True)
class PublishResult:
    svg_paths: tuple[Path, ...]
    png_paths: tuple[Path, ...]
    pdf_path: Path
    contact_sheet_path: Path
    handoff_pdf_path: Path
    handoff_contact_sheet_path: Path


def _child(path: Path, root: Path) -> Path:
    resolved = path.resolve()
    try:
        resolved.relative_to(root.resolve())
    except ValueError as error:
        raise ValueError(f"Publish path is outside Drawings: {resolved}") from error
    return resolved


def sha256_hex(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _load_inputs(brief_path: Path, manifest_path: Path, drawings_root: Path) -> tuple[dict, str]:
    if not brief_path.is_file() or not manifest_path.is_file():
        raise ValueError("Publish requires an existing brief and measured manifest.")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if tuple(manifest.get("sheet_ids", ())) != SHEET_IDS:
        raise ValueError("Measured manifest must contain the exact fifteen authoritative sheet IDs.")
    drawings_root.mkdir(parents=True, exist_ok=True)
    for sheet_id in SHEET_IDS:
        base = _child(drawings_root / "PNG" / f"{sheet_id}_Base.png", drawings_root)
        if not base.is_file():
            raise ValueError(f"Missing required base render: {base}")
    for name in PERSPECTIVE_NAMES:
        source = _child(drawings_root / "PNG" / "Perspectives" / name, drawings_root)
        if not source.is_file():
            raise ValueError(f"Missing required perspective source: {source}")
    return manifest, hashlib.sha256(brief_path.read_bytes()).hexdigest()


def _font() -> tuple[ImageFont.FreeTypeFont, bool]:
    for candidate in FONT_PATHS:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), 42), candidate.name.lower() == "msyh.ttc"
    raise RuntimeError("Publishing requires C:\\Windows\\Fonts\\msyh.ttc or C:\\Windows\\Fonts\\arial.ttf.")


def _title(sheet_id: str, has_chinese_font: bool) -> str:
    return SHEET_TITLES[sheet_id][0 if has_chinese_font else 1]


def _fit(image: Image.Image, box: tuple[int, int, int, int]) -> tuple[Image.Image, tuple[int, int]]:
    max_width, max_height = box[2] - box[0], box[3] - box[1]
    ratio = min(max_width / image.width, max_height / image.height)
    size = (round(image.width * ratio), round(image.height * ratio))
    return image.resize(size, Image.Resampling.LANCZOS), (box[0] + (max_width - size[0]) // 2, box[1] + (max_height - size[1]) // 2)


def _overlay(draw: ImageDraw.ImageDraw, sheet_id: str, font: ImageFont.FreeTypeFont, title: str) -> None:
    width, height = CANVAS_SIZE
    draw.rectangle((80, 80, width - 80, height - 80), outline="#e8eef5", width=8)
    draw.rectangle((80, height - 490, width - 80, height - 80), outline="#e8eef5", width=5)
    draw.line((160, height - 340, 760, height - 340), fill="#e8eef5", width=5)
    draw.polygon(((230, 280), (180, 410), (280, 410)), outline="#e8eef5", fill="#e8eef5")
    draw.text((185, 430), "N", font=font, fill="#e8eef5")
    draw.rectangle((width - 800, 240, width - 320, 270), outline="#e8eef5", width=3)
    draw.rectangle((width - 800, 240, width - 560, 270), fill="#e8eef5")
    draw.text((width - 800, 280), "5 m SCALE BAR", font=font, fill="#e8eef5")
    draw.text((180, height - 440), f"{sheet_id}  {title}", font=font, fill="#e8eef5")
    draw.text((180, height - 280), f"SCALE {SHEET_SCALES[sheet_id]}   UNITS: m / mm", font=font, fill="#e8eef5")
    draw.text((800, height - 280), SECTION_MARKS[sheet_id], font=font, fill="#e8eef5")


def _contact_sheet(drawings_root: Path, output_path: Path, font: ImageFont.FreeTypeFont) -> None:
    page = Image.new("RGB", CANVAS_SIZE, "#0b1017")
    draw = ImageDraw.Draw(page)
    draw.rectangle((80, 80, CANVAS_SIZE[0] - 80, CANVAS_SIZE[1] - 80), outline="#e8eef5", width=8)
    draw.text((150, 130), "PROJECTRIFT SHIP HUB - PERSPECTIVE CONTACT SHEET", font=font, fill="#e8eef5")
    source_root = drawings_root / "PNG" / "Perspectives"
    for index, name in enumerate(PERSPECTIVE_NAMES):
        column, row = index % 3, index // 3
        box = (150 + column * 1600, 300 + row * 1530, 1600 + column * 1600, 1660 + row * 1530)
        with Image.open(source_root / name) as source:
            rendered, position = _fit(source.convert("RGB"), box)
            page.paste(rendered, position)
        draw.rectangle(box, outline="#e8eef5", width=4)
        draw.text((box[0], box[3] + 15), name.removesuffix(".png").upper(), font=font, fill="#e8eef5")
    page.save(output_path, dpi=(300, 300))


def _final_png(drawings_root: Path, sheet_id: str, output_path: Path, font: ImageFont.FreeTypeFont, chinese: bool, contact_path: Path) -> None:
    page = Image.new("RGB", CANVAS_SIZE, "#0b1017")
    base_path = drawings_root / "PNG" / f"{sheet_id}_Base.png"
    with Image.open(base_path) as base:
        rendered, position = _fit(base.convert("RGB"), (160, 180, 4801, 2940))
        page.paste(rendered, position)
    if sheet_id == "A10_PerspectiveSheet":
        with Image.open(contact_path) as contact:
            rendered, position = _fit(contact.convert("RGB"), (360, 380, 4601, 2750))
            page.paste(rendered, position)
    _overlay(ImageDraw.Draw(page), sheet_id, font, _title(sheet_id, chinese))
    draw_sheet_annotations(page, sheet_id, font)
    page.save(output_path, dpi=(300, 300))


def _svg(sheet_id: str, output_path: Path, chinese: bool, brief_hash: str) -> None:
    title = html.escape(_title(sheet_id, chinese))
    annotation_markup, schedule_markup = render_sheet_annotations(sheet_id)
    perspectives = ""
    if sheet_id == "A10_PerspectiveSheet":
        perspectives = "".join(
            f'<image href="../PNG/Perspectives/{html.escape(name)}" x="{20 + (index % 3) * 125}" y="{38 + (index // 3) * 80}" width="115" height="70" preserveAspectRatio="xMidYMid meet"/>'
            for index, name in enumerate(PERSPECTIVE_NAMES)
        )
    source = html.escape(f"../PNG/{sheet_id}_Base.png")
    output_path.write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 297" width="420mm" height="297mm">
 <style>.t{{font-family:Arial,sans-serif;fill:#e8eef5}}.dimension{{font-family:Arial,sans-serif;fill:#efb05a;font-size:4px}}.extension-line{{stroke:#efb05a;stroke-width:0.45}}.dimension-line{{stroke:#efb05a;stroke-width:0.65}}</style>
 <defs><marker id="dimension-arrow" markerWidth="5" markerHeight="5" refX="2.5" refY="2.5" orient="auto-start-reverse" markerUnits="strokeWidth"><path d="M0,0 L5,2.5 L0,5 Z" fill="#efb05a"/></marker></defs>
 <rect width="420" height="297" fill="#0b1017"/><image href="{source}" x="12" y="14" width="396" height="226" preserveAspectRatio="xMidYMid meet"/>{perspectives}
 {annotation_markup}
 {schedule_markup}
 <rect x="7" y="7" width="406" height="283" fill="none" stroke="#e8eef5" stroke-width="1"/><rect id="title-block" x="7" y="258" width="406" height="32" fill="#0b1017" stroke="#e8eef5" stroke-width="0.7"/>
 <path d="M22 19 L18 29 L26 29 Z" fill="#e8eef5"/><text x="20" y="35" class="t" font-size="5">N</text><path d="M340 20h40m-40 0v4m40-4v4" stroke="#e8eef5" stroke-width="1"/><text x="340" y="31" class="t" font-size="4">5 m SCALE BAR</text>
 <text x="15" y="263" class="t" font-size="6">{html.escape(sheet_id)}  {title}</text><text x="15" y="276" class="t" font-size="4">SCALE {html.escape(SHEET_SCALES[sheet_id])}   UNITS: m / mm</text><text x="150" y="276" class="t" font-size="4">{html.escape(SECTION_MARKS[sheet_id])}</text>
 <text x="15" y="286" class="t" font-size="3">BRIEF SHA256: {html.escape(brief_hash)}</text>
</svg>''', encoding="utf-8")


def _pdf_book(png_paths: tuple[Path, ...], output_path: Path, brief_hash: str) -> None:
    jpeg_root = Path(tempfile.mkdtemp(prefix=".shiphub-pdf-jpeg-", dir=output_path.parent))
    previous_use_a85 = rl_config.useA85
    try:
        rl_config.useA85 = 0
        document = canvas.Canvas(
            str(output_path), pagesize=landscape(A3), pageCompression=1, invariant=1
        )
        document.setTitle("ProjectRift Ship Hub Complete Modeling Drawings v1")
        document.setAuthor("ProjectRift Project-Owned Art Pipeline")
        width, height = landscape(A3)
        for number, png_path in enumerate(png_paths, 1):
            sheet_id = png_path.stem
            jpeg_path = jpeg_root / f"{number:02d}_{sheet_id}.jpg"
            with Image.open(png_path) as source:
                rgb = source.convert("RGB")
                try:
                    rgb.save(
                        jpeg_path,
                        format="JPEG",
                        quality=90,
                        subsampling=0,
                        optimize=True,
                        progressive=False,
                        dpi=(300, 300),
                    )
                finally:
                    rgb.close()
            document.drawImage(ImageReader(str(jpeg_path)), 0, 0, width=width, height=height)
            document.setFillColorRGB(1, 1, 1)
            document.setFont("Helvetica", 6)
            document.drawRightString(width - 12, 8, f"{sheet_id} | {SHEET_SCALES[sheet_id]} | m / mm | {brief_hash} | {number:02d}/15")
            document.showPage()
        document.save()
        if output_path.stat().st_size >= MAX_PDF_BYTES:
            raise RuntimeError(
                f"Published PDF exceeds the {MAX_PDF_BYTES}-byte WPS compatibility limit: "
                f"{output_path.stat().st_size} bytes"
            )
    finally:
        rl_config.useA85 = previous_use_a85
        shutil.rmtree(jpeg_root, ignore_errors=True)


def _stage_handoff(stage: Path, pdf_path: Path, contact_path: Path) -> tuple[Path, Path]:
    handoff = stage / "Handoff"
    handoff.mkdir()
    pdf_copy = handoff / f"ProjectRift_ShipHub_CompleteDesign_v1r2_{sha256_hex(pdf_path)[:8]}.pdf"
    png_copy = handoff / f"ProjectRift_ShipHub_ContactSheet_v1r2_{sha256_hex(contact_path)[:8]}.png"
    shutil.copyfile(pdf_path, pdf_copy)
    shutil.copyfile(contact_path, png_copy)
    return pdf_copy, png_copy


def _enable_stage_tree_inheritance(stage: Path) -> None:
    if os.name != "nt":
        return
    result = subprocess.run(
        [
            "icacls.exe",
            str(stage),
            "/inheritance:e",
            "/T",
            "/Q",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    detail = "\n".join(
        output.strip() for output in (result.stdout, result.stderr) if output.strip()
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Publish stage ACL inheritance failed with code {result.returncode}: {detail}"
        )
    failed_count = re.search(
        r"Failed processing\s+([0-9]+)\s+files?", detail, flags=re.IGNORECASE
    )
    if failed_count and int(failed_count.group(1)) > 0:
        raise RuntimeError(f"Publish stage ACL inheritance reported: {detail}")

    escaped_stage = str(stage.resolve()).replace("'", "''")
    verify_script = f"""
$ErrorActionPreference = 'Stop'
$target = '{escaped_stage}'
$ordinaryUserSids = @('S-1-5-11', 'S-1-5-32-545')
$items = @((Get-Item -LiteralPath $target -Force)) + @(
    Get-ChildItem -LiteralPath $target -Recurse -Force -ErrorAction Stop
)
$failures = @(
    foreach ($item in $items) {{
        $acl = Get-Acl -LiteralPath $item.FullName -ErrorAction Stop
        if ($acl.AreAccessRulesProtected) {{
            "protected: $($item.FullName)"
            continue
        }}
        $inheritedOrdinaryAllow = @(
            foreach ($rule in $acl.Access) {{
                if (
                    $rule.IsInherited -and
                    $rule.AccessControlType -eq [System.Security.AccessControl.AccessControlType]::Allow
                ) {{
                    try {{
                        $sid = $rule.IdentityReference.Translate(
                            [System.Security.Principal.SecurityIdentifier]
                        ).Value
                    }} catch {{
                        $sid = $rule.IdentityReference.Value
                    }}
                    if ($ordinaryUserSids -contains $sid) {{
                        $rule
                    }}
                }}
            }}
        )
        if ($inheritedOrdinaryAllow.Count -eq 0) {{
            "missing inherited ordinary-user allow rule: $($item.FullName)"
        }}
    }}
)
if ($failures.Count -gt 0) {{
    [Console]::Error.WriteLine(($failures -join [Environment]::NewLine))
    exit 1
}}
"""
    verification = subprocess.run(
        ["powershell.exe", "-NoProfile", "-Command", verify_script],
        capture_output=True,
        text=True,
        check=False,
    )
    if verification.returncode != 0:
        verification_detail = "\n".join(
            output.strip()
            for output in (verification.stdout, verification.stderr)
            if output.strip()
        )
        raise RuntimeError(
            "Publish stage ACL inheritance postcondition failed "
            f"with code {verification.returncode}: {verification_detail}"
        )


def _commit(stage: Path, drawings_root: Path) -> None:
    _enable_stage_tree_inheritance(stage)
    backup = Path(tempfile.mkdtemp(prefix=".shiphub-publish-backup-", dir=drawings_root))
    targets = (
        "SVG",
        "FinalPNG",
        "ProjectRift_ShipHub_CompleteDesign_v1.pdf",
        "ProjectRift_ShipHub_ContactSheet_v1.png",
        "Handoff",
    )
    backups: list[tuple[Path, Path]] = []
    installed: list[Path] = []
    cleanup_backup = True
    try:
        for name in targets:
            destination = _child(drawings_root / name, drawings_root)
            if destination.exists():
                backup_path = backup / name
                os.replace(destination, backup_path)
                backups.append((destination, backup_path))
        for name in targets:
            destination = _child(drawings_root / name, drawings_root)
            os.replace(stage / name, destination)
            installed.append(destination)
    except Exception as commit_error:
        rollback_errors = []
        for destination in reversed(installed):
            try:
                if destination.exists():
                    os.replace(destination, stage / destination.name)
            except OSError as error:
                rollback_errors.append(f"remove {destination}: {error}")
        for destination, backup_path in reversed(backups):
            try:
                if backup_path.exists():
                    os.replace(backup_path, destination)
            except OSError as error:
                rollback_errors.append(f"restore {destination}: {error}")
        if rollback_errors:
            cleanup_backup = False
            raise RuntimeError(
                "Publish transaction failed and rollback was incomplete: "
                + "; ".join(rollback_errors)
                + f". Preserved backup directory: {backup}"
            ) from commit_error
        raise
    finally:
        if cleanup_backup:
            shutil.rmtree(backup, ignore_errors=True)


def publish_package(brief_path: Path, manifest_path: Path, drawings_root: Path) -> PublishResult:
    """Publish the authoritative base renders as an A3 SVG, PNG, PDF drawing package."""
    brief_path, manifest_path, drawings_root = Path(brief_path), Path(manifest_path), Path(drawings_root)
    _, brief_hash = _load_inputs(brief_path, manifest_path, drawings_root)
    font, chinese = _font()
    stage = Path(tempfile.mkdtemp(prefix=".shiphub-publish-", dir=drawings_root))
    try:
        svg_root, png_root = stage / "SVG", stage / "FinalPNG"
        svg_root.mkdir()
        png_root.mkdir()
        contact_path = stage / "ProjectRift_ShipHub_ContactSheet_v1.png"
        _contact_sheet(drawings_root, contact_path, font)
        svg_paths, png_paths = [], []
        for sheet_id in SHEET_IDS:
            svg_path, png_path = svg_root / f"{sheet_id}.svg", png_root / f"{sheet_id}.png"
            _svg(sheet_id, svg_path, chinese, brief_hash)
            _final_png(drawings_root, sheet_id, png_path, font, chinese, contact_path)
            svg_paths.append(svg_path)
            png_paths.append(png_path)
        pdf_path = stage / "ProjectRift_ShipHub_CompleteDesign_v1.pdf"
        _pdf_book(tuple(png_paths), pdf_path, brief_hash)
        handoff_pdf_path, handoff_contact_path = _stage_handoff(stage, pdf_path, contact_path)
        _commit(stage, drawings_root)
        return PublishResult(
            tuple(drawings_root / "SVG" / path.name for path in svg_paths),
            tuple(drawings_root / "FinalPNG" / path.name for path in png_paths),
            drawings_root / pdf_path.name,
            drawings_root / contact_path.name,
            drawings_root / "Handoff" / handoff_pdf_path.name,
            drawings_root / "Handoff" / handoff_contact_path.name,
        )
    finally:
        if stage.exists():
            shutil.rmtree(stage, ignore_errors=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Publish ProjectRift Ship Hub A3 drawing package.")
    parser.add_argument("--brief", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--drawings-root", required=True)
    arguments = parser.parse_args(argv)
    try:
        result = publish_package(Path(arguments.brief), Path(arguments.manifest), Path(arguments.drawings_root))
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"ShipHub publish failed: {error}", file=sys.stderr)
        return 1
    print(
        f"ShipHub published {len(result.svg_paths)} SVGs, {len(result.png_paths)} PNGs, and {result.pdf_path.name}. "
        f"Committed Handoff files: {result.handoff_pdf_path.name}; {result.handoff_contact_sheet_path.name}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
