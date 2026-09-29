// Synth AudioWorklet processor with one mu2000 instance inside.
// Synchronous emulation needs a fast CPU.
// MIDI applies within one quantum this way.
// No grouping, no lookahead lag.
// Booting runs chunked across quanta (silence out), then realtime audio.
// MIDI arrives as page messages.
// Static loader import only: dynamic import is disallowed on the scope.
import type { SmuModule } from "../smu-types.ts";
import type { MainToWorklet, WorkletToMain } from "./protocol.ts";
import { BUILD_TAG } from "./build-tag.ts";
import { loadSmu } from "./smu-standalone.ts";

const rate = 44_100;
const bootCap = 30 * rate;
// Small boot slices keep every render quantum short.
// A full second per quantum blocks the audio thread far too long.
// Browsers then stop calling the processor, stalling boot silently.
const bootSlice = 512;
const scratchFrames = 8192;

const moduleReady: Promise<SmuModule> = (async () => {
    const emu = await loadSmu();
    console.info("[worklet] wasm module loaded");
    return emu;
})();

// Module-load marker: proves which bundle the browser actually runs.
// Worklet rate is logged alongside it.
// Stale dist is the usual suspect: this line tells which build runs.
console.info(`[worklet] processor ${BUILD_TAG}, sampleRate=${sampleRate}`);

interface PendingMidi {
    port: number;
    bytes: number[];
}

function post(port: MessagePort, message: WorkletToMain): void {
    port.postMessage(message);
}

class SmuProcessor extends AudioWorkletProcessor {
    private emu: SmuModule | undefined;
    private state: "idle" | "booting" | "live" = "idle";
    private booted = 0;
    private bootSecond = -1;
    private reportedFatal = false;
    private readonly midi: PendingMidi[] = [];
    private outPtr = 0;
    // Resampling FIFO for non-44100 device rates.
    // Holds rendered float frames plus a fractional read position.
    // The FIFO is topped up every quantum, so nothing is ever lost.
    private readonly fifoLeft: number[] = [];
    private readonly fifoRight: number[] = [];
    private fifoPosition = 0;
    private loggedFirstQuantum = false;

    public constructor() {
        super();
        this.port.addEventListener("message", (event: MessageEvent): void => {
            void this.handleMessage(event.data);
        });
        // AddEventListener does not start the port (unlike onmessage).
        // Without this call, page-to-worklet messages never arrive.
        this.port.start();
    }

    private async handleMessage(data: unknown): Promise<void> {
        if (typeof data !== "object" || data === null) return;
        const message = data as MainToWorklet;
        switch (message.type) {
            case "init": {
                await this.init(message.roms);
                break;
            }
            case "midi": {
                if (this.state === "live" && this.midi.length < 4096) {
                    this.midi.push({
                        port: message.port,
                        bytes: message.bytes
                    });
                }
                break;
            }
            case "panic": {
                this.panic();
                break;
            }
        }
    }

    private async init(
        roms: { kind: number; data: ArrayBuffer }[]
    ): Promise<void> {
        try {
            console.info(`[worklet] init with ${roms.length} roms`);
            const emu = await moduleReady;
            this.emu = emu;
            if (emu._smu_init(0) < 0) throw new Error("init failed");
            for (const rom of roms) {
                const bytes = new Uint8Array(rom.data);
                const pointer = emu._malloc(bytes.length);
                emu.HEAPU8.set(bytes, pointer);
                const result = emu._smu_set_rom(
                    rom.kind,
                    pointer,
                    bytes.length
                );
                emu._free(pointer);
                if (result < 0) throw new Error("bad ROM");
            }
            if (emu._smu_reset() < 0) throw new Error("reset failed");
            console.info("[worklet] reset done, booting across quanta");
            this.outPtr = emu._malloc(scratchFrames * 4);
            this.booted = 0;
            this.bootSecond = -1;
            this.reportedFatal = false;
            this.state = "booting";
            // Heartbeat through the port, independent of process().
            // Delivery proof: this post arrives even if quanta never run.
            post(this.port, {
                type: "boot",
                fraction: 0,
                text: "Boot starting…"
            });
        } catch (error) {
            console.info(
                `[worklet] init failed: ${error instanceof Error ? error.message : String(error)}`
            );
            post(this.port, {
                type: "error",
                message: error instanceof Error ? error.message : String(error)
            });
        }
    }

    private panic(): void {
        const emu = this.emu;
        if (emu === undefined || this.state !== "live") return;
        // All-notes-off on every channel of every port.
        const message = new Uint8Array(3);
        const pointer = emu._malloc(3);
        for (let port = 0; port < 4; port++) {
            for (let channel = 0; channel < 16; channel++) {
                message[0] = 0xb0 | channel;
                message[1] = 123;
                message[2] = 0;
                emu.HEAPU8.set(message, pointer);
                emu._smu_midi_in(port, pointer, 3);
            }
        }
        emu._free(pointer);
    }

    // Fresh int16 view over the scratch buffer.
    // The wasm buffer can grow, so views are never kept.
    private view(emu: SmuModule): Int16Array {
        return new Int16Array(
            emu.HEAPU8.buffer,
            this.outPtr,
            scratchFrames * 2
        );
    }

    private renderResampled(
        emu: SmuModule,
        left: Float32Array,
        right: Float32Array
    ): void {
        const ratio = rate / sampleRate;
        // Top the FIFO up so the whole quantum interpolates from real frames.
        const want = Math.ceil(this.fifoPosition + left.length * ratio) + 1;
        let need = want - this.fifoLeft.length;
        while (need > 0) {
            const got = emu._smu_render_frames(
                this.outPtr,
                Math.min(512, need, scratchFrames)
            );
            if (got <= 0) break;
            const scratch = this.view(emu);
            for (let index = 0; index < got; index++) {
                this.fifoLeft.push(scratch[index * 2] / 32_768);
                this.fifoRight.push(scratch[index * 2 + 1] / 32_768);
            }
            need -= got;
        }
        for (let index = 0; index < left.length; index++) {
            const position = this.fifoPosition + index * ratio;
            const base = Math.floor(position);
            const frac = position - base;
            const left0 = this.fifoLeft[base] ?? 0;
            const left1 = this.fifoLeft[base + 1] ?? left0;
            const right0 = this.fifoRight[base] ?? 0;
            const right1 = this.fifoRight[base + 1] ?? right0;
            left[index] = left0 + (left1 - left0) * frac;
            right[index] = right0 + (right1 - right0) * frac;
        }
        this.fifoPosition += left.length * ratio;
        // Compact the consumed prefix once in a while.
        if (this.fifoPosition > 4096) {
            const drop = Math.floor(this.fifoPosition);
            this.fifoLeft.splice(0, drop);
            this.fifoRight.splice(0, drop);
            this.fifoPosition -= drop;
        }
    }

    // One quantum: boot chunks run here with silence out.
    // Live quanta drain the MIDI queue first, then render to float.
    // Never throws on the audio thread.
    // A throw would kill the processor silently.
    // Failures report to the page once instead.
    // Private work method first: class member order is enforced by lint.
    // The first parameter is inputs (always empty here).
    // Outputs come second: mixing them up stalls boot silently.
    private processInner(
        _inputs: Float32Array[][],
        outputs: Float32Array[][]
    ): boolean {
        if (!this.loggedFirstQuantum) {
            this.loggedFirstQuantum = true;
            console.info("[worklet] first quantum");
        }
        const [left, right] = outputs[0] ?? [];
        if (left === undefined || right === undefined) return true;
        const emu = this.emu;
        if (emu === undefined || this.state === "idle") {
            left.fill(0);
            right.fill(0);
            return true;
        }
        if (this.state === "booting") {
            const remaining = bootCap - this.booted;
            if (remaining <= 0) {
                this.state = "idle";
                post(this.port, {
                    type: "error",
                    message: "firmware did not boot"
                });
                return true;
            }
            const ran = Math.min(bootSlice, remaining);
            const ready = emu._smu_run_blank(ran);
            this.booted += ran;
            left.fill(0);
            right.fill(0);
            if (ready !== 0) {
                this.state = "live";
                console.info("[worklet] live");
                post(this.port, { type: "live" });
            } else if (this.booted >= bootCap) {
                this.state = "idle";
                post(this.port, {
                    type: "error",
                    message: "firmware did not boot"
                });
            } else {
                // Progress posts stay rare: one per emulated second.
                // Every-quantum posts would flood the main thread
                // (about 345 quanta/s at 128 frames).
                const second = Math.floor(this.booted / rate);
                if (second !== this.bootSecond) {
                    this.bootSecond = second;
                    post(this.port, {
                        type: "boot",
                        fraction: this.booted / bootCap,
                        text: `Booting… ${second} s`
                    });
                }
            }
            return true;
        }
        // No copy: message events cannot interleave a running quantum.
        for (const message of this.midi) {
            const pointer = emu._malloc(message.bytes.length);
            emu.HEAPU8.set(message.bytes, pointer);
            emu._smu_midi_in(message.port, pointer, message.bytes.length);
            emu._free(pointer);
        }
        this.midi.length = 0;
        if (sampleRate === rate) {
            const got = emu._smu_render_frames(this.outPtr, left.length);
            const scratch = this.view(emu);
            for (let index = 0; index < got; index++) {
                left[index] = scratch[index * 2] / 32_768;
                right[index] = scratch[index * 2 + 1] / 32_768;
            }
            return true;
        }
        this.renderResampled(emu, left, right);
        return true;
    }

    public override process(
        _inputs: Float32Array[][],
        outputs: Float32Array[][]
    ): boolean {
        try {
            return this.processInner(_inputs, outputs);
        } catch (error) {
            const message =
                error instanceof Error && error.stack !== undefined
                    ? error.stack
                    : String(error);
            console.info(`[worklet] process failed: ${message}`);
            const [left, right] = outputs[0] ?? [];
            left?.fill(0);
            right?.fill(0);
            if (!this.reportedFatal) {
                this.reportedFatal = true;
                this.state = "idle";
                post(this.port, { type: "error", message });
            }
            return true;
        }
    }
}

registerProcessor("smu-synth", SmuProcessor);
