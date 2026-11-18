"""Convert simulator screenshots (.ppm) to .png.  Usage: python3 ppm2png.py <dir>"""
import sys, pathlib
from PIL import Image

for p in sorted(pathlib.Path(sys.argv[1]).glob("*.ppm")):
    Image.open(p).save(p.with_suffix(".png"))
    print("wrote", p.with_suffix(".png"))
