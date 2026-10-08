#!/usr/bin/env python3
"""Generate monochrome Nu, Pogodi! artwork from pinned upstream assets."""

from __future__ import annotations

import argparse
import copy
import hashlib
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import urllib.request
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SVG = REPO_ROOT / "third_party" / "nu-pogodi" / "nupogodi.svg"
DEFAULT_PNG = REPO_ROOT / "third_party" / "nu-pogodi" / "fg.png"
DEFAULT_OUTPUT = REPO_ROOT / "src" / "apps" / "NuPogodiArtwork.generated.h"
WINDOWS_MAGICK = Path("C:/Program Files/ImageMagick-7.1.1-Q16-HDRI/magick.exe")

SVG_URL = "https://raw.githubusercontent.com/artyomsoft/nupogodi-sdl/master/resources/nupogodi/nupogodi.svg"
PNG_URL = "https://raw.githubusercontent.com/artyomsoft/nupogodi-sdl/master/resources/nupogodi/fg.png"
SVG_SHA256 = "546fdf894daa4db4cb8fdb931869f64b95fb19964bde35c05c138381b19d77ba"
PNG_SHA256 = "21033d220246ca54e6fb252fba5dd5bff70a4ad6e918b32e5214c36eec6a525d"

SVG_NS = "http://www.w3.org/2000/svg"
XLINK_NS = "http://www.w3.org/1999/xlink"
NUMERIC_TITLE = re.compile(r"\d+\.\d+\.\d+")
SCREENS = ((480, 800), (800, 480), (528, 792), (792, 528))
SVG_VIEWBOX_WIDTH = 1570.2845
SVG_VIEWBOX_HEIGHT = 989.1816
THRESHOLD = 160

SEGMENTS = {
    "bodies": ("1.1.0", "1.2.1"),
    "baskets": ("1.1.1", "1.0.0", "1.3.1", "1.3.0"),
    "eggs": (
        ("3.0.1", "3.0.0", "3.1.0", "3.1.1", "3.2.0"),
        ("3.2.1", "3.3.0", "3.3.1", "2.0.1", "2.0.0"),
        ("7.0.1", "8.3.1", "8.3.0", "8.2.0", "8.2.1"),
        ("8.1.0", "8.1.1", "8.0.0", "8.0.1", "0.3.1"),
    ),
    "misses": ("4.0.1", "4.1.1", "6.0.1"),
    "digits": (
        ("5.1.0", "5.0.0", "5.3.0", "5.3.1", "5.2.1", "5.1.1", "5.2.0"),
        ("6.1.0", "6.0.0", "6.3.0", "6.3.1", "6.2.1", "6.1.1", "6.2.0"),
        ("7.1.0", "7.0.0", "7.3.0", "7.3.1", "7.2.1", "7.1.1", "7.2.0"),
    ),
    "rabbit": ("4.3.1", "4.2.1"),
    "gameB": ("0.1.1",),
}


@dataclass(frozen=True)
class Variant:
    screen_w: int
    screen_h: int
    width: int
    height: int

    @property
    def name(self) -> str:
        return f"{self.screen_w}x{self.screen_h}"

    @property
    def symbol(self) -> str:
        return f"{self.screen_w}x{self.screen_h}".replace("x", "_")


@dataclass(frozen=True)
class Bitmap:
    x: int
    y: int
    width: int
    height: int
    data: bytes

    @property
    def rom_bytes(self) -> int:
        return len(self.data)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(url: str, path: Path, expected_sha256: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".download")
    with urllib.request.urlopen(url, timeout=30) as response:
        tmp.write_bytes(response.read())
    actual = sha256(tmp)
    if actual != expected_sha256:
        tmp.unlink(missing_ok=True)
        raise SystemExit(f"{url} hash mismatch: got {actual}, expected {expected_sha256}")
    tmp.replace(path)


def assert_pinned(path: Path, expected_sha256: str, label: str) -> None:
    if not path.exists():
        raise SystemExit(f"{label} is missing: {path}")
    actual = sha256(path)
    if actual != expected_sha256:
        raise SystemExit(f"{label} hash mismatch: got {actual}, expected {expected_sha256}")


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1].lower()


def validate_svg(path: Path) -> ET.Element:
    ET.register_namespace("", SVG_NS)
    root = ET.parse(path).getroot()
    for element in root.iter():
        name = local_name(element.tag)
        if name == "script":
            raise SystemExit("SVG contains a script element")
        for attr_name, value in element.attrib.items():
            attr = local_name(attr_name)
            if attr not in {"href", "src"}:
                continue
            if value.startswith("#"):
                continue
            if name == "image" and value.startswith("data:image/"):
                continue
            raise SystemExit(f"SVG contains an external reference: {value}")
    return root


def variants() -> list[Variant]:
    out = []
    ratio = SVG_VIEWBOX_WIDTH / SVG_VIEWBOX_HEIGHT
    for screen_w, screen_h in SCREENS:
        area_w = screen_w - 36
        area_h = screen_h - 288
        width = area_w
        height = round(width / ratio)
        if height > area_h:
            height = area_h
            width = round(height * ratio)
        out.append(Variant(screen_w, screen_h, width, height))
    return out


def title_text(element: ET.Element) -> str | None:
    title = element.find(f"{{{SVG_NS}}}title")
    if title is None:
        return None
    return title.text


def isolate_svg(root: ET.Element, segment: str, width: int, height: int) -> bytes:
    tree = copy.deepcopy(root)
    tree.set("width", str(width))
    tree.set("height", str(height))
    tree.set("preserveAspectRatio", "xMidYMid meet")
    for element in tree.iter():
        title = title_text(element)
        if title is None or not NUMERIC_TITLE.fullmatch(title or ""):
            continue
        style = element.get("style", "")
        if title == segment:
            element.set("style", style + ";display:inline;opacity:1;fill-opacity:1")
            element.set("display", "inline")
        else:
            element.set("style", style + ";display:none")
            element.set("display", "none")
    return ET.tostring(tree, encoding="utf-8", xml_declaration=True)


def render_svg(root: ET.Element, segment: str, variant: Variant, tmpdir: Path, magick: Path) -> Image.Image:
    svg_path = tmpdir / f"{variant.symbol}_{segment}.svg"
    png_path = tmpdir / f"{variant.symbol}_{segment}.png"
    svg_path.write_bytes(isolate_svg(root, segment, variant.width, variant.height))
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    result = subprocess.run(
        [
            str(magick),
            "-background",
            "white",
            str(svg_path),
            "-alpha",
            "remove",
            "-alpha",
            "off",
            str(png_path),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=flags,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode("utf-8", "replace"))
    image = Image.open(png_path).convert("L")
    if image.size != (variant.width, variant.height):
        image = image.resize((variant.width, variant.height), Image.Resampling.LANCZOS)
    return image


def encode_image(image: Image.Image, origin_x: int = 0, origin_y: int = 0) -> Bitmap:
    mask = image.point(lambda pixel: 0 if pixel <= THRESHOLD else 255, "L")
    inverted = mask.point(lambda pixel: 255 if pixel == 0 else 0, "L")
    box = inverted.getbbox()
    if box is None:
        raise ValueError("bitmap contains no ink")
    cropped = mask.crop(box)
    width, height = cropped.size
    row_bytes = math.ceil(width / 8)
    data = bytearray(row_bytes * height)
    pixels = cropped.load()
    for y in range(height):
        for x in range(width):
            if pixels[x, y] <= THRESHOLD:
                data[y * row_bytes + x // 8] &= ~(0x80 >> (x & 7))
            else:
                data[y * row_bytes + x // 8] |= 0x80 >> (x & 7)
        for bit in range(width, row_bytes * 8):
            data[y * row_bytes + bit // 8] |= 0x80 >> (bit & 7)
    return Bitmap(origin_x + box[0], origin_y + box[1], width, height, bytes(data))


def render_background(path: Path, variant: Variant) -> Bitmap:
    source = Image.open(path).convert("RGBA")
    scale = min(variant.width / source.width, variant.height / source.height)
    resized_w = max(1, round(source.width * scale))
    resized_h = max(1, round(source.height * scale))
    resized = source.resize((resized_w, resized_h), Image.Resampling.LANCZOS)
    flattened = Image.new("RGBA", resized.size, "white")
    flattened.alpha_composite(resized)
    origin_x = (variant.width - resized_w) // 2
    origin_y = (variant.height - resized_h) // 2
    return encode_image(flattened.convert("L"), origin_x, origin_y)


def flatten_segments() -> list[tuple[str, str]]:
    items: list[tuple[str, str]] = [("background", "background")]

    def add(prefix: str, value: object) -> None:
        if isinstance(value, str):
            items.append((prefix, value))
            return
        for index, child in enumerate(value):
            add(f"{prefix}_{index}", child)

    for key in ("bodies", "baskets", "eggs", "misses", "digits", "rabbit"):
        add(key, SEGMENTS[key])
    add("gameB", SEGMENTS["gameB"][0])
    return items


def render_all(root: ET.Element, png_path: Path, workers: int, magick: Path) -> tuple[dict[str, dict[str, Bitmap]], int]:
    result: dict[str, dict[str, Bitmap]] = {}
    total_rom = 0
    with tempfile.TemporaryDirectory(prefix="nu-pogodi-art-") as tmp:
        tmpdir = Path(tmp)
        for variant in variants():
            rendered: dict[str, Bitmap] = {"background": render_background(png_path, variant)}
            pairs = [(name, segment) for name, segment in flatten_segments() if name != "background"]

            def render_one(pair: tuple[str, str]) -> tuple[str, Bitmap]:
                name, segment = pair
                return name, encode_image(render_svg(root, segment, variant, tmpdir, magick))

            with ThreadPoolExecutor(max_workers=workers) as pool:
                for name, bitmap in pool.map(render_one, pairs):
                    rendered[name] = bitmap
            result[variant.name] = rendered
            total_rom += sum(bitmap.rom_bytes for bitmap in rendered.values())
    return result, total_rom


def c_bytes(data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        rows.append("  " + ", ".join(f"0x{byte:02x}" for byte in data[offset : offset + 16]) + ",")
    return "\n".join(rows)


def symbol_for(variant_name: str, bitmap_name: str) -> str:
    return f"kNuArt_{variant_name.replace('x', '_')}_{bitmap_name}"


def bitmap_ref(variant_name: str, bitmap_name: str, bitmaps: dict[str, Bitmap]) -> str:
    bitmap = bitmaps[bitmap_name]
    symbol = symbol_for(variant_name, bitmap_name)
    return f"{{{bitmap.x}, {bitmap.y}, {bitmap.width}, {bitmap.height}, {symbol}}}"


def nested_refs(variant_name: str, value: object, bitmaps: dict[str, Bitmap], prefix: str) -> str:
    if isinstance(value, str):
        return bitmap_ref(variant_name, prefix, bitmaps)
    return "{" + ", ".join(nested_refs(variant_name, child, bitmaps, f"{prefix}_{index}") for index, child in enumerate(value)) + "}"


def artwork_initializer(variant: Variant, bitmaps: dict[str, Bitmap]) -> str:
    parts = [
        str(variant.width),
        str(variant.height),
        bitmap_ref(variant.name, "background", bitmaps),
        nested_refs(variant.name, SEGMENTS["bodies"], bitmaps, "bodies"),
        nested_refs(variant.name, SEGMENTS["baskets"], bitmaps, "baskets"),
        nested_refs(variant.name, SEGMENTS["eggs"], bitmaps, "eggs"),
        nested_refs(variant.name, SEGMENTS["misses"], bitmaps, "misses"),
        nested_refs(variant.name, SEGMENTS["digits"], bitmaps, "digits"),
        nested_refs(variant.name, SEGMENTS["rabbit"], bitmaps, "rabbit"),
        bitmap_ref(variant.name, "gameB", bitmaps),
    ]
    return "{" + ", ".join(parts) + "}"


def generate_header(root: ET.Element, png_path: Path, workers: int, magick: Path) -> tuple[str, int]:
    rendered, total_rom = render_all(root, png_path, workers, magick)
    all_variants = variants()
    lines = [
        "#pragma once",
        "",
        "// Generated by tools/generate_nu_pogodi_art.py. Do not edit by hand.",
        f"// Source SVG SHA256: {SVG_SHA256}",
        f"// Source PNG SHA256: {PNG_SHA256}",
        "// Upstream artwork has no stated license; keep this generated file private/local.",
        "",
        "#include <cstdint>",
        "",
        "#ifndef PROGMEM",
        "#define PROGMEM",
        "#endif",
        "",
        "#undef PAPYRIX_NU_ORIGINAL_ART_AVAILABLE",
        "#define PAPYRIX_NU_ORIGINAL_ART_AVAILABLE 1",
        "",
        "namespace papyrix::games::art {",
        "namespace detail {",
        "",
    ]
    for variant in all_variants:
        bitmaps = rendered[variant.name]
        for name, _segment in flatten_segments():
            bitmap = bitmaps[name]
            lines.append(f"inline const uint8_t {symbol_for(variant.name, name)}[] PROGMEM = {{")
            lines.append(c_bytes(bitmap.data))
            lines.append("};")
            lines.append("")
        lines.append(f"inline const Artwork kArtwork_{variant.symbol} PROGMEM = {artwork_initializer(variant, bitmaps)};")
        lines.append("")
    lines.extend(
        [
            "}  // namespace detail",
            "",
            "inline const Artwork& artworkFor(int screenw, int screenh) {",
        ]
    )
    for variant in all_variants:
        lines.append(
            f"  if (screenw == {variant.screen_w} && screenh == {variant.screen_h}) return detail::kArtwork_{variant.symbol};"
        )
    lines.extend(
        [
            "  const int areaW = screenw - 36;",
            "  const int areaH = screenh - 288;",
            "  const Artwork* best = nullptr;",
            "  int bestArea = -1;",
        ]
    )
    for variant in all_variants:
        lines.extend(
            [
                f"  if ({variant.width} <= areaW && {variant.height} <= areaH && {variant.width * variant.height} > bestArea) {{",
                f"    best = &detail::kArtwork_{variant.symbol};",
                f"    bestArea = {variant.width * variant.height};",
                "  }",
            ]
        )
    lines.extend(
        [
            "  if (best != nullptr) return *best;",
            "  const int portrait = screenh >= screenw;",
            "  return portrait ? detail::kArtwork_480_800 : detail::kArtwork_800_480;",
            "}",
            "",
            "}  // namespace papyrix::games::art",
            "",
        ]
    )
    return "\n".join(lines), total_rom


def check_bounds(header: str) -> None:
    if " = {\n};" in header:
        raise SystemExit("generated an empty asset array")


def resolve_magick(path: Path | None) -> Path:
    if path is not None:
        return path
    discovered = shutil.which("magick")
    if discovered:
        return Path(discovered)
    return WINDOWS_MAGICK


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=DEFAULT_SVG)
    parser.add_argument("--background", type=Path, default=DEFAULT_PNG)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--magick", type=Path, help="path to ImageMagick magick executable")
    parser.add_argument("--fetch", action="store_true", help="download pinned upstream SVG and PNG before generating")
    parser.add_argument("--check", action="store_true", help="regenerate and compare without rewriting")
    args = parser.parse_args()

    if args.fetch:
        fetch(SVG_URL, args.input, SVG_SHA256)
        fetch(PNG_URL, args.background, PNG_SHA256)
    assert_pinned(args.input, SVG_SHA256, "SVG")
    assert_pinned(args.background, PNG_SHA256, "PNG")
    magick = resolve_magick(args.magick)
    if not magick.exists():
        raise SystemExit(f"ImageMagick not found: {magick}")
    root = validate_svg(args.input)
    header, total_rom = generate_header(root, args.background, max(1, args.workers), magick)
    check_bounds(header)
    if args.check:
        current = args.output.read_text(encoding="utf-8") if args.output.exists() else ""
        if current != header:
            raise SystemExit(f"{args.output} is not up to date")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(header, encoding="utf-8", newline="\n")
    print(
        textwrap.dedent(
            f"""\
            variants: {', '.join(f'{v.screen_w}x{v.screen_h}->{v.width}x{v.height}' for v in variants())}
            assets: {len(flatten_segments())} per variant, {len(flatten_segments()) * len(variants())} total
            rom_bytes: {total_rom}
            output: {args.output}
            """
        ).strip()
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
