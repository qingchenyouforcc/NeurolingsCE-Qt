import json
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from generate_sha256sums import (  # noqa: E402
    ChecksumManifestError,
    build_checksum_lines,
    load_release_json,
    write_checksum_manifest,
)


ASSETS = [
    (
        "NeurolingsCE_linux_arm64_v0.5.0.AppImage",
        "298b57e5e9e04c2e5ae91647b783af60cf426f90be7a0a7c46eb06d04824209d",
    ),
    (
        "NeurolingsCE_linux_x86_64_v0.5.0.AppImage",
        "16baeefaaa639dda259c409ca5ca81afee0c5aebb0344743767f142f96826277",
    ),
    (
        "NeurolingsCE_macos_arm64_v0.5.0.zip",
        "283eed321ab479536a54abe0caef04181755731e618cd52add9963877ef0926b",
    ),
    (
        "NeurolingsCE_macos_x86_64_v0.5.0.zip",
        "9f5132c4130521a5183d45761ad2132a765aa7639af3f65431aa75b6e0d91d8d",
    ),
    (
        "NeurolingsCE_mascot_pack_v0.5.0.zip",
        "9e73fbe47496838799670029958200d135d1d1ecc60b3645058344f2a9a956dc",
    ),
    (
        "NeurolingsCE_windows_x86_64_v0.5.0-setup.exe",
        "84f1b0fd239c1ab5bb8db40bf1388e975a3b7cc78164db212b3656b6e69187cb",
    ),
    (
        "NeurolingsCE_windows_x86_64_v0.5.0.msi",
        "1d2ae8b7385e2f7dfc1a35e0cd76351889f570ce2d87b173448406b19694f9e3",
    ),
    (
        "NeurolingsCE_windows_x86_64_v0.5.0.zip",
        "21f1f553b44523ef85e4ad87df493dd344eae7df24267f85a27568bf054ec0f7",
    ),
]


def asset(name: str, digest: str) -> dict[str, str]:
    return {"name": name, "digest": f"sha256:{digest}"}


class GenerateSha256SumsTests(unittest.TestCase):
    def test_eight_assets_are_sorted_and_end_with_one_lf(self) -> None:
        release = {"assets": [asset(name, digest) for name, digest in reversed(ASSETS)]}
        expected = "\n".join(
            f"{digest}  {name}" for name, digest in ASSETS
        ) + "\n"

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "nested" / "SHA256SUMS.txt"
            write_checksum_manifest(release, output)
            self.assertEqual(output.read_bytes(), expected.encode("utf-8"))
            self.assertTrue(output.read_bytes().endswith(b"\n"))
            self.assertNotIn(b"\r", output.read_bytes())

    def test_existing_checksum_assets_are_skipped_case_insensitively(self) -> None:
        release = {
            "assets": [
                {"name": "SHA256SUMS"},
                {"name": "sHa256SuMs.TxT", "digest": "not-a-digest"},
                asset(*ASSETS[0]),
            ]
        }

        self.assertEqual(
            build_checksum_lines(release),
            [f"{ASSETS[0][1]}  {ASSETS[0][0]}"],
        )

    def test_rejects_missing_or_invalid_digest(self) -> None:
        for invalid_asset in (
            {"name": "missing.bin"},
            asset("bad.bin", "a" * 63),
            {"name": "bad.bin", "digest": "sha256:" + "g" * 64},
            {"name": "bad.bin", "digest": " sha256:" + "a" * 64},
        ):
            with self.subTest(invalid_asset=invalid_asset):
                with self.assertRaises(ChecksumManifestError):
                    build_checksum_lines({"assets": [invalid_asset]})

    def test_rejects_duplicate_and_newline_names(self) -> None:
        duplicate = [asset("same.bin", "a" * 64), asset("same.bin", "b" * 64)]
        with self.assertRaises(ChecksumManifestError):
            build_checksum_lines({"assets": duplicate})

        for unsafe_name in (
            "bad\nname.bin",
            "bad\rname.bin",
            "folder/name.bin",
            "folder\\name.bin",
            "bad:name.bin",
            "bad\x00name.bin",
            ".",
            "..",
            "bad.bin.",
            "bad.bin ",
        ):
            with self.subTest(unsafe_name=unsafe_name):
                with self.assertRaises(ChecksumManifestError):
                    build_checksum_lines({"assets": [asset(unsafe_name, "a" * 64)]})

        with self.assertRaises(ChecksumManifestError):
            build_checksum_lines(
                {"assets": [asset("Same.bin", "a" * 64), asset("same.BIN", "b" * 64)]}
            )

    def test_rejects_bad_json_and_release_structure(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            bad_json = Path(temp_dir) / "bad.json"
            bad_json.write_text("{not json", encoding="utf-8")
            with self.assertRaises(ChecksumManifestError):
                load_release_json(bad_json)

        for invalid_release in (
            [],
            {},
            {"assets": {}},
            {"assets": ["not an object"]},
            {"assets": [{"name": None, "digest": "sha256:" + "a" * 64}]},
        ):
            with self.subTest(invalid_release=invalid_release):
                with self.assertRaises(ChecksumManifestError):
                    build_checksum_lines(invalid_release)

    def test_json_fixture_is_local_only(self) -> None:
        release = {"assets": [asset(*ASSETS[0])]}
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "release.json"
            source.write_text(json.dumps(release), encoding="utf-8")
            self.assertEqual(load_release_json(source), release)


if __name__ == "__main__":
    unittest.main()
