# references — the papers each design is based on

| design | paper |
|---|---|
| `software`, `hardware/replication_full` | Witten, Neal & Cleary 1987 — the classic arithmetic coder (`WittenACM87ArithmCoding.pdf`) |
| `hardware/interleaved` | C-slow retiming (`Cslow_Retiming_Virtex.pdf`) |
| `hardware/mcoder` | Marpe, Schwarz & Wiegand 2003 + Marpe & Wiegand 2003 — see below |
| `hardware/tans` | Duda 2013 — asymmetric numeral systems |

---

## hardware/mcoder

**Primary — the CABAC paper.** Describes the whole entropy coding scheme,
including the binary arithmetic coding engine, the 64-state probability model
and the context modelling this design implements.

> D. Marpe, H. Schwarz, and T. Wiegand, "Context-based adaptive binary
> arithmetic coding in the H.264/AVC video compression standard," *IEEE
> Transactions on Circuits and Systems for Video Technology*, vol. 13, no. 7,
> pp. 620–636, July 2003. doi:[10.1109/TCSVT.2003.815173](https://doi.org/10.1109/TCSVT.2003.815173)

**Most specific — the M-coder engine itself.** This is the paper for what we
actually built: interval subdivision *and* probability estimation both reduced
to table lookups, so the coder is multiplication-free. It also benchmarks
against the MQ coder, which is the alternative we considered and rejected.

> D. Marpe and T. Wiegand, "A highly efficient multiplication-free binary
> arithmetic coder and its application in video coding," *IEEE International
> Conference on Image Processing (ICIP 2003)*, Barcelona, Spain, September 2003,
> pp. 263–266. doi:[10.1109/ICIP.2003.1246667](https://doi.org/10.1109/ICIP.2003.1246667)
>
> Author's self-archived copy (free):
> <https://iphome.hhi.de/marpe/download/ieee03_multiplication_free.pdf>
> — drop it in this folder as `Marpe03_MCoder_multiplication_free.pdf`.

**Normative source of the tables.** `src/mcoder_tables.h` is transcribed from
here, not from either paper: `rangeTabLPS` is Table 9-44 and the state
transitions are Table 9-45. The encoder follows §9.3.4.2 (EncodeDecision),
§9.3.4.5/9.3.4.6 (EncodeTerminate/EncodeFlush); the decoder follows §9.3.3.2.

> ITU-T Recommendation H.264 / ISO-IEC 14496-10, *Advanced Video Coding for
> Generic Audiovisual Services*, §9.3 (CABAC parsing process).
> Free download: <https://www.itu.int/rec/T-REC-H.264>

**Background, cited by the CABAC paper** for the probability-estimation approach:

> P. G. Howard, "Practical implementations of arithmetic coding," in *Image and
> Text Compression*, J. A. Storer, Ed. Kluwer Academic Publishers, 1992,
> pp. 85–112.

### Note on author order

dblp lists the TCSVT paper as Marpe, **Wiegand, Schwarz**. That is wrong — the
authors' own page at Fraunhofer HHI, IEEE Xplore and the ACM DL all give
Marpe, **Schwarz, Wiegand**. Use the order above.
