from __future__ import annotations

from pathlib import Path
import re
import sys


def parse_array(content: str, name: str) -> list[int]:
    pat = re.compile(re.escape(name) + r"\s*\[\]\s*=\s*\{(.*?)\}\s*;", re.S)
    m = pat.search(content)
    if not m:
        raise RuntimeError(f"array {name} not found")
    body = m.group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//.*?$", "", body, flags=re.M)
    nums = re.findall(r"0x[0-9a-fA-F]+|\d+", body)
    return [int(n, 0) for n in nums]


def parse_glyph_dsc(content: str) -> list[dict]:
    m = re.search(r"glyph_dsc\s*\[\]\s*=\s*\{(.*?)\};", content, re.S)
    if not m:
        raise RuntimeError("glyph_dsc not found")
    body = m.group(1)
    pattern = re.compile(
        r"\.?\s*bitmap_index\s*=\s*(\d+)\s*,\s*"
        r"\.?\s*adv_w\s*=\s*(\d+)\s*,\s*"
        r"\.?\s*box_w\s*=\s*(\d+)\s*,\s*"
        r"\.?\s*box_h\s*=\s*(\d+)\s*,\s*"
        r"\.?\s*ofs_x\s*=\s*(-?\d+)\s*,\s*"
        r"\.?\s*ofs_y\s*=\s*(-?\d+)"
    )
    out = []
    for m in pattern.finditer(body):
        out.append(
            {
                "bitmap_index": int(m.group(1)),
                "adv_w": int(m.group(2)),
                "box_w": int(m.group(3)),
                "box_h": int(m.group(4)),
                "ofs_x": int(m.group(5)),
                "ofs_y": int(m.group(6)),
            }
        )
    return out


def parse_cmaps(content: str) -> list[dict]:
    m = re.search(r"cmaps\s*\[\]\s*=\s*\{(.*?)\};", content, re.S)
    if not m:
        raise RuntimeError("cmaps not found")
    body = m.group(1)
    pattern = re.compile(
        r"range_start\s*=\s*(\d+),"
        r".*?range_length\s*=\s*(\d+),"
        r".*?glyph_id_start\s*=\s*(\d+),"
        r".*?unicode_list\s*=\s*([^,]+),"
        r".*?glyph_id_ofs_list\s*=\s*([^,]+),"
        r".*?list_length\s*=\s*(\d+),"
        r".*?type\s*=\s*([A-Z0-9_]+)",
        re.S
    )
    out = []
    for m in pattern.finditer(body):
        out.append(
            {
                "range_start": int(m.group(1)),
                "range_length": int(m.group(2)),
                "glyph_id_start": int(m.group(3)),
                "unicode_list": m.group(4).strip(),
                "glyph_id_ofs_list": m.group(5).strip(),
                "list_length": int(m.group(6)),
                "type": m.group(7).strip(),
            }
        )
    return out


def parse_font_meta(content: str) -> tuple[int, int, int]:
    m = re.search(r"lv_font_t\s+\w+\s*=\s*\{(.*?)\};", content, re.S)
    if not m:
        raise RuntimeError("lv_font_t not found")
    body = m.group(1)
    line_height = int(re.search(r"line_height\s*=\s*(\d+)", body).group(1))
    base_line = int(re.search(r"base_line\s*=\s*(\d+)", body).group(1))

    m2 = re.search(r"font_dsc\s*=\s*\{(.*?)\};", content, re.S)
    if not m2:
        raise RuntimeError("font_dsc not found")
    body2 = m2.group(1)
    bpp = int(re.search(r"bpp\s*=\s*(\d+)", body2).group(1))
    return line_height, base_line, bpp


def parse_simple_array(content: str, name: str) -> list[int] | None:
    pat = re.compile(re.escape(name) + r"\s*\[\]\s*=\s*\{(.*?)\}\s*;", re.S)
    m = pat.search(content)
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//.*?$", "", body, flags=re.M)
    nums = re.findall(r"0x[0-9a-fA-F]+|-?\d+", body)
    out = []
    for n in nums:
        if n.startswith("0x") or n.startswith("-0x"):
            out.append(int(n, 0))
        else:
            out.append(int(n, 10))
    return out


def parse_kern_classes(content: str) -> dict | None:
    m = re.search(r"kern_classes\s*=\s*\{(.*?)\};", content, re.S)
    if not m:
        return None
    body = m.group(1)
    def get(name: str) -> str | None:
        mm = re.search(rf"{name}\s*=\s*([A-Za-z0-9_]+)", body)
        return mm.group(1) if mm else None
    left_map = get("left_class_mapping")
    right_map = get("right_class_mapping")
    values = get("class_pair_values")
    left_cnt = re.search(r"left_class_cnt\s*=\s*(\d+)", body)
    right_cnt = re.search(r"right_class_cnt\s*=\s*(\d+)", body)
    if not left_map or not right_map or not values or not left_cnt or not right_cnt:
        return None
    return {
        "left_map": left_map,
        "right_map": right_map,
        "values": values,
        "left_cnt": int(left_cnt.group(1)),
        "right_cnt": int(right_cnt.group(1)),
    }


def build_ranges(cmaps: list[dict]) -> list[dict]:
    ranges = []
    for c in cmaps:
        if c["unicode_list"] != "NULL":
            continue
        if c["type"] not in ("LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY", "LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL"):
            continue
        ranges.append(
            {
                "range_start": c["range_start"],
                "range_length": c["range_length"],
                "glyph_id_start": c["glyph_id_start"],
            }
        )
    return ranges


def build_sparse_map(cmaps: list[dict], arrays: dict[str, list[int]]) -> tuple[list[int], list[int]]:
    entries = []
    for c in cmaps:
        if c["unicode_list"] == "NULL":
            continue
        ulist = arrays.get(c["unicode_list"])
        if not ulist:
            continue
        ofs_list = arrays.get(c["glyph_id_ofs_list"]) if c["glyph_id_ofs_list"] != "NULL" else None
        for i, ofs in enumerate(ulist):
            code = c["range_start"] + ofs
            gid = c["glyph_id_start"] + i
            if ofs_list and i < len(ofs_list):
                gid = c["glyph_id_start"] + ofs_list[i]
            entries.append((code, gid))
    entries.sort(key=lambda x: x[0])
    codes = [c for c, _ in entries]
    gids = [g for _, g in entries]
    return codes, gids


def find_fallback_idx(ranges: list[dict], glyphs: list[dict], code: int) -> int:
    for r in ranges:
        if code >= r["range_start"] and code < r["range_start"] + r["range_length"]:
            return r["glyph_id_start"] + (code - r["range_start"])
    return 0


def emit_cppm(out_path: Path, module_name: str, font_name: str,
              glyph_bitmap: list[int], glyphs: list[dict], ranges: list[dict],
              line_height: int, base_line: int, bpp: int,
              kern: dict | None, arrays: dict[str, list[int]],
              sparse_codes: list[int], sparse_gids: list[int]) -> None:
    baseline = line_height - base_line
    fallback_idx = find_fallback_idx(ranges, glyphs, 63)  # '?'
    out = []
    out.append("module;")
    out.append("#include <cstdint>")
    out.append(f"export module {module_name};")
    out.append("")
    out.append("import charm.font;")
    out.append("")
    out.append("static constexpr std::uint8_t glyph_bitmaps[] = {")
    for i, v in enumerate(glyph_bitmap):
        if i % 16 == 0:
            out.append("    ")
        out[-1] += f"0x{v:02x}, "
        if i % 16 == 15:
            out.append("")
    out.append("};")
    out.append("")
    out.append("static constexpr Glyph glyph_table[] = {")
    for g in glyphs:
        adv = (g["adv_w"] + 8) >> 4
        y_offset = g["box_h"] + g["ofs_y"]
        out.append(
            f"    {{ glyph_bitmaps + {g['bitmap_index']}, {g['box_w']}, {g['box_h']}, "
            f"{adv}, {g['ofs_x']}, {y_offset}, {bpp} }},"
        )
    out.append("};")
    out.append("")
    out.append("static constexpr GlyphRange glyph_ranges[] = {")
    for r in ranges:
        out.append(f"    {{ {r['range_start']}, {r['range_length']}, {r['glyph_id_start']} }},")
    out.append("};")
    out.append("")
    if sparse_codes:
        out.append("static constexpr std::uint32_t sparse_codes[] = {")
        for i, v in enumerate(sparse_codes):
            if i % 16 == 0:
                out.append("    ")
            out[-1] += f"{v}, "
            if i % 16 == 15:
                out.append("")
        out.append("};")
        out.append("")
        out.append("static constexpr std::uint16_t sparse_glyph_ids[] = {")
        for i, v in enumerate(sparse_gids):
            if i % 16 == 0:
                out.append("    ")
            out[-1] += f"{v}, "
            if i % 16 == 15:
                out.append("")
        out.append("};")
        out.append("")
    if kern:
        out.append("static constexpr std::int8_t kern_class_values[] = {")
        vals = arrays.get(kern["values"], [])
        for i, v in enumerate(vals):
            if i % 16 == 0:
                out.append("    ")
            out[-1] += f"{v}, "
            if i % 16 == 15:
                out.append("")
        out.append("};")
        out.append("")
        out.append("static constexpr std::uint8_t kern_left_class_map[] = {")
        vals = arrays.get(kern["left_map"], [])
        for i, v in enumerate(vals):
            if i % 16 == 0:
                out.append("    ")
            out[-1] += f"{v}, "
            if i % 16 == 15:
                out.append("")
        out.append("};")
        out.append("")
        out.append("static constexpr std::uint8_t kern_right_class_map[] = {")
        vals = arrays.get(kern["right_map"], [])
        for i, v in enumerate(vals):
            if i % 16 == 0:
                out.append("    ")
            out[-1] += f"{v}, "
            if i % 16 == 15:
                out.append("")
        out.append("};")
        out.append("")
    out.append(f"export constexpr Font {font_name} = {{")
    out.append("    .table = glyph_table,")
    out.append("    .ranges = glyph_ranges,")
    out.append(f"    .fallback_glyph = &glyph_table[{fallback_idx}],")
    out.append(f"    .line_height = {line_height},")
    out.append(f"    .baseline = {baseline},")
    if sparse_codes:
        out.append("    .sparse_codes = sparse_codes,")
        out.append("    .sparse_glyph_ids = sparse_glyph_ids,")
    if kern:
        out.append("    .kern_class_values = kern_class_values,")
        out.append("    .kern_left_class_map = kern_left_class_map,")
        out.append("    .kern_right_class_map = kern_right_class_map,")
        out.append(f"    .kern_left_class_cnt = {kern['left_cnt']},")
        out.append(f"    .kern_right_class_cnt = {kern['right_cnt']},")
    out.append("};")
    out.append("")
    out.append(f"static_assert(validate_font({font_name}));")
    out_path.write_text("\n".join(out), encoding="utf-8")


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: convert_lvgl_font.py <lv_font_montserrat_12.c> <out.cppm>")
        return 1
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    content = src.read_text(encoding="utf-8")
    glyph_bitmap = parse_array(content, "glyph_bitmap")
    glyphs = parse_glyph_dsc(content)
    cmaps = parse_cmaps(content)
    ranges = build_ranges(cmaps)
    line_height, base_line, bpp = parse_font_meta(content)
    stem = out.stem
    module_name = f"charm.font.{stem}"
    font_name = "font_" + stem.replace("lv_font_", "")
    kern = parse_kern_classes(content)
    arrays = {}
    if kern:
        for key in (kern["values"], kern["left_map"], kern["right_map"]):
            vals = parse_simple_array(content, key)
            if vals is not None:
                arrays[key] = vals
    # sparse arrays referenced by cmaps
    for c in cmaps:
        if c["unicode_list"] != "NULL":
            vals = parse_simple_array(content, c["unicode_list"])
            if vals is not None:
                arrays[c["unicode_list"]] = vals
        if c["glyph_id_ofs_list"] != "NULL":
            vals = parse_simple_array(content, c["glyph_id_ofs_list"])
            if vals is not None:
                arrays[c["glyph_id_ofs_list"]] = vals

    sparse_codes, sparse_gids = build_sparse_map(cmaps, arrays)
    emit_cppm(out, module_name, font_name, glyph_bitmap, glyphs, ranges,
              line_height, base_line, bpp, kern, arrays, sparse_codes, sparse_gids)
    print(f"Generated {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
