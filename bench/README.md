# Benchmarks

Small experiments behind the claims about directives. They are not part of
`check-smr`: they are slow and the numbers depend on the machine.

Each experiment has its kernels, its patterns and a `run.sh`. The scripts need
`clang`, `smr` and `cir-translate` in `PATH`, as in the `smr-dev` container, and
build into a temporary directory (`WORK=dir` keeps it). Every build goes
through `scripts/smr-cc`: the CIR path, then `clang -O3` on the LLVM IR. "No
pattern" is `smr-cc` with an empty `SMR_PATS`, the same toolchain with only the
pattern missing. Plain `clang -O3` is a separate column.

```
bench/errno/run.sh
bench/phase/run.sh       # KERNELS="k8a k8b" to pick kernels
bench/vectorize/run.sh
bench/darknet/run.sh     # BIG=1 adds large square matrices
```

## Apple silicon

aarch64 VM (colima on Apple silicon), best run of 3 processes, no confidence
interval. Rerun on x86 before quoting these numbers. There is no x86 section
yet.

### errno: a pattern against a global flag

One program, two regions. Region A hoists `exp(x) + log(y)` out of a loop of
4e6 iterations. Region B calls `log` on negative inputs and counts how many set
`errno` (the right answer is 4). The pattern `a.pat` selects only A with
`licm(exp, log)`.

| build | time of A | errors counted by B |
|---|---|---|
| `clang -O3` | 11.2 ms | 4 |
| `clang -O3 -fno-math-errno` | 0.55 ms | 0, wrong |
| `smr-cc`, no pattern | 11.7 ms | 4 |
| `smr-cc`, pattern on A | 0.65 ms | 4 |

The flag is faster on A but `errno` is never set, so B is wrong. The pattern
applies the unsafe optimization where it is written and nowhere else.

### phase: licm, unroll and tiling, in both orders

Global pipelines on four kernels, time and size of the symbol. The choice of
pipeline matters more than its order.

- `k8a` (`val[i] += (exp(x) + log(y)) * i`): none 11.7 ms, `licm(exp, log)` 0.65
  ms. With unroll, `[licm, unroll(4)]` is 0.82 ms and 372 bytes, `[unroll(4),
  licm]` 0.93 ms and 508 bytes.
- `k8b` (`a[i] = b[i] + m[i] * c[0]`): `unroll(4)` makes it 54 percent slower.
- `k10a` (`m[i][j] += d[i] * p[j]`): `tiling(32)` makes it 10x slower, `unroll(4)`
  64 percent. A streaming loop has no reuse, so every tile only pays an alias
  check.

### vectorize: against clang -O3 and -ffast-math

Time per call in microseconds. `dot` is a float sum of products, `saxpy` is
`y[i] = alpha * x[i] + y[i]`, n = 16384.

| kernel | clang -O3 | clang -O3 -ffast-math | no pattern | vectorize | fast_math | unroll(4) | tiling(32) |
|---|---|---|---|---|---|---|---|
| dot | 8.2 | 1.8 | 8.9 | 0.68 | 8.9 | 9.2 | 8.9 |
| saxpy | 0.66 | 0.67 | 0.68 | 0.69 | 0.68 | 2.9 | 0.9 |

`vectorize` beats `-ffast-math` on `dot` but changes the bits of the result
(`0x1.5e8a4p+13` against `0x1.5e89e4p+13`), as `-ffast-math` does, so outputs
must be compared with a tolerance. `clang -O3` already vectorizes `saxpy`.
`fast_math` alone changes nothing on `dot`, and `unroll` and `tiling` never win
on these kernels.

### darknet: tiling of gemm_nn

The `gemm_nn` of darknet with `tiling(64)` as `$GLOBAL`, against no pattern and
the hand tiled version (BM = BN = BK = 64, order `i0 j0 k0 i k j`). Shapes are
the largest of yolov3, times in ms.

| K | M | N | base | ours | hand |
|---|---|---|---|---|---|
| 27 | 32 | 369664 | 28.7 | 28.6 | 31.5 |
| 288 | 64 | 92416 | 148.5 | 152.6 | 142.0 |
| 576 | 128 | 23104 | 151.2 | 155.2 | 141.2 |
| 1152 | 256 | 5776 | 113.9 | 108.3 | 138.3 |
| 2304 | 512 | 1444 | 117.4 | 108.6 | 144.8 |
| 4608 | 1024 | 361 | 156.7 | 130.5 | 180.7 |

On this machine tiling does not pay: with `BIG=1` the hand tiled version is 1.55
times slower than no tiling at 1024^3 to 4096^3 and ours equals the base. The
gap Pedro measures on an i7 (hand 1.44x, ours 1.2 to 1.3x on a whole darknet
run) needs x86 to reproduce.
