"""Unit tests for Kern IDE path helpers (stdlib only). Run: python -m unittest Kern-IDE.tests.test_ide_paths"""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from app.ide_paths import read_text_flexible, resolve_existing, same_file_path


class IdePathsTests(unittest.TestCase):
    def test_same_file_path_none(self) -> None:
        self.assertFalse(same_file_path(None, Path("a")))
        self.assertFalse(same_file_path(Path("a"), None))

    def test_same_file_path_equal(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.kn"
            p.write_text("a", encoding="utf-8")
            self.assertTrue(same_file_path(p, Path(d) / "x.kn"))

    def test_read_text_flexible_utf8(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "f.kn"
            p.write_bytes("hello\n".encode("utf-8"))
            text, warn = read_text_flexible(p)
            self.assertEqual(text, "hello\n")
            self.assertIsNone(warn)

    def test_read_text_flexible_replace(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "f.kn"
            p.write_bytes(b"ok\xff\xfe\n")
            text, warn = read_text_flexible(p)
            self.assertIsNotNone(text)
            self.assertIsNotNone(warn)

    def test_resolve_existing(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "a"
            p.mkdir()
            r = resolve_existing(Path(d) / "a")
            self.assertTrue(r.exists())


if __name__ == "__main__":
    unittest.main()
