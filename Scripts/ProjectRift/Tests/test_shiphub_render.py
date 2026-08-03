import binascii
import importlib.util
import struct
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image, ImageChops


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_HELPER_PATH = (
    Path(__file__).parents[1]
    / "ArtPipeline"
    / "shiphub"
    / "render_shiphub_drawings.py"
)


def _chunk(chunk_type: bytes, payload: bytes) -> bytes:
    body = chunk_type + payload
    return (
        struct.pack(">I", len(payload))
        + body
        + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)
    )


def _chunks(data: bytes) -> list[tuple[bytes, bytes]]:
    if not data.startswith(PNG_SIGNATURE):
        raise AssertionError("fixture is not a PNG")
    result = []
    offset = len(PNG_SIGNATURE)
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        result.append((chunk_type, payload))
        offset += length + 12
    return result


def _load_render_module():
    fake_mathutils = types.ModuleType("mathutils")
    fake_mathutils.Matrix = object
    fake_mathutils.Vector = object
    fake_modules = {
        "bmesh": types.ModuleType("bmesh"),
        "bpy": types.ModuleType("bpy"),
        "mathutils": fake_mathutils,
    }
    spec = importlib.util.spec_from_file_location(
        "shiphub_render_png_test_target", PNG_HELPER_PATH
    )
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, fake_modules):
        assert spec.loader is not None
        spec.loader.exec_module(module)
    return module


class ShipHubRenderPngTests(unittest.TestCase):
    def _volatile_fixture(self, path: Path) -> bytes:
        Image.new("RGBA", (3, 2), (17, 34, 51, 255)).save(path)
        chunks = _chunks(path.read_bytes())
        rebuilt = bytearray(PNG_SIGNATURE)
        for chunk_type, payload in chunks:
            rebuilt.extend(_chunk(chunk_type, payload))
            if chunk_type == b"IHDR":
                rebuilt.extend(_chunk(b"tEXt", b"Date\x002026/08/03 09:05:10"))
                rebuilt.extend(_chunk(b"tEXt", b"RenderTime\x00292.85"))
                rebuilt.extend(_chunk(b"tEXt", b"Software\x00Blender 5.2 LTS"))
                rebuilt.extend(_chunk(b"pHYs", struct.pack(">IIB", 72, 72, 1)))
        path.write_bytes(rebuilt)
        return bytes(rebuilt)

    def test_png_normalization_removes_blender_date_and_render_time_text(self) -> None:
        """Preserving Blender Date/RenderTime tEXt must fail deterministic render publication."""
        render = _load_render_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "render.png"
            self._volatile_fixture(path)

            render._write_300dpi_phys(path)

            text_payloads = [
                payload for chunk_type, payload in _chunks(path.read_bytes())
                if chunk_type == b"tEXt"
            ]
            self.assertNotIn(b"Date\x002026/08/03 09:05:10", text_payloads)
            self.assertNotIn(b"RenderTime\x00292.85", text_payloads)
            self.assertIn(b"Software\x00Blender 5.2 LTS", text_payloads)

    def test_png_normalization_preserves_decoded_pixels(self) -> None:
        render = _load_render_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "render.png"
            self._volatile_fixture(path)
            with Image.open(path) as source:
                before = source.convert("RGBA")

            render._write_300dpi_phys(path)

            with Image.open(path) as source:
                after = source.convert("RGBA")
            self.assertIsNone(ImageChops.difference(before, after).getbbox())

    def test_png_normalization_writes_one_300dpi_phys_and_is_byte_idempotent(self) -> None:
        render = _load_render_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "render.png"
            self._volatile_fixture(path)

            render._write_300dpi_phys(path)
            first = path.read_bytes()
            render._write_300dpi_phys(path)
            second = path.read_bytes()

            phys = [
                payload for chunk_type, payload in _chunks(second)
                if chunk_type == b"pHYs"
            ]
            self.assertEqual(
                [struct.pack(">IIB", 11811, 11811, 1)],
                phys,
            )
            self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
