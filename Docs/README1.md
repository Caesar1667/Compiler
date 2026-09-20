# Approximate-Arithmetic MAC Accelerator — Hardware (Verilog) Part

This is the **hardware/Verilog slice only** of the proposal *"A Composable
Heterogeneous-Model Framework with Approximate Arithmetic as a Per-Block
Property for Energy-Efficient Edge AI."* It does **not** cover the GUI, the
Model Zoo / PyTorch training side, OpenPCIe, or the Yosys/OpenROAD/OpenLane
ASIC flow mentioned in the proposal's *Scope* section — those are separate
work packages. What's here is a synthesizable RTL library plus testbenches
for the proposal's actual novel contribution: **approximate arithmetic as a
runtime-selectable, per-block property**, wired into a small but complete
MAC-array accelerator you can simulate and synthesize in Vivado today.

Every module below is its own file (one module per `.v` file, no exceptions)
so you can read, test, and swap each one independently.

**All 24 testbenches in `sim/` were actually compiled, elaborated, and run
in this machine's Vivado 2024.2 simulator (`xvlog`/`xelab`/`xsim`) while
this was built, and all pass.** `accelerator_top` was also pushed through
`synth_design` (out-of-context, part `xczu7ev-ffvc1156-2-e`, matching the
proposal's ZU7EV target) to confirm it's synthesizable, not just
simulatable. This full loop (write -> simulate -> find a real bug -> fix ->
re-simulate) is how every module here was actually built, not just written
and assumed correct — see "Bugs this process actually caught" below.

## Directory layout

```
rtl/    36 synthesizable modules (1 module per file)
sim/    22 self-checking testbenches (1 per file, same rule)
```

## Mapping to the proposal's architecture figure

| Figure block                    | File(s)                                    |
|----------------------------------|---------------------------------------------|
| PCIe/USB3.0 interface            | `usb3_fifo_if.v`                             |
| Unified buffer / Weight          | `unified_buffer.v` (`pipeline_sequencer`'s instruction memory), `weight_buffer.v` (`control_unit`'s burst weight-load staging RAM) — both wired in |
| quantize-to-int8 unit            | Grouped into **one block**, `quantize_unit.v`: COLS parallel `quantize_int8.v` lanes — wired in via `pipeline_sequencer.v`, `unit_id`=10 |
| MAC Array (32x32)                | `mac_array.v` (default 32x32, see below)     |
| PE / Approximate                 | `pe.v`, `mac_unit.v`                         |
| the multiplier/adders inside PE | `mult_drum_mitchell.v` (the one fixed hybrid multiplier, wrapped by `approx_mult.v`), `adder_exact.v`, `adder_etaii.v`, plus the shared `approx_add.v` select-mux |
| SSM unit                         | Grouped into **one block**, `ssm_unit.v`: per-lane Abar/Bbar/C/D coefficient storage + COLS parallel `ssm_scan_unit.v` cores — wired in via `pipeline_sequencer.v` |
| Rope Trick Unit                  | Grouped into **one block**, `rope_array_unit.v`: one shared `rope_lut.v` + COLS/2 parallel `rope_unit.v` cores — wired in via `pipeline_sequencer.v` |
| **Activation unit**              | Grouped into **one block**, `activation_unit.v`: `activation_relu.v`, `activation_sigmoid.v`, `activation_selu.v`, `activation_swiglu.v`, `activation_softmax.v` (+ helpers `exp_lut.v`, `reciprocal_lut.v`) — wired in via `pipeline_sequencer.v` |
| **Normalization and pooling unit** | Grouped into **one block**, `norm_pool_unit.v`: `norm_rmsnorm.v`, `norm_zscore.v`, `norm_batchnorm.v` (+ helper `rsqrt_lut.v`), `pool_unit.v` (max/min/mean select) — wired in via `pipeline_sequencer.v` |
| Control / INSTRUCTION            | `control_unit.v`, `pipeline_sequencer.v`     |
| (top-level wiring)                | `accelerator_top.v`                          |

The proposal figure draws "ACTIVATION UNIT" and "Normalization and pooling
unit" as two separate boxes — `activation_unit.v` and `norm_pool_unit.v`
are two separate Vivado hierarchy sub-blocks under `pipeline_sequencer`
matching them exactly (5 activation sub-units in one, 6 norm/pool
sub-units in the other), rather than either merging the two families
together or leaving 11 flat sibling instances directly under
`pipeline_sequencer`. Softmax stays in `activation_unit.v`, not
`norm_pool_unit.v`, since it's conventionally classified as an activation
function even though it rescales a whole vector the way a normalization
does. RoPE (`rope_array_unit.v`), SSM (`ssm_unit.v`), and QUANTIZE
(`quantize_unit.v`) are each their own separate grouped block in the
hierarchy too, matching the figure's other distinct boxes — every one of
them is a single instance under `pipeline_sequencer`, with its own
N-way-parallel per-lane cores only visible one level further down in the
Vivado hierarchy view, the same pattern as `activation_unit.v`/
`norm_pool_unit.v`. The register file's ADD opcode (used for residual
connections) is grouped the same way as `add_unit.v`, even though it
isn't its own figure box.

## The approximate arithmetic library (the proposal's real focus)

This is Objective 1–3 of the proposal made concrete, though multiplication
and accumulation now differ in how their approximation is chosen:

- **Multiplication** is a single fixed hybrid, `mult_drum_mitchell.v`: DRUM's
  exact windowed multiply for each operand's leading `K=2` bits, plus
  Mitchell's log-domain (multiplier-free) approximation for the two cross
  terms involving the remaining low-order bits (the smallest, doubly-
  truncated cross term is dropped — standard truncated-multiplier practice).
  This is a compile-time property of the hardware, not a runtime choice —
  see `approx_mult.v` and `mult_drum_mitchell.v`'s header comment for the
  exact algorithm. One useful, provable property: **the product is always
  exact whenever either factor is a power of two** (a clean bit with zero
  remainder on that side makes every cross term vanish) — several
  testbenches (`tb_mac_array.v`, `tb_accelerator_top*.v`, `tb_ssm_scan_unit.v`,
  `tb_norm_batchnorm.v`, `tb_activation_swiglu.v`) lean on this to get a
  genuine bit-exact PASS/FAIL assertion out of test vectors built from
  Q8.8's inherently power-of-two "nice" values (1.0, 0.5, 2.0, ...).
- **Accumulation** keeps Objective 1's runtime-selectable approximation, now
  narrowed to two modes: `mac_unit.v` instantiates both adder variants in
  parallel via `approx_add.v` and a 1-bit `add_sel` picks which one drives
  the accumulator on the next clock edge — `0` exact · `1` Error-Tolerant
  Adder II (ETA-II). The OR-gate substitution adder and the Lower-part OR
  Adder (LOA) that used to round out this bank are retired — ETA-II was
  already the more accurate of the two lower-part-approximate designs at a
  given `K` (see the characterization below), so keeping it alongside exact
  covers the accuracy/cost extremes without the two designs in between.

`sim/tb_mult_drum_mitchell.v` and `tb_adder_etaii.v` are exactly the
"per-primitive cost/error table" that Objective 3 asks for — each one
sweeps its approximate module against the exact baseline and reports
max/mean error. Measured on this machine:

| Module                  | Test vectors | Max \|error\| | Mean \|error\| |
|-------------------------|-------------:|--------------:|---------------:|
| DRUM+Mitchell (K=2)     | 100,000 (random, 8x8)   | 1088 | 140.1 |
| ETA-II (K=8)            | 100,000 (random, 32-bit) | 254  | 134.7 |

Swap `ADD_K` and re-run `tb_adder_etaii.v` to see how ETA-II's own
cost/accuracy curve moves — that sensitivity sweep *is* the deliverable.

## The rest of the library: SSM, quantization, RoPE, activations, normalization, pooling

These fill in the remaining blocks from the proposal's architecture figure.
All of them reuse `approx_mult.v` (the fixed hybrid multiplier) for their
elementwise multiplies, and the ones with an add step reuse `approx_add.v`
(still runtime-selectable via `add_sel`), and the ones that need a
nonlinear function (exp, reciprocal, 1/sqrt) use a small **LUT with direct
lookup and no interpolation** — deliberately the simplest, lowest-bug-risk
approximation shape, since Verilog has no synthesizable trig/transcendental
functions to begin with. Every LUT's actual achieved error is measured by
its own testbench the same way DRUM/Mitchell are characterized above:

| Module           | Domain swept              | Max error      | Mean error     |
|-------------------|---------------------------|---------------:|---------------:|
| `exp_lut`         | x in [-10,2], step 4/256  | 0.0625 (abs)   | 0.0134 (abs)   |
| `reciprocal_lut`  | v in (0,16], step 3/256   | 6.25% (rel)    | 1.42% (rel)    |
| `rsqrt_lut`       | v in (0,16], step 3/256   | 1.56% (rel)    | 0.43% (rel)    |

- **`ssm_scan_unit.v`** — one step of the Mamba-3 / S4-style selective-scan
  recurrence: `h_t = Abar*h_{t-1} + Bbar*x_t`, `y_t = C*h_t + D*x_t`. The two
  multiplies-then-add pairs use `approx_mult` for the multiplies but an
  **exact** `adder_exact` for the add — deliberate, because this recurrence
  runs for hundreds of steps and letting the state accumulate through an
  approximate adder every step would compound error unboundedly (the same
  reasoning as `mac_array.v`'s exact reduction tree). Computing Abar/Bbar/C/D
  from x_t (the "selective" part) is upstream of this unit and out of scope.
- **`quantize_int8.v`** — the affine requantize-to-INT8 stage between a wide
  accumulator and the next layer's INT8 activations, with runtime
  scale/shift/zero-point (so one instance covers every layer) and
  round-to-nearest before saturating.
- **`rope_lut.v` + `rope_unit.v`** — rotary position embedding. `rope_lut`
  is a 32-position cos/sin ROM (one frequency band; a real multi-frequency
  RoPE instantiates one per band) — there is no synthesizable trig function
  in Verilog, so this table is precomputed offline. `rope_unit` is the
  actual 2x2 rotation (`y1=x1*cos-x2*sin`, `y2=x1*sin+x2*cos`), 4 multiplies
  + 2 add/sub, all through `approx_mult`/`approx_add`.
- **Activations**: `activation_relu.v` (exact), `activation_sigmoid.v`
  (hard-sigmoid, PWL, no LUT needed), `activation_selu.v` (exact positive
  branch, `exp_lut`-based negative branch), `activation_swiglu.v`
  (`value * gate * sigmoid(gate)`), `activation_softmax.v` (max-subtract for
  stability + `exp_lut` + `reciprocal_lut`, no real divider).
- **Normalization**: `norm_rmsnorm.v` and `norm_zscore.v` are vector-wide
  (`VEC_LEN`, must be a power of 2) and combinational: an `approx_mult` per
  channel for squaring, an **exact** adder-tree reduction (same
  don't-compound-LUT-error-with-adder-error reasoning as the SSM unit), then
  `rsqrt_lut`. `norm_batchnorm.v` is inference-mode (running mean/inv_std
  already known from training, so it's purely elementwise, no reduction).
- **`pool_unit.v`** — 2x2 pooling with a runtime `pool_sel` (max/min/mean).

## The programmable pipeline: sharing blocks across CNN/RNN/Transformer/Mamba-3

The proposal's architecture figure feeds an `INSTRUCTION` signal into
`Control`, separate from the main data path — the intent is clearly that
`Control` sequences the datapath from a stored program, not a hard-wired
chain. `pipeline_sequencer.v` makes that concrete as a small
**register-addressed instruction set**: an 8-entry vector register file
(`R0..R7`, each a full `COLS`-wide Q8.8 vector) and 17-bit instructions
that name a source, a destination, and — for compute — which shared unit
to route through. `R7` auto-loads from `mac_array`'s rescaled accumulator
when a program starts; `R6` is what `READ_RESULTS` drains once the program
halts; `R0..R5` are free scratch. The full bit-field reference (with every
opcode and worked hex examples) lives in `OPCODES.md`.

```
[16:13] opcode   0x0 NOP      -- do nothing
                 0x1 COMPUTE  -- R[dst] <= unit[unit_id]( R[src] )
                 0x2 STORE    -- R[dst] <= R[src]                (pure copy)
                 0x3 ADD      -- R[dst] <= R[src] + R[src2]      (exact)
                 0xF HALT     -- stop immediately
[12:10] dst   (register 0-7)
[9:7]   src   (register 0-7)
[6:4]   src2  (register 0-7; ADD's 2nd operand, or SWIGLU's gate)
[3:0]   unit_id (COMPUTE only): 0 RELU 1 SIGMOID 2 SELU 3 RMSNORM
                4 ZSCORE 5 SOFTMAX 6 ROPE 7 SSM_STEP
                8 BATCHNORM 9 SWIGLU 10 QUANTIZE
                11 POOL_MAX 12 POOL_MIN 13 POOL_MEAN
```

Units 0/1/2/3/4/5/8/9 are grouped into one sub-block, `activation_norm_
unit.v`, matching the proposal figure's single "ACTIVATION UNIT" +
"Normalization ... unit" boxes (see the mapping table above) — SWIGLU is
the one unit in that group needing a second input, using `src2` as its
gate the same way `ADD` uses it as a second operand. ROPE, SSM_STEP,
QUANTIZE, and POOL each stay their own separate block in the hierarchy,
matching the figure's other distinct boxes.

POOL is the one unit family that changes the vector's effective length (4
lanes reduce to 1): `pool_unit.v` groups every 4 adjacent source lanes
(lanes 0-3, 4-7, ...) into one pooled value, and the `COLS/4` results land
in the destination register's **low** lanes with everything above that
zeroed — the register file itself stays a uniform `COLS`-wide container
even though pooling is not. `sim/tb_pipeline_sequencer.v` checks all three
reductions against the exact case already used by `tb_pool_unit.v`, plus
that a lane past `COLS/4` reads back as 0.

An earlier version of this pipeline had every stage read and overwrite one
implicit "current vector" in place — enough for a straight chain, but
unable to express a **residual connection**, `x + sublayer(norm(x))`,
which every real Transformer/Mamba-3 block needs: nothing could keep the
original `x` around once the chain started overwriting it. STORE and ADD
exist specifically to fix that:

```
STORE   R0, R7                  ; save input aside
COMPUTE R1, R7, RMSNORM
COMPUTE R2, R1, ROPE
COMPUTE R3, R2, SSM_STEP
ADD     R6, R0, R3              ; output = input + sublayer(x)
HALT
```

Because CNN/RNN/Transformer/Mamba-3 all lean on the same handful of
primitives — an activation, a normalization, and (for the two sequence
models) RoPE and one SSM step — just wired differently, **one piece of
hardware realizes any of the four by changing the stored program, not the
RTL**:

- CNN-style: `COMPUTE R6,R7,RELU`
- RNN-style: `COMPUTE R6,R7,SIGMOID`
- Transformer-style: `COMPUTE R1,R7,RMSNORM; COMPUTE R2,R1,ROPE; COMPUTE R6,R2,SOFTMAX`
- Mamba-3-style (with residual): the 6-instruction example above

Every unit is exactly the already-independently-tested standalone module
(`activation_sigmoid.v`, `norm_rmsnorm.v`, `rope_unit.v`, `ssm_scan_unit.v`,
...), all operating in the same Q(FRAC_BITS)=Q8.8, `PIPE_WIDTH`=16-bit
convention those modules already default to — `pipeline_sequencer.v` just
wires them to a selected register instead of a fixed vector, so nothing
about their own tested behavior changes. The bridge from `mac_array`'s raw
`ACC_WIDTH` integer accumulator into that Q8.8 domain is one
host-configurable arithmetic shift (`SET_RESCALE`), playing the same role
`quantize_int8.v`'s `shift_amt` does, just without the final INT8 saturate
(this pipeline stays in wider Q8.8 throughout, not INT8).

SSM_STEP is the one genuinely stateful unit: each of the `COLS` SSM lanes
(one per mac_array output column/channel) keeps its own persistent hidden
state, and that state legitimately **persists across separate
`RUN_PROGRAM` calls** — exactly what a recurrence streamed one token at a
time needs. `sim/tb_pipeline_sequencer.v` proves this directly: it replays
the same 3-step sequence as `tb_ssm_scan_unit.v`, but as 3 *separate*
`RUN_PROGRAM` invocations, and the state carries correctly between them.
That same testbench also runs a 2-instruction chained program
(`RELU` into a scratch register, then `SIGMOID` from there into `R6`) to
prove chaining through the register file works, and a STORE+ADD residual
test with an exact hand-computed expected value — the actual capability
this design exists for.

ROPE shares one `rope_lut` across every dimension-pair per the
simplification already noted in `rope_lut.v` (a real multi-frequency RoPE
would use a different table per pair); RMSNORM/ZSCORE/SOFTMAX/BATCHNORM
treat the selected source register as one whole vector (`VEC_LEN=COLS`, so
`COLS` must stay a power of 2, already true at the 32 default). SWIGLU
reads `src` as its value input and `src2` as its gate. QUANTIZE runs
`Q(FRAC_BITS) -> saturating INT8 -> sign-extended back to PIPE_WIDTH`
using global (per-tensor) scale/shift/zero_point set once via
`SET_QUANT_PARAMS` — after it, the register holds a plain INT8 integer
(Q0), not `Q(FRAC_BITS)`, until something rescales it again.
`sim/tb_pipeline_sequencer.v` covers all three (BATCHNORM reproduces
`tb_norm_batchnorm.v`'s exact case, SWIGLU is self-gated for an exact
expected value, QUANTIZE checks both pass-through and saturation).

`weight_buffer.v` is now wired into `control_unit.v` too, as a staging RAM
for bulk weight loads: `LOAD_WEIGHT_BURST` writes one byte at a time (fast,
no per-element row/col decode), then `FLUSH_WEIGHTS` streams every entry
into `mac_array`'s stationary registers via a small two-state FSM (the
same registered-RAM-read-latency pattern `pipeline_sequencer` already uses
for its own instruction fetch). `sim/tb_accelerator_top_32x32.v` uses this
path (instead of one `LOAD_WEIGHT` per element) to load its full 1024-entry
weight matrix, end to end through the USB3.0 pins.

`prog_len`, not solely the in-memory `HALT`, bounds how many instructions
run: with only an in-memory sentinel, an uninitialized or partially-loaded
program would decode `X` and could loop forever; counting down a
host-supplied length is simple and cannot hang. `HALT` lets a program also
end early, on top of that bound.

### Extended command protocol (host -> `control_unit.v`)

Ten opcodes were added for the pipeline and weight-buffer path on top of
the original five — see `OPCODES.md` for the complete bit-field reference;
summary:

| opcode | meaning | fields |
|---|---|---|
| `6` LOAD_INSTR      | write one 17-bit instruction word into the program | `[23:17]` address, `[16:0]` instruction word |
| `7` RUN_PROGRAM     | execute `prog_len` instructions from address 0 | `[7:0]` prog_len |
| `8` SET_RESCALE     | bridge shift, `ACC_WIDTH` -> Q(FRAC_BITS) | `[4:0]` shift amount |
| `9` LOAD_SSM_COEF   | one per-channel SSM coefficient | `[27:26]` which (0=Abar 1=Bbar 2=C 3=D), `[25:21]` channel, `[15:0]` value |
| `A` CLEAR_SSM_STATE | reset every SSM lane's hidden state | — |
| `B` SET_ROPE_POS    | shared RoPE position | `[4:0]` position |
| `C` LOAD_BN_PARAM   | one per-channel BATCHNORM parameter | `[27:26]` which (0=mean 1=inv_std 2=gamma 3=beta), `[25:21]` channel, `[15:0]` value |
| `D` SET_QUANT_PARAMS| global QUANTIZE scale/shift/zero_point | `[27:13]` scale, `[12:8]` shift, `[7:0]` zero_point |
| `E` LOAD_WEIGHT_BURST| write one byte into `weight_buffer` | `[19:8]` linear address, `[7:0]` data |
| `F` FLUSH_WEIGHTS   | stream `weight_buffer` into `mac_array` | — |

`READ_RESULTS` (opcode `4`) now drains `pipeline_sequencer`'s `R6` register
instead of the raw accumulator — both `sim/tb_accelerator_top.v` and
`sim/tb_accelerator_top_32x32.v` were updated to issue
`SET_RESCALE(0) -> LOAD_INSTR(COMPUTE R6,R7,RELU) -> LOAD_INSTR(HALT) ->
RUN_PROGRAM(2)` between the matmul and the read, reproducing their
original ReLU-only expected results through the new register-addressed
path — both still pass, at both 4x4 and the full 32x32 default.

**A real race this caught:** `rx_ready` was originally just
`(state==S_IDLE) && !pipe_busy`. But `pipe_busy` is `pipeline_sequencer`'s
own registered output, so it only rises one clock edge *after*
`RUN_PROGRAM` is decoded — during that single cycle, `rx_ready` was
wrongly still high, so a command sent immediately after `RUN_PROGRAM` with
no gap (like `READ_RESULTS`, exactly what both end-to-end tests do) could
be accepted before the program had actually started, reading stale
results. Fixed with a `pipe_running` latch in `control_unit.v` that is set
the *same* cycle `RUN_PROGRAM` is decoded and only clears on
`pipeline_sequencer`'s `done` pulse, closing the gap. This is a real
protocol hazard a real host driver could hit, not just a testbench
artifact — worth remembering if you add more back-to-back command
sequencing later.

### Bugs this process actually caught

Two of the new modules had real, non-obvious bugs that only showed up once
actually simulated — both are the same classic Verilog trap: **a
part-select of a signed packed vector (`x_in[i*W +: W]`) is always
UNSIGNED**, even though `x_in` itself is declared `signed`:

- `activation_softmax.v`'s max-finding compared that unsigned slice
  directly against the running max, so a negative input's bit pattern read
  as a huge positive number and always "won" — every output collapsed to a
  uniform 1/VEC_LEN regardless of the actual inputs. Fixed by wrapping each
  slice in `$signed(...)` before comparing.
- `norm_zscore.v` added that same unsigned slice into a *wider* signed
  accumulator, so a negative channel got zero-extended instead of
  sign-extended, corrupting the mean. Same `$signed(...)` fix. (The original
  test happened to use an all-non-negative vector, so it didn't catch this —
  `tb_norm_zscore.v` now also runs a vector with a negative value
  specifically to cover it.)

Whenever you touch a vector-wide module in this library, watch for this
exact pattern.

`pipeline_sequencer.v` hit a different, tooling-specific issue: its SSM
enable logic referenced the FSM's `state` register before that register's
`reg` declaration appeared later in the file. Plain Verilog generally
allows this (nets/regs are elaborated independent of textual order), but
Vivado's `xvlog` rejected it (`identifier 'state' is used before its
declaration`) — fixed by moving `state`/`remaining`'s declaration to the
top of the module. Worth knowing if you reorder sections in any of these
files: keep a signal's declaration above its first use, even where the
language doesn't strictly require it.

## Dataflow simplification (read this before extending the array)

A fully wire-length-optimal systolic array skews every row's input in time
so partial sums also flow, pipelined, through the grid. That's a real design
effort on its own and orthogonal to the proposal's actual contribution
(approximate arithmetic), so `mac_array.v` uses a simpler, still-genuinely-
systolic-per-PE but easier to verify scheme instead:

- Weight `W[i][j]` is stationary in `PE[i][j]`.
- Every cycle `valid_in=1`, row `i`'s activation is **broadcast** to every
  PE in that row (no column-to-column shifting) and locally accumulated.
- An exact adder tree reduces each column across the row dimension:
  `col_result[j] = sum_i PE[i][j].acc`.

For one `valid_in` pulse (the common case, and what every testbench here
does) this computes exactly one matrix-vector product `col_result = act_in
x W` — a full FC/conv-1x1 layer step. Streaming more pulses before the next
`clear_acc` keeps accumulating (useful for summing multiple frames against
the same stationary weights). If you later want true wire-length-matched
systolic skew, that's a drop-in change inside `mac_array.v` only — nothing
above or below it needs to change.

`ROWS`/`COLS` now **default to 32x32** (the proposal's target size) with
`DATA_WIDTH=8` (INT8) per PE — `mac_array`, `control_unit`, and
`accelerator_top` all default this way, and `sim/tb_mac_array_32x32.v`
actually instantiates `mac_array` at these exact defaults (1024 PEs, no
override) and checks all 32 output columns, confirming the design is
correct at full scale in simulation, not just at a scaled-down size.
Everything is written generically against `$clog2(ROWS)`/`$clog2(COLS)`, so
any size works.

**Read this before synthesizing at 32x32, though:** every `pe.v` instance
keeps the fixed hybrid multiplier (itself costlier than the old 3-way mux —
see "Synthesizing `accelerator_top`" below) and *all four* adders
instantiated in parallel so the accumulate mode stays runtime-selectable
(see `mac_unit.v` above) — great for a small array, expensive at 1024 PEs.
The current whole-`accelerator_top` 4x4 build measures 83,109 LUTs (see
below); the MAC array's own share of that scales with `ROWS x COLS` (64x
from 4x4 to 32x32) while the pipeline's share scales only with `COLS`
(8x), so a full 32x32 build lands well past a ZU7EV's 230,400 LUTs even
before accounting for anything else. So:
- **Simulation** at 32x32 works today, as `tb_mac_array_32x32.v` shows.
- **Synthesizing the full 32x32 default** was not attempted here (it would
  likely take a long time and may not fit the ZU7EV as-is).
- For a synthesis run that needs to actually fit today, override
  `ROWS`/`COLS` back down (4, 8, 16 — the earlier `synth_design` numbers
  above are for 4x4) at instantiation.
- For a real 1024-PE build, the next design step would be to stop keeping
  every adder variant instantiated per PE and instead fix `add_sel` to a
  single synthesis-time choice too (multiplication is already a single
  fixed hybrid, not a per-PE variant bank — see the arithmetic library
  section above) — a small generate-based variant-select in `pe.v`/
  `mac_unit.v`, guided by the cost/error tables above — that is a
  deliberate follow-on, not something this pass changed,
  since it would trade away the runtime-selectable demo this library is
  built to showcase.

## USB3.0 IO — what's actually implemented and why

A real SuperSpeed USB3.0 PHY/LTSSM cannot be built in FPGA fabric without a
hard transceiver + an off-the-shelf link-layer IP (the proposal itself
calls out third-party IP for this — OpenPCIe for the PCIe path). The
realistic, common way student/FPGA projects get USB3.0 IO is a USB3.0-to-
FPGA bridge chip (FTDI FT601/FT600, Cypress/Infineon FX3 in slave-FIFO mode)
that exposes a plain **synchronous FIFO** bus to the FPGA. `usb3_fifo_if.v`
implements exactly that bus (`fifo_data[31:0]`, `fifo_rxf_n`, `fifo_txe_n`,
`fifo_rd_n`, `fifo_wr_n`, `fifo_oe_n`) and turns it into a simple
valid/ready stream for the rest of the design — this is the genuine
"hardware Verilog part" of a USB3.0 link; swapping in a specific bridge
chip's exact IP/timing later is a board-bring-up task, not an RTL rewrite.

## Base command protocol (host -> `control_unit.v`, 32-bit words)

The original 5 opcodes drive weight/activation loading and the matmul
itself; the 6 pipeline opcodes documented above extend the same protocol:

| opcode `[31:28]` | meaning | fields |
|---|---|---|
| `1` LOAD_WEIGHT  | write one stationary weight | `[23:16]` row, `[15:8]` col, `[7:0]` data |
| `2` LOAD_ACT     | fill one lane of the activation vector | `[23:16]` row, `[7:0]` data |
| `3` RUN          | pulse `valid_in` for one cycle | `[24]` = also pulse `clear_acc` |
| `4` READ_RESULTS | drain `COLS` pipeline output words over TX | — |
| `5` SET_MODE     | choose the accumulate-side approximation mode | `[0]` add_sel (multiplication is a fixed hybrid, not selectable) |

Two testbenches drive this full protocol (base + pipeline opcodes) through
the external FIFO pins (i.e. they behave like the host-side FT601/FX3
driver would), both end-to-end from the USB3.0 pins through the MAC array,
the programmable pipeline, and back:

- **`sim/tb_accelerator_top.v`** — `accelerator_top` at a scaled-down 4x4
  override, for a fast smoke test: loads a 4x4 weight matrix and 4-element
  activation vector, runs one matrix-vector multiply, runs a 1-instruction
  RELU program, reads all 4 results back.
- **`sim/tb_accelerator_top_32x32.v`** — the real thing: `accelerator_top`
  at its **default** parameters (32x32, 1024 PEs, INT8), loading a full
  dense 32x32 weight matrix (1024 `LOAD_WEIGHT` words) and a 32-element
  activation vector purely through the FIFO pins, running the same RELU
  program, then reading all 32 results back and checking every one against
  a reference computed in the testbench. This is the test that proves the
  actual proposal-sized default system works end-to-end, not just a
  downsized stand-in — it passes, completing in under a second of
  wall-clock simulation time.

Both only exercise the RELU program (reproducing what the old hard-wired
path did) since that is what their existing reference models check;
`sim/tb_pipeline_sequencer.v` is where every other stage, and actual
multi-instruction chaining, gets exercised directly against
`pipeline_sequencer` (see above).

## Every module is wired into one top

`accelerator_top.v` is the single real top module, and every RTL file in
`rtl/` is now reachable from it — nothing sits as an untested-in-context,
standalone fragment anymore. Confirmed by actually elaborating
`accelerator_top` in Vivado and checking the instance tree, not just by
inspection: `weight_buffer` inside `control_unit`; `unified_buffer` as
`pipeline_sequencer`'s instruction memory; and inside `pipeline_sequencer`,
`activation_unit` (grouping RELU/SIGMOID/SELU/Softmax/SwiGLU),
`norm_pool_unit` (grouping RMSNorm/Z-score/BatchNorm/pooling, including
`pool_unit` one per 4-lane group), `rope_array_unit` (`rope_lut` + one
`rope_unit` per lane pair), `ssm_unit` (one `ssm_scan_unit` per lane),
`quantize_unit` (one `quantize_int8` per lane), and `add_unit` (one
`adder_exact` per lane, for the ISA's ADD opcode) as six separate, single-
instance sub-blocks — each one click open in the Vivado hierarchy view to
see how many parallel lanes it holds inside. Every one of those units is
also individually reachable at runtime through a `unit_id` in the
programmable pipeline (see `OPCODES.md`) — being present in the hierarchy
and being *usable* are the same thing here, not just co-located source
files.

Every module keeps its own standalone testbench too (so a bug in, say,
`rope_unit.v` is still easy to isolate), but the thing that actually ships
— the running system — is this one `accelerator_top`, and
`sim/tb_accelerator_top_32x32.v` exercises it end to end through nothing
but the external USB3.0 pins.

## Running the simulations in Vivado

### Option A — Vivado GUI
1. Create a new RTL project (or a "Project not required" one for quick sim).
2. Add every file in `rtl/` as design sources.
3. Add one file from `sim/` as a simulation-only source (e.g. `tb_mac_array.v`).
4. Set that testbench as the simulation top, then **Run Simulation ->
   Run Behavioral Simulation**. Each testbench calls `$finish` itself and
   prints `PASS`/`FAIL` lines to the Tcl console.
5. Repeat per testbench (only one `tb_*.v` should be a sim source at a time,
   since each defines a top-level `module` with no ports).

### Option B — Vivado's command-line simulator (no project needed)
This is exactly how every result in this README was produced, from a
terminal with Vivado's `bin/` on `PATH`:

```
xvlog -sv rtl/*.v sim/*.v
xelab -s snap_tb_mac_array tb_mac_array
xsim snap_tb_mac_array -runall
```

Swap `tb_mac_array` for any other testbench name in `sim/` to run it
instead (e.g. `tb_mult_drum_mitchell`, `tb_accelerator_top`, `tb_usb3_fifo_if`).

## Synthesizing `accelerator_top`

The **default** is 32x32 (see "Dataflow simplification" above for why that
was not synthesized as-is — it likely doesn't fit a ZU7EV with every
arithmetic variant kept instantiated per PE). Override `ROWS`/`COLS` down
for a synthesis run that needs to actually complete/fit:

```
read_verilog -sv [glob rtl/*.v]
synth_design -top accelerator_top -part xczu7ev-ffvc1156-2-e -generic ROWS=4 -generic COLS=4
```

This exact command (out-of-context, so it needs no XDC) was actually run
against the proposal's own ZU7EV part on this machine, with the **complete**
hierarchy included — every module in `rtl/` reachable from this one top,
`activation_unit`'s 5 grouped sub-units, `norm_pool_unit`'s 6 grouped
sub-units (including `pool_unit`), QUANTIZE, and `weight_buffer` inside
`control_unit`: **0 errors**, completing in about 17 minutes. Resource
usage at 4x4:

| Resource | Used | Available | Util% |
|---|---:|---:|---:|
| CLB LUTs | 82,180 | 230,400 | 35.7% |
| CLB Registers | 2,135 | 460,800 | 0.5% |
| DSPs | 16 | 1,728 | 0.9% |
| Block RAM | 0 | 312 | 0.0% |

0 latches. LUTs jumped from 48,821 to 83,109 (registers barely moved) when
multiplication switched from a 3-way exact/DRUM/Mitchell mux to the fixed
`mult_drum_mitchell.v` hybrid: every multiply site now costs a tiny K=2
exact multiplier **plus two full Mitchell instances** (one per cross term)
**plus** the dynamic hi/lo bit-masking that splits each operand — strictly
more logic than muxing three pre-built alternatives used to cost, even
though only one "mode" exists now. DSPs fell just as sharply, 80 -> 16,
for the mirror-image reason: `mult_exact.v` (the plain `a*b` that Vivado
happily mapped onto DSP48 slices) is gone entirely, and neither a 2x2
exact multiply nor Mitchell's shift-and-add reconstruction gives the tool
anything worth placing in a DSP48 over LUT fabric. Net effect: this
hybrid multiplier trades DSP blocks for LUTs, and costs more of the
latter than the 3-way mux it replaced — a real, expected consequence of
computing two log-domain corrections per multiply instead of just picking
one pre-built multiplier, not a synthesis anomaly. Retiring the OR-gate
and LOA adders (keeping only exact and ETA-II, `add_sel` now 1 bit) then
brought LUTs back down slightly, 83,109 -> 82,180: two fewer adder
instances at every accumulate site, a small saving next to the
multiplier's own cost. Still comfortably under half a ZU7EV at 4x4, but
no longer "under a quarter" the way the old design was; scale this cost
up before assuming the 1024-PE default fits anything.

You will see 32 "critical warning" messages of the form `Converted tricell
instance ... to logic` — these come from `usb3_fifo_if.v`'s tri-state
`fifo_data` bus being synthesized *out of context*, i.e. not directly at a
chip pin. A true top-level build (where `fifo_data` connects straight to
the USB3.0 bridge chip's pins via an XDC) will have Vivado infer a real
IOBUF there instead and this warning goes away; it is not a functional bug
in this OOC check. There were no other critical warnings (7 ordinary
warnings, unrelated to functionality).

Use a real ZU7EV part number for your board (the one above is a generic
placeholder) and add your board's XDC for the USB3.0 bridge chip's pins.

## Parameters worth sweeping

All of `ROWS`, `COLS`, `DATA_WIDTH`, `ACC_WIDTH`, `DRUM_K`, and `ADD_K` are
plain Verilog parameters on `accelerator_top`/`mac_array`/`mac_unit`. Re-run
`tb_mult_drum_mitchell`/`tb_adder_etaii` after changing `K` to build out the
actual Pareto cost/error table Objective 3 describes — that is the intended
use of this RTL library, not just a fixed demo. The newer modules add
`VEC_LEN` (norm/softmax vector width, must stay
a power of 2) and `FRAC_BITS` (fixed-point precision, default Q8.8) —
re-running `tb_exp_lut`/`tb_reciprocal_lut`/`tb_rsqrt_lut` after changing
`FRAC_BITS` shows how that trades off against the LUT tables' fixed 8-bit
internal precision.
