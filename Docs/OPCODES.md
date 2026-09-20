# Opcode Map

This accelerator has two layers of instructions, stacked on top of each
other:

1. **Host command words** (32-bit, sent over the USB3.0 FIFO to
   `control_unit.v`) — load weights/activations, run the matmul, configure
   the approximation mode, and drive `pipeline_sequencer.v`.
2. **Pipeline instruction words** (17-bit, one of the host commands writes
   these into `pipeline_sequencer.v`'s own instruction memory) — a small
   register-addressed program that decides which shared units (activation,
   normalization, RoPE, SSM, quantize) run, in what order, on the matmul's
   output.

Defaults referenced below: `ROWS=COLS=32`, `DATA_WIDTH=8`, `ACC_WIDTH=32`,
`BUS_WIDTH=32`, `PIPE_WIDTH=16`, `PROG_DEPTH=64`, `N_POS=32` — all plain
Verilog parameters on `accelerator_top`/`control_unit`/`pipeline_sequencer`,
so field widths below (e.g. `$clog2(ROWS)`) shrink/grow if you change them.

---

## 1. Host command words (`rx_data`, 32 bits, MSB-first)

Sent to `control_unit.v` over the USB3.0 link. `[31:28]` always selects the
opcode; the remaining bits are opcode-specific.

| Opcode | Name | Purpose |
|:--:|---|---|
| `0x1` | `LOAD_WEIGHT` | Write one stationary weight directly into `mac_array` |
| `0x2` | `LOAD_ACT` | Fill one lane of the activation vector |
| `0x3` | `RUN` | Pulse the matmul for one cycle |
| `0x4` | `READ_RESULTS` | Drain `pipeline_sequencer`'s `R6`, `COLS` words |
| `0x5` | `SET_MODE` | Choose `add_sel` (the accumulate-side approximate mode); multiplication is a fixed hybrid, not runtime-selectable |
| `0x6` | `LOAD_INSTR` | Write one 17-bit pipeline instruction word |
| `0x7` | `RUN_PROGRAM` | Execute `prog_len` pipeline instructions |
| `0x8` | `SET_RESCALE` | Set the `ACC_WIDTH -> Q(FRAC_BITS)` bridge shift |
| `0x9` | `LOAD_SSM_COEF` | Write one per-channel SSM coefficient |
| `0xA` | `CLEAR_SSM_STATE` | Reset every SSM lane's hidden state to 0 |
| `0xB` | `SET_ROPE_POS` | Set the shared RoPE position |
| `0xC` | `LOAD_BN_PARAM` | Write one per-channel BATCHNORM parameter |
| `0xD` | `SET_QUANT_PARAMS` | Set the global QUANTIZE scale/shift/zero_point |
| `0xE` | `LOAD_WEIGHT_BURST` | Write one byte into `weight_buffer` (fast bulk load) |
| `0xF` | `FLUSH_WEIGHTS` | Stream `weight_buffer`'s `ROWS*COLS` entries into `mac_array` |

### `0x1` LOAD_WEIGHT
| Bits | Field |
|---|---|
| `[31:28]` | `4'h1` |
| `[16 +: $clog2(ROWS)]` (within `[23:16]`) | `weight_row_addr` |
| `[8 +: $clog2(COLS)]` (within `[15:8]`) | `weight_col_addr` |
| `[DATA_WIDTH-1:0]` (within `[7:0]`) | `weight_data`, signed |

Writes straight into `mac_array` — one weight per USB3.0 word. For loading
a whole layer, `LOAD_WEIGHT_BURST` + `FLUSH_WEIGHTS` (below) is faster and
exercises `weight_buffer.v`.

### `0x2` LOAD_ACT
| Bits | Field |
|---|---|
| `[31:28]` | `4'h2` |
| `[16 +: $clog2(ROWS)]` (within `[23:16]`) | row / lane index |
| `[DATA_WIDTH-1:0]` (within `[7:0]`) | activation data byte, signed |

### `0x3` RUN
| Bits | Field |
|---|---|
| `[31:28]` | `4'h3` |
| `[24]` | `clear_acc` — 1 to start a fresh accumulation, 0 to accumulate onto the previous result |

### `0x4` READ_RESULTS
No fields. Triggers a drain of `pipeline_sequencer`'s `R6` register,
`COLS` words, one per clock as `tx_ready` allows, each lane sign-extended
from `PIPE_WIDTH` up to a full 32-bit word.

### `0x5` SET_MODE
| Bits | Field |
|---|---|
| `[31:28]` | `4'h5` |
| `[0]` | `add_sel` — `0` exact, `1` ETA-II |

Multiplication is not part of `SET_MODE` — it is a fixed hybrid
(DRUM-windowed leading bits + Mitchell log-domain cross terms, see
`mult_drum_mitchell.v`), not a runtime choice. `add_sel` applies to
`mac_array` **and** every approximate unit inside `pipeline_sequencer`
(they share the same `add_sel`).

### `0x6` LOAD_INSTR
| Bits | Field |
|---|---|
| `[31:28]` | `4'h6` |
| `[17 +: $clog2(PROG_DEPTH)]` (within `[23:17]`) | instruction address |
| `[16:0]` | the 17-bit pipeline instruction word — see §2 below |

### `0x7` RUN_PROGRAM
| Bits | Field |
|---|---|
| `[31:28]` | `4'h7` |
| `[7:0]` | `prog_len` — number of instructions to execute, starting at address 0 |

Loads `R7` from the rescaled matmul accumulator, then executes exactly
`prog_len` instructions (or fewer, if a `HALT` is hit first). `R6` holds
the result once done.

### `0x8` SET_RESCALE
| Bits | Field |
|---|---|
| `[31:28]` | `4'h8` |
| `[4:0]` | arithmetic right-shift amount applied to the raw `ACC_WIDTH` accumulator when bridging it into `R7`'s `Q(FRAC_BITS)` domain |

### `0x9` LOAD_SSM_COEF
| Bits | Field |
|---|---|
| `[31:28]` | `4'h9` |
| `[27:26]` | which coefficient — `00` Abar, `01` Bbar, `10` C, `11` D |
| `[25 -: $clog2(COLS)]` (within `[25:21]`) | channel (lane) index |
| `[PIPE_WIDTH-1:0]` (within `[15:0]`) | coefficient value, signed `Q(FRAC_BITS)` |

### `0xA` CLEAR_SSM_STATE
No fields. Pulses every SSM lane's `clear_state`, resetting all hidden
states to 0 (do this before starting a new sequence).

### `0xB` SET_ROPE_POS
| Bits | Field |
|---|---|
| `[31:28]` | `4'hB` |
| `[$clog2(N_POS)-1:0]` (within `[4:0]`) | shared RoPE position, indexes `rope_lut` |

### `0xC` LOAD_BN_PARAM
| Bits | Field |
|---|---|
| `[31:28]` | `4'hC` |
| `[27:26]` | which parameter — `00` mean, `01` inv_std, `10` gamma, `11` beta |
| `[25 -: $clog2(COLS)]` (within `[25:21]`) | channel (lane) index |
| `[PIPE_WIDTH-1:0]` (within `[15:0]`) | parameter value, signed `Q(FRAC_BITS)` |

Same layout as `LOAD_SSM_COEF`, just targeting BATCHNORM's 4 per-channel
parameters instead of the SSM's 4 coefficients.

### `0xD` SET_QUANT_PARAMS
| Bits | Field |
|---|---|
| `[31:28]` | `4'hD` |
| `[27:13]` | `scale`, signed (15 bits; sign-extended to `quantize_int8`'s 16-bit port) |
| `[12:8]` | `shift` amount |
| `[7:0]` | `zero_point`, signed |

Global (per-tensor), not per-channel — applies to every lane the QUANTIZE
unit processes. Defaults to scale=1.0 (Q8.8 `256`), shift=8, zero_point=0
(an identity transform) on reset.

### `0xE` LOAD_WEIGHT_BURST
| Bits | Field |
|---|---|
| `[31:28]` | `4'hE` |
| `[8 +: $clog2(ROWS*COLS)]` (within `[19:8]`) | linear address, `0..ROWS*COLS-1` |
| `[DATA_WIDTH-1:0]` (within `[7:0]`) | weight byte, signed |

Writes into `weight_buffer.v` (now instantiated inside `control_unit.v`),
not `mac_array` directly. Follow with `FLUSH_WEIGHTS` to actually load the
array.

### `0xF` FLUSH_WEIGHTS
No fields. Streams `weight_buffer`'s `ROWS*COLS` entries into `mac_array`'s
stationary weight registers, one per ~2 cycles. Assumes `COLS` is a power
of 2 (true at the 32 default) so the linear address splits into row/col by
plain bit-slicing: `col = addr[$clog2(COLS)-1:0]`,
`row = addr[$clog2(ROWS*COLS)-1:$clog2(COLS)]`. `rx_ready` stays low for
the whole flush (same mechanism that already gates `RUN_PROGRAM`/
`READ_RESULTS`), so a host can safely queue the next command right after.

---

## 2. Pipeline instruction word (17 bits, written via `LOAD_INSTR`)

Decoded by `pipeline_sequencer.v` against its own 8-entry register file
`R0..R7` (`R7` = auto-loaded input, `R6` = drained output, `R0..R5` =
scratch).

| Bits | Field |
|---|---|
| `[16:13]` | opcode |
| `[12:10]` | `dst` register (0-7) |
| `[9:7]` | `src` register (0-7) |
| `[6:4]` | `src2` register (0-7) — `ADD`'s second operand, or `SWIGLU`'s gate input |
| `[3:0]` | `unit_id` (0-10) — only used by `COMPUTE` |

### Opcodes

| Opcode | Name | Effect |
|:--:|---|---|
| `0x0` | `NOP` | Do nothing (safe filler) |
| `0x1` | `COMPUTE` | `R[dst] <= unit[unit_id]( R[src] )` (`SWIGLU` also reads `R[src2]` as its gate) |
| `0x2` | `STORE` | `R[dst] <= R[src]` (pure copy, no compute) |
| `0x3` | `ADD` | `R[dst] <= R[src] + R[src2]` (exact) |
| `0xF` | `HALT` | Stop immediately, output is whatever `R6` currently holds |

### Unit IDs (`COMPUTE` only)

Units 0, 1, 2, 5, 9 (RELU/SIGMOID/SELU/SOFTMAX/SWIGLU) are grouped into
one sub-block, `activation_unit.v`, matching the proposal figure's
"ACTIVATION UNIT" box. Units 3, 4, 8, 11, 12, 13 (RMSNORM/ZSCORE/
BATCHNORM/POOL_MAX/POOL_MIN/POOL_MEAN) are grouped into a separate
sub-block, `norm_pool_unit.v`, matching the figure's own distinct
"Normalization and pooling unit" box. ROPE (`rope_array_unit.v`),
SSM_STEP (`ssm_unit.v`), and QUANTIZE (`quantize_unit.v`) are each their
own separate grouped sub-block in `pipeline_sequencer.v` too (the figure's
other distinct boxes) — every one of these 5 is a single instance in the
hierarchy, with its own N-way-parallel per-lane cores (the "Source file"
column below names those inner per-lane primitives) only visible one
level further down in Vivado's hierarchy view. The ADD opcode (`0x3`,
used for residual connections) is grouped the same way as `add_unit.v`,
even though it isn't its own figure box.

| `unit_id` | Unit | Source file | Notes |
|:--:|---|---|---|
| `0` | RELU | `activation_relu.v` | Exact |
| `1` | SIGMOID | `activation_sigmoid.v` | Hard-sigmoid, exact (no LUT) |
| `2` | SELU | `activation_selu.v` | Exact positive branch, LUT-approximated negative branch |
| `3` | RMSNORM | `norm_rmsnorm.v` | Whole-register vector op, `rsqrt_lut`-approximated |
| `4` | ZSCORE | `norm_zscore.v` | Whole-register vector op, `rsqrt_lut`-approximated |
| `5` | SOFTMAX | `activation_softmax.v` | Whole-register vector op, `exp_lut`+`reciprocal_lut`-approximated |
| `6` | ROPE | `rope_array_unit.v` (`rope_unit.v` x COLS/2 + `rope_lut.v`) | Rotates adjacent lane pairs using the shared position (`SET_ROPE_POS`) |
| `7` | SSM_STEP | `ssm_unit.v` (`ssm_scan_unit.v` x COLS) | **Stateful** — advances every lane's hidden state by one step; state persists across separate `RUN_PROGRAM` calls |
| `8` | BATCHNORM | `norm_batchnorm.v` | Exact (no LUT); per-channel mean/inv_std/gamma/beta via `LOAD_BN_PARAM` |
| `9` | SWIGLU | `activation_swiglu.v` | `src` = value, `src2` = gate; exact if the gate's hard-sigmoid doesn't saturate |
| `10` | QUANTIZE | `quantize_unit.v` (`quantize_int8.v` x COLS) | `Q(FRAC_BITS) -> saturating INT8 -> sign-extended back to PIPE_WIDTH`; global params via `SET_QUANT_PARAMS`. The register's value is then a plain INT8 integer (Q0), not `Q(FRAC_BITS)`, until rescaled again |
| `11` | POOL_MAX | `pool_unit.v` | **Changes vector length**: every 4 adjacent source lanes reduce to 1; the `COLS/4` results land in the destination's low lanes, everything above zeroed |
| `12` | POOL_MIN | `pool_unit.v` | Same grouping as POOL_MAX, `pool_sel=01` |
| `13` | POOL_MEAN | `pool_unit.v` | Same grouping as POOL_MAX, `pool_sel=10` |

Any other `unit_id` value (14-15) passes `R[src]` through unchanged.

---

## 3. Worked examples

### CNN-style: `matmul -> RELU`
```
SET_RESCALE(0)
LOAD_INSTR(0, COMPUTE R6, R7, RELU)   ; 17'h03B80
LOAD_INSTR(1, HALT)                   ; 17'h1E000
RUN_PROGRAM(2)
READ_RESULTS
```

### RNN-style: `matmul -> SIGMOID`
```
LOAD_INSTR(0, COMPUTE R6, R7, SIGMOID)   ; 17'h03B81
LOAD_INSTR(1, HALT)
RUN_PROGRAM(2)
```

### Transformer-style: `matmul -> RMSNORM -> ROPE -> SOFTMAX`
```
LOAD_INSTR(0, COMPUTE R1, R7, RMSNORM)   ; 17'h02783
LOAD_INSTR(1, COMPUTE R2, R1, ROPE)      ; 17'h02886
LOAD_INSTR(2, COMPUTE R6, R2, SOFTMAX)   ; 17'h03905
LOAD_INSTR(3, HALT)
RUN_PROGRAM(4)
```

### Mamba-3-style, with a residual connection
```
STORE   R0, R7                  ; 17'h04380 -- save input aside
COMPUTE R1, R7, RMSNORM         ; 17'h02783
COMPUTE R2, R1, ROPE            ; 17'h02886
COMPUTE R3, R2, SSM_STEP        ; 17'h02D07
ADD     R6, R0, R3              ; 17'h07830 -- output = input + sublayer(x)
HALT
```
This is the case a single always-in-place vector (the pipeline's earlier
design) could not express — see `sim/tb_pipeline_sequencer.v` for this
exact program run.

### BatchNorm, SwiGLU, and Quantize
```
; BATCHNORM: load per-channel params first, then run it like any other unit
LOAD_BN_PARAM(which=mean,    ch=0, value)
LOAD_BN_PARAM(which=inv_std, ch=0, value)
LOAD_BN_PARAM(which=gamma,   ch=0, value)
LOAD_BN_PARAM(which=beta,    ch=0, value)
LOAD_INSTR(0, COMPUTE R6, R7, BATCHNORM)
LOAD_INSTR(1, HALT)
RUN_PROGRAM(2)

; SWIGLU: needs two inputs -- src (value) and src2 (gate) can be any two
; registers, e.g. two different linear-projection outputs stored earlier
LOAD_INSTR(0, COMPUTE R6, R1, R2, SWIGLU)   ; value=R1, gate=R2
LOAD_INSTR(1, HALT)
RUN_PROGRAM(2)

; QUANTIZE: set the global scale/shift/zero_point once, then use it
SET_QUANT_PARAMS(scale, shift, zero_point)
LOAD_INSTR(0, COMPUTE R6, R7, QUANTIZE)
LOAD_INSTR(1, HALT)
RUN_PROGRAM(2)

; POOL_MAX: 4 adjacent lanes -> 1, result in the destination's low lanes,
; everything above zeroed (POOL_MIN/POOL_MEAN work the same way)
LOAD_INSTR(0, COMPUTE R6, R7, POOL_MAX)
LOAD_INSTR(1, HALT)
RUN_PROGRAM(2)
```

---

## Reference

See `rtl/control_unit.v`'s header comment and `rtl/pipeline_sequencer.v`'s
header comment for the authoritative, always-in-sync source of this map —
this file is a convenience summary for writing host-side driver software;
the RTL comments are the source of truth if they ever diverge.
