import argparse
import json
import re
from pathlib import Path


APP_NAME = "NeurolingsCE"
DEFAULT_MIN_SUPPORTED_VERSION = "0.2.0"


def normalize_version(tag: str) -> str:
    tag = (tag or "").strip()
    if tag.lower().startswith("v"):
        tag = tag[1:]
    match = re.search(r"(\d+(?:\.\d+)+)", tag)
    return match.group(1) if match else tag


def detect_platform(asset_name: str) -> str | None:
    name = asset_name.lower()

    if name in {"sha256sums", "sha256sums.txt"}:
        return "sha256sums"

    if "windows" in name or "win" in name:
        if "arm64" in name or "aarch64" in name:
            return "windows-arm64"
        return "windows-x86_64"

    if "linux" in name or name.endswith(".appimage"):
        if "arm64" in name or "aarch64" in name:
            return "linux-arm64"
        return "linux-x86_64"

    if "macos" in name or "darwin" in name or "mac" in name:
        if "arm64" in name or "aarch64" in name:
            return "macos-arm64"
        return "macos-x86_64"

    return None


def clean_release_notes(body: str | None, max_len: int = 1200) -> str:
    if not body:
        return ""
    body = body.strip().replace("\r\n", "\n")
    return body[:max_len]


def asset_sha256(asset: dict) -> str:
    digest = (asset.get("digest") or "").strip().lower()
    if digest.startswith("sha256:"):
        return digest[len("sha256:") :]
    return ""


def build_manifest(release: dict, min_supported_version: str) -> dict:
    tag = release["tag_name"]
    version = normalize_version(tag)
    assets: dict[str, dict] = {}

    for asset in release.get("assets", []):
        asset_name = asset.get("name", "")
        platform = detect_platform(asset_name)
        download_url = asset.get("browser_download_url", "")
        if platform is None or not asset_name or not download_url:
            continue

        assets[platform] = {
            "name": asset_name,
            "url": download_url,
            "sha256": asset_sha256(asset),
            "size": asset.get("size", 0),
            "content_type": asset.get("content_type", ""),
        }

    return {
        "schema": 1,
        "app": APP_NAME,
        "channel": "stable",
        "version": version,
        "tag": tag,
        "published_at": release.get("published_at", ""),
        "mandatory": False,
        "min_supported_version": min_supported_version,
        "release_page": release.get("html_url", ""),
        "notes": clean_release_notes(release.get("body")),
        "assets": assets,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate the static NeurolingsCE update manifest from a GitHub release JSON."
    )
    parser.add_argument("--release-json", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument(
        "--min-supported-version",
        default=DEFAULT_MIN_SUPPORTED_VERSION,
        help="Minimum client version supported by this release manifest.",
    )
    args = parser.parse_args()

    release = json.loads(Path(args.release_json).read_text(encoding="utf-8"))
    manifest = build_manifest(release, args.min_supported_version)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
