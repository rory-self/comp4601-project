#!/usr/bin/env python3
"""
Generate demo/text_page.pgm -- the worst case for the M-coder relative to V5.

Bilevel "scanned text": pure 0/255, long white runs between glyph rows.  This is
the regime where V5's probability model wins, and it wins for a specific,
checkable reason:

  V5 updates with  prob += (4096-prob)>>5,  a 1/32 exponential that only stops
  once 4096-prob < 32, i.e. p_LPS floors at 31/4096 = 0.0076.  Reaching that
  floor from 0.5 takes roughly 32*ln(2048/31) ~= 134 consecutive identical bits
  in one context.  The M-coder's 64-state FSM plateaus earlier, at
  p_LPS = 0.01875 -- 2.5x less skew.

  So V5 wins only where a *single context* sees runs longer than ~134 bits.
  Long white margins in bilevel text do exactly that.

Which means the advantage is destroyed by chunking: at K=8 a 4096-byte block
splits into 512-byte chunks, every chunk restarts its model, and the deep
contexts see only a handful of samples each.  What matters then is how fast the
model converges from its initial state, and CABAC's tuned FSM wins that.

Measured on this image (mcoder_test, whole file):

    K=1   V5 5488   MC 6111   +11.35%   <- V5 wins
    K=2   V5 6306   MC 6613    +4.87%   <- V5 wins
    K=4   V5 7654   MC 7474    -2.35%      crossover
    K=8   V5 10234  MC 9126   -10.83%   <- M-coder wins (the shipped config)

Usage:  python3 gen_text_page.py   ->  text_page.pgm
"""
import random

W = H = 256
random.seed(11)                     # fixed seed: the image is reproducible

px = [255] * (W * H)                # white page
y = 14
while y < H - 18:
    x = 18
    while x < W - 18:               # a row of "glyphs"
        wgl, h = random.randrange(3, 7), 7
        if random.random() < 0.82:  # occasional word gap
            for dy in range(h):
                for dx in range(wgl):
                    if random.random() < 0.85:
                        px[(y + dy) * W + x + dx] = 0
        x += wgl + random.randrange(2, 4)
    y += 13                         # long white run between lines

with open("text_page.pgm", "wb") as f:
    f.write(b"P5\n%d %d\n255\n" % (W, H))
    f.write(bytes(px))
print("wrote text_page.pgm  (%dx%d, %d bytes of pixels)" % (W, H, W * H))
