# Compose demo frames into docs/demo.gif (DM42-style screen + caption)
import glob, os
from PIL import Image, ImageDraw, ImageFont

F = "/usr/share/fonts/truetype/dejavu/"
fsm = ImageFont.truetype(F + "DejaVuSansCondensed-Bold.ttf", 13)
fmenu = ImageFont.truetype(F + "DejaVuSansCondensed-Bold.ttf", 14)
fhdr = ImageFont.truetype(F + "DejaVuSansCondensed-Bold.ttf", 15)
fcap = ImageFont.truetype(F + "DejaVuSans-Bold.ttf", 18)
INK, BG, FRAME = (20, 24, 22), (205, 212, 196), (40, 40, 44)
AN = [(1, "BAT", 6), (2, "USER", 48), (4, "G", 112), (8, "RAD", 124), (16, "SHIFT", 172), (32, "0", 236),
      (64, "1", 248), (128, "2", 260), (256, "3", 272), (512, "4", 284), (1024, "PRGM", 302), (2048, "ALPHA", 352)]
S = 1.5
W, H = 400, 240
frames, durs = [], []
for txt in sorted(glob.glob("frames/*.txt")):
    lines = open(txt, encoding="utf-8").read().split("\n")
    hold, cap, annun, info = int(lines[0]), lines[1], int(lines[2]), lines[3]
    menu = lines[4:10]
    scr = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(scr)
    d.rectangle([0, 0, W, 21], fill=INK)
    d.text((6, 2), "21:44  10.11.2026", font=fhdr, fill=BG)
    if info:
        tw = d.textlength(info, font=fhdr)
        d.text((236 - tw / 2, 2), info, font=fhdr, fill=BG)
    d.text((W - 70, 2), "3.0V", font=fhdr, fill=BG)
    d.rectangle([W - 30, 5, W - 6, 16], fill=BG)
    d.rectangle([W - 6, 8, W - 4, 13], fill=BG)
    d.rectangle([W - 28, 7, W - 8, 14], fill=INK)
    d.rectangle([W - 28, 7, W - 11, 14], fill=BG)
    lcd = Image.open(txt[:-4] + ".pgm").convert("L")
    mask = lcd.point(lambda v: 255 if v < 128 else 0)
    scr.paste(Image.new("RGB", lcd.size, INK), (0, 44), mask)
    for bit, t, x in AN:
        if annun & bit:
            d.text((x, 112), t, font=fsm, fill=INK)
    if any(menu):
        y = H - 30
        for i in range(6):
            x = 2 + i * 66
            d.rounded_rectangle([x, y, x + 62, y + 26], radius=4, fill=INK)
            tw = d.textlength(menu[i], font=fmenu)
            d.text((x + 31 - tw / 2, y + 5), menu[i], font=fmenu, fill=BG)
    scr = scr.resize((int(W * S), int(H * S)), Image.NEAREST)
    pad = 16
    out = Image.new("RGB", (scr.width + 2 * pad, scr.height + 2 * pad + 40), FRAME)
    out.paste(scr, (pad, pad))
    od = ImageDraw.Draw(out)
    if cap:
        tw = od.textlength(cap, font=fcap)
        od.text(((out.width - tw) / 2, scr.height + pad + 10), cap, font=fcap, fill=(235, 200, 90))
    frames.append(out.convert("P", palette=Image.ADAPTIVE, colors=32))
    durs.append(max(hold, 1) * 100)
os.makedirs("../docs", exist_ok=True)
frames[0].save("../docs/demo.gif", save_all=True, append_images=frames[1:], duration=durs, loop=0, optimize=True)
print(len(frames), os.path.getsize("../docs/demo.gif"))
