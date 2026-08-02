# 400 MHz attempt — HLS said 402 MHz, the router said no

**Result: FAILED to close timing. Documented as a negative result.**

The platform offers fixed clocks of 100 / 200 / 400 MHz. Only one design ever had
an HLS Fmax above 400: the arith interleaved coder at `GROUPS=2` with a registered
DSP multiply.

| stage | figure |
|---|---|
| HLS C-synthesis estimate | **402.58 MHz**, II=1 (`hls_g2.cfg`, 2.5 ns target) |
| Vivado place & route @ 400 MHz (clock id 2) | **FAILED** |

```
ERROR: [VPL 101-2] design did not meet timing - pulse width violation
```

## Why this matters (spec step 7b: calculated vs actual)
HLS's Fmax is an *estimate from its own scheduling model*: it accounts for logic
delay on the critical path but not for real placement, routing congestion, clock
skew, or pulse-width constraints on the actual clocking network. A 402 MHz estimate
against a 400 MHz target is a **0.6% margin** — far inside the error bar of that
estimate. Post-route reality consumed it.

## The general lesson
This is the same conclusion the whole project keeps reaching from different
directions: **design to the clock you will actually get, not to Fmax.**
- The platform's clocks are quantised (100/200/400), so an Fmax of 293 MHz buys
  nothing over 200 — the design still runs at 200.
- An Fmax barely above a clock step will not survive place & route.
- Real throughput came instead from cycle count and from using more of the chip
  (multiple compute units: **1.95×**), not from chasing the clock.

Reproduce: `v++ -c --mode hls --config hls_g2.cfg` then link with `link_400.cfg`
(clock id 2). Log: `link400.log`.
