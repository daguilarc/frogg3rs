# Froggers Daisy Field Manual

Firmware for the original **Daisy Field** Eurorack module. It builds as two
separate programs, and the Field holds one at a time — flashing one replaces the
other.

| variant | app directory | artifact | reverb page | external input |
|---|---|---|---|---|
| **Froggers Solo** | `src/FroggersSolo/` | `build/FroggersSolo.bin` | yes | only the oscillators' ring mod of the input is output |
| **Froggers Guitar** | `src/FroggersGuitar/` | `build/FroggersGuitar.bin` | no | the dry input is mixed with that ring mod |

The Field holds whichever you flashed last. Guitar is the one with no Reverb
page.

The firmware builds and flashes from this tree; `make firmware-test` at the repository root builds and runs its host-side tests. The two variants are the table above.

This instrument is unrelated to the Sheaf app that shares this repository. It uses a different parameter
model entirely: seven knobs per page plus a page-local `FUEG` fuegoizer, `M1..M7` modulation assignment,
a cross-coupler, and a Marbles-style random page. None of that maps onto the app's banks.

- Current app (`app/`) → [`MANUAL.md`](MANUAL.md), with the terse glossary at [`QUICK_DICT.md`](QUICK_DICT.md)

---

Firmware for **Daisy Field**: external input plus three VCOs, drive/DSP, algorithmic reverb (Solo only), CV
modulation, and Marbles-style random sources.

## Quick Start

- Power on and connect audio in/out.
- **`SW1` / `SW2`**: previous / next parameter page (works anytime, including during mod-assign — paging exits assign mode).
- **Knobs 1–7**: page parameters (after modulation and fuegoization — see below).
- **Knob 8 (`FUEG`)**: fuegoizer amount on pages that use it.
- **`A1..A7`**: hold to assign modulation from `M1..M7`.
- **`A8` / `B8`**: VCO1 / VCO2 waveform.

## Signal flow (audio)

```text
external input + three VCOs
  → mix (external present: parallel ring mod alone on Solo, dry + ring mod on Guitar;
     VCO-only at OLVL when silent)
  → Drive (polynomial drive, fuzz, digital reorganizer, SRR)
  → Pure delay
  → Comb filter
  → Resonant bump
  → Reverb (wet/dry) — Solo only
  → output
```

**External input** (when audio is detected above ~−40 dBFS): each VCO
ring-modulates the external signal and the firmware averages those three
products — parallel ring mod.

- **Froggers Solo:** that average is the whole signal. The oscillators' own voice
  is gone, not quieter.
- **Froggers Guitar:** the dry external signal is summed with it, weighted 7:5 —
  the dry path is 1.4× the ring-mod path, and the two weights add to 1, so the
  level matches Solo at the same knobs. You hear the instrument and the ring mod
  together. Both terms go into one chain, so the drive and filter act on the sum.

When the input is silent both variants are identical: **VCO-only** at `OLVL` ×
the oscillator mix. `FUEG` does not change the external mix on either.

Modulation and Marbles run in parallel; they shape **parameter values**, not a separate audio bus.

## Fuegoizer (`FUEG`, knob 8)

The fuegoizer is key to how Froggers feels on the Field. The FUEG parameter controls how strongly knobs **1–7** are “fuegoized” before they reach the DSP.

Each affected parameter is a value from **0 to 1** (after CV/modulation). The firmware treats that value like an **8-bit number** (0–255). With fuegoization active:

1. **Modulation is applied first** (your `M1..M7` amounts) — crossfade blend, not attenuator multiply; see [Modulation](#modulation-m1m7).
2. **FUEG** picks how many **low bits** participate in a scramble (more `FUEG` → more low bits).
3. Those low bits are **XOR-mangled** (bit flips, shifts) in a pattern that depends on **which knob** on the page you are on.
4. The result is what the synth actually uses.

**At `FUEG` = minimum:** knobs behave normally — smooth, predictable control.

**As you raise `FUEG`:** the **fine resolution** of knobs 1–7 gets destroyed on purpose — stepped, gritty, “wrong” values between stable islands. Small knob moves can jump the sound dramatically. This is the main performance character of the instrument.

### What it does to the hardware knobs (pickup)

When fuegoization is active, the firmware does **not** require the physical knob to match the stored value exactly. It only compares the **high bits** (the part fuego leaves alone). So:

- **High `FUEG`:** easier to “catch” a parameter when you touch a knob, but **coarser** effective resolution.
- **Low `FUEG`:** normal pick-up behavior; you must land closer to the stored value.

See [Knob tracking](#knob-tracking) for what the display symbols mean and how to activate a knob.

### Which pages have `FUEG`

| Page   | Knobs 1–7 fuegoized? | Knob 8 |
|--------|----------------------|--------|
| Audio  | Yes                  | `FUEG` (also PM3 on Audio page — see below) |
| Marbles| Yes                  | `FUEG` |
| Reverb | Yes                  | `FUEG` (Solo only) |
| Filter | Yes                  | `FUEG` |
| Drive  | Yes                  | `FUEG` |

`B1` randomize **does not** randomize `FUEG` itself (so you can randomize the other seven without losing your fuego setting).

### Audio page exception: PM3

On the **Audio** page only, the **stored Audio-page `FUEG` value** (raw, not fuegoized) is read for one extra DSP role — always from the Audio page parameter store, even when you are viewing another page:

1. **PM3 depth** — extra phase modulation from **VCO2 → VCO3** when the cross-coupler is on the 2→3 side.

So on Audio, knob 8 is the page fuegoizer **and** PM3 depth.

Diego designed it this way because he ran out of parameters on this page and got lazy, sorry.

---

## Knob tracking

Each parameter row on the OLED ends with a **tracking badge** — a single character that tells you whether the physical knob is controlling that parameter yet.

| Badge | Meaning |
|-------|---------|
| **`>`** | The physical knob is **below** the stored value. The parameter is **not** updating. Turn that knob **clockwise** until you pass through the stored value; the badge clears to a blank and the knob takes over. |
| **`<`** | The physical knob is **above** the stored value. Turn **counter-clockwise** until you pass through the stored value to activate tracking. |
| **` `** (blank) | **Tracking** — the knob is live and moves the parameter normally. |
| **`-`** | Idle — seen briefly when you first land on a page before pickup is evaluated. |

This is intentional **pickup** behavior: after a page change, randomize, or any time the stored value and the physical knob disagree, the firmware waits for you to **cross** the stored value before it hands control to the knob. That avoids jumps when you touch a knob that was left in a very different position.

With **fuegoization** active, “close enough” compares only the **high bits** (the part fuego leaves alone), so pickup can feel easier but coarser — see above.

**Mod-assign mode** (`A1..A7` held): the same symbols apply to modulation depth instead of the main parameter value.

---

## Page order (`SW1` / `SW2`)

**Froggers Solo** — five pages:

1. **Audio** — oscillators, coupling, PM, level  
2. **Marbles** — random mod sources `M6` / `M7`  
3. **Reverb** — stereo-ish algorithmic reverb send  
4. **Filter** — delay, comb, resonant peak (in series before reverb)  
5. **Drive** — distortion / digital destruction (first in the FX chain after osc mix)

**Froggers Guitar** — four pages, the same list without Reverb:

1. **Audio**  
2. **Marbles**  
3. **Filter**  
4. **Drive**

`SW1` / `SW2` wrap around whatever the page count is, so Guitar has no gap where
Reverb used to be.
(note: on some initializations, device may start on the last page; we may try to fix this)

---

## Buttons

### A row

| Key | Action |
|-----|--------|
| `A1..A7` | Hold to select modulation source **`M1..M7`**; turn knobs to set depth |
| `A8` | Cycle **VCO1** wave: sine → saw → square → sine |

### B row

| Key | Action |
|-----|--------|
| `B1` | Randomize knobs 1–7 on **current** page (not `FUEG`) |
| `B2` | Randomize knobs 1–7 on **all** pages |
| `B3` | Randomize modulation assignments on **current** page |
| `B4` | Randomize modulation on **all** pages |
| `B5` | **Marbles increment** — step both Marbles bags (see [Marbles](#marbles-page)) |
| `B6` | Randomize **XCPL** toward **1→2** coupling (lower half of knob) |
| `B7` | Randomize **XCPL** toward **2→3** coupling (upper half of knob) |
| `B8` | Cycle **VCO2** wave: sine → saw → square → sine |

---

## Modulation (`M1..M7`)

### Assignment

1. Open the page with the targets you want.  
2. Hold **`A1..A7`** for the source you want (`M1` = first CV, …, `M7` = Marbles 2).  
3. Turn knobs to set how much that source pushes each parameter.  
4. Release the key to exit mod-assign mode.

The display shows **`M#`** and per-knob tracking when assigning.

Mod depth is a **crossfade** between the stored knob value and the mod source (`base × (1 − depth) + mod × depth`), not `knob × CV`. Fuegoization runs **after** modulation — see [Fuegoizer](#fuegoizer-fueg-knob-8).

### Sources

| Source | Origin |
|--------|--------|
| `M1..M4` | External CV inputs |
| `M5` | **VCO feature**: every 64 samples, average \|VCO1+VCO2+VCO3\| is sampled, tanh-compressed and smoothed → slow 0–1 control (not audio-rate) |
| `M6` | Marbles channel 1 (smoothed marble value) |
| `M7` | Marbles channel 2 |

### External CV bypass

If `M1..M4` is assigned but no active CV is detected, that mod source is **ignored** and the parameter uses knob + other mods only.

---

## Audio page

Three oscillators feed the mix, with cross-coupling and three phase-mod paths.

**Cross-coupler (`XCPL`, knob 4):** one knob, two directions from noon. **CCW** turns on VCO1↔VCO2 coupling; **CW** turns on VCO2↔VCO3; **noon** = independent VCOs.

**Phase mod:** `PM1A` = VCO2 modulates VCO1 when 1→2 coupling is active. `PM2A` = VCO1 and VCO3 modulate VCO2. `PM3A` (via stored Audio-page `FUEG`) = VCO2 modulates VCO3 when 2→3 coupling is on.

**External input:** optional line/mic. When silent, output is VCO-only at `OLVL` on both variants. When loud enough (gate open): parallel ring mod alone on Solo, dry-plus-ring-mod at 7:5 on Guitar. `FUEG` does not change that mix shape.

| Knob | Label | What it does |
|------|-------|----------------|
| 1 | `V1VO` | VCO1 frequency (exponential, ~20 Hz–20 kHz range via phase increment) |
| 2 | `V2VO` | VCO2 frequency (same mapping) |
| 3 | `V3VO` | VCO3 frequency (sine only) |
| 4 | `XCPL` | **Cross-coupler** — one knob, two directions from noon: **CCW** enables 1→2 coupling strength; **CW** enables 2→3; **noon** = independent VCOs. Strength uses exponential curve from noon. |
| 5 | `PM1A` | Phase-mod depth: **VCO2 modulates VCO1** when 1→2 coupling is active |
| 6 | `PM2A` | Phase-mod depth: **VCO1 and VCO3 modulate VCO2** (1→2 and 2→3 paths) |
| 7 | `OLVL` | Oscillator level for **VCO-only** output when external input is silent (no effect on external ring-mod mix) |
| 8 | `FUEG` | Fuegoizer for knobs 1–7; **also PM3 depth** (VCO2 → VCO3 PM when 2→3 coupling is on) |

**Waveforms:** VCO1 (`A8`) and VCO2 (`B8`): sine → saw → square. VCO3 is always sine.

**Coupling detail:** PM amounts are gated by coupling — e.g. `PM1` only does work when `XCPL` is turned toward 1→2.

---

## Marbles page

Inspired by **Mutable Instruments Marbles**. Two independent “bags” of random values, **manually** advanced with **`B5`**. Outputs feed **`M6`** and **`M7`**.

| Knob | Label | What it does |
|------|-------|----------------|
| 1 | `PROB` | Per-channel chance that a **`B5`** press actually steps (higher = more likely) |
| 2 | `DJV1` | Channel 1 **Deja vu** — below noon: step through bag, sometimes **re-roll** current marble; above noon: mostly step, sometimes **random jump** in bag |
| 3 | `SZ1` | Channel 1 bag size (**2–8** marbles) |
| 4 | `SLW1` | Channel 1 output slew (low-pass on the active marble value → `M6`) |
| 5 | `DJV2` | Same as `DJV1` for channel 2 |
| 6 | `SZ2` | Bag size channel 2 |
| 7 | `SLW2` | Slew channel 2 → `M7` |
| 8 | `FUEG` | Fuegoizer for knobs 1–7 on this page |

### `B5` (increment trigger)

Not a clock input — **you** are the clock.

Each press, **per channel**:

1. Roll against **`PROB`**; on fail, that channel does nothing this press.  
2. Otherwise advance the bag (next index, maybe jump or re-randomize per **`DJV`**).  
3. The active marble value is smoothed and written to **`M6`** / **`M7`**.

No presses = frozen random level (still smoothed). Fast presses = fast random walks.

**Typical use:** set Marbles on this page; assign `M6`/`M7` on Filter/Audio (and Reverb on Solo); tap **`B5`** in time with music.

---

## Reverb page (Froggers Solo only)

Dual delay-line reverb with pre-delay, damping, and delay-time LFO. Froggers
Guitar does not have this page; its delay lines are not in that binary at all.

While the mix rests at zero the reverb is bypassed entirely, so it costs nothing
when you are not using it. The bypass has separate enter and exit thresholds so a
knob parked at the very bottom does not chatter in and out.

| Knob | Label | What it does |
|------|-------|----------------|
| 1 | `RVMX` | Wet/dry mix (0 = dry only, 1 = wet only) |
| 2 | `RSIZ` | Room size — scales base delay lengths of the two lines |
| 3 | `RDEC` | Feedback / decay time (higher = longer tail) |
| 4 | `RPRE` | Pre-delay before energy enters the tank |
| 5 | `RDMP` | High-frequency damping in the feedback path (higher knob = brighter tail) |
| 6 | `RMOD` | LFO depth on delay times (chorus-like motion) |
| 7 | `RRAT` | LFO rate for that modulation |
| 8 | `FUEG` | Fuegoizer for knobs 1–7 |

---

## Filter page

Serial tone shaping **after** Drive, **before** reverb (Solo) or as the last stage (Guitar).

| Knob | Label | What it does |
|------|-------|----------------|
| 1 | `DELF` | **Pure delay** line frequency / length (short delay, comb-ish color) |
| 2 | `BUPF` | **Resonant bump** center frequency |
| 3 | `BUPR` | Bump **gain / resonance** (peak height) |
| 4 | `BUPW` | Bump **width** (Q) |
| 5 | `COMF` | **Comb** delay time (comb filter pitch) |
| 6 | `COMQ` | Comb **feedback** (resonance of comb peaks) |
| 7 | `CMLP` | Low-pass inside the comb feedback loop (darker comb when higher) |
| 8 | `FUEG` | Fuegoizer for knobs 1–7 |

---

## Drive page

First processing stage on `(input + oscillators)`.

| Knob | Label | What it does |
|------|-------|----------------|
| 1 | `GAIN` | Polynomial **drive** amount |
| 2 | `SHAPE` | Polynomial **curve / coefficients** (harmonic character) |
| 3 | `SRR1` | **Sample-rate reducer** 1 — lower rate = heavier decimation |
| 4 | `SRR2` | **Sample-rate reducer** 2 (second stage) |
| 5 | `DIGR` | **Digital reorganizer** XOR flip amount on sample bits |
| 6 | `HASH` | How many low bits the reorganizer scrambles (more = harsher digital grit) |
| 7 | `FUZZ` | Blend between sine-shaped and tanh-saturated distortion in the drive core |
| 8 | `FUEG` | Fuegoizer for knobs 1–7 |

**Drive chain (inside `FrogBlock`):** polynomial drive (+ fuzz blend) at 2× oversampling → digital reorganizer → SRR1 → SRR2.

---

## Build and flash (firmware update)

Matches [dazed-and-con-fielded](https://github.com/jvictor0/dazed-and-con-fielded) except that the app directories are **`src/FroggersSolo`** and **`src/FroggersGuitar`** (not `src/Froggers`).

The two variants are separate builds with separate `build/` directories, so
building one does not disturb the other. Pick the one you want on the device.

### Toolchain

Building the firmware needs these host tools on `PATH`:

- `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-ar`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`
- `dfu-util`
- `make`

The build requires **Arm GNU Toolchain 14.3.rel1** installed under `/Applications/ArmGNUToolchain/14.3.rel1`. On Apple Silicon, download `arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi.tar.xz` from [Arm GNU downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads), extract it, and `sudo mv` the folder to that path.

`make` looks for `arm-none-eabi-g++` in `14.3.rel1/bin` or `14.3.rel1/arm-none-eabi/bin`. It does not use Homebrew or any other version on `PATH`.

**First time (or after libDaisy changes), from repo root:**

```sh
make vendor-libs
```

**Every firmware update:**

```sh
cd src/FroggersGuitar   # or: cd src/FroggersSolo
make clean
make
```

Put the Field in **DFU mode** (required every flash):

1. Connect USB (Seed port on the Field).
2. Hold **BOOT**.
3. Press **RESET** (or power-cycle) while holding **BOOT**.
4. Release **BOOT** — verify with `dfu-util -l` that **`[0483:df11]`** appears.

Then flash (from the same app directory):

```sh
make program-dfu
```

`program-dfu` uses `-s 0x08000000:leave` and resets into the new firmware when flash succeeds. That address comes from `APP_TYPE=BOOT_NONE`, the one supported build mode for apps in this repo — it links against `STM32H750IB_flash.lds` and targets internal flash at `0x08000000`. The underlying Daisy build system also has `BOOT_SRAM` and `BOOT_QSPI`, which link against `STM32H750IB_sram.lds` / `STM32H750IB_qspi.lds` and load through the bootloader at `0x90040000` instead — this repo does not build apps that way.

**Confirm which one you flashed:** page through with `SW1` / `SW2`. Guitar has
four pages and no Reverb; Solo has five.

### Updating the bootloader

The vendored bootloader binary is `External/libDaisy/core/dsy_bootloader_v6_4-intdfu-2000ms.bin` — the correct variant for a Field connected over its normal Seed USB port. Put the Field into STM32 ROM DFU mode with the same BOOT-button procedure above, then from the repo root:

```sh
make program-boot
```

That flashes the bootloader itself to `0x08000000` with `:leave`.

---

## Troubleshooting

- **`SW1` / `SW2` seem dead:** the tactile switch LEDs should light while each switch is held. If an LED lights but OLED page labels do not change (`V1VO` ↔ `PROB` ↔ `RVMX` …), the UI path is wrong; if neither LED lights, the switch or flash failed. Re-flash **`src/FroggersSolo`** or **`src/FroggersGuitar`** (not dazed `src/Froggers`). Old dazed firmware blocks page switches while **`A1..A7`** mod-assign is held.  
- **`SW1` stuck / always pressed (`r=1 p=1` on diag firmware):** a hardware fault on `D30`, not a firmware one — a pin audit cannot clear the raw stuck state. Firmware suppression stops runaway page flips but cannot fix a stuck switch.  
- **Intermittent slowness on `SW2` / randomize (`B1`–`B4`):** the firmware decouples control polling from OLED refresh (~30 FPS), rations LED I2C traffic to the same rate, and spreads a queued **Rand All** (`B2`/`B4`) over one page per poll so the loop stays fast under audio load. Bootloader is not involved (`BOOT_NONE` @ `0x08000000`).  
- **Weird knob behavior on a page:** check **`FUEG`** — high fuegoization is supposed to make small moves jumpy.  
- **Modulation seems dead on CV:** confirm cable and that the input is above the auto-bypass threshold.  
- **Marbles not changing:** you must press **`B5`**; it does not free-run.  
- **After editing firmware:** from the app directory you are building, run `clean → make`, DFU mode, then `program-dfu` before judging behavior.
