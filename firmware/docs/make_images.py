#!/usr/bin/env python3
"""Turns the PBM framebuffers from render_screens.c into the PNGs used in
OPERATION.md, and draws the screen map and the crank timing diagram.

    python3 docs/make_images.py <pbm dir> <output dir>

Needs Pillow and matplotlib. Run via docs/make_images.sh.
"""
import glob
import os
import sys

from PIL import Image, ImageDraw, ImageFont

SCALE = 4
PIXEL = (210, 235, 255)     # lit OLED pixel
OFF = (18, 22, 30)          # unlit pixel
BEZEL = (40, 44, 52)
MARGIN = 14


def read_pbm(path):
    tokens = open(path).read().split()
    assert tokens[0] == "P1"
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    return w, h, [[bits[y * w + x] == "1" for x in range(w)] for y in range(h)]


def oled_image(path):
    """Scaled-up screen with a visible pixel grid, on a dark bezel."""
    w, h, px = read_pbm(path)
    img = Image.new("RGB", (w * SCALE + 2 * MARGIN, h * SCALE + 2 * MARGIN), BEZEL)
    d = ImageDraw.Draw(img)
    d.rectangle([MARGIN - 4, MARGIN - 4, MARGIN + w * SCALE + 3, MARGIN + h * SCALE + 3], fill=OFF)
    for y in range(h):
        for x in range(w):
            if px[y][x]:
                x0, y0 = MARGIN + x * SCALE, MARGIN + y * SCALE
                d.rectangle([x0, y0, x0 + SCALE - 2, y0 + SCALE - 2], fill=PIXEL)
    return img


def font(size):
    for name in ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def screen_map(screen, out):
    """The idle screen with every field labelled."""
    f = font(15)
    pad_l, pad_r, pad_t = 310, 290, 10
    W = screen.width + pad_l + pad_r
    H = screen.height + pad_t * 2 + 20
    img = Image.new("RGB", (W, H), (255, 255, 255))
    img.paste(screen, (pad_l, pad_t))
    d = ImageDraw.Draw(img)
    cell_w, row_h = 6 * SCALE, 8 * SCALE

    def row_y(row):
        return pad_t + MARGIN + row * row_h + row_h // 2

    left = [
        (0, "RPM setpoint (selected: inverted)"),
        (1, "idle switch  on = closed"),
        (2, "air temp NTC  DAC mV"),
        (3, "air-flow meter  DAC mV"),
        (4, "ECU current, supply voltage (U1)"),
        (5, "idle-valve current, voltage (U3)"),
        (6, "ignition: fall-rise, crank deg"),
        (7, "injector: open time @ fall deg"),
    ]
    right = [
        (0, "ECU power  on / off / TRIP"),
        (1, "knock Hz  (* = burst mode)"),
        (2, "coolant NTC  DAC mV"),
        (3, "lambda  DAC mV"),
    ]
    blue = (30, 90, 200)
    for row, text in left:
        y = row_y(row)
        x_screen = pad_l + MARGIN
        d.line([(pad_l - 12, y), (x_screen - 2, y)], fill=blue, width=2)
        tw = d.textlength(text, font=f)
        d.text((pad_l - 18 - tw, y - 9), text, fill=(20, 20, 20), font=f)
    for row, text in right:
        y = row_y(row)
        x_field_end = pad_l + MARGIN + 20 * cell_w
        x_edge = pad_l + screen.width
        d.line([(x_field_end, y), (x_edge + 12, y)], fill=blue, width=2)
        d.text((x_edge + 18, y - 9), text, fill=(20, 20, 20), font=f)
    d.text((pad_l, H - 24), "Top four rows: adjustable with MENU / - / +.  Bottom four: live readouts.",
           fill=(90, 90, 90), font=font(13))
    img.save(out)


def timing_diagram(out):
    """VW-18 crank signal, the reference edge, and how angles and bursts are
    measured (850 rpm, 2 pulses/rev, 50 % duty)."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    period = 180.0  # crank degrees per reference at 2 ppr
    fig, axes = plt.subplots(4, 1, figsize=(10, 5.6), sharex=True,
                             gridspec_kw={"hspace": 0.35})
    x_max = 400

    def square(ax, edges, start_high, label, color):
        xs, ys, level = [0], [1 if start_high else 0], start_high
        for e in edges:
            xs += [e, e]
            ys += [1 if level else 0, 0 if level else 1]
            level = not level
        xs.append(x_max)
        ys.append(1 if level else 0)
        ax.plot(xs, ys, color=color, lw=2)
        ax.set_ylim(-0.3, 1.5)
        ax.set_yticks([])
        ax.set_ylabel(label, rotation=0, ha="right", va="center", fontsize=10)
        for s in ("top", "right", "left"):
            ax.spines[s].set_visible(False)

    # VW-18: 50 % duty, falling edges (references) at 20 and 200 and 380 deg.
    refs = [20, 20 + period, 20 + 2 * period]
    edges = []
    for r in refs:
        edges += [r, r + period / 2]
    square(axes[0], [e for e in edges if e < x_max], True, "VW-18\ncrank", "black")
    for r in refs:
        for ax in axes:
            ax.axvline(r, color="tab:red", lw=0.8, ls="--")
    axes[0].annotate("reference = falling edge", xy=(refs[1], 0.5), xytext=(refs[1] + 30, 1.25),
                     arrowprops=dict(arrowstyle="->", color="tab:red"), color="tab:red", fontsize=9)
    axes[0].annotate("", xy=(refs[0], 1.35), xytext=(refs[1], 1.35),
                     arrowprops=dict(arrowstyle="<->"))
    axes[0].text((refs[0] + refs[1]) / 2, 1.42, "one reference period = 360/ppr deg",
                 ha="center", fontsize=9)

    # Ignition: low from 138 to 158 deg after the reference.
    f, r = refs[0] + 138, refs[0] + 158
    square(axes[1], [f, r, f + period, r + period], True, "VW-25\nignition", "tab:blue")
    axes[1].annotate("", xy=(refs[0], 1.25), xytext=(f, 1.25), arrowprops=dict(arrowstyle="<->"))
    axes[1].text((refs[0] + f) / 2, 1.3, "ign_fall_deg", ha="center", fontsize=9)
    axes[1].text(r + 3, 0.1, "ign_rise_deg\n(end of low)", fontsize=8, va="bottom")

    # Injector: once per two references.
    f2 = refs[0] + 62.5
    square(axes[2], [f2, f2 + 12], True, "VW-12\ninjector", "tab:green")
    axes[2].text(f2 + 15, 0.1, "inj_low_us = open time", fontsize=8, va="bottom")

    # Knock burst: 10 deg after the reference, 30 deg long.
    ax = axes[3]
    for rr in refs:
        b0 = rr + 10
        if b0 + 30 > x_max:
            continue
        import numpy as np

        t = np.linspace(b0, b0 + 30, 400)
        ax.plot(t, 0.6 + 0.45 * np.sin((t - b0) * 2.0), color="tab:purple", lw=1)
    ax.plot([0, x_max], [0.6, 0.6], color="tab:purple", lw=0.6, alpha=0.4)
    ax.annotate("", xy=(refs[0], 1.3), xytext=(refs[0] + 10, 1.3), arrowprops=dict(arrowstyle="<->"))
    ax.text(refs[0] + 12, 1.18, "burst <start_deg> <len_deg>", fontsize=9)
    ax.set_ylim(-0.3, 1.5)
    ax.set_yticks([])
    ax.set_ylabel("TP1\nknock", rotation=0, ha="right", va="center", fontsize=10)
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.set_xlim(0, x_max)
    ax.set_xlabel("crank degrees  (850 rpm, 2 pulses/rev: 1 deg = 196 us)")
    fig.savefig(out, dpi=110, bbox_inches="tight")
    plt.close(fig)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    screens = {}
    for p in sorted(glob.glob(os.path.join(src, "*.pbm"))):
        name = os.path.splitext(os.path.basename(p))[0]
        screens[name] = oled_image(p)
        screens[name].save(os.path.join(dst, name + ".png"))
        print("wrote", name + ".png")
    screen_map(screens["04_idle_850"], os.path.join(dst, "screen_map.png"))
    print("wrote screen_map.png")
    timing_diagram(os.path.join(dst, "timing.png"))
    print("wrote timing.png")


if __name__ == "__main__":
    main()
