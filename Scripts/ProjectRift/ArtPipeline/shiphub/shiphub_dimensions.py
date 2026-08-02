from __future__ import annotations

from dataclasses import dataclass
from html import escape
from math import cos, radians, sin
from typing import Literal

from PIL import Image, ImageDraw, ImageFont


Point = tuple[float, float]


@dataclass(frozen=True)
class LinearDimension:
    dimension_id: str
    label: str
    kind: Literal["overall", "object"]
    orientation: Literal["horizontal", "vertical", "diameter"]
    measured_start: Point
    measured_end: Point
    line_start: Point
    line_end: Point
    text_at: Point
    rotate_text: bool = False
    chain_ticks: tuple[Point, ...] = ()


@dataclass(frozen=True)
class AngularDimension:
    dimension_id: str
    label: str
    center: Point
    radius: float
    start_degrees: float
    end_degrees: float
    text_at: Point


@dataclass(frozen=True)
class ScheduleNote:
    note_id: str
    key: str
    label: str
    anchor: Point | None
    text_at: Point


SHEET_ANNOTATIONS: dict[str, tuple[LinearDimension | AngularDimension | ScheduleNote, ...]] = {
    "A01_FloorPlan": (
        LinearDimension("room-width", "28 m OVERALL WIDTH", "overall", "horizontal", (66, 218), (354, 218), (66, 247), (354, 247), (174, 244)),
        LinearDimension("room-depth", "24 m OVERALL DEPTH", "overall", "vertical", (62, 48), (62, 215), (43, 48), (43, 215), (36, 160), True),
        LinearDimension("table-diameter", "\u00d8 8 m NAVIGATION TABLE", "object", "diameter", (174, 139), (246, 139), (174, 139), (246, 139), (181, 134)),
        ScheduleNote("main-path", "C", "MAIN PATH CLEARANCE: 5 m", (96, 171), (250, 222)),
        ScheduleNote("pod-centers", "P1-P5", "X = -4, -2, 0, 2, 4 m", (210, 62), (250, 229)),
        ScheduleNote("dock-coordinates", "D1-D4", "(-5.3,-5.3), (5.3,-5.3), (-5.3,5.3), (5.3,5.3) m", (335, 197), (250, 236)),
    ),
    "A02_ReflectedCeilingPlan": (
        LinearDimension("ring-outer", "\u00d8 16 m SERVICE RING OUTER", "object", "diameter", (150, 135), (270, 135), (150, 135), (270, 135), (174, 130)),
        LinearDimension("ring-inner", "\u00d8 10 m SERVICE RING INNER", "object", "diameter", (172, 145), (248, 145), (172, 145), (248, 145), (182, 158)),
    ),
    "A03_NorthElevation": (
        LinearDimension("overall-width", "28 m CLEAR WIDTH", "overall", "horizontal", (68, 218), (352, 218), (68, 238), (352, 238), (174, 234)),
        LinearDimension("clear-height", "8 m NOMINAL HEIGHT", "overall", "vertical", (68, 58), (68, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A04_SouthElevation": (
        LinearDimension("overall-width", "28 m CLEAR WIDTH", "overall", "horizontal", (68, 218), (352, 218), (68, 238), (352, 238), (174, 234)),
        LinearDimension("clear-height", "7 m CLEAR HEIGHT", "overall", "vertical", (68, 58), (68, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A05_WestElevation": (
        LinearDimension("overall-depth", "24 m CLEAR DEPTH", "overall", "horizontal", (76, 218), (344, 218), (76, 238), (344, 238), (174, 234)),
        LinearDimension("clear-height", "7 m CLEAR HEIGHT", "overall", "vertical", (76, 58), (76, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A06_EastElevation": (
        LinearDimension("overall-depth", "24 m CLEAR DEPTH", "overall", "horizontal", (76, 218), (344, 218), (76, 238), (344, 238), (174, 234)),
        LinearDimension("clear-height", "7 m CLEAR HEIGHT", "overall", "vertical", (76, 58), (76, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A07_LongitudinalSection": (
        LinearDimension("section-length", "28 m SECTION LENGTH", "overall", "horizontal", (72, 218), (348, 218), (72, 238), (348, 238), (174, 234)),
        LinearDimension("clear-height", "7 m CLEAR HEIGHT", "overall", "vertical", (72, 58), (72, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A08_TransverseSection": (
        LinearDimension("section-length", "24 m SECTION LENGTH", "overall", "horizontal", (72, 218), (348, 218), (72, 238), (348, 238), (174, 234)),
        LinearDimension("clear-height", "7 m CLEAR HEIGHT", "overall", "vertical", (72, 58), (72, 218), (43, 58), (43, 218), (36, 160), True),
    ),
    "A09_ExplodedModulePlan": (
        LinearDimension("ring-lift", "10.5 m CEILING-RING LIFT", "overall", "vertical", (345, 68), (345, 210), (365, 68), (365, 210), (372, 144), True),
        LinearDimension("structural-bay", "4 m STRUCTURAL BAY", "object", "horizontal", (82, 218), (146, 218), (82, 228), (146, 228), (90, 224)),
    ),
    "A10_PerspectiveSheet": (),
    "D01_Cryopod": (
        LinearDimension("pod-width", "1.6 m POD WIDTH", "object", "horizontal", (74, 202), (132, 202), (74, 222), (132, 222), (78, 218)),
        LinearDimension("pod-length", "1.6 x 1.6 x 3 m", "object", "horizontal", (158, 202), (262, 202), (158, 222), (262, 222), (183, 218)),
        LinearDimension("pod-height", "1.6 m POD HEIGHT", "object", "vertical", (74, 82), (74, 202), (54, 82), (54, 202), (47, 148), True),
        AngularDimension("recline-angle", "18 degrees recline", (212, 164), 26, 0, 18, (217, 156)),
        AngularDimension("door-angle", "75-degree door angle", (326, 171), 26, 0, 75, (331, 151)),
        ScheduleNote("operating-envelope", "E", "1.2 m envelope", (274, 202), (267, 246)),
    ),
    "D02_NavigationTable": (
        LinearDimension("table-diameter", "\u00d8 8 m TABLE DIAMETER", "object", "diameter", (54, 136), (154, 136), (54, 136), (154, 136), (62, 131)),
        LinearDimension("display-diameter", "\u00d8 6 m DISPLAY DIAMETER", "object", "diameter", (280, 136), (362, 136), (280, 136), (362, 136), (282, 131)),
        LinearDimension("table-height", "1.1 m TABLE HEIGHT", "object", "vertical", (224, 114), (224, 190), (204, 114), (204, 190), (197, 157), True),
    ),
    "D03_MainAirlock": (
        LinearDimension("opening-width", "4 x 3.5 m CLEAR OPENING", "object", "horizontal", (54, 196), (142, 196), (54, 218), (142, 218), (59, 214)),
        LinearDimension("opening-height", "3.5 m CLEAR HEIGHT", "object", "vertical", (54, 94), (54, 196), (42, 94), (42, 196), (35, 150), True),
        LinearDimension("airlock-depth", "1.2 m DEPTH", "object", "horizontal", (185, 196), (244, 196), (185, 218), (244, 218), (190, 214)),
        ScheduleNote("muster-area", "M", "8 x 5 m muster area", (294, 171), (267, 246)),
    ),
    "D04_ConstructDock": (
        LinearDimension("dock-diameter", "\u00d8 1 m DOCK DIAMETER", "object", "diameter", (148, 146), (204, 146), (148, 146), (204, 146), (153, 141)),
        LinearDimension("recess-depth", "0.08 m RECESS", "object", "vertical", (290, 142), (290, 183), (270, 142), (270, 183), (263, 164), True),
        ScheduleNote("dock-locations", "D1-D4", "4 dock locations", (176, 184), (267, 246)),
    ),
    "D05_WallBayInterface": (
        LinearDimension("bay-width-chain", "1 m / 2 m / 4 m BAY WIDTHS", "object", "horizontal", (68, 204), (330, 204), (68, 224), (330, 224), (150, 220), False, ((105.4, 224), (180.3, 224))),
        LinearDimension("wall-thickness", "0.4 m WALL THICKNESS", "object", "horizontal", (338, 135), (382, 135), (338, 145), (382, 145), (339, 141)),
    ),
}


def annotation_specs(sheet_id: str) -> tuple[LinearDimension | AngularDimension | ScheduleNote, ...]:
    return SHEET_ANNOTATIONS.get(sheet_id, ())


def _point_attributes(start: Point, end: Point) -> str:
    return f'x1="{start[0]:g}" y1="{start[1]:g}" x2="{end[0]:g}" y2="{end[1]:g}"'


def _linear_markup(dimension: LinearDimension) -> str:
    extension_lines = ""
    if dimension.measured_start != dimension.line_start:
        extension_lines += f'<line class="extension-line" {_point_attributes(dimension.measured_start, dimension.line_start)}/>'
    if dimension.measured_end != dimension.line_end:
        extension_lines += f'<line class="extension-line" {_point_attributes(dimension.measured_end, dimension.line_end)}/>'
    rotation = f' transform="rotate(-90 {dimension.text_at[0]:g} {dimension.text_at[1]:g})"' if dimension.rotate_text else ""
    tick_markup = "".join(
        f'<line class="chain-tick" x1="{tick[0]:g}" y1="{tick[1] - 3:g}" x2="{tick[0]:g}" y2="{tick[1] + 3:g}" stroke="#efb05a" stroke-width="0.45"/>'
        for tick in dimension.chain_ticks
    )
    return (
        f'<g class="dimension-chain" data-dimension-id="{escape(dimension.dimension_id)}" '
        f'data-kind="{dimension.kind}" data-orientation="{dimension.orientation}" data-label="{escape(dimension.label)}">'
        f'{extension_lines}<line class="dimension-line" {_point_attributes(dimension.line_start, dimension.line_end)} '
        f'marker-start="url(#dimension-arrow)" marker-end="url(#dimension-arrow)"/>{tick_markup}'
        f'<text x="{dimension.text_at[0]:g}" y="{dimension.text_at[1]:g}" class="dimension"{rotation}>{escape(dimension.label)}</text></g>'
    )


def _angular_markup(dimension: AngularDimension) -> str:
    start_angle, end_angle = radians(dimension.start_degrees), radians(dimension.end_degrees)
    start = (dimension.center[0] + dimension.radius * cos(start_angle), dimension.center[1] + dimension.radius * sin(start_angle))
    end = (dimension.center[0] + dimension.radius * cos(end_angle), dimension.center[1] + dimension.radius * sin(end_angle))
    large_arc = int(abs(dimension.end_degrees - dimension.start_degrees) > 180)
    sweep = int(dimension.end_degrees > dimension.start_degrees)
    return (
        f'<path class="angular-dimension" data-dimension-id="{escape(dimension.dimension_id)}" '
        f'data-kind="angular" data-orientation="angular" data-label="{escape(dimension.label)}" '
        f'data-center-x="{dimension.center[0]:g}" data-center-y="{dimension.center[1]:g}" '
        f'd="M {start[0]:g} {start[1]:g} A {dimension.radius:g} {dimension.radius:g} 0 {large_arc} {sweep} {end[0]:g} {end[1]:g}" '
        f'fill="none" stroke="#efb05a" stroke-width="0.65" marker-start="url(#dimension-arrow)" marker-end="url(#dimension-arrow)"/>'
        f'<text x="{dimension.text_at[0]:g}" y="{dimension.text_at[1]:g}" class="dimension">{escape(dimension.label)}</text>'
    )


def _schedule_markup(note: ScheduleNote) -> str:
    anchor_markup = ""
    if note.anchor is not None:
        anchor_markup = f'<text x="{note.anchor[0]:g}" y="{note.anchor[1]:g}" class="dimension">{escape(note.key)}</text>'
    return (
        f'<g class="schedule-note" data-dimension-id="{escape(note.note_id)}" data-kind="schedule" data-orientation="keyed">'
        f'{anchor_markup}<text x="{note.text_at[0]:g}" y="{note.text_at[1]:g}" class="dimension">{escape(note.key)}: {escape(note.label)}</text></g>'
    )


def render_sheet_annotations(sheet_id: str) -> tuple[str, str]:
    plot_markup, schedules = [], []
    for specification in annotation_specs(sheet_id):
        if isinstance(specification, LinearDimension):
            plot_markup.append(_linear_markup(specification))
        elif isinstance(specification, AngularDimension):
            plot_markup.append(_angular_markup(specification))
        elif isinstance(specification, ScheduleNote):
            schedules.append(_schedule_markup(specification))
    schedule_markup = f'<g id="keyed-notes">{"".join(schedules)}</g>' if schedules else ""
    return "".join(plot_markup), schedule_markup


def draw_sheet_annotations(
    image: Image.Image,
    sheet_id: str,
    font: ImageFont.FreeTypeFont,
) -> None:
    """Rasterize the shared A3 annotation specs onto a final 300-DPI sheet."""
    specifications = annotation_specs(sheet_id)
    if not specifications:
        return

    scale_x, scale_y = image.width / 420.0, image.height / 297.0
    line_scale = min(scale_x, scale_y)
    orange = "#efb05a"
    extension_width = max(1, round(0.45 * line_scale))
    dimension_width = max(1, round(0.65 * line_scale))
    text_font = font.font_variant(size=max(1, round(4.0 * scale_y)))
    draw = ImageDraw.Draw(image)

    def point(value: Point) -> tuple[int, int]:
        return round(value[0] * scale_x), round(value[1] * scale_y)

    def arrow(tip: Point, base_direction: Point) -> None:
        direction_length = (base_direction[0] ** 2 + base_direction[1] ** 2) ** 0.5
        unit = (
            base_direction[0] / direction_length,
            base_direction[1] / direction_length,
        )
        perpendicular = (-unit[1], unit[0])
        length, half_width = 3.25, 1.625
        base = (tip[0] + unit[0] * length, tip[1] + unit[1] * length)
        draw.polygon(
            (
                point(tip),
                point((base[0] + perpendicular[0] * half_width, base[1] + perpendicular[1] * half_width)),
                point((base[0] - perpendicular[0] * half_width, base[1] - perpendicular[1] * half_width)),
            ),
            fill=orange,
        )

    def text(value: Point, label: str, rotate: bool = False) -> None:
        at = point(value)
        if not rotate:
            draw.text(at, label, font=text_font, fill=orange, anchor="ls")
            return
        layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).text(at, label, font=text_font, fill=orange, anchor="ls")
        layer = layer.rotate(90, resample=Image.Resampling.BICUBIC, center=at)
        image.paste(layer, (0, 0), layer)

    for specification in specifications:
        if isinstance(specification, LinearDimension):
            if specification.measured_start != specification.line_start:
                draw.line(
                    (point(specification.measured_start), point(specification.line_start)),
                    fill=orange,
                    width=extension_width,
                )
            if specification.measured_end != specification.line_end:
                draw.line(
                    (point(specification.measured_end), point(specification.line_end)),
                    fill=orange,
                    width=extension_width,
                )
            draw.line(
                (point(specification.line_start), point(specification.line_end)),
                fill=orange,
                width=dimension_width,
            )
            delta = (
                specification.line_end[0] - specification.line_start[0],
                specification.line_end[1] - specification.line_start[1],
            )
            arrow(specification.line_start, delta)
            arrow(specification.line_end, (-delta[0], -delta[1]))
            for tick in specification.chain_ticks:
                draw.line(
                    (point((tick[0], tick[1] - 3)), point((tick[0], tick[1] + 3))),
                    fill=orange,
                    width=extension_width,
                )
            text(specification.text_at, specification.label, specification.rotate_text)
        elif isinstance(specification, AngularDimension):
            center = point(specification.center)
            radius_x, radius_y = specification.radius * scale_x, specification.radius * scale_y
            draw.arc(
                (
                    round(center[0] - radius_x),
                    round(center[1] - radius_y),
                    round(center[0] + radius_x),
                    round(center[1] + radius_y),
                ),
                start=specification.start_degrees,
                end=specification.end_degrees,
                fill=orange,
                width=dimension_width,
            )
            start_angle, end_angle = radians(specification.start_degrees), radians(specification.end_degrees)
            start = (
                specification.center[0] + specification.radius * cos(start_angle),
                specification.center[1] + specification.radius * sin(start_angle),
            )
            end = (
                specification.center[0] + specification.radius * cos(end_angle),
                specification.center[1] + specification.radius * sin(end_angle),
            )
            start_tangent = (-sin(start_angle), cos(start_angle))
            end_tangent = (-sin(end_angle), cos(end_angle))
            arrow(start, start_tangent)
            arrow(end, (-end_tangent[0], -end_tangent[1]))
            text(specification.text_at, specification.label)
        elif isinstance(specification, ScheduleNote):
            if specification.anchor is not None:
                text(specification.anchor, specification.key)
            text(specification.text_at, f"{specification.key}: {specification.label}")
