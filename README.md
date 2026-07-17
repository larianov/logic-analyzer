<div align="center">

# 🔬 RP2040 Logic Analyzer

**A PIO + DMA logic analyzer for the Raspberry Pi Pico that samples digital
signals up to 200 MHz and exports them to GTKWave.**

[![Platform](https://img.shields.io/badge/platform-RP2040-blueviolet)](https://www.raspberrypi.com/products/rp2040/)
[![Language](https://img.shields.io/badge/language-C%2B%2B-00599C)](https://isocpp.org/)
[![SDK](https://img.shields.io/badge/Pico%20SDK-2.2%2B-green)](https://github.com/raspberrypi/pico-sdk)
[![Output](https://img.shields.io/badge/output-FST%20%2F%20GTKWave-orange)](https://gtkwave.sourceforge.net/)
[![License](https://img.shields.io/badge/license-MIT-lightgrey)](https://opensource.org/license/mit)

![Logic Analyzer capture](docs/hero_web.mp4)

</div> 

---

## ✨ Features

- **Up to 200 MHz** signal sampling), 8 channels in parallel
- **Hardware-accelerated** — PIO does the sampling, DMA moves the data, the CPU stays free
- **Zero-jitter** captures when the sample rate is chosen correctly (see [Accuracy](#-accuracy))
- **Automatic PLL tuning** — picks the exact system clock needed for a requested rate
- **Auto-detects the Pico** over USB (by vendor ID) with a retrying handshake protocol
- **Host-side FST encoding** via `fstapi` — the Pico stays lean, the host builds the file
- **Terminal UI** built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — no GUI toolkit needed
- **Opens directly in GTKWave**, the standard waveform viewer

---

## 🚀 Quick Start

The project has two halves: **firmware** that runs on the Pico, and a **host TUI**
that configures the capture and pulls the data back over USB.

### Prerequisites

- Raspberry Pi Pico (RP2040)
- [CMake 3.25+](https://cmake.org/download/)
- [Pico SDK 2.2.0+](https://github.com/raspberrypi/pico-sdk) 
- [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 13.2.Rel1+
  (`arm-none-eabi-gcc`) — needs GCC 13's libstdc++ for `<format>`, since the shared
  `config.hpp` header uses `std::format` on both the firmware and host side
- A host compiler with C++20 `<format>` support — GCC 13+ (or Clang 17+ with libc++) —
  the whole project builds as **C++20**
- [`picotool`](https://github.com/raspberrypi/picotool) (optional — for flashing without BOOTSEL)
- [GTKWave](https://gtkwave.sourceforge.net/) to view captures
- Internet access on first host build — [FTXUI](https://github.com/ArthurSonzogni/FTXUI) (the TUI
  library) is pulled automatically via CMake `FetchContent`, no manual install needed

### 1. Build & flash the firmware

```bash
cmake --preset er              # "er" = embedded release → build-embedded/
cd build-embedded
make -j$(nproc) load           # builds + flashes via picotool
```

> **No `picotool`?** Build without `load`, then hold **BOOTSEL** while plugging in
> the Pico and copy the UF2 manually:
> ```bash
> make -j$(nproc)
> cp logic_analyzer.uf2 /media/$USER/RPI-RP2/
> ```

### 2. Build the host tool

```bash
cmake --preset hr              # "hr" = host release → build-host/
cd build-host
make -j$(nproc)
```

### 3. Run it

```bash
./host/logic_analyzer_host     # from inside build-host/
```

This opens the TUI. Configure the capture, hit record, and it writes `capture.fst`.

### 4. View the capture

```bash
gtkwave capture.fst
```

---

## 🖥 Using the TUI

![ screenshot: the host TUI — tui.png ](docs/tui.png)

The interface has four panels: **configuration**, **status**, **controls**, and
**errors**.

### Fields

| Field          | Format / range              | Meaning                                                        |
| -------------- | --------------------------- | -------------------------------------------------------------- |
| **Channel(s)** | `0–28`, e.g. `15` or `15+3` | Start pin, optionally `start+extra` to capture a range         |
| **Frequency**  | `1 Hz – 200 MHz`            | Sample rate — set to **signal_freq × 4** (see [Accuracy](#-accuracy)) |
| **Samples**    | `1 – 200,000`               | Number of samples to capture                                   |

> **Sample cap:** 200,000 samples is the ceiling, set by the Pico's RAM buffer —
> Enough for tens of milliseconds of capture even
> at 200 MHz.

> **Channel format:** `15` captures a single pin (gpio15). `15+3` captures a range
> — pin 15 plus 3 more (gpio15 → gpio18).

> **Forbidden pins:** GPIO **23–25** are rejected (single channel or as part of a
> `start+extra` range) — on most Pico boards these are wired to the onboard SMPS
> mode / VBUS-sense / onboard LED, so sampling them isn't useful. If your board
> doesn't use them for that, you can re-enable them by removing the two range
> checks in `confirm_channel()` in `host/src/parser.cpp` (the ones erroring with
> `"doesn't support 23-25 gpio pins"`).

### Keys

| Key                | Action              |
| ------------------ | ------------------- |
| `Tab` / `Shift+Tab`| Move between fields |
| `Enter`            | Confirm field       |
| `Ctrl+R`           | Start recording     |
| `Ctrl+Q`           | Quit                |

When a capture finishes, **Status** shows `Capture: DONE` and the output file, and
you can open it in GTKWave.

---

## 🧠 How It Works

The work is split across **two sides**: the Pico *captures* raw samples, and the
host *pulls them back and encodes* the FST file.

### On the Pico (capture)

```
Input pins ──▶ PIO ──▶ RX FIFO ──▶ DMA ──▶ RAM buffer
             (sample)  (4-deep)   (move)   (raw bytes)
```

1. **PIO** snapshots the input pins at the sample rate. Each snapshot is one sample.
2. **RX FIFO** is a tiny 4-word hardware queue the PIO pushes samples into.
3. **DMA** drains the FIFO into a RAM buffer without touching the CPU.

The Pico only ever holds **raw sample bytes** — it does no encoding.

### Over USB (transfer)

The host talks to the Pico over a USB serial link (`termios`, raw mode). It:

1. **Auto-detects the Pico** by scanning `/dev/tty*` for a device whose USB vendor
   ID is `2e8a` (Raspberry Pi), then confirms it with a **handshake** — sends
   `0x05` (ping), expects `0x06` (pong).
2. **Sends the capture config** (channels, rate, sample count) and waits for an
   `0x55` acknowledgement.
3. **Waits** roughly as long as the capture should take, then does a small
   **sync handshake** (`0x5A` → `0xA5`) to confirm the data is ready.
4. **Streams the raw buffer back** in chunks until all samples are received.

The whole exchange is retried (up to 6 attempts) if any step fails, so a flaky
cable or a mistimed capture recovers gracefully.

### On the host (encode)

```
raw buffer ──▶ decode per-channel bits ──▶ FST file ──▶ GTKWave
             (unpack samples)            (fstapi)      (view)
```

Only **after** the raw buffer arrives does the host encode it into an **FST file**
using **`fstapi`** (the FST writer library). It unpacks each sample into
per-channel bit changes and writes the timescale from the real (effective) sample
rate. GTKWave then opens the result.

> **Why encode on the host?** The Pico's RAM is small and its CPU is busy sampling —
> keeping it to raw capture and moving all the FST work to the host keeps captures
> fast and the firmware simple.

---

## ⚙️ Choosing the Sample Rate (`hz`)

> **`hz` is the *sample rate* — how often the pins are read — NOT the signal frequency.**

### The rule of thumb

For a signal of frequency **F**, set:

```
hz = F × 4
```

The ×4 ratio (4:1) is the sweet spot: correct duty cycle, immunity to jitter, and
reasonable DMA load.

| Signal frequency | Recommended `hz` | Ratio |
| :--------------: | :--------------: | :---: |
| 1 MHz            | 4 MHz+           | 4:1+  |
| 5 MHz            | 20 MHz           | 4:1   |
| 10 MHz           | 40 MHz (or 200)  | 4:1   |
| 20 MHz           | 80 MHz           | 4:1   |
| 25 MHz           | 100 MHz (or 200) | 4:1   |
| 30 MHz           | 120 MHz          | 4:1   |
| 50 MHz           | 200 MHz          | 4:1   |

### Typical protocols

| Protocol          | Signal   | Recommended `hz` |
| ----------------- | -------- | ---------------- |
| I²C Standard      | 100 kHz  | 1–10 MHz         |
| I²C Fast          | 400 kHz  | 2–10 MHz         |
| I²C Fast+         | 1 MHz    | 4–10 MHz         |
| I²C High Speed    | 3.4 MHz  | 20–40 MHz        |
| UART (≤3 Mbaud)   | ≤3 MHz   | 20–40 MHz        |
| SPI (slow)        | 1–10 MHz | 40–80 MHz        |
| SPI (fast)        | 20–50 MHz| 100–200 MHz      |

---

## 🎯 Accuracy

Whether a capture is pixel-perfect or slightly jittery comes down to how the
signal's period lines up with the sampling grid. Here's the full picture, with the
math.

### The one quantity that matters: samples per half-period

Everything below is governed by this:

```
samples_per_half_period = hz / (2 × F)
```

where `F` is the signal frequency and `hz` is the sample rate. For a 50% square
wave, a transition happens every half-period, so this number tells you how the
transition lines up with the sampling grid.

### 1. It must be a whole number (duty-cycle correctness)

If `samples_per_half_period` isn't an integer, transitions fall *between* samples
and the pulse width gets rounded — the duty cycle distorts.

**Worked example — 20 MHz signal at 100 MHz sampling:**

```
samples_per_half_period = 100 / (2 × 20) = 2.5   → NOT whole
```

A 5-sample period can't split into two equal halves, so the analyzer reports
`3 + 2` samples instead of `2.5 + 2.5` → a 30 ns / 20 ns pulse instead of
25 / 25. **Measured: ~0.2% of pulses wrong** (this is the "20 MHz duty was off"
case).

**Fix — same signal at 80 MHz sampling:**

```
samples_per_half_period = 80 / (2 × 20) = 2   → whole ✔  → perfect duty
```

### 2. It should be even (edge jitter / metastability)

Even when the number is a whole integer, **odd** values (3, 5, 7…) put the
transition *exactly on a sample boundary*. The sampler then catches the signal
mid-flip — a metastable moment — and the result flickers ±1 sample from period to
period. **Even** values put the transition safely between samples, so there's no
flicker.

```
samples_per_half_period   result
──────────────────────    ───────────────────────────
1  (2:1 Nyquist)          zero jitter (special case)
2, 4, 6, 8 … (even)       zero jitter  ✔
3, 5, 7 …    (odd > 1)    ±1 sample jitter  ✗
```

**Measured proof (all at clk_sys = 200 MHz):**

| Signal  | `hz`    | samples / half-period | Parity        | Real error |
| :-----: | :-----: | :-------------------: | :-----------: | :--------: |
| 10 MHz  | 200 MHz | 10                    | even          | 0.000%     |
| 20 MHz  | 200 MHz | 5                     | **odd**       | ~0.2%      |
| 25 MHz  | 200 MHz | 4                     | even          | 0.000%     |
| 50 MHz  | 200 MHz | 2                     | even          | ~0.002%    |

> **The golden rule:** make `hz` a multiple of `F × 4`. Then
> `hz / (2 × F) = 2 × k` — always even — so the duty is correct *and* there's no
> edge jitter, in a single condition.

### 3. The sampling must be evenly spaced (integer divider)

This is the strongest effect and connects back to the
[three clock cases](#-the-base-clock--how-sampling-frequency-is-resolved). If the
PIO ends up on a **fractional** divider, samples land at uneven intervals and jitter
explodes — this dwarfs the parity effect above.

That's why Case A and Case B (integer divider) give clean results, and why you
should never force a rate that lands on a fractional divider. The firmware avoids
this by re-tuning `clk_sys`, but the takeaway is:

> A rate on an **integer** divider (Cases A/B) is essential. Parity (rule 2) only
> matters *after* this is satisfied.

### 4. Phase drift between two separate boards

If the signal comes from a **separate** board (a second Pico, a signal generator),
that source has its own crystal, so its clock slowly drifts against the analyzer's.
At the **2:1 Nyquist limit** this drift can make the signal "collapse" into flat
stretches — a stroboscopic effect where both samples of a period briefly land on the
same level. **Use a 4:1 margin or higher for external sources** so there are always
samples on both HIGH and LOW.

### 5. Breadboard noise

Breadboards badly distort fast edges (tens of MHz) via parasitic capacitance and
poor contacts, adding real jitter to the transition instant. Removing the
breadboard alone took one test from visible jitter to **0 errors on 200 000
samples**. **Use direct wiring or soldered connections for high-frequency signals.**

### ✅ The zero-jitter recipe

Put it all together — for a signal of frequency `F`:

1. **Sample rate:** `hz = F × 4` (even samples-per-half-period → correct duty, no edge jitter)
2. **Clock:** make sure `hz` lands on an integer divider — a rate that divides
   200 MHz (Case A), or one the PLL re-tunes exactly (Case B)
3. **Wiring:** direct connection, no breadboard (for high frequencies)
4. **External sources:** keep a 4:1 margin or more (avoid the 2:1 Nyquist edge)

### Verified configurations

| Signal  | clk_sys | `hz`    | Ratio | Case | Result |
| :-----: | :-----: | :-----: | :---: | :--: | :----: |
| 100 MHz | 200 MHz | 200 MHz | 2:1   | A    | 100%   |
| 50 MHz  | 200 MHz | 200 MHz | 4:1   | A    | ~100%  |
| 25 MHz  | 200 MHz | 200 MHz | 8:1   | A    | 100%   |
| 10 MHz  | 200 MHz | 200 MHz | 20:1  | A    | 100%   |
| 30 MHz  | 120 MHz | 120 MHz | 4:1   | B    | 100%   |

---

## 🕹 The Base Clock & How Sampling Frequency Is Resolved

The firmware runs at a **fixed `clk_sys` of 200 MHz** by default (it draws a bit
more core voltage to reach this, handled automatically at boot). The PIO samples
straight off this clock, so the fastest possible sample rate is 200 MHz.

When you request a sample rate, one of **three things** happens depending on how
that rate relates to 200 MHz. This is the core mechanism of the analyzer:

### Case A — the rate divides 200 MHz evenly ✅

If `200 MHz % hz == 0`, the PIO just divides the 200 MHz clock by an integer. The
sampling is perfectly even and nothing special happens.

```
hz = 50 MHz  →  200 / 50 = 4   (integer divider, even spacing)  ✔
```

Rates in this family: **200, 100, 50, 40, 25, 20, 10, 8, 5, 4, 2, 1 MHz…**
These are the "free" rates — always use one if you can.

### Case B — the rate doesn't divide 200 MHz, but the PLL can hit it exactly 🔧

Some useful rates (like 120 MHz) don't divide 200 MHz. Instead of falling back to
a jittery fractional divider, the firmware **re-tunes `clk_sys` itself** via the
PLL so the requested rate becomes an exact integer division. This is what
`calculate_best_pll()` does — it searches PLL parameters for a system clock that
produces *exactly* the rate you asked for.

```
Request hz = 120 MHz
  200 % 120 ≠ 0  →  can't divide evenly at 200 MHz
  calculate_best_pll(120 MHz) searches PLL settings…
    → VCO = 480 MHz, post-dividers 1×4, fbdiv 40
    → clk_sys = 120 MHz exactly, PIO divider = 1
  Result: sampling at exactly 120 MHz, evenly spaced.  ✔
```

Here **your requested frequency is honored exactly** — only the base clock moves.
The PIO divider stays integer, so there's no jitter from uneven spacing.

### Case C — no exact clock exists, so the *rate* is nudged ⚠️

For some requests the PLL can't produce an exact match. `calculate_best_pll` then
returns the **closest** clock it can build, and the effective sample rate becomes
whatever that clock actually gives:

```
best_sys_clk = calculate_best_pll(hz)   // closest achievable, not exact
pio_div      = best_sys_clk / hz         // integer division
effective_hz = best_sys_clk / pio_div    // the REAL rate you get
```

**Worked example — you request 137 MHz:**

```
calculate_best_pll(137 MHz):
  searches PLL settings, no exact 137 MHz clock exists
  closest buildable: clk_sys = 136,800 MHz  (fbdiv/post-dividers chosen)
  pio_div = 1
  effective_hz = 136,800 MHz          → 200 kHz below what you asked
```

So you typed **137 MHz**, the analyzer actually samples at **136.800 MHz** — off by
~0.15%. The samples are still evenly spaced (integer divider), and the host uses this
*effective* rate for the FST timescale, so the waveform timing stays correct — the
rate is just not the exact number you typed.

**What "off by a bit" looks like in practice:**

| You request | You actually get (clk_sys = effective_hz) | Error   |
| :---------: | :---------------------------------------: | :-----: |
| 137 MHz     | 136.800 MHz                               | +0.15%  |
| 143 MHz     | 142.800 MHz                               | −0.14%  |
| 167 MHz     | 166.500 MHz                               | −0.30%  |
| 179 MHz     | 178.500 MHz                               | −0.28%  |

The nudge is tiny (a fraction of a percent) and only shows up for "awkward"
in-between rates. Stick to Case A/B rates and you never hit this.

### Which case am I in?

| You want                        | Case | What moves         | Requested `hz` honored? |
| ------------------------------- | :--: | ------------------ | :---------------------: |
| A rate that divides 200 MHz     | A    | nothing            | ✅ exactly              |
| A rate the PLL can hit exactly  | B    | `clk_sys`          | ✅ exactly              |
| A rate with no exact clock      | C    | `clk_sys` + `hz`   | ⚠️ nudged to nearest   |

---

## 📈 FST & GTKWave

### What is FST?

FST (Fast Signal Trace) is GTKWave's native binary format. Each pin is a separate
variable, and only state *changes* are stored, keeping files small.

The time axis is computed automatically from the effective sample rate — you don't
need to set anything. Even in [Case C](#case-c--no-exact-clock-exists-so-the-rate-is-nudged-)
(where the rate is nudged) the timing comes out correct, because the host derives
the timescale from the *real* rate, not the one you typed.

### Opening a capture

```bash
gtkwave capture.fst
```

1. In the left **SST** panel, pick the signal group (e.g. `GPIO`).
2. The pin list appears below (`gpio0`, `gpio1`, …).
3. Select the pins you want and click **Append** (or drag them into **Waves**).
4. Scroll / use the zoom buttons to navigate.

![ screenshot: signal selection panel — gtkwave-signals.png ](docs/capture.png)

### Measuring frequency & duty cycle

1. Left-click on an edge (e.g. the start of a HIGH pulse) to place the cursor.
2. Place a marker on the next identical edge (one full period).
3. GTKWave shows the time delta at the top (`Marker: B±…`).
4. `frequency = 1 / period`.

For duty cycle, measure the HIGH width separately and divide by the full period.

![ screenshot: measuring a period with markers — gtkwave-measure.png ](docs/marker.png)

> **Note:** when zoomed far out, GTKWave may render pulses with pixel artifacts
> that look like width "jitter." That is a drawing artifact, not real jitter —
> analyze the raw samples for a true measurement.

---

## ⚠️ Things to Keep in Mind

<details>
<summary><b>The first & last pulse are always clipped</b></summary>

A capture starts and ends at an arbitrary point in the signal's phase, so the very
first and last pulses are usually incomplete. Ignore the edge pulses when reading a
capture — everything in between is accurate.
</details>

<details>
<summary><b>"Jitter" when zoomed far out is usually a drawing artifact</b></summary>

When many periods are squeezed into a few pixels, GTKWave rounds edges to the pixel
grid, which can look like the pulse width is wobbling. Zoom in (or check the raw
data) before concluding there's real jitter — see [Accuracy](#-accuracy) for what
real jitter looks like and how to avoid it.
</details>

<details>
<summary><b>Awkward frequencies get nudged slightly</b></summary>

If you request an in-between rate the PLL can't hit exactly (e.g. 137 MHz), the
analyzer samples at the closest achievable rate instead (136.800 MHz). The timing
stays correct — it's just not the exact number you typed. Stick to rates that are a
multiple of your signal × 4 and you won't notice this. See
[Case C](#case-c--no-exact-clock-exists-so-the-rate-is-nudged-).
</details>

---

## 📂 Repository Structure

```
.
├── embedded/            # firmware (runs on the Pico)
│   ├── include/
│   ├── src/             # capture loop, PIO/DMA, PLL tuning
│   └── CMakeLists.txt
├── host/                # host-side TUI (config + FST export)
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── CMakeLists.txt
├── CMakePresets.json    # "er" = embedded release, "hr" = host release
├── pico_sdk_import.cmake
└── README.md
```

---

## 🗺 Roadmap

- [ ] Configurable trigger conditions
- [ ] Protocol decoders (I²C / SPI / UART)
- [ ] Streaming capture (beyond RAM buffer size)
- [ ] Web-based viewer

---

## 📜 License

Released under the MIT License. See [`LICENSE`](LICENSE) for details.

<!-- TODO: add a LICENSE file and update the badge at the top -->

---

<div align="center">
<sub>Built with PIO, DMA, and a lot of oscilloscope-free debugging.</sub>
</div>
