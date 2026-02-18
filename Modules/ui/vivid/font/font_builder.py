# Updated font_builder.py for new C++ font structure
# Install dependencies:
# pip install pillow
# --------------
# Usage:
#   python font_builder.py \
#       --font-file /path/to/YourFont.ttf \
#       --size 16 \
#       --chars 32-126 \
#       --out-module font_generated.cppm
# --------------
# Or Usage (example):
#   python .\font_builder.py --font-file .\JetBrainsMono-Light.ttf --size 12 --chars 32-126 --out-module .\font_generated.cppm --format bin
# --------------

import argparse
import os
from PIL import Image, ImageDraw, ImageFont

def parse_chars(range_str):
    """Parse character ranges like '32-126,160-255' into a list of character codes."""
    parts = range_str.split(',')
    chars = []
    for part in parts:
        if '-' in part:
            start, end = map(int, part.split('-'))
            chars.extend(range(start, end+1))
        else:
            chars.append(int(part))
    return sorted(chars)

def prompt_if_none(value, prompt_text):
    if value is None:
        return input(prompt_text).strip()
    return value

def build_glyph_ranges(codes):
    """Build contiguous ranges from sorted character codes."""
    if not codes:
        return []

    ranges = []
    range_start = codes[0]
    range_length = 1
    glyph_id_start = 0

    for i in range(1, len(codes)):
        if codes[i] == codes[i-1] + 1:
            range_length += 1
        else:
            ranges.append((range_start, range_length, glyph_id_start))
            range_start = codes[i]
            glyph_id_start = i
            range_length = 1

    # Add the last range
    ranges.append((range_start, range_length, glyph_id_start))
    return ranges

def main():
    parser = argparse.ArgumentParser(description="Generate a C++23 bitmap font module from a TTF/OTF.")
    parser.add_argument('--font-file', help='Path to TTF/OTF font file.')
    parser.add_argument('--size', type=int, help='Font pixel size (height).')
    parser.add_argument('--chars', help='Character code ranges, e.g. "32-126,160-255".')
    parser.add_argument('--out-module', help='Output C++ module filename.')
    parser.add_argument('--format', choices=['hex', 'bin'], default='hex',
                        help='Output format for bitmap data (hex or bin).')
    parser.add_argument('--bpp', type=int, choices=[1, 2, 4, 8], default=4,
                        help='Bits per pixel for glyph bitmaps (default: 4).')
    parser.add_argument('--fallback-char', type=int, default=63,
                        help='Character code to use as fallback glyph (default: 63 for "?").')
    args = parser.parse_args()

    # Interactive prompts if not provided
    font_file = prompt_if_none(args.font_file, "Enter path to font file: ")
    size = args.size if args.size is not None else int(input("Enter font size (pixels): ").strip())
    chars = prompt_if_none(args.chars, "Enter character ranges (e.g. 32-126): ")
    out_module = prompt_if_none(args.out_module, "Enter output module filename: ")

    print(f"Using font file: {font_file}")
    print(f"Font size: {size}px")
    print(f"Character ranges: {chars}")
    print(f"Output module: {out_module}")
    print(f"Output format: {args.format}")
    print(f"Fallback character: {args.fallback_char} ('{chr(args.fallback_char)}')")
    print(f"Bitmap bpp: {args.bpp}")

    if not os.path.isfile(font_file):
        print(f"Error: font file '{font_file}' does not exist.")
        return

    codes = parse_chars(chars)
    try:
        font = ImageFont.truetype(font_file, size)
    except Exception as e:
        print(f"Error loading font: {e}")
        return

    # Get font metrics
    # Use a sample character to estimate baseline and line height
    sample_char = 'A'
    try:
        # Get ascent and descent from font
        ascent, descent = font.getmetrics()

        # line_height is the total vertical space
        line_height = ascent + abs(descent)

        # baseline is the distance from top to the baseline
        baseline = ascent

        print(f"Font metrics: ascent={ascent}, descent={descent}")
        print(f"Calculated: line_height={line_height}, baseline={baseline}")
    except Exception as e:
        print(f"Warning: Could not get font metrics ({e}), using fallback values")
        # Fallback if getmetrics is not available
        baseline = int(size * 0.8)
        line_height = size

    glyphs = []
    fallback_index = None

    for idx, code in enumerate(codes):
        ch = chr(code)

        # Get character bounding box
        try:
            bbox = font.getbbox(ch)
            # bbox = (left, top, right, bottom)
            # offset_x: horizontal offset from cursor
            offset_x = bbox[0]
            # offset_y: vertical distance from baseline to TOP of glyph
            # Positive values mean the glyph top is ABOVE baseline
            # For PIL, bbox[1] is negative for characters above baseline

            # offset_y = -bbox[1]  # Convert to distance above baseline
            # CONTRACT:
            # y_offset = distance from glyph top to font baseline
            glyph_top = bbox[1]
            offset_y = baseline - glyph_top

        except:
            offset_x = 0
            offset_y = 0

        # Get bitmap
        mask_mode = '1' if args.bpp == 1 else 'L'
        mask = font.getmask(ch, mode=mask_mode)
        width, height = mask.size

        # Pack bitmap into bytes
        rows = []
        for y in range(height):
            if args.bpp == 1:
                row_bits = 0
                bits_count = 0
                for x in range(width):
                    pixel = mask.getpixel((x, y))
                    row_bits = (row_bits << 1) | (1 if pixel else 0)
                    bits_count += 1
                    if bits_count == 8:
                        rows.append(row_bits)
                        row_bits = 0
                        bits_count = 0
                if bits_count > 0:
                    row_bits <<= (8 - bits_count)
                    rows.append(row_bits)
            elif args.bpp == 2:
                row_bits = 0
                bits_count = 0
                for x in range(width):
                    pixel = mask.getpixel((x, y))
                    value = (pixel >> 6) & 0x03
                    row_bits = (row_bits << 2) | value
                    bits_count += 2
                    if bits_count == 8:
                        rows.append(row_bits)
                        row_bits = 0
                        bits_count = 0
                if bits_count > 0:
                    row_bits <<= (8 - bits_count)
                    rows.append(row_bits)
            elif args.bpp == 4:
                nibble = None
                for x in range(width):
                    pixel = mask.getpixel((x, y))
                    value = (pixel >> 4) & 0x0F
                    if nibble is None:
                        nibble = value
                    else:
                        rows.append((nibble << 4) | value)
                        nibble = None
                if nibble is not None:
                    rows.append((nibble << 4) & 0xF0)
            else:
                for x in range(width):
                    pixel = mask.getpixel((x, y))
                    rows.append(pixel)

        # Get advance width (horizontal spacing)
        try:
            # Try to get the advance width from font metrics
            # advance = font.getlength(ch)
            advance = int(round(font.getlength(ch)))
        except:
            # Fallback to using width
            advance = width

        advance = int(advance)

        # Debug output for some characters
        if ch in 'yesABC':
            print(f"Char '{ch}': bbox={bbox}, offset_x={offset_x}, offset_y={offset_y}, width={width}, height={height}")

        glyphs.append((code, width, height, advance, offset_x, offset_y, args.bpp, rows))

        # Track fallback glyph index
        if code == args.fallback_char:
            fallback_index = idx

    # Build ranges
    ranges = build_glyph_ranges(codes)

    # Generate C++ module
    try:
        with open(out_module, 'w', encoding='utf-8') as f:
            module_name = os.path.splitext(os.path.basename(out_module))[0]

            # Module header
            f.write('module;\n')
            f.write('#include <cstdint>\n')
            f.write('#include <span>\n')
            f.write(f'export module {module_name};\n\n')

            # Import charm.font module
            f.write('import charm.font;\n\n')

            # Bitmap data
            f.write('static constexpr uint8_t glyph_bitmaps[] = {\n')
            for code, w, h, adv, off_x, off_y, bpp, rows in glyphs:
                f.write(f'    // code {code} (\'{chr(code) if 32 <= code < 127 else "?"}\')\n')
                for b in rows:
                    if args.format == 'hex':
                        f.write(f'    0x{b:02X},\n')
                    elif args.format == 'bin':
                        f.write(f'    0b{b:08b},\n')
            f.write('};\n\n')

            # Glyph table
            f.write('static constexpr Glyph glyph_table[] = {\n')
            pos = 0
            for code, w, h, adv, off_x, off_y, bpp, rows in glyphs:
                ch_display = chr(code) if 32 <= code < 127 else '?'
                f.write(f'    // {ch_display} (code {code})\n')
                f.write(f'    {{ glyph_bitmaps + {pos}, {w}, {h}, {adv}, {off_x}, {off_y}, {bpp} }},\n')
                pos += len(rows)
            f.write('};\n\n')

            # Glyph ranges
            f.write('static constexpr GlyphRange glyph_ranges[] = {\n')
            for range_start, range_length, glyph_id_start in ranges:
                f.write(f'    {{ {range_start}, {range_length}, {glyph_id_start} }},\n')
            f.write('};\n\n')

            # Font structure
            f.write('export constexpr Font font = {\n')
            f.write(f'    .table = glyph_table,\n')
            f.write(f'    .ranges = glyph_ranges,\n')
            if fallback_index is not None:
                f.write(f'    .fallback_glyph = &glyph_table[{fallback_index}],\n')
            else:
                f.write(f'    .fallback_glyph = nullptr,\n')
            f.write(f'    .line_height = {line_height},\n')
            f.write(f'    .baseline = {baseline}\n')
            f.write('};\n')

        print(f"\nSuccessfully generated font module: {out_module}")
        print(f"  Glyphs: {len(glyphs)}")
        print(f"  Ranges: {len(ranges)}")
        print(f"  Line height: {line_height}")
        print(f"  Baseline: {baseline}")
        if fallback_index is not None:
            print(f"  Fallback glyph: '{chr(args.fallback_char)}' (index {fallback_index})")

    except Exception as e:
        print(f"Error writing output module: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    import sys
    if len(sys.argv) == 1:
        print("No arguments provided, using default debug values...")

        # 获取脚本所在目录
        script_dir = os.path.dirname(os.path.abspath(__file__))

        sys.argv.extend([
            '--font-file', os.path.join(script_dir, 'JetBrainsMono-Light.ttf'),
            '--size', '12',
            '--chars', '32-126',
            '--out-module', os.path.join(script_dir, 'font_generated.cppm'),
            '--format', 'bin',
            '--bpp', '4'
        ])
    main()
