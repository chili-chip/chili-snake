#!/usr/bin/env python3
"""Generates the launcher splash (128x96) and icon (8x8) for snek.

Run from the repo root:  python3 tools/make_art.py
Needs Pillow. The output PNGs are committed, so this only needs re-running
when the art changes.
"""
from PIL import Image

BG_LIGHT = (26, 46, 32)
BG_DARK = (20, 37, 26)
RED = (225, 40, 52)
RED_SHADE = (150, 20, 34)
RED_SHINE = (255, 190, 190)
STEM = (110, 70, 30)
LEAF = (100, 210, 70)
WHITE = (250, 250, 240)
BLACK = (15, 20, 15)

LETTERS = [
    [".####", "#....", ".###.", "....#", "####."],
    ["#...#", "##..#", "#.#.#", "#..##", "#...#"],
    ["#####", "#....", "####.", "#....", "#####"],
    ["#...#", "#..#.", "###..", "#..#.", "#...#"],
]


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def scale(c, f):
    return tuple(min(255, int(v * f)) for v in c)


def rect(img, x, y, w, h, col):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if 0 <= xx < img.width and 0 <= yy < img.height:
                img.putpixel((xx, yy), col)


def checker(img, cell):
    for y in range(img.height):
        for x in range(img.width):
            light = ((x // cell) + (y // cell)) % 2 == 0
            img.putpixel((x, y), BG_LIGHT if light else BG_DARK)


def apple(img, cx, cy):
    for y in range(-3, 4):
        for x in range(-3, 4):
            if x * x + y * y <= 10:
                img.putpixel((cx + x, cy + y), RED)
    for dx, dy in [(2, 2), (1, 3), (3, 1)]:
        img.putpixel((cx + dx, cy + dy), RED_SHADE)
    for dx, dy in [(-2, -1), (-1, -2)]:
        img.putpixel((cx + dx, cy + dy), RED_SHINE)
    img.putpixel((cx, cy - 4), STEM)
    for dx, dy in [(1, -4), (2, -5), (1, -5)]:
        img.putpixel((cx + dx, cy + dy), LEAF)


def snake_cells(img, cells, cell=8):
    """Draws a snake through grid cells, head first."""
    head = (160, 240, 96)
    tail = (40, 140, 70)
    n = len(cells)
    for i in reversed(range(n)):
        cx, cy = cells[i]
        px, py = cx * cell, cy * cell
        col = mix(head, tail, i / max(n - 1, 1))
        if i and i % 2:
            col = scale(col, 0.85)
        inset = 0 if i == 0 else (2 if i == n - 1 else 1)
        s = cell - inset * 2
        rect(img, px + inset + 1, py + inset, s - 2, s, col)
        rect(img, px + inset, py + inset + 1, s, s - 2, col)
        for j in (i - 1, i + 1):
            if 0 <= j < n:
                dx, dy = cells[j][0] - cx, cells[j][1] - cy
                bi = max(inset, 1)
                w = cell - bi * 2
                if dx == 1:
                    rect(img, px + 4, py + bi, 4, w, col)
                elif dx == -1:
                    rect(img, px, py + bi, 4, w, col)
                elif dy == 1:
                    rect(img, px + bi, py + 4, w, 4, col)
                elif dy == -1:
                    rect(img, px + bi, py, w, 4, col)
        if i == 0:
            # Head facing right.
            rect(img, px + 4, py + 1, 2, 2, WHITE)
            rect(img, px + 4, py + 5, 2, 2, WHITE)
            img.putpixel((px + 5, py + 1), BLACK)
            img.putpixel((px + 5, py + 5), BLACK)
            img.putpixel((px + 8, py + 3), (230, 50, 70))
            img.putpixel((px + 9, py + 3), (230, 50, 70))
            img.putpixel((px + 10, py + 2), (230, 50, 70))
            img.putpixel((px + 10, py + 4), (230, 50, 70))


def logo(img, y, block=5):
    letter_w = 5 * block
    gap = block
    total = 4 * letter_w + 3 * gap
    x0 = (img.width - total) // 2
    for l, letter in enumerate(LETTERS):
        lx = x0 + l * (letter_w + gap)
        wave = [0, -2, 0, 2][l]
        for row, line in enumerate(letter):
            for col, ch in enumerate(line):
                if ch != "#":
                    continue
                bx, by = lx + col * block, y + row * block + wave
                rect(img, bx + 1, by + 2, block, block, (5, 10, 7))
    for l, letter in enumerate(LETTERS):
        lx = x0 + l * (letter_w + gap)
        wave = [0, -2, 0, 2][l]
        for row, line in enumerate(letter):
            for col, ch in enumerate(line):
                if ch != "#":
                    continue
                bx, by = lx + col * block, y + row * block + wave
                c = mix((180, 245, 110), (50, 165, 80), (l * 5 + col) / 19)
                rect(img, bx, by, block, block, c)
                rect(img, bx, by + block - 1, block, 1, scale(c, 0.7))
                rect(img, bx + block - 1, by, 1, block, scale(c, 0.7))
                img.putpixel((bx, by), scale(c, 1.2))
    # Eyes on the S.
    rect(img, x0 + 11, y + 1, 2, 2, WHITE)
    rect(img, x0 + 16, y + 1, 2, 2, WHITE)
    img.putpixel((x0 + 12, y + 1), BLACK)
    img.putpixel((x0 + 17, y + 1), BLACK)


def splash():
    img = Image.new("RGB", (128, 96))
    checker(img, 8)
    logo(img, 10)
    # A snake slithering towards an apple along the bottom.
    path = [(9, 9), (8, 9), (7, 9), (7, 8), (6, 8), (5, 8), (4, 8), (4, 9), (3, 9), (2, 9), (1, 9), (1, 10), (0, 10)]
    snake_cells(img, path)
    apple(img, 12 * 8 + 4, 9 * 8 + 4)
    img.save("assets/splash.png")


def icon():
    img = Image.new("RGB", (8, 8), BG_DARK)
    # Snake head filling the icon, eyes looking right, tongue out.
    green = (140, 230, 90)
    rect(img, 1, 0, 6, 8, green)
    rect(img, 0, 1, 8, 6, green)
    rect(img, 4, 1, 2, 2, WHITE)
    rect(img, 4, 5, 2, 2, WHITE)
    img.putpixel((5, 1), BLACK)
    img.putpixel((5, 5), BLACK)
    img.putpixel((7, 3), (230, 50, 70))
    img.putpixel((7, 4), (230, 50, 70))
    img.save("assets/icon.png")


if __name__ == "__main__":
    splash()
    icon()
