# Display mockups: HP-41 style screen for the DM42's 400x240 1-bit LCD
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
F = "/usr/share/fonts/truetype/dejavu/"
W, H = 400, 240
S = 4  # supersampling for the grey artwork

def font(sz, bold=True):
    return ImageFont.truetype(F + ("DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"), sz)

def rrect(d, box, r, fill, s=S):
    d.rounded_rectangle([v * s for v in box], radius=r * s, fill=fill)

def grey_art(lcd_grey=232):
    """the case, bezel and keys in grey levels, supersampled"""
    im = Image.new("L", (W * S, H * S), 0)
    d = ImageDraw.Draw(im)
    # case: very dark with a soft vertical sheen
    for y in range(22 * S, H * S):
        t = (y - 22 * S) / ((H - 22) * S)
        d.line([(0, y), (W * S, y)], fill=int(26 + 22 * (1 - t) ** 2))
    # bezel around the LCD: grey metal, bevel light on top, dark below
    rrect(d, (4, 28, 396, 132), 7, 150)
    rrect(d, (5, 29, 395, 130), 6, 120)
    rrect(d, (5, 29, 395, 80), 6, 175)
    rrect(d, (5, 60, 395, 130), 6, 105)
    for i in range(10):  # smooth the bevel gradient
        rrect(d, (5, 29 + i * 5, 395, 130), 6, int(170 - i * 7))
    rrect(d, (10, 34, 390, 126), 4, 40)            # recess shadow
    rrect(d, (11, 35, 389, 125), 3, lcd_grey)     # LCD glass
    # shadow on the glass under the top bezel edge
    for i in range(6):
        d.rectangle([11 * S, (35 + i) * S, 389 * S, (36 + i) * S], fill=int(lcd_grey - (6 - i) * 14))
    # keys row: ON USER  [battery slot]  PRGM ALPHA
    keys = [(14, "ON"), (92, "USER"), (240, "PRGM"), (318, "ALPHA")]
    for x, _ in keys:
        rrect(d, (x - 1, 143, x + 71, 173), 4, 120)   # light edge
        rrect(d, (x, 145, x + 70, 173), 3, 12)        # black key cap
        rrect(d, (x + 2, 146, x + 68, 152), 2, 45)    # top highlight
    # battery slot grille
    for i in range(3):
        rrect(d, (181, 147 + i * 8, 219, 151 + i * 8), 1, 95)
    return im.resize((W, H), Image.LANCZOS), keys

def bayer(im):
    m = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
    a = np.asarray(im, dtype=float) / 255.0
    th = np.tile(m, (H // 4 + 1, W // 4 + 1))[:H, :W]
    return Image.fromarray(((a > th) * 255).astype(np.uint8))

def overlay(bw, text_pgm, annun, menu=None, clean=False):
    """crisp 1-bit content: top bar, LCD digits, annunciators, key legends, menu"""
    d = ImageDraw.Draw(bw)
    d.fontmode = "1"
    # LCD area made clean so the digits stay sharp
    if clean:
        d.rounded_rectangle([11, 35, 389, 125], radius=3, fill=255)
    strip = Image.open(text_pgm).convert("L")
    mask = strip.point(lambda v: 255 if v < 128 else 0)
    bw.paste(0, (22, 48), mask)
    f9 = font(9)
    x = 22
    for t, on in annun:
        if on:
            d.text((x, 108), t, font=f9, fill=0)
        x += d.textlength(t, font=f9) + (9 if t not in "01234" else 4)
    # top bar
    d.rectangle([0, 0, W, 20], fill=0)
    fb = font(13)
    d.text((5, 2), "05:34  11.11.2026", font=fb, fill=255)
    d.text((W - 68, 2), "3.0V", font=fb, fill=255)
    d.rectangle([W - 30, 5, W - 7, 15], fill=255); d.rectangle([W - 7, 8, W - 5, 12], fill=255)
    d.rectangle([W - 28, 7, W - 9, 13], fill=0); d.rectangle([W - 28, 7, W - 12, 13], fill=255)
    # key legends
    fk = font(11)
    for kx, label in [(14, "ON"), (92, "USER"), (240, "PRGM"), (318, "ALPHA")]:
        tw = d.textlength(label, font=fk)
        d.text((kx + 35 - tw / 2, 154), label, font=fk, fill=255)
    if menu:
        d.rectangle([0, 206, W, H], fill=255)
        fm = font(12)
        for i, m in enumerate(menu):
            x0 = 2 + i * 66
            d.rounded_rectangle([x0, 210, x0 + 63, 238], radius=4, fill=0)
            tw = d.textlength(m, font=fm)
            d.text((x0 + 32 - tw / 2, 217), m, font=fm, fill=255)
    return bw

AN = [("BAT", 0), ("USER", 1), ("G", 0), ("RAD", 0), ("SHIFT", 0), ("0", 0), ("1", 1), ("2", 0), ("3", 0), ("4", 0), ("PRGM", 0), ("ALPHA", 0)]

def solid(text, menu=None, annun=AN, lcd_texture=False):
    art, _ = grey_art(lcd_grey=222)
    a = np.asarray(art, dtype=float)
    yy, xx = np.mgrid[0:H, 0:W]
    bezel = (yy >= 28) & (yy <= 133) & (xx >= 3) & (xx <= 397)
    inner = (yy >= 34) & (yy <= 126) & (xx >= 10) & (xx <= 390)
    bw = np.zeros((H, W), dtype=np.uint8)            # solid black case
    dith = np.asarray(bayer(art))
    ring = bezel & ~inner
    bw[ring] = dith[ring]                            # metal bezel: ordered dither
    lcd = (yy >= 35) & (yy <= 125) & (xx >= 11) & (xx <= 389)
    bw[lcd] = dith[lcd] if lcd_texture else 255
    im = Image.fromarray(bw)
    d = ImageDraw.Draw(im)
    for x in (14, 92, 240, 318):                     # keys: black caps, light rim, highlight line
        d.rounded_rectangle([x, 144, x + 70, 173], radius=4, outline=255, width=1)
        d.line([x + 4, 147, x + 66, 147], fill=255)
    for i in range(3):
        d.rectangle([182, 148 + i * 8, 218, 150 + i * 8], fill=255)
    return overlay(im, text, annun, menu, clean=not lcd_texture)

def variant(kind, text, menu=None, annun=AN):
    if kind == "line":
        bw = Image.new("L", (W, H), 255)
        d = ImageDraw.Draw(bw)
        d.rounded_rectangle([4, 28, 396, 132], radius=7, outline=0, width=2)
        d.rounded_rectangle([10, 34, 390, 126], radius=4, outline=0, width=1)
        for x in (14, 92, 240, 318):
            d.rounded_rectangle([x, 145, x + 70, 173], radius=3, fill=0)
        for i in range(3):
            d.rectangle([181, 147 + i * 8, 219, 150 + i * 8], fill=0)
        return overlay(bw, text, annun, menu, clean=True)
    art, _ = grey_art(lcd_grey=228 if kind == "fs" else 222)
    if kind == "fs":
        bw = art.convert("1").convert("L")          # Floyd-Steinberg
        return overlay(bw, text, annun, menu, clean=True)
    bw = bayer(art)                                  # ordered dither, LCD keeps a faint grey texture
    return overlay(bw, text, annun, menu, clean=False)

samples = [
    ("A  Black case, metal bezel, clean LCD", solid("/tmp/s1.pgm")),
    ("B  Black case, metal bezel, grey LCD texture", solid("/tmp/s1.pgm", lcd_texture=True)),
    ("C  Clean line art, no shading", variant("line", "/tmp/s1.pgm")),
    ("A  with a softkey menu (DISP) and SHIFT", solid("/tmp/s2.pgm", menu=["FIX", "SCI", "ENG", "RDX.", "RDX,", "GRP"],
        annun=[(t, 1 if t in ("USER", "SHIFT") else 0) for t, _ in AN])),
]
for i, (_, im) in enumerate(samples):
    im.convert("1").save(f"/home/claude/hp41/tools/mock/sample_{'ABCD'[i]}.png")
# contact sheet at 2x so it can be judged on a normal screen
Z = 2
sheet = Image.new("RGB", (2 * (W * Z + 30) + 10, 2 * (H * Z + 50) + 10), (60, 60, 64))
sd = ImageDraw.Draw(sheet)
for i, (title, im) in enumerate(samples):
    x = 10 + (i % 2) * (W * Z + 30); y = 10 + (i // 2) * (H * Z + 50)
    lcdlook = im.convert("L").point(lambda v: 255 if v > 128 else 0).convert("RGB")
    a = np.asarray(lcdlook).copy()
    a[a[:, :, 0] > 128] = (214, 218, 206)      # DM42 LCD background colour
    a[a[:, :, 0] <= 128] = (28, 30, 32)
    sheet.paste(Image.fromarray(a).resize((W * Z, H * Z), Image.NEAREST), (x, y))
    sd.text((x, y + H * Z + 8), title, font=font(18), fill=(240, 210, 120))
sheet.save("/home/claude/hp41/tools/mock/display_samples.png")
print("ok")
