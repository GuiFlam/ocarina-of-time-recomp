#!/usr/bin/env python3
# Written for ocarina-of-time-recomp (MIT, Copyright (c) 2026 Jeff Aigner).
"""Compare screenshots: is a change's picture still a picture, and is it the same as before?

    python tools/shot_diff.py A.png [B.png] [--out side.png] [--crop X,Y,W,H]

For each image: size, mean brightness, distinct colours and a noise score: change between neighbouring pixels over
change between neighbouring 8x8 blocks. A rendered, upscaled frame is smooth at pixel scale (0.4-0.6); the broken
neural upscaler of 2026-11-06 drew a lattice of noise that scored 2.1-3.1. Above --noise-limit (1.2) it exits 1; a
flat image (loading, a fade) scores 0 and counts as blank, also exit 1.
With two images: mean absolute difference and PSNR, and --out writes them side by side (scaled to 1280 wide) for a
look. Screenshots from two runs differ when the scene moved between them (the cinematic runs on a clock): judge a
difference with the side-by-side image; the noise score is the automatic part.
Needs Pillow and numpy (tools/requirements.txt; for now /mnt/c/dev/metroid-prime-4-recomp/.venv/bin/python3).
"""
import argparse
import sys

import numpy as np
from PIL import Image


def noise_score(a: np.ndarray) -> float:
    """Change between neighbouring pixels over change between neighbouring 8x8 blocks. An upscaled frame is smooth at
    pixel scale (0.4-0.6 over tonight's good screenshots, a logo on black included); the broken upscaler's lattice
    of noise scores 2.1-3.1 (build/logs/shot-tc1.png, bisect-ALL.png, 2026-11-06)."""
    g = a.astype(np.float32).mean(axis=2)
    hf = (np.abs(np.diff(g, axis=1)).mean() + np.abs(np.diff(g, axis=0)).mean()) / 2
    h, w = g.shape
    b = g[: h // 8 * 8, : w // 8 * 8].reshape(h // 8, 8, w // 8, 8).mean(axis=(1, 3))
    lf = (np.abs(np.diff(b, axis=1)).mean() + np.abs(np.diff(b, axis=0)).mean()) / 2
    return float(hf / (lf + 1e-3)) if lf > 0.5 else 0.0   # a flat image (loading, a fade) has no detail to judge


def stats(path: str, crop):
    im = Image.open(path).convert("RGB")
    if crop:
        x, y, w, h = crop
        im = im.crop((x, y, x + w, y + h))
    a = np.asarray(im)
    small = a[:: max(1, a.shape[0] // 360), :: max(1, a.shape[1] // 640)]
    colours = len(np.unique(small.reshape(-1, 3), axis=0))
    return im, a, {"size": im.size, "mean": round(float(a.mean()), 1), "colours": colours,
                   "noise": round(noise_score(a), 2)}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("a")
    ap.add_argument("b", nargs="?")
    ap.add_argument("--out")
    ap.add_argument("--crop", type=lambda s: tuple(int(v) for v in s.split(",")))
    ap.add_argument("--noise-limit", type=float, default=1.2, help="exit 1 when an image scores above this")
    a = ap.parse_args()
    ims = [stats(p, a.crop) for p in [a.a] + ([a.b] if a.b else [])]
    bad = False
    for p, (_, _, s) in zip([a.a, a.b], ims):
        flag = s["noise"] > a.noise_limit
        blank = s["noise"] == 0.0
        bad |= flag or blank
        print(f"{p}: {s['size'][0]}x{s['size'][1]}, mean {s['mean']}, {s['colours']} colours, noise {s['noise']}"
              + ("  <- noise at pixel scale: not a picture (broken shader?)" if flag else "")
              + ("  <- blank: nothing to judge (still loading, a fade?)" if blank else ""))
    if a.b:
        x, y = ims[0][1].astype(np.float32), ims[1][1].astype(np.float32)
        if x.shape == y.shape:
            mad = float(np.abs(x - y).mean())
            mse = float(((x - y) ** 2).mean())
            psnr = 99.0 if mse == 0 else 10 * np.log10(255 ** 2 / mse)
            print(f"difference: mean {mad:.2f} per channel, PSNR {psnr:.1f} dB")
        else:
            print("difference: sizes differ")
    if a.out:
        sides = [im for im, _, _ in ims]
        w = sum(i.size[0] for i in sides) + 16 * (len(sides) - 1)
        h = max(i.size[1] for i in sides)
        canvas = Image.new("RGB", (w, h))
        x = 0
        for i in sides:
            canvas.paste(i, (x, 0))
            x += i.size[0] + 16
        canvas.resize((1280, max(1, int(1280 * h / w)))).save(a.out)
        print(f"side by side: {a.out}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
