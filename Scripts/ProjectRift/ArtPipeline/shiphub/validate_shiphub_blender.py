"""Validate the authoritative Ship Hub BLEND and exported geometry in Blender."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy


EXPECTED_RELATIVE_PATHS = {
    "blend": Path("Blender/SM_ShipHub_Complete_White_v1.blend"),
    "fbx": Path("Exports/SM_ShipHub_Complete_White_v1.fbx"),
    "glb": Path("Exports/SM_ShipHub_Complete_White_v1.glb"),
    "manifest": Path("Reports/layout-manifest.json"),
    "report": Path("Reports/export-validation.json"),
}
BOUNDS_TOLERANCE_M = 0.01


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--blend", required=True)
    parser.add_argument("--fbx", required=True)
    parser.add_argument("--glb", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--report", required=True)
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(arguments)


def _resolved(path: str | Path) -> Path:
    return Path(path).resolve()


def _require_exact_paths(arguments: argparse.Namespace) -> dict[str, Path]:
    project_root = _resolved(arguments.project_root)
    if not project_root.is_dir():
        raise ValueError(f"Project root is missing: {project_root}")
    output_root = _resolved(
        project_root / "SourceArt/ProjectRift/ShipHub/CompleteDesign"
    )
    supplied = {
        "blend": _resolved(arguments.blend),
        "fbx": _resolved(arguments.fbx),
        "glb": _resolved(arguments.glb),
        "manifest": _resolved(arguments.manifest),
        "report": _resolved(arguments.report),
    }
    for label, path in supplied.items():
        expected = _resolved(output_root / EXPECTED_RELATIVE_PATHS[label])
        if path != expected:
            raise ValueError(f"Unexpected {label} path: {path}; expected {expected}")
        try:
            path.relative_to(output_root)
        except ValueError as error:
            raise ValueError(f"{label} path escapes the approved output root: {path}") from error
    for label in ("blend", "fbx", "glb", "manifest"):
        path = supplied[label]
        if not path.is_file() or path.stat().st_size <= 0:
            raise ValueError(f"Required {label} input is missing or empty: {path}")
    if not supplied["report"].parent.is_dir():
        raise ValueError(f"Report directory is missing: {supplied['report'].parent}")
    supplied["project_root"] = project_root
    supplied["output_root"] = output_root
    return supplied


def _clean(value: float) -> float:
    rounded = round(float(value), 6)
    return 0.0 if rounded == -0.0 else rounded


def _bounds(objects: list[bpy.types.Object]) -> dict[str, list[float]]:
    dependency_graph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(dependency_graph)
        mesh = evaluated.to_mesh()
        try:
            points.extend(evaluated.matrix_world @ vertex.co.copy() for vertex in mesh.vertices)
        finally:
            evaluated.to_mesh_clear()
    if not points:
        raise ValueError("Cannot measure an empty mesh-object selection.")
    return {
        "Min": [_clean(min(point[axis] for point in points)) for axis in range(3)],
        "Max": [_clean(max(point[axis] for point in points)) for axis in range(3)],
    }


def _bounds_delta(
    actual: dict[str, list[float]], expected: dict[str, list[float]]
) -> dict[str, list[float]]:
    return {
        key: [_clean(actual[key][axis] - expected[key][axis]) for axis in range(3)]
        for key in ("Min", "Max")
    }


def _bounds_match(
    actual: dict[str, list[float]], expected: dict[str, list[float]]
) -> bool:
    return all(
        abs(actual[key][axis] - expected[key][axis]) <= BOUNDS_TOLERANCE_M
        for key in ("Min", "Max")
        for axis in range(3)
    )


def _issue(rule_id: str, expected, actual, artifact: str) -> dict:
    return {
        "RuleId": rule_id,
        "Expected": expected,
        "Actual": actual,
        "Artifact": artifact,
    }


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _collection_tree(root: bpy.types.Collection) -> set[bpy.types.Collection]:
    result = {root}
    for child in root.children:
        result.update(_collection_tree(child))
    return result


def _cleanup_owned_import(
    owned_objects: list[bpy.types.Object],
    owned_collections: set[bpy.types.Collection],
) -> bool:
    object_pointers = {obj.as_pointer() for obj in owned_objects}
    collection_pointers = {collection.as_pointer() for collection in owned_collections}
    for obj in owned_objects:
        if obj.as_pointer() in {candidate.as_pointer() for candidate in bpy.data.objects}:
            bpy.data.objects.remove(obj, do_unlink=True)
    for collection in sorted(
        owned_collections, key=lambda item: len(_collection_tree(item)), reverse=True
    ):
        if collection.as_pointer() in {
            candidate.as_pointer() for candidate in bpy.data.collections
        }:
            bpy.data.collections.remove(collection)
    remaining_object_pointers = {obj.as_pointer() for obj in bpy.data.objects}
    remaining_collection_pointers = {
        collection.as_pointer() for collection in bpy.data.collections
    }
    return not (object_pointers & remaining_object_pointers) and not (
        collection_pointers & remaining_collection_pointers
    )


def _validate_import(
    export_path: Path,
    export_format: str,
    source_bounds: dict[str, list[float]],
    source_count: int,
    issues: list[dict],
) -> dict:
    original_objects = set(bpy.data.objects)
    original_collections = set(bpy.data.collections)
    temporary_collection = bpy.data.collections.new(
        f"_SHIPHUB_VALIDATION_{export_format.upper()}"
    )
    bpy.context.scene.collection.children.link(temporary_collection)
    bpy.context.view_layer.update()
    temporary_layer_collection = bpy.context.view_layer.layer_collection.children.get(
        temporary_collection.name
    )
    if temporary_layer_collection is None:
        raise RuntimeError("Temporary validation collection is not in the active view layer.")
    previous_active_collection = bpy.context.view_layer.active_layer_collection
    bpy.context.view_layer.active_layer_collection = temporary_layer_collection
    bpy.ops.object.select_all(action="DESELECT")
    owned_objects: list[bpy.types.Object] = []
    owned_collections: set[bpy.types.Collection] = {temporary_collection}
    result = {
        "Passed": False,
        "ObjectCount": 0,
        "BoundsM": None,
        "BoundsDeltaM": None,
        "Isolation": {
            "TemporaryCollection": temporary_collection.name,
            "ExclusiveTemporaryCollection": False,
            "CleanupVerified": False,
        },
    }
    try:
        if export_format == "fbx":
            operation = bpy.ops.import_scene.fbx(filepath=str(export_path))
        else:
            operation = bpy.ops.import_scene.gltf(filepath=str(export_path))
        if "FINISHED" not in operation:
            raise RuntimeError(
                f"{export_format.upper()} import did not finish: {sorted(operation)}"
            )
        imported_objects = [
            obj for obj in bpy.context.selected_objects if obj not in original_objects
        ]
        if not imported_objects:
            raise RuntimeError(
                f"{export_format.upper()} importer did not select any imported objects."
            )
        owned_objects = list(imported_objects)
        for obj in imported_objects:
            for collection in list(obj.users_collection):
                if collection not in original_collections:
                    owned_collections.add(collection)
            if temporary_collection not in obj.users_collection:
                temporary_collection.objects.link(obj)
            for collection in list(obj.users_collection):
                if collection != temporary_collection:
                    collection.objects.unlink(obj)
        owned_collections.update(_collection_tree(temporary_collection))
        exclusive_collection = all(
            list(obj.users_collection) == [temporary_collection]
            for obj in imported_objects
        )
        result["Isolation"]["ExclusiveTemporaryCollection"] = exclusive_collection
        if not exclusive_collection:
            issues.append(
                _issue(
                    f"EXPORT_{export_format.upper()}_TEMPORARY_COLLECTION",
                    temporary_collection.name,
                    {
                        obj.name: [collection.name for collection in obj.users_collection]
                        for obj in imported_objects
                    },
                    export_path.name,
                )
            )
        imported_meshes = sorted(
            (obj for obj in imported_objects if obj.type == "MESH"), key=lambda obj: obj.name
        )
        result["ObjectCount"] = len(imported_meshes)
        if len(imported_meshes) != source_count:
            issues.append(
                _issue(
                    f"EXPORT_{export_format.upper()}_OBJECT_COUNT",
                    source_count,
                    len(imported_meshes),
                    export_path.name,
                )
            )
        imported_bounds = _bounds(imported_meshes)
        result["BoundsM"] = imported_bounds
        result["BoundsDeltaM"] = _bounds_delta(imported_bounds, source_bounds)
        if not _bounds_match(imported_bounds, source_bounds):
            issues.append(
                _issue(
                    f"EXPORT_{export_format.upper()}_BOUNDS",
                    {"BoundsM": source_bounds, "ToleranceM": BOUNDS_TOLERANCE_M},
                    imported_bounds,
                    export_path.name,
                )
            )
        result["Passed"] = (
            len(imported_meshes) == source_count
            and _bounds_match(imported_bounds, source_bounds)
            and exclusive_collection
        )
    except Exception as error:  # Blender import failures must become report evidence.
        issues.append(
            _issue(
                f"EXPORT_{export_format.upper()}_IMPORT",
                "successful isolated Blender import",
                str(error),
                export_path.name,
            )
        )
    finally:
        bpy.context.view_layer.active_layer_collection = previous_active_collection
        try:
            cleanup_verified = _cleanup_owned_import(owned_objects, owned_collections)
        except Exception as error:
            cleanup_verified = False
            issues.append(
                _issue(
                    f"EXPORT_{export_format.upper()}_CLEANUP",
                    "owned temporary collection and imported objects removed",
                    str(error),
                    export_path.name,
                )
            )
        result["Isolation"]["CleanupVerified"] = cleanup_verified
        if not cleanup_verified:
            result["Passed"] = False
            if not any(
                issue["RuleId"] == f"EXPORT_{export_format.upper()}_CLEANUP"
                for issue in issues
            ):
                issues.append(
                    _issue(
                        f"EXPORT_{export_format.upper()}_CLEANUP",
                        "owned temporary collection and imported objects removed",
                        "validation-owned data remains",
                        export_path.name,
                    )
                )
    return result


def _write_report(path: Path, report: dict) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def main() -> int:
    arguments = _arguments()
    paths = _require_exact_paths(arguments)
    manifest = json.loads(paths["manifest"].read_text(encoding="utf-8"))
    issues: list[dict] = []

    open_result = bpy.ops.wm.open_mainfile(filepath=str(paths["blend"]), load_ui=False)
    if "FINISHED" not in open_result:
        raise RuntimeError(f"Authoritative BLEND open did not finish: {sorted(open_result)}")

    expected_collections = list(manifest.get("collections", []))
    actual_collections = sorted(collection.name for collection in bpy.data.collections)
    if sorted(expected_collections) != actual_collections:
        issues.append(
            _issue(
                "BLEND_COLLECTIONS",
                sorted(expected_collections),
                actual_collections,
                paths["blend"].name,
            )
        )

    expected_scene_objects = list(manifest.get("objects", []))
    actual_mesh_count = sum(obj.type == "MESH" for obj in bpy.data.objects)
    if actual_mesh_count != len(expected_scene_objects):
        issues.append(
            _issue(
                "BLEND_MESH_OBJECT_COUNT",
                len(expected_scene_objects),
                actual_mesh_count,
                paths["blend"].name,
            )
        )

    expected_export_names = list(manifest.get("exports", {}).get("expected_object_names", []))
    source_objects = [bpy.data.objects.get(name) for name in expected_export_names]
    missing_names = [
        name for name, obj in zip(expected_export_names, source_objects) if obj is None
    ]
    source_meshes = [obj for obj in source_objects if obj is not None and obj.type == "MESH"]
    if missing_names or len(source_meshes) != len(expected_export_names):
        issues.append(
            _issue(
                "BLEND_EXPORT_SELECTION",
                expected_export_names,
                [obj.name for obj in source_meshes],
                paths["blend"].name,
            )
        )
    source_bounds = _bounds(source_meshes)
    source = {
        "ObjectCount": len(source_meshes),
        "BoundsM": source_bounds,
    }
    fbx = _validate_import(
        paths["fbx"], "fbx", source_bounds, len(source_meshes), issues
    )
    glb = _validate_import(
        paths["glb"], "glb", source_bounds, len(source_meshes), issues
    )
    report = {
        "Schema": "projectrift.shiphub.export-validation.v1",
        "Passed": not issues and fbx["Passed"] and glb["Passed"],
        "IssueCount": len(issues),
        "Issues": issues,
        "BlenderVersion": bpy.app.version_string,
        "BoundsToleranceM": BOUNDS_TOLERANCE_M,
        "CollectionCount": len(actual_collections),
        "SceneMeshObjectCount": actual_mesh_count,
        "InputSHA256": {
            "BLEND": _sha256(paths["blend"]),
            "Manifest": _sha256(paths["manifest"]),
            "FBX": _sha256(paths["fbx"]),
            "GLB": _sha256(paths["glb"]),
        },
        "Source": source,
        "FBX": fbx,
        "GLB": glb,
    }
    _write_report(paths["report"], report)
    print(
        f"ShipHub Blender validation: {'PASS' if report['Passed'] else 'FAIL'} "
        f"({report['IssueCount']} issues)."
    )
    return 0 if report["Passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
