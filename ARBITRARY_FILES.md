# Can our coders process arbitrary files?

Tested, not assumed. Every coder was run over real-world files of different kinds
(a PDF, an aarch64 ELF binary, a markdown document, an image, and binary data
matching the tree method's own distribution), encoding and then decoding each and
comparing byte-for-byte.

## Adaptive coders (arith K-way, M-coder) — **yes, fully general**

| file | bytes | coded | ratio | lossless |
|---|---:|---:|---:|---|
| Intro_Arith_coding.pdf | 1,366,213 | 1,344,113 | 98.4% | **YES** |
| mc_host_arm (ELF binary) | 77,808 | 22,360 | **28.7%** | **YES** |
| EXPLORATION.md (text) | 13,263 | 9,435 | 71.1% | **YES** |
| file0.bin | 32,768 | 28,616 | 87.3% | **YES** |
| image.pgm | 65,551 | 40,408 | 61.6% | **YES** |

Lossless on everything, and the ratios behave sensibly: an already-compressed PDF
barely shrinks (98.4%), a binary with lots of structure compresses well (28.7%).
The model adapts per block, so no prior knowledge of the file is needed.

## Tree method / static tANS — **always correct, but only compresses its own class**

| file | bytes | coded | ratio | lossless |
|---|---:|---:|---:|---|
| **file0.bin (matches the baked table)** | 32,768 | 27,527 | **84.0%** | **YES** |
| Intro_Arith_coding.pdf | 1,366,213 | 1,692,645 | 123.9% | **YES** |
| mc_host_arm (ELF binary) | 77,808 | 111,528 | 143.3% | **YES** |
| EXPLORATION.md (text) | 13,263 | 14,057 | 106.0% | **YES** |
| image.pgm | 65,551 | 76,657 | 116.9% | **YES** |

**It never corrupts or crashes on arbitrary input** — the baked table is built with
Laplace smoothing so all 256 symbols have a non-zero slot, which makes every byte
encodable. But it *expands* data that does not match the distribution it was built
for (up to 143%). That is the tree method working as designed: it trades generality
for speed, and it is only the right tool when the files share a frequency table.

> An earlier table (built from a single image's histogram without smoothing) had
> zero-probability symbols and **segfaulted** on out-of-alphabet input. Smoothing
> every symbol to ≥1 slot is what makes the coder total. Worth knowing: a static
> table coder must guarantee full alphabet coverage or it is not safe on real input.

## Practical limits (all designs)
- **Block size.** Kernels take a fixed maximum per call (4,096 B for the arith and
  M-coder kernels, 16,384 B for the tANS kernel). Larger files are split by the
  host — the demo and the tests above do exactly that, and it is how the 1.36 MB
  PDF above was processed.
- **Raw byte streams only.** These are entropy coders, not archivers: no container,
  no file headers, no metadata. Input is a byte sequence; output is the compressed
  byte sequence plus our small per-chunk length header.
- **Edge cases tested:** empty input (n=0), tiny input (n=10), and inputs that are
  not multiples of the lane count all round-trip correctly.

## Summary
- Need to compress **anything**: use the adaptive coders — general and lossless.
- Have **many files sharing one distribution** and want speed: use the tree method
  — 2.63× faster on the board, but only for that class.
