"""Validated, machine-readable G3 contract for ShipHub's first wall door."""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA = "projectrift.shiphub.wall-door-first-article.v1"
ASSET_ID = "SM_ShipHub_WallDoor_400_A"
REFERENCE_PREFIX = "SourceArt/ProjectRift/ShipHub/"


@dataclass(frozen=True)
class WallDoorContract:
    schema: str
    asset_id: str
    stage: str
    units: str
    bounds_cm: tuple[float, float, float]
    opening_cm: tuple[float, float]
    opening_min_cm: tuple[float, float, float]
    snap_cm: float
    pivot: str
    material_slot_limit: int
    texture_size: int
    texture_channels: tuple[str, ...]
    states: tuple[str, ...]
    base_mesh: str
    collision_pieces: tuple[str, ...]
    references: tuple[str, ...]


def _read_required(data: dict[str, Any], key: str, expected_type: type[Any] | tuple[type[Any], ...]) -> Any:
    value = data.get(key)
    if value is None:
        raise ValueError(f"{key}: required")
    if not isinstance(value, expected_type):
        expected_name = (
            expected_type.__name__
            if isinstance(expected_type, type)
            else " or ".join(item.__name__ for item in expected_type)
        )
        raise ValueError(f"{key}: expected {expected_name}")
    return value


def _numbers(data: dict[str, Any], key: str, length: int) -> tuple[float, ...]:
    value = _read_required(data, key, list)
    if len(value) != length or any(isinstance(item, bool) or not isinstance(item, (int, float)) for item in value):
        raise ValueError(f"{key}: expected {length} numeric values")
    return tuple(float(item) for item in value)


def _strings(data: dict[str, Any], key: str) -> tuple[str, ...]:
    value = _read_required(data, key, list)
    if any(not isinstance(item, str) for item in value):
        raise ValueError(f"{key}: expected string values")
    return tuple(value)


def _validate_reference_syntax(reference: str, index: int) -> str | None:
    path = PurePosixPath(reference)
    if (
        not reference
        or "\\" in reference
        or path.is_absolute()
        or ".." in path.parts
        or "" in reference.split("/")
        or not reference.startswith(REFERENCE_PREFIX)
    ):
        return f"References[{index}]: must be a ProjectA-relative path under {REFERENCE_PREFIX}"
    return None


def load_contract(path: Path) -> WallDoorContract:
    """Load structurally sound contract data, normalizing input defects to ValueError."""
    try:
        parsed = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"Contract: cannot read JSON ({error})") from None
    if not isinstance(parsed, dict):
        raise ValueError("Contract: JSON root must be an object")

    errors: list[str] = []

    def collect(reader: Any) -> Any:
        try:
            return reader()
        except (TypeError, ValueError) as error:
            errors.append(str(error))
            return None

    schema = collect(lambda: _read_required(parsed, "Schema", str))
    asset_id = collect(lambda: _read_required(parsed, "AssetId", str))
    stage = collect(lambda: _read_required(parsed, "Stage", str))
    units = collect(lambda: _read_required(parsed, "Units", str))
    bounds_cm = collect(lambda: _numbers(parsed, "BoundsCm", 3))
    opening_cm = collect(lambda: _numbers(parsed, "DoorOpeningCm", 2))
    opening_min_cm = collect(lambda: _numbers(parsed, "DoorOpeningMinCm", 3))
    snap_cm = collect(lambda: float(_read_required(parsed, "SnapCm", (int, float))))
    pivot = collect(lambda: _read_required(parsed, "Pivot", str))
    material_slot_limit = collect(lambda: _read_required(parsed, "MaterialSlotLimit", int))
    texture_size = collect(lambda: _read_required(parsed, "TextureSize", int))
    texture_channels = collect(lambda: _strings(parsed, "TextureChannels"))
    states = collect(lambda: _strings(parsed, "States"))
    base_mesh = collect(lambda: _read_required(parsed, "BaseMesh", str))
    collision_pieces = collect(lambda: _strings(parsed, "CollisionPieces"))
    references = collect(lambda: _strings(parsed, "References"))
    if references is not None:
        errors.extend(
            issue
            for index, reference in enumerate(references)
            if (issue := _validate_reference_syntax(reference, index)) is not None
        )

    if errors:
        raise ValueError("; ".join(errors))
    return WallDoorContract(
        schema=schema,
        asset_id=asset_id,
        stage=stage,
        units=units,
        bounds_cm=bounds_cm,
        opening_cm=opening_cm,
        opening_min_cm=opening_min_cm,
        snap_cm=snap_cm,
        pivot=pivot,
        material_slot_limit=material_slot_limit,
        texture_size=texture_size,
        texture_channels=texture_channels,
        states=states,
        base_mesh=base_mesh,
        collision_pieces=collision_pieces,
        references=references,
    )


def _exact(path: str, actual: Any, expected: Any, issues: list[str]) -> None:
    if actual != expected:
        issues.append(f"{path}: expected {expected!r}, got {actual!r}")


def validate_contract(
    contract: WallDoorContract, project_root: Path | None = None
) -> list[str]:
    """Return all semantic G3 contract violations without creating artifacts."""
    issues: list[str] = []
    _exact("Schema", contract.schema, SCHEMA, issues)
    _exact("AssetId", contract.asset_id, ASSET_ID, issues)
    _exact("Stage", contract.stage, "G3", issues)
    _exact("Units", contract.units, "cm", issues)
    _exact("BoundsCm", contract.bounds_cm, (400.0, 30.0, 400.0), issues)
    _exact("DoorOpeningCm", contract.opening_cm, (240.0, 280.0), issues)
    _exact("DoorOpeningMinCm", contract.opening_min_cm, (80.0, 0.0, 0.0), issues)
    _exact("SnapCm", contract.snap_cm, 50.0, issues)
    _exact("Pivot", contract.pivot, "BottomLeftBack", issues)
    _exact("MaterialSlotLimit", contract.material_slot_limit, 2, issues)
    _exact("TextureSize", contract.texture_size, 2048, issues)
    _exact("TextureChannels", contract.texture_channels, ("BC", "N", "ORM", "StateMask"), issues)
    _exact("States", contract.states, ("Damaged", "Patched", "Online"), issues)
    _exact("BaseMesh", contract.base_mesh, ASSET_ID, issues)
    _exact("CollisionPieces", contract.collision_pieces, ("LeftJamb", "RightJamb", "Lintel"), issues)
    if len(contract.references) != 2:
        issues.append(f"References: expected exactly 2 entries, got {len(contract.references)}")
    for index, reference in enumerate(contract.references):
        syntax_issue = _validate_reference_syntax(reference, index)
        if syntax_issue:
            issues.append(syntax_issue)
            continue
        if project_root is not None and not (project_root / reference).is_file():
            issues.append(f"References[{index}]: missing project file {reference}")
    return issues


def _preview(contract: WallDoorContract) -> dict[str, Any]:
    return {
        "Schema": contract.schema,
        "AssetId": contract.asset_id,
        "Stage": contract.stage,
        "BoundsCm": list(contract.bounds_cm),
        "DoorOpeningCm": list(contract.opening_cm),
        "DoorOpeningMinCm": list(contract.opening_min_cm),
        "SnapCm": contract.snap_cm,
        "Pivot": contract.pivot,
        "MaterialSlotLimit": contract.material_slot_limit,
        "TextureSize": contract.texture_size,
        "States": list(contract.states),
        "References": list(contract.references),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--project-root", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args(argv)

    try:
        contract = load_contract(args.contract)
        issues = validate_contract(contract, args.project_root)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    if issues:
        print("; ".join(issues), file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(_preview(contract), indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
