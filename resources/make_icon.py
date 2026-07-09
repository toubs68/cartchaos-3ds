#!/usr/bin/env python3
# Generates a 48x48 cartoon shopping-cart icon as a PNG (no PIL needed).
import zlib, struct, math, sys, os

W = H = 48

def add(a, b, t):
    return tuple(int(a[i] + (b[i]-a[i])*t) for i in range(3))

# Palette
BG      = (120, 200, 255)   # sky blue (matches in-game sky)
CART    = (200, 200, 210)   # metal grey
CART_LO = (150, 150, 160)
RED     = (220, 60, 60)
WHITE   = (255, 255, 255)
BLACK   = (20, 20, 25)
DARK    = (60, 60, 70)

# Start transparent
img = [[(0,0,0,0) for _ in range(W)] for _ in range(H)]

def px(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        a = 255 if len(c) == 3 else c[3]
        img[y][x] = (c[0], c[1], c[2], a)

def rect(x0, y0, x1, y1, c):
    for y in range(int(y0), int(y1)+1):
        for x in range(int(x0), int(x1)+1):
            px(x, y, c)

def disc(cx, cy, r, c):
    for y in range(int(cy-r), int(cy+r)+1):
        for x in range(int(cx-r), int(cx+r)+1):
            if (x-cx)**2 + (y-cy)**2 <= r*r:
                px(x, y, c)

def line(x0, y0, x1, y1, c, w=1):
    n = int(max(abs(x1-x0), abs(y1-y0)))+1
    for i in range(n+1):
        t = i/n
        rect(x0+(x1-x0)*t - w/2, y0+(y1-y0)*t - w/2,
             x0+(x1-x0)*t + w/2, y0+(y1-y0)*t + w/2, c)

# Background rounded (just fill)
for y in range(H):
    for x in range(W):
        img[y][x] = (BG[0], BG[1], BG[2], 255)

# Basket body (trapezoid) — drawn as horizontal bars
top_y, bot_y = 12, 36
top_x0, top_x1 = 12, 36
bot_x0, bot_x1 = 16, 32
for y in range(top_y, bot_y+1):
    t = (y-top_y)/(bot_y-top_y)
    x0 = top_x0 + (bot_x0-top_x0)*t
    x1 = top_x1 + (bot_x1-top_x1)*t
    rect(x0, y, x1, y, CART)
    # shading on right edge
    rect(x1-2, y, x1, y, CART_LO)

# Basket grid lines
for i in range(1, 5):
    yy = top_y + (bot_y-top_y)*i/5
    t = (yy-top_y)/(bot_y-top_y)
    x0 = top_x0 + (bot_x0-top_x0)*t
    x1 = top_x1 + (bot_x1-top_x1)*t
    line(x0, yy, x1, yy, DARK, 1)
for i in range(1, 4):
    xx = top_x0 + (top_x1-top_x0)*i/4
    line(xx, top_y, xx-4, bot_y, DARK, 1)
    xx2 = bot_x0 + (bot_x1-bot_x0)*i/4

# Red handle / top bar
line(11, top_y, 37, top_y, RED, 2)
# Handle stick
line(37, top_y, 41, top_y-4, RED, 2)

# Wheels
disc(18, 39, 4, BLACK)
disc(30, 39, 4, BLACK)
disc(18, 39, 1.5, WHITE)
disc(30, 39, 1.5, WHITE)

# A little speed swoosh (white) behind cart
for i in range(3):
    line(4+i*2, 22+i*3, 11, 22+i*3, WHITE, 1)

# ---- PNG encode ----
def png(out_path):
    raw = bytearray()
    for y in range(H):
        raw.append(0)  # filter type 0
        for x in range(W):
            r,g,b,a = img[y][x]
            raw += bytes((r,g,b,a))
    comp = zlib.compress(bytes(raw), 9)
    def chunk(typ, data):
        c = struct.pack(">I", len(data)) + typ + data
        c += struct.pack(">I", zlib.crc32(typ+data) & 0xffffffff)
        return c
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)  # 8-bit RGBA
    with open(out_path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", comp))
        f.write(chunk(b"IEND", b""))

png(os.path.join(os.path.dirname(__file__), "icon.png"))
print("icon.png written", os.path.getsize(os.path.join(os.path.dirname(__file__), "icon.png")), "bytes")
