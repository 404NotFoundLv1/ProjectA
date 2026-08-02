from __future__ import annotations

import math
from collections.abc import Sequence

import bpy
from mathutils import Matrix, Vector


def reset_scene() -> None:
    """Remove every scene datablock that can leak into the white-model authority."""
    for obj in tuple(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for collection in tuple(bpy.data.collections):
        bpy.data.collections.remove(collection)
    for datablocks in (
        bpy.data.materials,
        bpy.data.meshes,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for datablock in tuple(datablocks):
            datablocks.remove(datablock)


def ensure_collection(name: str):
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
    scene_root = bpy.context.scene.collection
    if collection.name not in {child.name for child in scene_root.children}:
        scene_root.children.link(collection)
    return collection


def create_id_material(
    name: str,
    rgba: tuple[float, float, float, float],
):
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name=name)
    material.diffuse_color = rgba
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    if principled is not None:
        principled.inputs["Base Color"].default_value = rgba
        principled.inputs["Roughness"].default_value = 0.62
        if "Alpha" in principled.inputs:
            principled.inputs["Alpha"].default_value = rgba[3]
    return material


def _create_mesh_object(
    name: str,
    vertices: Sequence[Sequence[float]],
    faces: Sequence[Sequence[int]],
    location_m: Sequence[float],
    collection,
    material,
):
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, (), faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    obj.location = tuple(float(value) for value in location_m)
    collection.objects.link(obj)
    mesh.materials.append(material)
    return obj


def create_box(name: str, size_m, location_m, collection, material):
    half_x, half_y, half_z = (float(value) / 2.0 for value in size_m)
    vertices = (
        (-half_x, -half_y, -half_z),
        (half_x, -half_y, -half_z),
        (half_x, half_y, -half_z),
        (-half_x, half_y, -half_z),
        (-half_x, -half_y, half_z),
        (half_x, -half_y, half_z),
        (half_x, half_y, half_z),
        (-half_x, half_y, half_z),
    )
    faces = (
        (3, 2, 1, 0),
        (4, 5, 6, 7),
        (0, 1, 5, 4),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 0, 4, 7),
    )
    return _create_mesh_object(
        name, vertices, faces, location_m, collection, material
    )


def create_cylinder(
    name: str,
    radius_m,
    depth_m,
    location_m,
    collection,
    material,
    vertices=64,
):
    radius = float(radius_m)
    half_depth = float(depth_m) / 2.0
    vertex_count = int(vertices)
    mesh_vertices = []
    for z_value in (-half_depth, half_depth):
        mesh_vertices.extend(
            (
                radius * math.cos(2.0 * math.pi * index / vertex_count),
                radius * math.sin(2.0 * math.pi * index / vertex_count),
                z_value,
            )
            for index in range(vertex_count)
        )
    faces = [tuple(reversed(range(vertex_count)))]
    faces.append(tuple(range(vertex_count, vertex_count * 2)))
    for index in range(vertex_count):
        next_index = (index + 1) % vertex_count
        faces.append(
            (
                index,
                next_index,
                vertex_count + next_index,
                vertex_count + index,
            )
        )
    return _create_mesh_object(
        name, mesh_vertices, faces, location_m, collection, material
    )


def create_prism_from_polygon(
    name: str,
    vertices_xy_m,
    z_min_m,
    z_max_m,
    collection,
    material,
):
    polygon = tuple((float(x), float(y)) for x, y in vertices_xy_m)
    count = len(polygon)
    mesh_vertices = [
        (x, y, float(z_min_m)) for x, y in polygon
    ] + [
        (x, y, float(z_max_m)) for x, y in polygon
    ]
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    for index in range(count):
        next_index = (index + 1) % count
        faces.append((index, next_index, count + next_index, count + index))
    return _create_mesh_object(
        name, mesh_vertices, faces, (0.0, 0.0, 0.0), collection, material
    )


def apply_object_transforms(obj) -> None:
    transform = Matrix.LocRotScale(
        Vector((0.0, 0.0, 0.0)),
        obj.rotation_euler.to_quaternion(),
        Vector(obj.scale),
    )
    obj.data.transform(transform)
    obj.rotation_euler = (0.0, 0.0, 0.0)
    obj.scale = (1.0, 1.0, 1.0)
    obj.data.update()


def _clean_float(value: float) -> float:
    rounded = round(float(value), 6)
    return 0.0 if abs(rounded) < 0.0000005 else rounded


def measured_bounds(obj) -> dict[str, list[float]]:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        world_vertices = [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
        if not world_vertices:
            raise ValueError(f"Mesh object has no measured vertices: {obj.name}")
        return {
            "min": [
                _clean_float(min(vertex[axis] for vertex in world_vertices))
                for axis in range(3)
            ],
            "max": [
                _clean_float(max(vertex[axis] for vertex in world_vertices))
                for axis in range(3)
            ],
        }
    finally:
        evaluated.to_mesh_clear()
