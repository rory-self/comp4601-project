# M-coder: pseudocode

Algorithmic description of the design in `hardware/mcoder/src`. This is the
*method*, stripped of HLS pragmas and C plumbing; the reasoning behind each
choice is in [how_it_works.md](how_it_works.md).

Source map:

| section | file |
|---|---|
| A. Top level (K-way) | `mcoder_enc.cpp` / `mcoder_hls.cpp` |
| B. Bin coder | `mcoder.h` — `mc_code_bin` |
| C. Renormalisation | `mcoder.h` — `mc_shift_of`, `mc_renorm_low` |
| D. Packer | `mcoder.h` — `mc_pack_*`; `mcoder_hls.cpp` — `mc_pack_stage` |
| E. Terminate + flush | `mcoder.h` — `mc_enc_flush` |
| F. Decoder | `mcoder.h` — `mc_decode_bin`; `mcoder_dec.cpp` |

## Notation and state

```
K          number of independent chunks (shipped: K = 8)
low        10-bit register, interval base            in [0, 1023]
range      9-bit register, interval width            in [256, 510] between bins
offset     9-bit decoder register, always < range
ctx        7-bit context = (state << 1) | mps
             state : 6-bit probability state, 0 = p_LPS 0.5 .. 62 = p_LPS 0.01875
             mps   : the more-probable symbol value (0 or 1)
tree[1..255]   256 bit-tree contexts, one per (bit position, prefix) pair

ROMs (H.264 Tables 9-44 / 9-45, 384 bytes total, read-only):
  RangeTabLPS[state][q]   64 x 4 bytes    q = range quantile
  TransIdxLPS[state]      64 bytes
  TransIdxMPS[state]      64 bytes

Renorm step classes (what one shift position emits):
  E1  low <  1/2 of the low-range   -> emit 0, then drain the deferred run
  E2  low >= 1/2                    -> emit 1, then drain the deferred run
  E3  low straddles the midpoint    -> defer one more bit (carry unresolved)

MASK_LOW = 0x3FF (10 bits)      MASK_RANGE = 0x1FF (9 bits)
RANGE_INIT = 510                CTX_INIT = 0  (state 0, mps 0)
```

Everything is exact integer arithmetic in ≤10 bits. **There is no multiply and
no division anywhere in the encoder or the decoder.**

## A. Top level — K-way split

```
ENCODE(in[0..n-1]) -> out
    chunk <- ceil(n / K)

    # K independent engines: no shared state, so these run concurrently
    # (unrolled instances in hardware, a loop in software).
    for c in 0 .. K-1  in parallel:
        start   <- c * chunk
        rlen[c] <- clamp(n - start, 0, chunk)
        clen[c] <- ENCODE_CHUNK(in[start .. start+rlen[c]-1], cout[c])

    # Header: K x { uint16 raw_len, uint16 comp_len }, little-endian.
    # Carrying raw_len is what lets the decoder stop without a per-symbol
    # continuation bin -> 8 bins/byte instead of 9.
    emit for each c:  rlen[c] as 2 bytes, clen[c] as 2 bytes
    emit for each c:  cout[c][0 .. clen[c]-1]
    return total bytes written
```

```
ENCODE_CHUNK(in[0..n-1]) -> compressed bytes
    tree[i] <- CTX_INIT   for i in 0..255
    low <- 0 ;  range <- RANGE_INIT ;  packer <- fresh

    for k in 0 .. n-1:
        b <- in[k] ;  ctx <- 1                  # bit-tree root
        for j in 7 down to 0:                   # MSB first
            bit   <- (b >> j) & 1
            token <- CODE_BIN(tree[ctx], bit)   # section B
            PACK(token)                         # section D
            ctx   <- (ctx << 1) | bit           # context = prefix decoded so far

    FLUSH()                                     # section E
```

The context index `ctx` walks strictly down the bit-tree inside a byte, and bit
position `j` only ever addresses `[2^j, 2^(j+1)-1]`. Those ranges are disjoint,
so **the same context is re-used at most once every 8 bins** — the property the
hardware relies on to keep a 2-cycle read-modify-write pipelining at II=1.

## B. Bin coder — one binary decision

The whole engine is here. Note that both the MPS and LPS candidate intervals are
computed unconditionally, in parallel, and only *selected* at the end: the
branch is a mux, not a schedule.

```
CODE_BIN(ctx, bin) -> renorm token
    state <- ctx >> 1 ;  mps <- ctx & 1

    # --- interval split: one ROM read, no multiply ---
    q    <- (range >> 6) & 3                # range quantile, 4 buckets
    rLPS <- RangeTabLPS[state][q]           # ROM addressed by state alone,
                                            # q only picks a byte of the word
    rMPS <- (range - rLPS) & MASK_RANGE

    # --- both renorm candidates, computed in parallel ---
    sM <- (rMPS >= 256) ? 0 : 1             # provably 0 or 1: rMPS is always >= 128
    sL <- 8 - floor(log2(rLPS))             # 0..7, an 8-way priority mux

    # --- select ---
    if bin != mps:                          # LPS: take the upper sub-interval
        low   <- (low + range - rLPS) & MASK_LOW
        range <- (rLPS << sL) & MASK_RANGE
        s     <- sL
        if state == 0: mps <- mps XOR 1     # MPS flips only at the 50/50 state
        state <- TransIdxLPS[state]
    else:                                   # MPS: keep the lower sub-interval
        range <- (rMPS << sM) & MASK_RANGE
        s     <- sM
        state <- TransIdxMPS[state]

    ctx <- (state << 1) | mps               # adaptation: one ROM read
    return RENORM(low, s)                   # section C
```

`sM ∈ {0,1}` is a proof, not an optimisation guess: `q` partitions `range` into
`[256,319] [320,383] [384,447] [448,511]`, `rLPS` is largest at state 0, so the
smallest `range − rLPS` per quantile is `256−128, 320−176, 384−208, 448−240` —
all ≥ 128, and a 9-bit value ≥ 128 needs at most one doubling. This deletes the
priority encoder and barrel shifter from the MPS path entirely.

## C. Renormalisation — closed form, no loop

The textbook coder renormalises with a `while` loop that shifts one bit at a
time and emits as it goes. That loop has a data-dependent trip count, so it
cannot be pipelined. Here all `s` shift positions are classified **at once**.

The step recurrence `L' = (L << 1) & (L[9] ? 0x3FF : 0x1FF)` only ever touches
bit 9, so it unrolls into a barrel shift plus a prefix-AND:

```
    L_i[8:0] = (L_0 << i) & 0x1FF                        one barrel shift
    L_i[9]   = L_0[9] AND L_0[8] AND ... AND L_0[9-i]    a prefix-AND (depth-3 tree)
```

```
RENORM(low, s) -> token{ cls[0..s-1], s }
    L <- low & MASK_LOW
    p <- L[9]                               # bit 9 entering step 0

    for i in 0 .. 7  in parallel:           # fully unrolled, fixed latency
        b9 <- p                             # prefix-AND result at index i
        b8 <- L[8-i]                        # a constant bit select
        cls[i] <- b9 ? E2 : (b8 ? E3 : E1)
        p  <- b9 AND b8                     # advance the prefix-AND

    low <- ((L << s) & 0x1FF) | (p_at_index_s << 9)
    return { cls[0 .. s-1], s }             # only the first s steps are valid
```

The token — up to 8 two-bit classifications plus a length — is the **entire**
interface to the packer. Crucially, *the packer never needs `low`*: what a step
emits is fully determined by its class. That is what allows one stream write per
bin, and therefore II=1.

```
SHIFT_OF(range) = 8 - floor(log2(range))    # 0..7, range in [2,510]

RENORM_CLASSIFY(low, range)                 # general form, used by the flush
    s     <- SHIFT_OF(range)
    range <- (range << s) & MASK_RANGE
    return RENORM(low, s)
```

## D. Packer — resolving the deferred carry

The packer owns `outstanding` (the count of straddling E3 steps whose bit value
is not yet known) and the byte accumulator. `first` suppresses the leading bit,
which the decoder never reads.

**Software form** (`mc_pack_steps`), the reference semantics:

```
PACK(token)
    for i in 0 .. token.s - 1:
        c <- token.cls[i]
        if c == E3:
            outstanding <- outstanding + 1          # value still unknown
        else:
            b <- (c == E2) ? 1 : 0
            if first: first <- false                # drop the leading bit
            else:     APPEND_BIT(b)
            APPEND_BIT(NOT b)  x outstanding        # the carry resolves them all
            outstanding <- 0

APPEND_BIT(b)
    acc <- (acc << 1) | b ;  nacc <- nacc + 1
    if nacc == 8:  emit byte acc ;  nacc <- 0

PACK_TAIL()
    if nacc > 0:  emit byte (acc << (8 - nacc))     # pad the final partial byte
```

**Hardware form** (`mc_pack_stage`): the same semantics as one flat pipelined
loop doing *exactly one unit of work per cycle*. The nested
token → step → carry-drain loops above cost ~10 cycles of loop control per step
in hardware (measured: 25 cycles/byte with the coder idling at 1 cycle/bin);
flattening them took the packer from 2,762,242 to 33,284 cycles.

```
PACK_STAGE(token_fifo) -> bytes, length
    run <- 0 ; rem <- 0 ; outstanding <- 0 ; first <- true

    loop until end-of-stream token:             # II=1, one unit per cycle
        emit <- false

        if run > 0:                             # (1) drain one deferred bit
            emit <- true ; bit <- runbit ; run <- run - 1

        else if rem == 0:                       # (2) fetch the next token
            tk <- token_fifo.read()
            if tk.eos: break
            cls <- tk.cls ; rem <- tk.s

        else:                                   # (3) consume one step
            c   <- cls & 3                      # consume from the bottom:
            cls <- cls >> 2                     #   a constant shift is free wiring
            rem <- rem - 1
            if c == E3:
                outstanding <- outstanding + 1
            else:
                b <- (c == E2) ? 1 : 0
                if first: first <- false
                else:     emit <- true ; bit <- b
                runbit <- NOT b
                run    <- outstanding           # schedule the drain
                outstanding <- 0

        if emit: APPEND_BIT(bit)

    PACK_TAIL()
```

Steady state is ~0.58 cycles per bin (0.55 classification steps plus a rare
carry bit) against the coder's 1 bin per cycle, so the FIFO absorbs the bursts
and **the packer never gates the coder**. Emitting the carry run one bit per
cycle also removes any bound on run length: an arbitrarily long run is correct,
just slow.

## E. Terminate and flush

H.264 9.3.4.5 / 9.3.4.6. Pins the final `low` inside the interval so that every
preceding bin decodes unambiguously.

```
FLUSH()
    range <- range - 2                          # EncodeTerminate(1)
    low   <- low + range
    range <- 2
    PACK(RENORM_CLASSIFY(low, range))           # s = SHIFT_OF(2) = 7
    PACK({ cls = [ low[9] ? E2 : E1,
                   low[8] ? E2 : E1,
                   E2 ],                        # the literal stop bit
           s = 3 })
    PACK_TAIL()
```

## F. Decoder

Mirrors the encoder exactly — same ROMs, same context layout, same
initialisation — and is strictly simpler, because **the decoder has no carry to
resolve**: renorm is a single barrel shift on `range` and `offset`. It is
host/verification side only and never goes into the kernel.

```
DECODE_INIT(bits)
    range  <- RANGE_INIT
    offset <- READ_BITS(9)          # pairs with the encoder's suppressed first bit

DECODE_BIN(ctx) -> bin
    state <- ctx >> 1 ;  mps <- ctx & 1
    q     <- (range >> 6) & 3
    rLPS  <- RangeTabLPS[state][q]

    range <- range - rLPS
    if offset >= range:                         # LPS
        bin    <- NOT mps
        offset <- offset - range
        range  <- rLPS
        if state == 0: mps <- mps XOR 1
        state  <- TransIdxLPS[state]
    else:                                       # MPS
        bin   <- mps
        state <- TransIdxMPS[state]
    ctx <- (state << 1) | mps

    s      <- SHIFT_OF(range)                   # renorm: one shift, no emit
    range  <- range << s
    offset <- (offset << s) | READ_BITS(s)
    return bin
```

```
DECODE_CHUNK(bits, n) -> out[0..n-1]
    tree[i] <- CTX_INIT for i in 0..255
    DECODE_INIT(bits)
    for k in 0 .. n-1:                          # n from the header: no stop bin
        b <- 0 ; ctx <- 1
        for j in 7 down to 0:
            bit <- DECODE_BIN(tree[ctx])
            b   <- (b << 1) | bit
            ctx <- (ctx << 1) | bit
        out[k] <- b
```

```
DECODE(comp, comp_len) -> out
    if comp_len < 4*K: fail
    read rlen[0..K-1], clen[0..K-1] from the header

    # The header is untrusted: the XRT host reads it out of device memory, and
    # rlen drives how many bytes get written, so validate before use.
    if any rlen[c] < 0 or clen[c] < 0:      fail
    if sum(rlen) > MAX_IN:                  fail        # would overrun `out`

    off <- 4*K ; on <- 0
    for c in 0 .. K-1:
        if off + clen[c] > comp_len:        fail
        on  <- on + DECODE_CHUNK(comp[off .. off+clen[c]-1], rlen[c]) -> out[on..]
        off <- off + clen[c]
    return on
```

## Cost summary

Per input byte, per chunk:

| | count |
|---|---|
| bins coded | 8 |
| ROM reads | 24 (1 × RangeTabLPS + 1 × TransIdx per bin) |
| multiplies / divides | **0** |
| renorm iterations | **0** (closed form) |
| coder cycles (hardware, II=1) | 8 |
| packer cycles | ~4.6 (0.58/bin), overlapped |

Coder cycles divide by `K`; the AXI byte-copy of input and output does not,
which is why at `K=8` the coder is only ~15.6% of measured runtime.
