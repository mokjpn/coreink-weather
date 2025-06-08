#!/usr/bin/env python3
"""
png2bmp1b_header.py
モノクロ PNG → 1bpp BMP → const uint8_t 配列(.h) 生成
usage: python png2bmp1b_header.py input.png output.h VAR_NAME
"""
import sys, io, pathlib, textwrap, re
from PIL import Image

def png_to_1b_bmp_bytes(png_path: pathlib.Path) -> bytes:
    """PNG を 1bpp BMP (BI_RGB) に変換しバイト列を返す"""
    img = Image.open(png_path).convert('1')     # 1-bit モードへ :contentReference[oaicite:7]{index=7}
    buf = io.BytesIO()
    img.save(buf, format='BMP')                 # Pillow が 1-bit BMP を生成 :contentReference[oaicite:8]{index=8}
    return buf.getvalue()

def write_header(bmp_bytes: bytes, hdr_path: pathlib.Path, varname: str):
    guard = '_' + re.sub(r'\W+', '_', varname).upper() + '_H_'
    with hdr_path.open('w', encoding='utf-8') as f:
        f.write(textwrap.dedent(f"""\
            #ifndef {guard}
            #define {guard}
            #include <stdint.h>
            const uint8_t {varname}[] EXT_RAM_ATTR = {{
        """))
        for i, b in enumerate(bmp_bytes):
            f.write(f'0x{b:02X},')
            if (i + 1) % 16 == 0:
                f.write('\n')
        f.write(textwrap.dedent(f"""
            }};
            const uint32_t {varname}_len = {len(bmp_bytes)};
            #endif /* {guard} */
        """))

def main():
    if len(sys.argv) != 4:
        print("usage: png2bmp1b_header.py input.png output.h VAR_NAME")
        sys.exit(1)

    png_path = pathlib.Path(sys.argv[1])   # ① Path に変換
    hdr_path = pathlib.Path(sys.argv[2])   # ② Path に変換
    varname  = sys.argv[3]                 # ③ 文字列のまま

    bmp_bytes = png_to_1b_bmp_bytes(png_path)
    write_header(bmp_bytes, hdr_path, varname)
    print(f"wrote {hdr_path} ({len(bmp_bytes)} bytes, 1-bpp BMP)")

if __name__ == "__main__":
    main()
    
