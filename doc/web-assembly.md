# WebAssembly port — audio only (no GUI)

Compiling the S-MU2000 audio core to WebAssembly.
Out of scope: panel/GUI, VST3/CLAP/AU, DAW plugins in browser.

## Status

- [x] **Step 1 — Node render.** `emcc` build of the core + thin glue that renders MIDI to WAV under Node. ROMs and MIDI passed in as memory buffers (done 2026-09-29, see "Step 1" below).
- [x] **Step 2 — Web render.** Browser UI: upload ROMs + MIDI, render offline, download/play WAV. Reuses the Step 1 module (done 2026-09-29, see "Step 2" below).
- [x] **Step 3 — Web live.** WebMIDI inputs (up to 4 → ports A–D) +
  synchronous worklet synth, direct MIDI response (done 2026-09-30;
  fast CPU required, dropouts on slower hardware).

Test data: ROMs in `roms/` (gitignored, never committed). Example MIDI: `ONTILMOR.mid` (repo root).

## Tool status (checked 2026-09-29)

| Need                         | Status                                                           |
| ---------------------------- | ---------------------------------------------------------------- |
| `emcc 6.0.9`                 | present (`C:/.../emsdk/upstream/emscripten/`)                    |
| `node v26.7.0`               | present                                                          |
| `cmake 4.4.3`, `python 3.13` | present                                                          |
| `g++`                        | missing on this machine — not needed, wasm path uses `emcc` only |
| ROMs                         | in `roms/` (gitignored, correct)                                 |
| Test MIDI                    | present (`ONTILMOR.mid`)                                         |

Nothing to download.

## Why the core should compile almost as-is

- **JIT off on wasm32** (confirmed by building). `src/mame/cpu/sh2_jit.cpp:36` enables the SH2 JIT only on `__x86_64__`/`__aarch64__`/`_M_X64`; `src/mame/sound/swp30_jit.cpp:31-36` follows the same pattern. `emcc` targets wasm32 (`__wasm32__`/`__EMSCRIPTEN__`), so both fall back to `SMU2000_*_JIT 0` = interpreter path (`sh.cpp`, `swp30.cpp`). `compat/exec_mem.h` (`mmap`/`VirtualAlloc`/`MAP_JIT`) is then uncompiled — no source changes were needed here.
- **Threads avoidable.** `mu2000::set_threaded(false)` keeps everything single-threaded (`render.cpp:354` already does this with `--single`). The `std::atomic::wait/notify` slave path (`mu2000.cpp:341-373`) is then never entered. So **no `-pthread`, no SharedArrayBuffer/COOP/COEP** for steps 1–2.
- **Platform shims are no-ops.** `compat/platform.h:76` (`denormals_off`) is guarded by `__SSE2__`/`__x86_64__` — inactive under wasm. `perf_ticks()` falls back to `steady_clock`. `getenv` returns null, fine.
- **Two gaps to close with small new code (no core edits):**
  1. ROM/MIDI loading is `fopen`-path based (`mu2000::load_program/load_wave`, `smf::load(path)`). Wasm receives `ArrayBuffer`s from JS, so add: feed `set_program_rom/set_wave_rom/set_sintab_rom` (`mu2000.h:52-56`) from vectors + a wave-interleave helper (4×8 MB → 32 MB, logic copied from `mu2000.cpp:414-443`) + `smf::load_from_memory(const u8*, size_t)` (split file read from parse in `smf.cpp:61-182`).

## Step 1 — Node render (done 2026-09-29)

Files: `src/wasm/wasm_render.cpp`, `web/src/render.ts`, `web/scripts/build.ts`
(`npm run build:wasm` in `web/`), TS + eslint + prettier suite copied from
`../stb-vorbis`.

Run from `web/`: `npm run build:wasm`, then
`npm run render -- --roms ../roms ../ONTILMOR.mid out.wav 5`.

Verified: 5 s and 20 s of `ONTILMOR.mid` render **byte-identical** to native
`build/render.exe --single` (interpreter == JIT). Speed ~0.7× realtime at
`-O3` (link) / `-O2` (objects). The build was also validated on Linux (WSL,
system emscripten 6.0.9), which caught and fixed an `em++` discovery bug
(Debian layout uses `em++`, not `emcc++`).

New files:

- `src/wasm/wasm_render.cpp` — C API with `EMSCRIPTEN_KEEPALIVE`, no embind dependency:
  - `smu_init()` — create one `mu2000`, `set_threaded(false)`, `set_usb_host(...)`.
  - `smu_set_rom(kind, ptr, len)` — kinds: program (4 MB), wave0–3 (8 MB each, interleaved on finalize), sintab (64 KB). Returns error code, `smu_error()` string for details.
  - `smu_reset()` — `mu.reset()` after all ROMs set.
  - `smu_boot(max)` / query `smu_midi_ready()` — boot-wait loop mirror of `render.cpp:376-393`. `smu_run_blank(n)` runs n samples discarding output (chunked boot for pages/worklet).
  - `smu_load_midi(ptr, len)` — parse SMF from memory, store event list.
  - `smu_midi_in(port, bytes...)` — live path (also used by step 3).
  - `smu_render_frames(out_ptr, nframes, ...)` — feed due MIDI events by sample clock, `run_sample` loop (`render.cpp:571-577`), write interleaved int16 stereo.
- `web/scripts/build.ts` (`tsx scripts/build.ts`) — incremental `emcc` objects to `web/out/obj/`, `em++` link to `web/out/smu_render.mjs` (+ generated `smu_render.d.mts`): `-O3 -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_ES6=1`, core sources + `wasm_render.cpp` + `smf.cpp`, no `-pthread`, no SDL. `emcc`/`em++` discovery mirrors stb-vorbis (`EMCC`/`EMSDK` env, sibling `../emsdk`, `~/emsdk`, `PATH`).
- `web/src/render.ts` — Node driver: read ROM/MIDI files, call the module, write WAV. Types for the generated module in `web/src/smu-types.ts`.
- `smf::load_from_memory` — refactor: keep `load(path)` as read-file + new `load_from_memory` parse entry.

Test: `npm run render -- --roms ../roms ../ONTILMOR.mid out.wav`, byte-compare vs native `build/render.exe` output (interpreter vs JIT must be bit-exact). `blocktime` gives the expected ~5–8× slowdown baseline.

## Step 2 — Web render (done 2026-09-29)

Files: `web/public/render.html` (page source), `web/src/browser/render-app.ts`
(bundled minified by `web/scripts/build-page.ts` into `web/dist/render.js`),
`web/src/common/wav.ts` (WAV encoder shared with the CLI),
`web/src/browser/roms.ts` + `idb.ts` (ROM picker card + local persistence).

Run from `web/`: `npm run build:page`, then serve `dist/` over HTTP
(`npm run serve`, or `python3 -m http.server -d dist`) and open the page.
The page: one ROM folder picker (`webkitdirectory`, files matched by basename
so `roms/` drops in as-is; sine table optional), one MIDI picker, Render
button, progress bar, `<audio>` preview + WAV download. Picked ROMs persist
in IndexedDB (Forget button clears them). Render is chunked on the main
thread (2 s audio per chunk, `setTimeout` yields); boot uses 1-sample
`smu_run_blank` steps so it ends on the exact ready sample.

Notes:

- The browser uses a standalone wasm (`build-standalone.ts`: same sources,
  `-sSTANDALONE_WASM=1`, no JS glue), embedded as base64 and loaded by
  `src/browser/smu-standalone.ts` — no `.wasm` fetch, so AudioWorklets work
  and `file://` is only blocked by worklet loading. The Node CLI keeps the
  glue module (`out/smu_render.mjs`).
- Manual instantiation must call the `_initialize` export (static
  constructors). Miss it and `attotime::never` reads as zero, so
  never-expiring timers fire immediately: the synth boots but stays silent.
  Found by comparing SWP register streams (identical) against render output
  (silent), then bisecting link flags to exonerate the optimizer.
- Boot exactness matters: chunked blank-boot overshoots the ready sample by
  up to one chunk (1827 samples observed), which audibly changes the render
  (108k of 441k samples differed, max 2942). 1-sample steps cost ~7 s for the
  ~351k-sample boot and render bit-identical to the CLI.

## Step 3 — Web live (2026-09-29, direct worklet design, needs fast CPU)

Files: `web/public/live.html`, `web/src/browser/live-app.ts` (page),
`web/src/browser/processor.ts` (synth worklet, bundled to
`dist/smu-processor.js`), `web/src/browser/protocol.ts` (page/worklet
message types), `web/src/browser/build-tag.ts` (shared build tag logged
by both bundles), `web/scripts/worklet-harness.mjs` (`npm run harness`).

The synth instance lives in the worklet and renders synchronously, one
128-frame quantum at a time: the page posts ROMs (transferred), then MIDI
bytes per port, and MIDI applies within the same quantum — no grouping,
no lookahead lag. Boot runs chunked across quanta (512-sample slices,
silence out, progress posted back roughly once per emulated second).
The wasm module loads via static `import` of the loader (dynamic
`import()` is disallowed on `WorkletGlobalScope`); the AudioContext
opens at the device native rate with a linear-resampling FIFO for
non-44100 rates. This design needs a fast CPU (desktop Chromium measures
about realtime); slower machines miss every quantum and Chromium
discards the audio while Firefox plays late buffers.

WebMIDI inputs map to ports A–D via per-device dropdowns (first four
default to A–D, several devices may share a port); per-port activity dots,
message counters and a last-message readout visualize traffic; Panic sends
all-notes-off on every channel of every port. A Test tone button (plain
oscillator, bypasses everything) tells downstream silence (device/mute/
policy — browsers mute per origin) apart from synth silence.

Validated with the worklet harness in Node (real ROMs, real
`process(inputs, outputs)` arity): boot → live → note-on (audible,
rms ~0.02) → `HARNESS OK`. Played from real browsers and a MIDI file
player: Firefox fine; Chromium fine on a Ryzen 5800X3D, dropouts on
slower hardware.

Worker render-ahead detour (tried 2026-09-30, reverted same day): synth
in a Worker rendering ~1 s ahead, thin worklet player with ack pacing.
Survived Chromium dropouts on slow hardware, but MIDI lagged up to the
lookahead and grouped into bursts (queue dumped per chunk), which is
worse for live playing than occasional dropouts on fast hardware.
Reverted to the direct design; nothing of it remains in the tree.

First design (AudioWorklet owns the synth) and what it taught:

- Boot ran a full second of emulation per `process()` call
  (`_smu_run_blank(44100)` ≈ 470 ms wall on a fast desktop, 160× the
  ~2.9 ms quantum budget). Both browsers stopped calling the processor:
  stall with no error and not even one progress message. Fix was 512
  samples per quantum — but that only cured boot, not live rendering.
- `process(outputs)` signature bug: the browser calls
  `process(inputs, outputs, parameters)`, so the single parameter
  received `inputs` (always `[]` with `numberOfInputs: 0`) and boot
  never ran anywhere. The Node harness masked it by passing buffers as
  the first argument. Always drive test doubles with the real arity.
- `performance.now()` is absent from the AudioWorklet scope in some
  browsers (`ReferenceError`); slice timing used `Date.now()`.
- Measured live cost 13–36 ms per 128-frame quantum (4–12× over
  budget) on Chromium: synth rendered (peaks to 0.23) yet zero sound.
  Firefox plays late buffers; Chromium discards them — total dropout
  from deficit that ~1× emulation can never repay. Hence the worker
  redesign; no JS-gluing survives a 5× deficit.
- The SH2/MEG JITs cannot run under WebAssembly: they emit native
  machine code into executable memory, which the sandbox forbids
  (both already compile to 0 on wasm32). Codegen is maxed (`-O3`);
  speed must come from architecture (render-ahead), not flags.

## Risks / notes

- Interpreter is ~5–8× slower than JIT; fine for offline render. For live
  it measures ~1× realtime on desktop Chromium (boot pace) with 13–36 ms
  worst quanta — survivable only via worker render-ahead (step 3).
- ROMs are never bundled, committed, or baked into the wasm — always user-supplied at runtime, persisted locally in IndexedDB after the first pick.
- `bootcache.h`/`nvram.h`/`voicecache` disk persistence deferred; every boot starts from scratch like `render.cpp` default.
