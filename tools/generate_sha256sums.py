"""Generate a deterministic SHA256SUMS.txt from GitHub release metadata."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


SHA256_DIGEST_RE = re.compile(r"sha256:([0-9a-fA-F]{64})\Z")
CHECKSUM_ASSET_NAMES = {"sha256sums", "sha256sums.txt"}
UNSAFE_NAME_CHARACTERS = set('<>:"/\\|?*')


class ChecksumManifestError(ValueError):
    """Raised when release metadata cannot produce a safe checksum manifest."""


def _release_assets(release: Any) -> list[dict[str, Any]]:
    if not isinstance(release, dict):
        raise ChecksumManifestError("release metadata must be a JSON object")

    assets = release.get("assets")
    if not isinstance(assets, list):
        raise ChecksumManifestError("release metadata assets must be a JSON array")

    normalized: list[dict[str, Any]] = []
    for index, asset in enumerate(assets):
        if not isinstance(asset, dict):
            raise ChecksumManifestError(f"asset {index} must be a JSON object")
        normalized.append(asset)
    return normalized


def _validate_asset_name(raw_name: Any, index: int) -> str:
    if not isinstance(raw_name, str) or not raw_name:
        raise ChecksumManifestError(f"asset {index} name must be a non-empty string")

    if raw_name in {".", ".."}:
        raise ChecksumManifestError(f"asset {index} name is not a safe file name: {raw_name!r}")
    if raw_name[-1] in {".", " "}:
        raise ChecksumManifestError(
            f"asset {index} name must not end with a dot or space: {raw_name!r}"
        )
    if any(character in UNSAFE_NAME_CHARACTERS for character in raw_name):
        raise ChecksumManifestError(f"asset {index} name is not a safe file name: {raw_name!r}")
    if any(ord(character) < 0x20 or ord(character) == 0x7F for character in raw_name):
        raise ChecksumManifestError(f"asset {index} name contains a control character: {raw_name!r}")
    try:
        raw_name.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ChecksumManifestError(
            f"asset {index} name must be valid UTF-8: {raw_name!r}"
        ) from error
    return raw_name


def build_checksum_lines(release: Any) -> list[str]:
    """Return sorted SHA-256 checksum lines for every non-SUMS release asset."""

    entries: list[tuple[str, str]] = []
    seen_names: set[str] = set()

    for index, asset in enumerate(_release_assets(release)):
        raw_name = asset.get("name")
        if isinstance(raw_name, str) and raw_name.lower() in CHECKSUM_ASSET_NAMES:
            continue

        raw_name = _validate_asset_name(raw_name, index)
        name_key = raw_name.casefold()
        if name_key in seen_names:
            raise ChecksumManifestError(f"duplicate release asset name: {raw_name!r}")
        seen_names.add(name_key)

        raw_digest = asset.get("digest")
        if not isinstance(raw_digest, str):
            raise ChecksumManifestError(
                f"asset {raw_name!r} digest must match sha256:<64 hex characters>"
            )
        digest_match = SHA256_DIGEST_RE.fullmatch(raw_digest)
        if digest_match is None:
            raise ChecksumManifestError(
                f"asset {raw_name!r} digest must match sha256:<64 hex characters>"
            )
        entries.append((raw_name, digest_match.group(1).lower()))

    entries.sort(key=lambda entry: entry[0].encode("utf-8"))
    return [f"{digest}  {name}" for name, digest in entries]


def load_release_json(path: Path) -> Any:
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as error:
        raise ChecksumManifestError(f"could not read release metadata: {error}") from error

    try:
        return json.loads(source)
    except json.JSONDecodeError as error:
        raise ChecksumManifestError(f"invalid release metadata JSON: {error}") from error


def write_checksum_manifest(release: Any, output: Path) -> None:
    lines = build_checksum_lines(release)
    payload = ("\n".join(lines) + "\n").encode("utf-8")
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(payload)
    except OSError as error:
        raise ChecksumManifestError(f"could not write checksum manifest: {error}") from error


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate deterministic SHA256SUMS.txt from GitHub release JSON."
    )
    parser.add_argument("--release-json", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    try:
        release = load_release_json(args.release_json)
        write_checksum_manifest(release, args.output)
    except ChecksumManifestError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
